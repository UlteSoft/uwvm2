#!/usr/bin/env python3
"""Stopped-label lifetime regressions; formatter broker, not native authority."""
import io
import unittest
import test_dap_wasm_state as typed

dap = typed.dap


class Broker(typed.Broker):
    def __init__(self):
        super().__init__()
        self.override = None
        self.after_locals = None

    def status(self):
        return self.override if self.override is not None else super().status()

    def request(self, command):
        if command.startswith("locals wasm 1 0 "):
            self.commands.append(command)
            first, count = map(int, command.split()[-2:])
            rows = "local 0 i32 = 5\n" if first == 0 and count else ""
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=1\n" + rows
        else:
            result = super().request(command)
        if command.startswith("locals ") and self.after_locals is not None:
            self.after_locals()
        return result


class Lifetime(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.sequence = 0

    def send(self, command, **args):
        self.sequence += 1
        self.adapter.handle({"seq": self.sequence, "command": command, "arguments": args})
        rows = typed.messages(self.output.getvalue())
        self.output.seek(0); self.output.truncate()
        responses = [row for row in rows if row["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def handles(self):
        result = self.send("stackTrace", threadId=1)
        self.assertTrue(result["success"], result)
        frame = result["body"]["stackFrames"][0]
        result = self.send("scopes", frameId=frame["id"])
        self.assertTrue(result["success"], result)
        scope = next(row["variablesReference"] for row in result["body"]["scopes"] if row["name"] == "Wasm locals")
        return frame["id"], scope, frame["source"]["sourceReference"]

    def change(self, kind):
        if kind == "new-stop-same-pc": self.broker.stop += 1
        elif kind == "native": self.broker.native = True
        elif kind == "other-participant":
            self.broker.override = self.broker.status() + "thread 2 module=0 function=2 byte-offset=0 generation=7\n"
        elif kind == "malformed":
            self.broker.override = self.broker.status() + "stop-id 99\n"
        else: self.broker.override = {"running": "running\n", "exited": "guest exited: 0\n", "closed": "debug domain closed\n"}[kind]

    def test_current_labels_and_locals_remain_usable(self):
        frame, scope, source = self.handles()
        self.assertTrue(self.send("scopes", frameId=frame)["success"])
        result = self.send("variables", variablesReference=scope)
        self.assertTrue(result["success"], result)
        self.assertEqual(result["body"]["variables"][0]["value"], "i32=5")
        self.assertTrue(self.send("source", sourceReference=source)["success"])

    def test_external_change_retires_all_three_public_routes_before_copy(self):
        for kind in ("new-stop-same-pc", "native", "other-participant", "running", "exited", "closed", "malformed"):
            for command, index, key in (("scopes", 0, "frameId"), ("variables", 1, "variablesReference"), ("source", 2, "sourceReference")):
                with self.subTest(kind=kind, command=command):
                    self.setUp()
                    handles = self.handles()
                    self.change(kind)
                    begin = len(self.broker.commands)
                    result = self.send(command, **{key: handles[index]})
                    self.assertFalse(result["success"], result)
                    self.assertFalse(any(command.startswith("locals ") for command in self.broker.commands[begin:]))
                    self.assertFalse(self.adapter.frames)
                    self.assertFalse(self.adapter.scopes)

    def test_locals_copy_cannot_publish_after_stop_changes(self):
        for kind in ("new-stop-same-pc", "native", "other-participant", "running", "exited", "closed", "malformed"):
            with self.subTest(kind=kind):
                self.setUp()
                _, scope, _ = self.handles()
                self.broker.after_locals = lambda: self.change(kind)
                begin = len(self.broker.commands)
                result = self.send("variables", variablesReference=scope)
                self.assertTrue(any(command.startswith("locals ") for command in self.broker.commands[begin:]))
                self.assertFalse(result["success"], result)
                self.assertNotIn("body", result)
                self.assertFalse(self.adapter.scopes)

    def test_fresh_frames_work_after_rejecting_stale_labels(self):
        for command, index, key in (("scopes", 0, "frameId"), ("variables", 1, "variablesReference"), ("source", 2, "sourceReference")):
            with self.subTest(command=command):
                self.setUp()
                handles = self.handles()
                self.broker.stop += 1
                self.assertFalse(self.send(command, **{key: handles[index]})["success"])
                fresh = self.handles()
                self.assertGreater(fresh[0], handles[0])
                self.assertTrue(self.send(command, **{key: fresh[index]})["success"])

    def test_native_frame_scope_requires_current_complete_stop(self):
        for kind in ("new-stop-same-pc", "running", "exited", "closed", "malformed"):
            with self.subTest(kind=kind):
                self.setUp(); self.broker.native = True
                frame = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]["id"]
                self.assertTrue(self.send("scopes", frameId=frame)["success"])
                self.change(kind)
                self.assertFalse(self.send("scopes", frameId=frame)["success"])
                self.assertFalse(self.adapter.native_register_scopes)


if __name__ == "__main__":
    unittest.main()
