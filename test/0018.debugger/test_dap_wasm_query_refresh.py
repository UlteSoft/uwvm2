#!/usr/bin/env python3
"""Fresh logical Wasm queries vs stopped-handle lifetimes; broker DATA only."""
import importlib.util
import io
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("wasm_refresh_fixture", Path(__file__).with_name("test_dap_wasm_state.py"))
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
dap = fixture.dap


class Broker(fixture.Broker):
    terminal = None

    def request(self, command):
        if command == "status" and self.terminal is not None:
            self.commands.append(command)
            return self.terminal
        if command.startswith(("set wasm ", "set wasip1 ", "wasm-script ")):
            self.commands.append(command)
            return "mutation transcript (modeled DATA only)\n"
        return super().request(command)


class FreshWasmQueries(unittest.TestCase):
    def prepare(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.adapter.broker = self.broker = Broker()
        self.adapter.step_level = "wasm"
        self.adapter.observe(self.broker.status(), notify=False)
        self.sequence = 0
        frame = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]["id"]
        scopes = self.send("scopes", frameId=frame)["body"]["scopes"]
        scope = next(row["variablesReference"] for row in scopes if row["name"] == "Wasm globals")
        obj = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"][0]["variablesReference"]
        return frame, scope, obj

    def send(self, command, **args):
        self.output.seek(0)
        self.output.truncate()
        self.sequence += 1
        self.adapter.handle({"seq": self.sequence, "type": "request", "command": command, "arguments": args})
        rows = [row for row in fixture.messages(self.output.getvalue()) if row["type"] == "response"]
        self.assertEqual(len(rows), 1)
        return rows[0]

    def query(self, command):
        args = {"threadId": 1, "selection": "globals"}
        if command == "uwvm/wasmMembers":
            args.update(root=0, path=[])
        return self.send(command, **args)

    def reject_old(self, handles):
        frame, scope, obj = handles
        before = len(self.broker.commands)
        self.assertFalse(self.send("scopes", frameId=frame)["success"])
        self.assertFalse(self.send("variables", variablesReference=scope)["success"])
        self.assertFalse(self.send("variables", variablesReference=obj)["success"])
        self.assertEqual(self.broker.commands[before:], [])

    def test_fresh_query_recovers_on_first_request_after_mutation(self):
        for command in ("uwvm/wasmState", "uwvm/wasmMembers"):
            for expression in ("set wasm global 0 0 1 bits i32 0",
                               "set wasip1 env 0 4b4559 7632", "wasm-script status"):
                with self.subTest(command=command, expression=expression):
                    old = self.prepare()
                    self.assertTrue(self.send("evaluate", context="repl", expression=expression)["success"])
                    self.assertIsNone(self.adapter.stop_key)
                    reply = self.query(command)
                    self.assertTrue(reply["success"], reply)
                    self.assertTrue(reply["body"]["variables"])
                    new = reply["body"]["variables"][0]["variablesReference"]
                    self.assertGreater(new, old[2])
                    self.assertTrue(self.send("variables", variablesReference=new)["success"])
                    self.reject_old(old)

    def test_fresh_query_observes_new_stop_without_resurrecting_old_handles(self):
        for command in ("uwvm/wasmState", "uwvm/wasmMembers"):
            with self.subTest(command=command):
                old = self.prepare()
                self.broker.stop += 1
                reply = self.query(command)
                self.assertTrue(reply["success"], reply)
                self.assertEqual(self.adapter.threads[0]["stop_id"], self.broker.stop)
                self.reject_old(old)

    def test_old_scope_and_object_cannot_rebind_to_new_stop(self):
        for kind in ("scope", "object"):
            with self.subTest(kind=kind):
                old = self.prepare()
                self.broker.stop += 1
                before = len(self.broker.commands)
                self.assertFalse(self.send("variables", variablesReference=old[1 if kind == "scope" else 2])["success"])
                self.assertEqual(self.broker.commands[before:], ["status"])
                self.assertEqual(self.adapter.wasm_object_views, {})

    def test_running_exited_native_and_mid_copy_changes_refuse(self):
        for command in ("uwvm/wasmState", "uwvm/wasmMembers"):
            for state in ("running\n", "guest exited: 0\n", "native", "mid-copy"):
                with self.subTest(command=command, state=state):
                    self.prepare()
                    self.adapter.invalidate_stop()
                    if state == "native":
                        self.broker.native = True
                    elif state == "mid-copy":
                        self.broker.after_copy = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
                    else:
                        self.broker.terminal = state
                    before = len(self.broker.commands)
                    self.assertFalse(self.query(command)["success"])
                    if state != "mid-copy":
                        self.assertEqual(self.broker.commands[before:], ["status"])
                    else:
                        commands = self.broker.commands[before:]
                        self.assertEqual(commands[0], "status")
                        self.assertEqual(commands[-1], "status")
                        self.assertEqual(len(commands), 3)
                    self.assertEqual(self.adapter.wasm_object_views, {})


if __name__ == "__main__":
    unittest.main()
