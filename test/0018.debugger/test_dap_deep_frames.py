#!/usr/bin/env python3
"""Paged protocol DATA tests; actual generated-frame ownership is tested in Linux."""
import importlib.util
import io
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("deep_frames_dap", Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dap)


def packet(first=4096, count=3, total=10000, wasm=True, physical=0):
    prefix = "wasm-frames" if wasm else "source-frames"
    text = f"{prefix} stop=41 thread=1 selected=0 count={count} total={total} first={first} physical={physical}\n"
    for i in range(first, first + count):
        kind = "inline" if i < physical else "physical" if i == physical else "caller"
        availability = "caller-unavailable" if kind == "caller" else "current"
        text += (f"  frame {i} kind={kind} module=0 function=1 generation=1 runtime-epoch=7 "
                 f"scope-unit=0 scope-offset=0 variables={availability} name=Wasm activation\n")
    return text + prefix + " end\n"


class Broker:
    def __init__(self):
        self.stop = 41
        self.commands = []
        self.retire = False
        self.page_error = None

    def request(self, command):
        self.commands.append(command)
        status = f"stopped: selected participant step\nstop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation=7\n"
        if command == "status":
            return status
        if command == "bt 1":
            return status + "  #0 module=0 function=1 [demo] run\n"
        if command.startswith("frames wasm "):
            if self.page_error is not None:
                return "error: " + self.page_error + "\n"
            words = command.split()
            first, count = map(int, words[-2:])
            first = min(first, 10000)
            result = packet(first, min(count, 10000 - first))
            if self.retire:
                self.stop += 1
            return result
        if command.startswith("operands 1 "):
            first, count = map(int, command.split()[-2:]); end = min(first+count, 2)
            rows = ["operand 0 i32 = 4096\n", "operand 1 i64 = 123456789\n"]
            result = (f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=2\n"
                      "Note: Last Wasm safepoint snapshot; may differ from current native state.\n") + ''.join(rows[first:end])
            if end < 2: result += f"Wasm state truncated: rows={end-first} objects=0\n"
            return result
        return "error: unsupported fixture command\n"


class DeepFrameTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.adapter.step_level = "wasm"
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.sequence = 0

    def send(self, command, **arguments):
        self.sequence += 1
        self.output.seek(0)
        self.output.truncate()
        self.adapter.handle({"seq": self.sequence, "type": "request", "command": command, "arguments": arguments})
        stream = io.BytesIO(self.output.getvalue())
        messages = []
        while line := stream.readline():
            self.assertEqual(stream.readline(), b"\r\n")
            messages.append(json.loads(stream.read(int(line[16:-2]))))
        return next(m for m in messages if m["type"] == "response")

    def test_deep_pages_and_typed_operand_routing(self):
        result = self.send("stackTrace", threadId=1, startFrame=4096, levels=3)
        self.assertTrue(result["success"], result)
        self.assertEqual(result["body"]["totalFrames"], 10000)
        frames = result["body"]["stackFrames"]
        self.assertEqual(len(frames), 3)
        self.assertEqual(self.adapter.frames[frames[0]["id"]][1], 4096)
        scopes = self.send("scopes", frameId=frames[0]["id"])["body"]["scopes"]
        ref = next(s["variablesReference"] for s in scopes if s["name"].startswith("Wasm operand"))
        values = self.send("variables", variablesReference=ref)
        self.assertTrue(values["success"], values)
        self.assertTrue(any(c.startswith("operands 1 4096 ") for c in self.broker.commands))

    def test_end_page_and_empty_window(self):
        result = self.send("stackTrace", threadId=1, startFrame=9999, levels=20)
        self.assertEqual(len(result["body"]["stackFrames"]), 1)
        result = self.send("stackTrace", threadId=1, startFrame=20000, levels=20)
        self.assertEqual(result["body"], {"stackFrames": [], "totalFrames": 10000})

    def test_retired_stop_cannot_publish_page(self):
        self.broker.retire = True
        result = self.send("stackTrace", threadId=1, startFrame=4096, levels=3)
        self.assertFalse(result["success"])
        self.assertFalse(self.adapter.frames)

    def test_failed_complete_page_cannot_publish_diagnostic_backtrace(self):
        for error in ("exhausted", "invalid state", "Wasm capture unavailable"):
            with self.subTest(error=error):
                self.broker.page_error = None
                valid = self.send("stackTrace", threadId=1, startFrame=0, levels=1)
                frame = valid["body"]["stackFrames"][0]["id"]
                self.assertTrue(self.send("scopes", frameId=frame)["success"])
                self.broker.page_error = error
                result = self.send("stackTrace", threadId=1, startFrame=0, levels=128)
                self.assertFalse(result["success"], result)
                self.assertIn("complete Wasm activation page unavailable", result["message"])
                self.assertFalse(self.adapter.frames)
                self.assertFalse(self.adapter.scopes)
                self.assertFalse(self.adapter.wasm_scope_stops)

    def test_parser_accepts_caller_only_source_page(self):
        result = dap.parse_source_frames(packet(wasm=False, physical=2), 1, 41, with_page=True)
        self.assertEqual((result["first"], result["total"]), (4096, 10000))

    def test_invalid_pages_fail_closed(self):
        for text in (packet().replace("first=4096", "first=4095"), packet().replace("total=10000", "total=4097"),
                     packet().replace("frame 4097", "frame 4096"), packet().replace("kind=caller", "kind=physical", 1),
                     packet().replace("stop=41", "stop=42"), packet().replace("physical=0", "physical=10000")):
            with self.subTest(text=text):
                with self.assertRaises(ValueError):
                    dap.parse_source_frames(text, 1, 41, wasm=True)

    def test_invalid_dap_page_arguments(self):
        for args in ({"startFrame": True}, {"startFrame": -1}, {"startFrame": 1 << 64}, {"levels": -1}, {"levels": 1.0}):
            with self.subTest(args=args):
                self.assertFalse(self.send("stackTrace", threadId=1, **args)["success"])


if __name__ == "__main__":
    unittest.main()
