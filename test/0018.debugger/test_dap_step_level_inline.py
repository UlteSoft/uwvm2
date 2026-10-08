#!/usr/bin/env python3
"""Formatter-shaped DAP level/inline tests, not runtime/DWARF qualification.

Only the keeper runs this in the Linux test cgroup. No test text mints a
real source owner, pause ticket, activation, address or value permission.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("step_inline_dap", ROOT / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


class Broker:
    def __init__(self):
        self.commands = []
        self.stop = 41
        self.identified = True
        self.native = False
        self.source_error = False
        self.native_next_error = False
        self.native_finish_error = False
        self.labels = ("  inline same call-site src/nested.cpp:10:2\n"
                       "  inline same call-site src/nested.cpp:20:4\n")

    def status(self):
        text = "stopped: native instruction step\n" if self.native else "stopped: selected participant step\n"
        if self.identified:
            text += f"stop-id {self.stop}\n"
        text += "thread 1 module=0 function=1 byte-offset=4 generation=7\n"
        return text + ("  native-pc=0x1004\n" if self.native else "  source src/nested.cpp:11:1\n")

    def request(self, command):
        self.commands.append(command)
        if command == "status":
            return self.status()
        if command == "bt 1":
            if self.native:
                return self.status() + self.labels + "  backtrace unavailable for this stop\n"
            return self.status() + self.labels + "  #0 module=0 function=1 [demo] physical\n  #1 module=0 function=2 [demo] caller\n"
        if command == f"frames 1 {self.stop}":
            # This suite specifically covers the legacy display-label fallback;
            # actual ordinal metadata/value routing has its own protocol suite.
            return "error: current source frame unavailable\n"
        if command.startswith("step source ") and self.source_error:
            return "error: source stepping unavailable: no embedded DWARF custom sections\n"
        if command.startswith("finish asm "):
            if not self.native or self.native_finish_error:
                return "error: native finish unavailable: no authenticated Wasm parent\n"
            self.stop += 1
            return self.status()
        if command.startswith("ni "):
            if not self.native or self.native_next_error:
                return "error: native next requires a current authenticated native trap\n"
            self.stop += 1
            return self.status()
        if command.startswith("step "):
            self.native = command.startswith("step asm ")
            self.stop += 1
            return self.status()
        if command == "continue":
            return "running\n"
        if command.startswith("locals wasm 1 0 "):
            first, count = map(int, command.split()[-2:])
            rows = "local 0 i32 = 5\n" if first == 0 and count else ""
            return f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=1\n" + rows
        if command == "locals 1":
            return "local 0 i32=5\n"
        raise AssertionError(command)

    def close(self):
        pass


def messages(raw):
    stream, result = io.BytesIO(raw), []
    while line := stream.readline():
        assert line.startswith(b"Content-Length: ") and line.endswith(b"\r\n")
        assert stream.readline() == b"\r\n"
        result.append(json.loads(stream.read(int(line[16:-2]))))
    return result


class StepInlineTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.sequence = 0

    def send(self, command, **arguments):
        self.sequence += 1
        self.adapter.handle({"seq": self.sequence, "type": "request", "command": command, "arguments": arguments})
        result = messages(self.output.getvalue())
        self.output.seek(0); self.output.truncate()
        responses = [item for item in result if item["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def attach(self, **arguments):
        with mock.patch.object(dap, "UnixBroker", return_value=self.broker), mock.patch.object(dap, "WindowsBroker", return_value=self.broker):
            return self.send("attach", socketDir="/test-only/private", pipeName="test-only-pipe", capability="test-only-token", **arguments)

    def test_source_default_maps_all_three_default_statement_policies(self):
        self.assertTrue(self.attach()["success"])
        self.assertEqual(self.adapter.step_level, "source")
        for request, policy in (("stepIn", "into"), ("next", "over"), ("stepOut", "out")):
            self.assertTrue(self.send(request, threadId=1)["success"])
            self.assertEqual(self.broker.commands[-1], f"step source 1 {policy}")
            self.assertTrue(self.send(request, threadId=1, granularity="statement")["success"])
            self.assertEqual(self.broker.commands[-1], f"step source 1 {policy}")

    def test_wasm_level_keeps_all_policies_at_wasm_instruction_granularity(self):
        self.assertTrue(self.attach(stepLevel="wasm")["success"])
        for granularity in (None, "statement", "instruction"):
            for request, suffix in (("stepIn", ""), ("next", " over"), ("stepOut", " out")):
                arguments = {} if granularity is None else {"granularity": granularity}
                self.assertTrue(self.send(request, threadId=1, **arguments)["success"])
                self.assertEqual(self.broker.commands[-1], f"step wasm 1{suffix}")
        for granularity in ("line", "unknown"):
            before = self.broker.commands[:]
            self.assertFalse(self.send("stepIn", threadId=1, granularity=granularity)["success"])
            self.assertEqual(self.broker.commands, before)
        self.assertFalse(any(command.startswith(("step asm ", "step source ")) for command in self.broker.commands))

    def test_explicit_native_level_keeps_exact_single_instruction(self):
        for level, command in (("native", "step asm 1"),):
            with self.subTest(level=level):
                self.adapter = dap.Adapter(self.output)
                self.assertTrue(self.attach(stepLevel=level)["success"])
                self.assertTrue(self.send("stepIn", threadId=1)["success"])
                self.assertEqual(self.broker.commands[-1], command)
                for granularity in (None, "statement", "instruction"):
                    arguments = {} if granularity is None else {"granularity": granularity}
                    self.assertTrue(self.send("next", threadId=1, **arguments)["success"])
                    self.assertEqual(self.broker.commands[-1], "ni 1")
                for granularity in (None, "statement", "instruction"):
                    arguments = {} if granularity is None else {"granularity": granularity}
                    self.assertTrue(self.send("stepOut", threadId=1, **arguments)["success"])
                    self.assertEqual(self.broker.commands[-1], "finish asm 1")
                self.assertTrue(self.send("next", threadId=1, granularity="line")["success"])
                self.assertEqual(self.broker.commands[-1], "step source 1 over")
                self.assertTrue(self.send("stepIn", threadId=1, granularity="instruction")["success"])
                self.assertEqual(self.broker.commands[-1], "step asm 1")

    def test_native_finish_refusal_never_falls_back(self):
        self.assertTrue(self.attach(stepLevel="native")["success"])
        before = len(self.broker.commands)
        self.assertFalse(self.send("stepOut", threadId=1)["success"])
        self.assertEqual(self.broker.commands[before:], ["finish asm 1"])
        self.assertTrue(self.send("stepIn", threadId=1)["success"])
        self.broker.native_finish_error = True
        before = len(self.broker.commands)
        self.assertFalse(self.send("stepOut", threadId=1, granularity="instruction")["success"])
        self.assertEqual(self.broker.commands[before:], ["finish asm 1"])

    def test_source_instruction_stepout_uses_native_parent(self):
        self.assertTrue(self.attach(stepLevel="source")["success"])
        self.assertTrue(self.send("stepIn", threadId=1, granularity="instruction")["success"])
        self.assertTrue(self.send("stepOut", threadId=1, granularity="instruction")["success"])
        self.assertEqual(self.broker.commands[-1], "finish asm 1")

    def test_native_next_refusal_preserves_error_without_a_fallback(self):
        self.assertTrue(self.attach(stepLevel="native")["success"])
        for native, refused in ((False, False), (True, True)):
            self.broker.native = native
            self.broker.native_next_error = refused
            before = len(self.broker.commands)
            reply = self.send("next", threadId=1)
            self.assertFalse(reply["success"])
            self.assertIn("authenticated native trap", reply["message"])
            self.assertEqual(self.broker.commands[before:], ["ni 1"])
            self.assertEqual(self.adapter.frames, {})
        for value in (0, -1, True, "1"):
            before = self.broker.commands[:]
            self.assertFalse(self.send("next", threadId=value)["success"])
            self.assertEqual(self.broker.commands, before)

    def test_source_instruction_next_uses_native_policy(self):
        self.assertTrue(self.attach()["success"])
        self.broker.native = True
        self.assertTrue(self.send("next", threadId=1, granularity="instruction")["success"])
        self.assertEqual(self.broker.commands[-1], "ni 1")
        self.assertFalse(any(command.startswith("step wasm ") for command in self.broker.commands))

    def test_invalid_attach_levels_are_rejected_before_connecting(self):
        for value in (None, True, 1, [], {}, "unknown"):
            with self.subTest(value=value), mock.patch.object(dap, "UnixBroker") as unix, mock.patch.object(dap, "WindowsBroker") as windows:
                self.assertFalse(self.send("attach", stepLevel=value, socketDir="unused", pipeName="unused", capability="unused")["success"])
                unix.assert_not_called(); windows.assert_not_called()
                self.assertIsNone(self.adapter.broker)

    def test_missing_dwarf_is_an_error_with_no_wasm_fallback(self):
        self.assertTrue(self.attach()["success"])
        self.broker.source_error = True
        self.assertFalse(self.send("next", threadId=1)["success"])
        self.assertEqual(self.broker.commands[-1], "step source 1 over")
        self.assertNotIn("step wasm 1", self.broker.commands)
        self.assertEqual(self.adapter.frames, {})

    def test_current_inline_labels_precede_physical_frames_in_display_order(self):
        self.assertTrue(self.attach()["success"])
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        self.assertEqual([row["name"] for row in frames[:2]], ["Inlined same call-site src/nested.cpp:10:2", "Inlined same call-site src/nested.cpp:20:4"])
        self.assertTrue(frames[2]["name"].startswith("#0 "))
        self.assertTrue(frames[3]["name"].startswith("#1 "))
        self.assertEqual(len({row["id"] for row in frames}), 4)
        for row in frames[:2]:
            self.assertEqual((row["line"], row["column"], row["presentationHint"]), (0, 0, "label"))
            self.assertFalse(row["canRestart"])
            self.assertNotIn("source", row); self.assertNotIn("instructionPointerReference", row)
            self.assertEqual(self.send("scopes", frameId=row["id"])["body"]["scopes"], [])
        scopes = self.send("scopes", frameId=frames[2]["id"])["body"]["scopes"]
        self.assertEqual([row["name"] for row in scopes], ["Wasm locals", "Source variables", "WASIp1 arguments", "WASIp1 environment", "WASIp1 descriptors", "WASIp1 preopens"])
        self.assertEqual(self.send("variables", variablesReference=scopes[0]["variablesReference"])["body"]["variables"][0]["value"], "i32=5")

    def test_inline_labels_cannot_read_source_values_memory_or_restart(self):
        self.assertTrue(self.attach()["success"])
        for command, key in (("source", "sourceReference"), ("variables", "variablesReference"), ("restartFrame", "frameId")):
            frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
            self.assertFalse(self.send(command, **{key: frames[0]["id"]})["success"])
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        label = "uwvm-inline:" + str(frames[0]["id"])
        self.assertFalse(self.send("readMemory", memoryReference=label, count=4)["success"])
        self.assertFalse(self.send("disassemble", memoryReference=label, instructionCount=1)["success"])
        # Global DAP support never grants native-code authority to an inline
        # display label; the explicit inline disassemble rejection above remains.
        self.assertTrue(self.send("initialize")["body"].get("supportsDisassembleRequest", False))
        self.assertNotIn("locals source 1", self.broker.commands)
        self.assertFalse(any(command.startswith("memory ") for command in self.broker.commands))

    def test_inline_ids_retire_on_same_site_new_stop_and_resume(self):
        self.assertTrue(self.attach()["success"])
        old = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]["id"]
        self.broker.stop += 1
        new = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]["id"]
        self.assertNotEqual(old, new)
        self.assertFalse(self.send("scopes", frameId=old)["success"])
        new = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]["id"]
        self.assertTrue(self.send("continue", threadId=1)["success"])
        self.assertFalse(self.send("scopes", frameId=new)["success"])
        self.assertEqual(self.adapter.inline_frames, set())

    def test_unidentified_or_native_stop_never_exposes_inline_frames(self):
        self.assertTrue(self.attach()["success"])
        self.broker.identified = False
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        self.assertEqual(len(frames), 2); self.assertEqual(self.adapter.inline_frames, set())
        self.broker.identified = True; self.broker.native = True
        self.assertFalse(self.send("stackTrace", threadId=1)["success"])
        self.assertEqual(self.adapter.inline_frames, set())
        self.broker.labels = "  inline metadata unavailable: native trap has no current Wasm source position\n"
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        self.assertEqual(len(frames), 1); self.assertIn("Native JIT instruction", frames[0]["name"])

    def test_literal_escaped_labels_and_absent_callsite_are_only_display(self):
        self.assertTrue(self.attach()["success"])
        self.broker.labels = r"  inline literal\x1b name" + "\n  inline \n"
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        self.assertEqual(frames[0]["name"], r"Inlined literal\x1b name")
        self.assertEqual(frames[1]["name"], "Inlined <anonymous>")
        self.assertTrue(all("source" not in row for row in frames[:2]))

    def test_ambiguous_or_invalid_display_grammar_fails_without_partial_frames(self):
        self.assertTrue(self.attach()["success"])
        for label in ("  inline a call-site b call-site c:1:2\n", "  inline a\x1b\n", "  inline a call-site p:18446744073709551616:0\n",
                      "  inline a call-site p:not-a-line:1\n", "  inline a\n  inline metadata unavailable: invalid\n",
                      "  inline " + "a" * 4097 + "\n", "  inline a\n" * 65):
            with self.subTest(label=label[:80]):
                self.broker.labels = label
                self.assertFalse(self.send("stackTrace", threadId=1)["success"])
                self.assertEqual(self.adapter.frames, {}); self.assertEqual(self.adapter.inline_frames, set())
        with self.assertRaises(ValueError):
            dap.parse_inline_labels("thread 2 module=0 function=1 byte-offset=4 generation=7\n  inline a\n", 1)
        with self.assertRaises(ValueError):
            dap.parse_inline_labels(self.broker.status() + "  #0 module=0 function=1\n  inline too-late\n", 1)


if __name__ == "__main__":
    unittest.main()
