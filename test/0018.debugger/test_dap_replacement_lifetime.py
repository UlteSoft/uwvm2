#!/usr/bin/env python3
"""Queued DAP replacement/reference controls, formatter-shaped broker only.

These tests never call Adapter.poll or import a VM. Only the remote keeper
executes them inside the existing test cgroup. Text replies prove adapter
lifetimes, not genuine source ownership, JIT generations or host read authority.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest

PATH = Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py"
SPEC = importlib.util.spec_from_file_location("uwvm_dap_replacement_lifetime", PATH)
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


class Broker:
    def __init__(self, native=False):
        self.commands = []
        self.stop = 41
        self.epoch = 7
        self.native = native
        self.function_generation = 1
        self.replacement_error = None
        self.status_error = None
        self.advance_epoch = False
        self.before_replacement = None
        self.script_error = None
        self.script_commits_replacement = False
        self.script_reply = None
        self.script_transport_error = None

    def status(self):
        head = "stopped: native instruction step\n" if self.native else "stopped: breakpoint\n"
        head += f"stop-id {self.stop}\nthread 3 module=0 function=1 byte-offset=4 generation={self.epoch}\n"
        return head + ("  native-pc=0x1100\n" if self.native else "  source scope.cpp:10:1\n")

    def request(self, command):
        self.commands.append(command)
        if command == "status":
            return self.status_error or self.status()
        if command == "bt 3":
            if self.native:
                return self.status() + "  backtrace unavailable for this stop\n"
            return (self.status() + "  inline helper call-site scope.cpp:7:2\n"
                    "  #0 module=0 function=1 [demo] current\n"
                    "  #1 module=0 function=2 [demo] caller\n")
        if command == f"frames 3 {self.stop}":
            # Keep this legacy-label lifetime fixture distinct from the real
            # source-frame ordinal protocol suite. Production source errors
            # retire IDs and refresh the complete stop before fallback labels.
            return "error: current source frame unavailable\n"
        if command == "locals 3":
            return "local 0 i32=5\n"
        if command == "locals source 3":
            return f"source-stop {self.stop}\nsource parameter value type=int = i32=5\n"
        if command.startswith("disassemble-range "):
            _, thread, stop, count, byte, instruction, symbols = command.split()
            # A small valid formatter packet, not a claim that this fixture owns
            # actual host machine code. Real copy authority has separate tests.
            if (thread, stop, count, byte, instruction, symbols) != ("3", str(self.stop), "1", "0", "0", "0"):
                raise AssertionError(command)
            return (f"native-disassembly-range stop={self.stop} thread=3 module=0 function=1 "
                    f"function-generation={self.function_generation} runtime-epoch={self.epoch} "
                    "reference-pc=0x1100 owner-begin=0x1000 owner-end=0x1200 "
                    "byte-offset=0 instruction-offset=0 resolve-symbols=0\n"
                    "  instruction 0 pc=0x1100 bytes=90  nop\nnative-disassembly-end\n")
        if command.split() and command.split()[0] == "wasm-script":
            if self.before_replacement is not None:
                self.before_replacement()
            if self.script_error is not None:
                return self.script_error
            if self.script_commits_replacement:
                self.function_generation += 1
                self.stop += 1
            if self.script_transport_error is not None:
                raise self.script_transport_error
            # Formatter-shaped transcript only; it cannot prove a real JIT
            # transaction, and the adapter must not infer a stop from it.
            return self.script_reply or "wasm-script command 1\nwasm-events end\nwasm-script end\n"
        if command.split() and command.split()[0] == "replace":
            if self.before_replacement is not None:
                self.before_replacement()
            if self.replacement_error is not None:
                return self.replacement_error
            self.function_generation += 1
            self.stop += 1
            if self.advance_epoch:
                self.epoch += 1
            # The formatter returns no stopped location: the adapter must query
            # actual status instead of inferring it from this generation label.
            return f"function replaced; generation {self.function_generation}\n"
        if command.split() and command.split()[0] == "replace_function":
            return "error: invalid command; type help\n"
        raise AssertionError(command)


def messages(data):
    result, stream = [], io.BytesIO(data)
    while line := stream.readline():
        assert line.startswith(b"Content-Length: ") and line.endswith(b"\r\n")
        assert stream.readline() == b"\r\n"
        result.append(json.loads(stream.read(int(line[16:-2]))))
    return result


class ReplacementLifetimeTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.sequence = 0
        self.last_messages = []

    def send(self, command, **arguments):
        self.sequence += 1
        self.output.seek(0)
        self.output.truncate()
        self.adapter.handle({"seq": self.sequence, "type": "request",
                             "command": command, "arguments": arguments})
        self.last_messages = messages(self.output.getvalue())
        responses = [row for row in self.last_messages if row["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def assert_retired(self):
        self.assertIsNone(self.adapter.stop_key)
        for attribute in ("frames", "scopes", "native_frames", "native_code_refs",
                          "source_frames", "source_scope_stops", "inline_frames", "source_frame_ordinals", "wasip1_scope_stops"):
            self.assertFalse(getattr(self.adapter, attribute), attribute)

    def capture(self, native=False):
        self.broker.native = native
        self.adapter.step_level = "native"
        response = self.send("stackTrace", threadId=3)
        self.assertTrue(response["success"], response)
        frames = response["body"]["stackFrames"]
        scopes, source_references = [], []
        for frame in frames:
            response = self.send("scopes", frameId=frame["id"])
            self.assertTrue(response["success"], response)
            scopes.extend(row["variablesReference"] for row in response["body"]["scopes"])
            if "sourceReference" in frame.get("source", {}):
                source_references.append(frame["source"]["sourceReference"])
        references = [frame["instructionPointerReference"] for frame in frames
                      if "instructionPointerReference" in frame]
        self.assertTrue(self.adapter.native_frames if native else self.adapter.native_code_refs)
        if not native:
            self.assertTrue(self.adapter.inline_frames)
            self.assertTrue(self.adapter.source_frames)
            self.assertTrue(self.adapter.source_scope_stops)
            self.assertEqual(len(scopes), 6)
        return frames, scopes, source_references, references

    def reject_old(self, old):
        frames, scopes, source_references, references = old
        # Immediate consecutive handle calls: there is no idle poll or helper
        # refresh before using the IDs retired by the replacement request.
        for command, key, values, extra in (
                ("scopes", "frameId", [frame["id"] for frame in frames], {}),
                ("variables", "variablesReference", scopes, {}),
                ("source", "sourceReference", source_references, {}),
                ("disassemble", "memoryReference", references, {"instructionCount": 1})):
            for value in values:
                with self.subTest(command=command, reference=value):
                    before = self.broker.commands[:]
                    self.assertFalse(self.send(command, **{key: value}, **extra)["success"])
                    self.assertEqual(self.broker.commands, before)
        for reference in references:
            before = self.broker.commands[:]
            self.assertFalse(self.send("readMemory", memoryReference=reference, count=1)["success"])
            self.assertEqual(self.broker.commands, before)

    def test_success_retires_before_send_then_observes_new_same_pc_stop(self):
        old = self.capture()
        prior_key = self.adapter.stop_key
        self.broker.before_replacement = self.assert_retired
        before = len(self.broker.commands)
        reply = self.send("evaluate", expression="replace 0 2 1 /host/new.body", context="repl")
        self.assertTrue(reply["success"], reply)
        self.assertEqual(self.broker.commands[before:], ["replace 0 2 1 /host/new.body", "status"])
        self.assertNotEqual(self.adapter.stop_key, prior_key)
        self.assertEqual((self.adapter.threads[0]["function"], self.adapter.threads[0]["offset"],
                          self.adapter.threads[0]["generation"], self.adapter.threads[0]["stop_id"]),
                         (1, 4, 7, 42))
        self.assertTrue(any(row.get("event") == "stopped" for row in self.last_messages))
        self.reject_old(old)
        fresh = self.capture()
        self.assertGreater(min(frame["id"] for frame in fresh[0]), max(frame["id"] for frame in old[0]))

    def test_invalid_body_failure_retires_without_changing_actual_generation(self):
        for native in (False, True):
            with self.subTest(native=native):
                self.setUp()
                old = self.capture(native=native)
                self.broker.before_replacement = self.assert_retired
                self.broker.replacement_error = "error: control request rejected: replacement body failed WebAssembly validation\n"
                before = len(self.broker.commands)
                result = self.send("evaluate", expression="replace 0 2 1 /host/invalid.body", context="repl")
                self.assertFalse(result["success"])
                self.assertEqual(self.broker.commands[before:], ["replace 0 2 1 /host/invalid.body"])
                self.assertEqual((self.broker.stop, self.broker.function_generation, self.broker.epoch), (41, 1, 7))
                self.assert_retired()
                self.reject_old(old)

    def test_same_pc_new_publication_and_native_generation_never_reuses_ids(self):
        for native in (False, True):
            with self.subTest(native=native):
                self.setUp()
                old = self.capture(native=native)
                old_key = self.adapter.stop_key
                self.broker.advance_epoch = True
                self.broker.before_replacement = self.assert_retired
                self.assertTrue(self.send("evaluate", expression="replace 0 2 1 /host/new.body")["success"])
                self.assertNotEqual(old_key, self.adapter.stop_key)
                self.assertEqual((self.adapter.threads[0]["offset"], self.adapter.threads[0]["generation"],
                                  self.broker.function_generation), (4, 8, 2))
                self.reject_old(old)
                fresh = self.capture(native=native)
                self.assertGreater(min(frame["id"] for frame in fresh[0]), max(frame["id"] for frame in old[0]))

    def test_success_then_unavailable_or_malformed_status_keeps_every_id_retired(self):
        for status in (
                "error: debugger channel unavailable\n", "function replaced; generation 2\n",
                "running\n", "debug domain closed\n", "prepared; no Wasm instruction executed\n",
                "stopped: breakpoint\nstop-id 42\n",
                "stopped: breakpoint\nthread 3 module=0 function=1 byte-offset=4 generation=7\n",
                "stopped: breakpoint\nstop-id 42\nthread 0 module=0 function=1 byte-offset=4 generation=7\n",
                "stopped: breakpoint\nstop-id 42\nthread 3 module=0 function=1 byte-offset=4 generation=0\n",
                "stopped: breakpoint\nstop-id 42\nthread 3 module=0 function=1 byte-offset=4 generation=7\n"
                "thread 3 module=0 function=2 byte-offset=0 generation=7\n",
                "stopped: breakpoint\nstop-id 42\nthread 3 module=0 function=1 byte-offset=4 generation=7\n"
                "stopped: breakpoint\n",
                "stopped: breakpoint\nstop-id 42\nthread 3 module=0 function=18446744073709551616 byte-offset=4 generation=7\n"):
            with self.subTest(status=status):
                self.setUp()
                old = self.capture()
                self.broker.before_replacement = self.assert_retired
                self.broker.status_error = status
                result = self.send("evaluate", expression="replace 0 2 1 /host/new.body")
                self.assertFalse(result["success"])
                self.assertIn("replacement acknowledged; stop refresh failed", result["message"])
                self.assertIn("all debugger references remain retired", result["message"])
                self.assertEqual(self.broker.function_generation, 2)
                self.assert_retired()
                self.reject_old(old)

    def test_direct_command_and_real_whitespace_spelling_retire_before_transport(self):
        self.capture()
        self.broker.before_replacement = self.assert_retired
        reply = self.adapter.command(" \treplace\t0 2 1 /host/new.body\r")
        self.assertEqual(reply, "function replaced; generation 2\n")
        self.assertEqual(self.broker.commands[-2:], [" \treplace\t0 2 1 /host/new.body\r", "status"])
        self.assertFalse(self.adapter.frames)
        self.assertEqual(self.adapter.threads[0]["stop_id"], 42)

    def test_script_replacement_retires_before_batch_and_queued_ids_fail_without_poll(self):
        for native in (False, True):
            with self.subTest(native=native):
                self.setUp()
                old = self.capture(native=native)
                self.broker.before_replacement = self.assert_retired
                self.broker.script_commits_replacement = True
                self.broker.script_reply = ("wasm-script command 1\nwasm-events end\n"
                                           "wasm-script command 2\nfunction replaced; generation 2\nwasm-script end\n")
                expression = "wasm-script info wasm-events; replace 0 2 1 /host/new.body"
                before = len(self.broker.commands)
                result = self.send("evaluate", expression=expression, context="repl")
                self.assertTrue(result["success"], result)
                self.assertEqual(self.broker.commands[before:], [expression])
                self.assertEqual((self.broker.stop, self.broker.function_generation), (42, 2))
                self.assert_retired()  # transcript is not a fresh actual stop.
                self.reject_old(old)
                fresh = self.capture(native=native)
                self.assertGreater(min(frame["id"] for frame in fresh[0]), max(frame["id"] for frame in old[0]))

    def test_diagnostic_only_script_and_truncated_output_conservatively_retire_all_ids(self):
        for expression, transcript in (
                ("wasm-script info wasm-events", "wasm-script command 1\nwasm-trace enabled=0 filter=all overwritten=0\nwasm-events end\nwasm-script end\n"),
                ("wasm-script ptype object; status",
                 "wasm-script output-truncated shown=0 executed=2 last-command-status=0\nwasm-script end\n")):
            with self.subTest(expression=expression):
                self.setUp()
                old = self.capture()
                self.broker.before_replacement = self.assert_retired
                self.broker.script_reply = transcript
                before = len(self.broker.commands)
                self.assertTrue(self.send("evaluate", expression=expression)["success"])
                self.assertEqual(self.broker.commands[before:], [expression])
                self.assertEqual((self.broker.stop, self.broker.function_generation), (41, 1))
                self.assert_retired()
                self.reject_old(old)

    def test_partial_child_failure_and_lost_commit_reply_never_restore_stopped_references(self):
        for lost_reply in (False, True):
            with self.subTest(lost_reply=lost_reply):
                self.setUp()
                old = self.capture()
                self.broker.before_replacement = self.assert_retired
                expression = "wasm-script info wasm-events; replace 0 2 1 /host/new.body"
                if lost_reply:
                    # Simulate a transport failure after a possible commit. A
                    # failed response cannot prove whether publication happened.
                    self.broker.script_commits_replacement = True
                    self.broker.script_transport_error = OSError("lost batch reply")
                else:
                    self.broker.script_reply = ("wasm-script command 1\nwasm-trace enabled=0 filter=all overwritten=0\nwasm-events end\n"
                                               "wasm-script command 2\nerror: function generation changed; inspect again before replacing\n"
                                               "wasm-script end\n")
                before = len(self.broker.commands)
                response = self.send("evaluate", expression=expression)
                self.assertEqual(response["success"], not lost_reply)
                self.assertEqual(self.broker.commands[before:], [expression])
                self.assert_retired()
                self.reject_old(old)
                self.assertEqual(self.broker.function_generation, 2 if lost_reply else 1)

    def test_malformed_leading_prefix_is_conservatively_retired_then_rejected(self):
        # The VM requires the exact raw "wasm-script " prefix. Adapter split()
        # may classify broader whitespace spellings conservatively, but never
        # claims those spellings executed a real script.
        old = self.capture()
        self.broker.before_replacement = self.assert_retired
        self.broker.script_error = "error: invalid command; type help\n"
        expression = " \twasm-script\tstatus; replace 0 2 1 /host/new.body\r"
        before = len(self.broker.commands)
        self.assertFalse(self.send("evaluate", expression=expression)["success"])
        self.assertEqual(self.broker.commands[before:], [expression])
        self.assertEqual((self.broker.stop, self.broker.function_generation), (41, 1))
        self.assert_retired()
        self.reject_old(old)

    def test_rejected_script_retires_before_transport_and_never_resurrects_old_ids(self):
        for error in ("error: invalid bounded script\n", "error: bounded debugger response exceeded 65536 bytes\n"):
            with self.subTest(error=error):
                self.setUp()
                old = self.capture()
                self.broker.before_replacement = self.assert_retired
                self.broker.script_error = error
                expression = "wasm-script status; replace 0 2 1 /host/invalid.body"
                before = len(self.broker.commands)
                self.assertFalse(self.send("evaluate", expression=expression)["success"])
                self.assertEqual(self.broker.commands[before:], [expression])
                self.assertEqual((self.broker.stop, self.broker.function_generation), (41, 1))
                self.assert_retired()
                self.reject_old(old)

    def test_nonexistent_replace_function_alias_stays_an_error_without_extra_query(self):
        self.capture()
        before = len(self.broker.commands)
        reply = self.send("evaluate", expression="replace_function 0 2 1 /host/new.body")
        self.assertFalse(reply["success"])
        self.assertEqual(self.broker.commands[before:], ["replace_function 0 2 1 /host/new.body"])
        self.assertEqual(self.broker.function_generation, 1)
        self.assert_retired()


if __name__ == "__main__":
    unittest.main()
