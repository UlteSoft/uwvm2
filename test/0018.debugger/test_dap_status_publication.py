#!/usr/bin/env python3
"""DAP wire publication/lifetime regression; modeled replies grant no JIT authority."""
import importlib.util
import io
import json
from pathlib import Path
import unittest


path = Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py"
spec = importlib.util.spec_from_file_location("dap_status_publication", path)
dap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dap)
STATUS = ("stopped: native instruction step\nstop-id 17\n"
          "thread 3 module=0 function=2 byte-offset=4 generation=8\n"
          "  native-pc=0x0000000000001234\n")


def messages(data):
    stream = io.BytesIO(data)
    rows = []
    while header := stream.readline():
        assert header.startswith(b"Content-Length: ") and header.endswith(b"\r\n")
        length = int(header[16:-2])
        assert stream.readline() == b"\r\n"
        payload = stream.read(length)
        assert len(payload) == length
        rows.append(json.loads(payload))
    return rows


class Broker:
    def __init__(self):
        self.target = None
        self.reply = STATUS
        self.commands = []

    def request(self, text):
        self.commands.append(text)
        if text == self.target:
            return self.reply
        if text in ("status", "bt 3"):
            return STATUS
        raise AssertionError(text)


class StatusPublicationTests(unittest.TestCase):
    # Each route publishes a complete runtime snapshot, not a scalar/watch reply.
    def routes(self):
        yield "native", "pause", {}, "pause"
        yield "native", "continue", {"threadId": 3}, "continue"
        for level in ("native", "source", "wasm"):
            for command, policy in (("stepIn", "into"), ("next", "over"), ("stepOut", "out")):
                if level == "native":
                    wire = {"stepIn": "step asm 3", "next": "ni 3", "stepOut": "finish asm 3"}[command]
                elif level == "source":
                    wire = f"step source 3 {policy}"
                else:
                    wire = "step wasm 3" + ("" if command == "stepIn" else f" {policy}")
                yield level, command, {"threadId": 3}, wire
        for text in ("status", " wait ", "ni", "finish asm 3"):
            yield "native", "evaluate", {"context": "repl", "expression": text}, text

    def prepare(self, level):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.adapter.step_level = level
        self.sequence = 0
        frame = self.send("stackTrace", {"threadId": 3})[0]["body"]["stackFrames"][0]["id"]
        scope = self.send("scopes", {"frameId": frame})[0]["body"]["scopes"][0]["variablesReference"]
        return frame, scope

    def send(self, command, arguments):
        self.sequence += 1
        self.output.seek(0)
        self.output.truncate()
        self.adapter.handle({"seq": self.sequence, "command": command, "arguments": arguments})
        return messages(self.output.getvalue())

    def test_malformed_runtime_reply_has_one_failure_and_no_events(self):
        invalid = (
            STATUS.replace("0x0000000000001234", "0x0"),
            STATUS.replace("stop-id 17", "stop-id 17x"),
            STATUS + "running\n",
            "native finish from=0x1234 to=0x5678\n" + STATUS.replace("0x0000000000001234", "0x0"),
        )
        for level, command, arguments, wire in self.routes():
            for reply in invalid:
                with self.subTest(level=level, command=command, wire=wire, reply=reply):
                    frame, scope = self.prepare(level)
                    self.broker.target, self.broker.reply = wire, reply
                    rows = self.send(command, arguments)
                    responses = [row for row in rows if row["type"] == "response"]
                    self.assertEqual(len(responses), 1, rows)
                    self.assertFalse(responses[0]["success"], rows)
                    self.assertEqual(responses[0]["request_seq"], self.sequence)
                    self.assertFalse(any(row["type"] == "event" for row in rows), rows)
                    self.assertNotIn(frame, self.adapter.frames)
                    self.assertNotIn(scope, self.adapter.native_register_scopes)
                    self.assertIsNone(self.adapter.stop_key)
                    self.assertFalse(self.adapter.finished)

    def test_valid_snapshot_keeps_response_before_events_and_retires_old_handles(self):
        for level, command, arguments, wire in self.routes():
            for reply in (STATUS.replace("stop-id 17", "stop-id 18"), "guest exited: 7\n"):
                with self.subTest(level=level, command=command, wire=wire, reply=reply):
                    frame, scope = self.prepare(level)
                    self.broker.target, self.broker.reply = wire, reply
                    rows = self.send(command, arguments)
                    self.assertEqual(rows[0]["type"], "response", rows)
                    self.assertTrue(rows[0]["success"], rows)
                    self.assertEqual(sum(row["type"] == "response" for row in rows), 1, rows)
                    self.assertNotIn(frame, self.adapter.frames)
                    self.assertNotIn(scope, self.adapter.native_register_scopes)
                    events = [row for row in rows if row["type"] == "event"]
                    if reply.startswith("guest exited:"):
                        # continue also emits its ordinary continued event first.
                        self.assertEqual([row["event"] for row in events][-2:], ["exited", "terminated"])
                        self.assertEqual(events[-2]["body"]["exitCode"], 7)
                    else:
                        self.assertEqual(events[-1]["event"], "stopped")
                        self.assertEqual(self.adapter.threads[0]["stop_id"], 18)


if __name__ == "__main__":
    unittest.main()
