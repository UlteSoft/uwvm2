#!/usr/bin/env python3
"""R1 source-frame protocol controls, formatter text only, not VM authority.

The keeper alone runs this in the Linux cgroup. Rows mirror console.h's
source-frames header/row/end and source-stop numeric output. Every Broker row
is synthetic protocol DATA, not proof that an actual guest read occurred. These tests cover
adapter routing and retirement; actual pause tickets, control blocks, scope
selection and copied guest storage must be qualified by native product tests.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest


PATH = Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py"
SPEC = importlib.util.spec_from_file_location("uwvm_dap_source_frames", PATH)
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


class Broker:
    def __init__(self):
        self.commands = []
        self.stop = 41
        self.epoch = 7
        self.native = False
        self.state = "stopped"
        self.frame_error = False
        self.frame_packet = None
        self.saved_caller = False
        self.reply_stop = None
        self.print_packet = None
        self.after_copy = None
        self.after_frames = None
        self.before_replace = None
        self.replace_error = None

    def status(self):
        if self.state != "stopped":
            return self.state + "\n"
        text = ("stopped: native instruction step\n" if self.native else "stopped: selected participant step\n")
        text += f"stop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation={self.epoch}\n"
        return text + ("  native-pc=0x1004\n" if self.native else "  source src/scope.cpp:11:1\n")

    def frames(self):
        # Equal display names are deliberately distinct concrete DIE identities;
        # ordinal 0/1/2 remains the controller's current inner/outer/physical path.
        rows = (("inline", 1, 101, "current", "same"),
                ("inline", 1, 102, "current", "same"),
                ("physical", 1, 103, "current", "physical"),
                ("caller", 2, 104 if self.saved_caller else 0,
                 "current" if self.saved_caller else "caller-unavailable", "caller"))
        text = f"source-frames stop={self.stop} thread=1 selected=0 count=4\n"
        for ordinal, (kind, function, scope, availability, name) in enumerate(rows):
            text += (f"  frame {ordinal} kind={kind} module=0 function={function} generation=1 "
                     f"runtime-epoch={self.epoch} scope-unit=0 scope-offset={scope} variables={availability} name={name}\n")
        return text + "source-frames end\n"

    def request(self, command):
        self.commands.append(command)
        if command == "status":
            return self.status()
        if command == "bt 1":
            if self.native:
                return self.status() + "  backtrace unavailable for this stop\n"
            return (self.status() + "  inline same call-site src/scope.cpp:7:2\n"
                    "  #0 module=0 function=1 [demo] physical\n"
                    "  #1 module=0 function=2 [demo] caller\n")
        if command == f"frames 1 {self.stop}":
            if self.frame_error:
                return "error: current source frame unavailable\n"
            result = self.frame_packet if self.frame_packet is not None else self.frames()
            if self.after_frames is not None:
                self.after_frames()
            return result
        if command.startswith("locals source "):
            words = command.split()
            allowed = ("0", "1", "2", "3") if self.saved_caller else ("0", "1", "2")
            if len(words) != 5 or words[2:4] != ["1", str(self.stop)] or words[4] not in allowed:
                return "error: source frame or stop is stale\n"
            ordinal = int(words[4])
            reply_stop = self.stop if self.reply_stop is None else self.reply_stop
            result = f"source-stop {reply_stop}\nsource local value type=int = i32={10 + ordinal}\n"
            if self.after_copy is not None:
                self.after_copy()
            return result
        if command.startswith("print-frame "):
            words = command.split(maxsplit=4)
            allowed = ("0", "1", "2", "3") if self.saved_caller else ("0", "1", "2")
            if len(words) != 5 or words[1:3] != ["1", str(self.stop)] or words[3] not in allowed:
                return "error: source frame or stop is stale\n"
            ordinal = int(words[3])
            reply_stop = self.stop if self.reply_stop is None else self.reply_stop
            result = self.print_packet if self.print_packet is not None else f"source-stop {reply_stop}\nsource local {words[4]} type=int = i32={10 + ordinal}\n"
            if self.after_copy is not None:
                self.after_copy()
            return result
        if command.startswith("locals wasm 1 0 "):
            first, count = map(int, command.split()[-2:])
            rows = "local 0 i32 = 5\n" if first == 0 and count else ""
            return f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=1\n" + rows
        if command == "locals 1":
            return "local 0 i32=5\n"
        if command in ("continue", "c"):
            self.state = "running"
            return self.status()
        if command.startswith("replace "):
            if self.before_replace is not None:
                self.before_replace()
            if self.replace_error is not None:
                return self.replace_error
            self.stop += 1  # Same physical PC and runtime epoch, new real stop label.
            return "function replaced; generation 2\n"
        raise AssertionError(command)

    def close(self):
        self.state = "debug domain closed"


def messages(raw):
    stream, result = io.BytesIO(raw), []
    while line := stream.readline():
        assert line.startswith(b"Content-Length: ") and line.endswith(b"\r\n")
        assert stream.readline() == b"\r\n"
        result.append(json.loads(stream.read(int(line[16:-2]))))
    return result


class SourceFrameTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.sequence = 0

    def send(self, command, **arguments):
        self.sequence += 1
        self.output.seek(0)
        self.output.truncate()
        self.adapter.handle({"seq": self.sequence, "type": "request", "command": command, "arguments": arguments})
        responses = [entry for entry in messages(self.output.getvalue()) if entry["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def frames(self):
        reply = self.send("stackTrace", threadId=1)
        self.assertTrue(reply["success"], reply)
        return reply["body"]["stackFrames"]

    def source_scope(self, frame):
        result = self.send("scopes", frameId=frame["id"])
        self.assertTrue(result["success"], result)
        return next(row["variablesReference"] for row in result["body"]["scopes"]
                    if row["name"] in ("Source variables", "Caller variables unavailable"))

    def assert_retired(self):
        self.assertIsNone(self.adapter.stop_key)
        for name in ("frames", "scopes", "source_frames", "source_frame_ordinals", "source_scope_stops",
                     "inline_frames", "native_frames", "native_code_refs"):
            self.assertFalse(getattr(self.adapter, name), name)

    def test_actual_header_rows_end_and_distinct_equal_name_ordinals(self):
        frames = self.frames()
        self.assertEqual(self.broker.commands, ["status", "bt 1", "frames 1 41", "status"])
        self.assertEqual([row["name"] for row in frames[:3]], ["same", "same", "physical"])
        self.assertEqual(len({row["id"] for row in frames}), 4)
        self.assertEqual(len(self.adapter.source_frame_ordinals), 4)
        self.assertTrue(all("instructionPointerReference" not in row for row in frames[:2] + frames[3:]))
        self.assertEqual(frames[2]["instructionPointerReference"], "wasm:0:1:4")
        self.assertTrue(all(not row["canRestart"] for row in frames))

    def test_source_page_retired_during_copy_cannot_publish_frame_ids(self):
        for field, value in (("stop", 42), ("epoch", 8), ("state", "running"), ("native", True)):
            with self.subTest(field=field):
                self.setUp()
                self.broker.after_frames = lambda: setattr(self.broker, field, value)
                reply = self.send("stackTrace", threadId=1)
                self.assertFalse(reply["success"], reply)
                self.assertIn("source frame stop changed", reply["message"])
                self.assert_retired()

    def test_same_named_values_use_exact_atomic_selected_frame_not_global_cursor(self):
        frames = self.frames()
        for ordinal in (1, 0, 2):  # Queued reselects, with no poll in between.
            reference = self.source_scope(frames[ordinal])
            before = len(self.broker.commands)
            result = self.send("variables", variablesReference=reference)
            self.assertTrue(result["success"], result)
            self.assertEqual(result["body"]["variables"][0]["value"], str(10 + ordinal))
            self.assertEqual(self.broker.commands[before:], ["status", f"locals source 1 41 {ordinal}", "status"])
            value = result["body"]["variables"][0]
            self.assertEqual(value["variablesReference"], 0)
            self.assertNotIn("memoryReference", value)

    def test_watch_hover_use_same_frame_atomic_print_and_post_status(self):
        frames = self.frames()
        for ordinal, context, expression in ((1, "watch", "value"), (0, "hover", "object.field[-2]"),
                                             (2, "variables", "ns::value")):
            before = len(self.broker.commands)
            result = self.send("evaluate", frameId=frames[ordinal]["id"], context=context, expression=expression)
            self.assertTrue(result["success"], result)
            if expression == "value":
                self.assertEqual(result["body"]["result"], str(10 + ordinal))
            else:
                self.assertIn(f"i32={10 + ordinal}", result["body"]["result"])
            self.assertEqual(result["body"]["variablesReference"], 0)
            self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 {ordinal} {expression}", "status"])

    def test_pointer_watch_syntax_routes_through_same_atomic_frame_command(self):
        # Synthetic formatter DATA exercises routing only. Neither a parsed
        # pointer expression nor this fake Broker grants actual memory access.
        frames = self.frames()
        cases = ((0, "watch", "*ptr", "*ptr"),
                 (1, "hover", "* ptr -> field", "*ptr->field"),
                 (2, "variables", "(*ptr).field", "(*ptr).field"),
                 (1, "watch", "(*ns::ptr)[-2].0", "(*ns::ptr)[-2].0"),
                 (0, "hover", "ptr -> field [ 1 ]", "ptr->field[1]"))
        for ordinal, context, expression, token in cases:
            before = len(self.broker.commands)
            result = self.send("evaluate", frameId=frames[ordinal]["id"], context=context, expression=expression)
            self.assertTrue(result["success"], result)
            self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 {ordinal} {token}", "status"])
            self.assertEqual(result["body"]["variablesReference"], 0)
            self.assertIn(token, result["body"]["result"])
            self.assertNotIn("memoryReference", result["body"])

    def test_source_names_matching_console_aliases_do_not_fake_resume_or_retire_frames(self):
        frame = self.frames()[1]
        stop_key = self.adapter.stop_key
        for expression in ("c", "continue", "pause", "s", "step"):
            with self.subTest(expression=expression):
                before = len(self.broker.commands)
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression=expression)
                self.assertTrue(result["success"], result)
                self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 1 {expression}", "status"])
                self.assertEqual(self.adapter.state, "stopped")
                self.assertEqual(self.adapter.stop_key, stop_key)
                self.assertIn(frame["id"], self.adapter.source_frame_ordinals)
                self.assertFalse(any(row.get("event") == "continued" for row in messages(self.output.getvalue())))

    def test_pointer_watch_actual_stop_or_generation_retirement_discards_reply(self):
        # Same PC can have a different actual stop or code generation. DATA
        # labels never keep the old frame alive through the second status.
        for changed in ("stop", "epoch"):
            with self.subTest(changed=changed):
                self.setUp()
                frame = self.frames()[1]
                self.broker.after_copy = lambda: setattr(self.broker, changed, getattr(self.broker, changed) + 1)
                before = len(self.broker.commands)
                reply = self.send("evaluate", frameId=frame["id"], context="watch", expression="(*ptr).field")
                self.assertFalse(reply["success"], reply)
                self.assertNotIn("body", reply)
                self.assertEqual(self.broker.commands[before:], ["status", "print-frame 1 41 1 (*ptr).field", "status"])
                self.assert_retired()

    def test_source_expression_precedence_namespaces_and_exact_finite_limits(self):
        # Pure syntax DATA. Different unary/postfix order is retained verbatim
        # for the controller's existing parse_source_expression to interpret.
        cases = (("*ptr[1]", "*ptr[1]"), ("(*ptr)[1]", "(*ptr)[1]"),
                 ("tuple.0", "tuple.0"), ("p .* .next.*.value", "p.*.next.*.value"),
                 ("p" + ".*" * 32, "p" + ".*" * 32),
                 ("ns::inner::ptr->0", "ns::inner::ptr->0"),
                 (" ( * ptr ) . field ", "(*ptr).field"),
                 ("ptr[-9223372036854775808]", "ptr[-9223372036854775808]"),
                 ("ptr[9223372036854775807]", "ptr[9223372036854775807]"),
                 ("*" * 32 + "p", "*" * 32 + "p"),
                 ("p" + ".x" * 32, "p" + ".x" * 32),
                 ("p" + "->x" * 16, "p" + "->x" * 16),
                 ("(" * 31 + "*p" + ")" * 31, "(" * 31 + "*p" + ")" * 31),
                 ("*" + "p" * 255, "*" + "p" * 255))
        for expression, token in cases:
            with self.subTest(expression=expression):
                self.assertEqual(dap.validate_source_expression(expression), token)
        invalid = ("p" + ".*" * 33, "p.?", "p. *", "p.*()", "p.*;continue", "*" * 33 + "p", "p" + ".x" * 33, "p" + "->x" * 17,
                   "(" * 32 + "*p" + ")" * 32, "*" + "p" * 256,
                   "ptr[-9223372036854775809]", "ptr[9223372036854775808]",
                   "a b", "ns ::p", "ns:: p", "ptr.0field", "ptr[01]")
        for expression in invalid:
            with self.subTest(expression=expression):
                with self.assertRaises(ValueError):
                    dap.validate_source_expression(expression)

    def test_pointer_expression_rejects_calls_assignment_numeric_refs_and_injection_before_broker(self):
        invalid = ("*0x1000", "*1234", "$pc", "ref(1)", "ptr()", "ptr->field=1",
                   "ptr->field++", "(Type*)ptr", "&ptr", "*(ptr+1)", "ptr;continue",
                   "*ptr\nstatus", "*ptr\rstatus", "ptr\t->field", "λ", "a b", "ns ::p", "ns:: p")
        for expression in invalid:
            with self.subTest(expression=expression):
                frame = self.frames()[0]
                before = len(self.broker.commands)
                reply = self.send("evaluate", frameId=frame["id"], context="watch", expression=expression)
                self.assertFalse(reply["success"], reply)
                self.assertEqual(self.broker.commands[before:], [])

    def test_scalar_watch_hover_routes_original_expression_atomically(self):
        cases = ("value + 2 * 3", "(value + 2) * 3", "pair.0 + pair.1", "p.* + 1",
                 "sizeof(unsigned long) + sizeof(value)", "value as i64", "@as(i64, value)",
                 "static_cast<double>(value) / .5", "int32(value)", "(unsigned char)257",
                 "' ' + 1", "0 && missing", "1 || missing", "0xff &^ 3 << 1", "value ? 1 : 2", "value > 0 ? value : 1/0",
                 "0 ? call_free_missing : 7", "1 ? real32 : real64",
                 "sizeof(value + shadow)", "sizeof(value / 0)", "sizeof(real32 + real64)",
                 "sizeof(1 << -1)", "sizeof(\'(\' + \')\')", "sizeof(sizeof(packet) + 1)",
                 "sizeof value", "sizeof -value", "sizeof !flag", "sizeof sizeof packet",
                 "sizeof -1 + 2", "sizeof +*p", "'\\x41' + value", "'\\101'", "sizeof('\\x28' + '\\x29')", "sizeof '\\a'", "'\\\\' + '\\x27'")
        frames = self.frames()
        for index, expression in enumerate(cases):
            with self.subTest(expression=expression):
                ordinal = index % 3
                before = len(self.broker.commands)
                result = self.send("evaluate", frameId=frames[ordinal]["id"],
                                   context=("watch", "hover", "variables")[ordinal], expression=expression)
                self.assertTrue(result["success"], result)
                self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 {ordinal} {expression}", "status"])
                self.assertEqual(result["body"]["variablesReference"], 0)
                self.assertNotIn("memoryReference", result["body"])

    def test_scalar_watch_shared_controller_syntax_corpus(self):
        corpus = json.loads(Path(__file__).with_name("dap_source_expression_cases.json").read_text())
        for row in corpus["positive"]:
            expression = row["expression"]
            with self.subTest(expression=expression):
                self.assertEqual(dap.validate_source_evaluation_expression(expression), expression)
        for expression in corpus["negative"]:
            with self.subTest(expression=expression):
                frame = self.frames()[0]
                before = len(self.broker.commands)
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression=expression)
                self.assertFalse(result["success"], result)
                self.assertEqual(self.broker.commands[before:], [])

    def test_scalar_watch_exact_bounds_include_unselected_operands(self):
        for expression in ("1+" * 31 + "1", "!" * 30 + "1", "(" * 15 + "1" + ")" * 15,
                           "p" + ".*" * 32 + "+1", "p" + "->x" * 16 + "+1",
                           "*" + "p" + ".*" * 31 + "+1", "1+" + "a" * 254):
            with self.subTest(expression=expression):
                self.assertEqual(dap.validate_source_evaluation_expression(expression), expression)
        for expression in ("1+" * 64 + "1", "!" * 31 + "1", "(" * 16 + "1" + ")" * 16,
                           "p" + ".*" * 33 + "+1", "p" + "->x" * 17 + "+1",
                           "*" + "p" + ".*" * 32 + "+1", "1+" + "a" * 255,
                           "0 && (" + "!" * 30 + "1)"):
            with self.subTest(expression=expression):
                with self.assertRaises(ValueError):
                    dap.validate_source_evaluation_expression(expression)

    def test_scalar_watch_rejects_unsafe_unselected_arms_before_broker(self):
        cases = ("0 && call()", "1 || (value=2)", "0 && *(value + 1)",
                 "1 || *0x1000", "0 && (int*)value", "1 || ++value",
                 "0 && value++", "0 && value;continue", "1 || value\nstatus",
                 "sizeof(call())", "@as(*i32, value)", "(void*)value", "1 ? value : call()", "1 ? value : (value=2)",
                 "value, 1", "a b + 1", "ns ::value + 1", "sizeof(value + call())",
                 "sizeof(value = 2)", "sizeof(++value)", "sizeof(*(value + 1))", "sizeof ++value", "sizeof value++",
                 "sizeof call()", "sizeof *0x1000", "sizeof *(value + 1)", "'\\x80'", "'\\0000'", "0 && '\\x'", "1 ? 7 : '\\1234'", "sizeof('\\x41';continue)")
        for expression in cases:
            with self.subTest(expression=expression):
                frame = self.frames()[0]
                before = len(self.broker.commands)
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression=expression)
                self.assertFalse(result["success"], result)
                self.assertEqual(self.broker.commands[before:], [])

    def test_scalar_watch_controller_failure_does_not_publish_or_mint_pointers(self):
        for expression in ("ptr+1", "1 / 0", "missing + 1"):
            with self.subTest(expression=expression):
                self.setUp()
                frame = self.frames()[0]
                self.broker.print_packet = "error: source scalar expression unavailable\n"
                before = len(self.broker.commands)
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression=expression)
                self.assertFalse(result["success"], result)
                self.assertNotIn("body", result)
                self.assertEqual(self.broker.commands[before:], ["status", f"print-frame 1 41 0 {expression}"])
                self.assert_retired()

    def test_scalar_watch_changed_or_wrong_stop_discards_copied_reply(self):
        for changed in ("stop", "epoch", "reply_stop"):
            with self.subTest(changed=changed):
                self.setUp()
                frame = self.frames()[1]
                if changed == "reply_stop":
                    self.broker.reply_stop = self.broker.stop + 1
                else:
                    self.broker.after_copy = lambda: setattr(self.broker, changed, getattr(self.broker, changed) + 1)
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression="value + 1")
                self.assertFalse(result["success"], result)
                self.assertNotIn("body", result)
                self.assert_retired()

    def test_object_and_scalar_formatter_envelopes_publish_only_owned_display(self):
        for payload in ("$expression: type=int, offset=0, bytes=4, value=9",
                        "packet: type=TupleProbe, offset=0, bytes=8\n  __0: type=int, offset=4, bytes=4, value=-9",
                        "p: type=pointer, offset=0, bytes=4, value=guest:0x100"):
            with self.subTest(payload=payload):
                self.setUp()
                self.assertTrue(self.send("initialize", supportsVariableType=True)["success"])
                frame = self.frames()[1]
                packet = "source-value stop=41 name=value + 2\n" + payload + "\nsource-value end\n"
                self.broker.print_packet = packet
                result = self.send("evaluate", frameId=frame["id"], context="watch", expression="value + 2")
                self.assertTrue(result["success"], result)
                if payload.startswith("$expression: type=int,"):
                    self.assertEqual(result["body"]["result"], "9")
                    self.assertEqual(result["body"]["type"], "int")
                elif payload.startswith("packet:"):
                    self.assertEqual(result["body"]["result"], "TupleProbe")
                    self.assertGreater(result["body"]["variablesReference"], 0)
                else:
                    self.assertEqual(result["body"]["result"], "guest:0x100")
                    self.assertEqual(result["body"]["variablesReference"], 0)
                self.assertNotIn("memoryReference", result["body"])

    def test_object_formatter_stale_duplicate_missing_and_mixed_headers_retire(self):
        header = "source-value stop=41 name=value + 2\n"
        body = "$expression: type=int, offset=0, bytes=4, value=9\n"
        packets = (header + body, header + "source-value end\n", header + body + "source-type end\n",
                   header + body + "source-value end\nsource-value end\n",
                   header.replace("stop=41", "stop=42") + body + "source-value end\n",
                   header.replace("stop=41", "stop=18446744073709551616") + body + "source-value end\n",
                   header + header + body + "source-value end\n",
                   header + "source-stop 41\n" + body + "source-value end\n",
                   "source-stop 41\n" + header + body + "source-value end\n",
                   header + body + "source-value end\nextra\n",
                   "source-type stop=41 name=value\n" + body + "source-type end\n")
        for packet in packets:
            with self.subTest(packet=packet):
                self.setUp()
                frame = self.frames()[0]
                self.broker.print_packet = packet
                result = self.send("evaluate", frameId=frame["id"], context="hover", expression="value + 2")
                self.assertFalse(result["success"], result)
                self.assertNotIn("body", result)
                self.assert_retired()

    def test_only_current_physical_frame_gets_wasm_locals(self):
        frames = self.frames()
        for ordinal, frame in enumerate(frames):
            scopes = self.send("scopes", frameId=frame["id"])["body"]["scopes"]
            expected = ["Source variables", "Wasm locals", "WASIp1 arguments", "WASIp1 environment", "WASIp1 descriptors", "WASIp1 preopens"] if ordinal == 2 else ["Caller variables unavailable"] if ordinal == 3 else ["Source variables"]
            self.assertEqual([row["name"] for row in scopes], expected)
            if ordinal == 2:
                reply = self.send("variables", variablesReference=scopes[1]["variablesReference"])
                self.assertEqual(reply["body"]["variables"][0]["value"], "i32=5")
                self.assertEqual(self.broker.commands[-3:], ["status", "locals wasm 1 0 0 1", "status"])

    def test_caller_scope_is_unavailable_and_never_prints_or_copies_values(self):
        caller = self.frames()[3]
        reference = self.source_scope(caller)
        before = len(self.broker.commands)
        result = self.send("variables", variablesReference=reference)
        self.assertTrue(result["success"], result)
        self.assertEqual(result["body"]["variables"][0]["type"], "unavailable")
        self.assertEqual(self.broker.commands[before:], ["status"])
        before = len(self.broker.commands)
        self.assertFalse(self.send("evaluate", frameId=caller["id"], context="watch", expression="value")["success"])
        self.assertEqual(self.broker.commands[before:], [])

    def test_authenticated_saved_caller_routes_values_to_its_actual_ordinal(self):
        self.broker.saved_caller = True
        caller = self.frames()[3]
        self.assertIn("saved caller values", caller["name"])
        self.assertNotIn("instructionPointerReference", caller)
        scopes = self.send("scopes", frameId=caller["id"])["body"]["scopes"]
        self.assertEqual([row["name"] for row in scopes], ["Source variables"])
        values = self.send("variables", variablesReference=scopes[0]["variablesReference"])
        self.assertTrue(values["success"], values)
        self.assertEqual(values["body"]["variables"][0]["value"], "13")
        self.assertIn("locals source 1 41 3", self.broker.commands)
        value = self.send("evaluate", frameId=caller["id"], context="watch", expression="value")
        self.assertTrue(value["success"], value)
        self.assertIn("print-frame 1 41 3 value", self.broker.commands)

    def test_saved_caller_requires_source_scope_and_actual_unchanged_stop(self):
        self.broker.saved_caller = True
        packet = self.broker.frames()
        paged = packet.replace("count=4", "count=4 total=4 first=0 physical=2")
        self.assertTrue(dap.parse_source_frames(paged, 1, 41)[3]["current"])
        with self.assertRaises(ValueError):
            dap.parse_source_frames(paged.replace("source-frames", "wasm-frames"), 1, 41, wasm=True)
        with self.assertRaises(ValueError):
            dap.parse_source_frames(packet.replace("scope-offset=104", "scope-offset=0"), 1, 41)
        caller = self.frames()[3]
        reference = self.source_scope(caller)
        self.broker.after_copy = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertFalse(self.adapter.source_frame_ordinals)

    def test_unknown_frame_and_forbidden_expressions_fail_before_broker(self):
        for identifier in (0, 999999, True, "1"):
            self.frames()
            before = len(self.broker.commands)
            self.assertFalse(self.send("evaluate", frameId=identifier, context="watch", expression="value")["success"])
            self.assertEqual(self.broker.commands[before:], [])
        for expression in ("value()", "value;continue", "value[+1]", "value[01]", "value\nstatus", "λ", "a" * 257):
            frame = self.frames()[0]
            before = len(self.broker.commands)
            self.assertFalse(self.send("evaluate", frameId=frame["id"], context="hover", expression=expression)["success"])
            self.assertEqual(self.broker.commands[before:], [])

    def test_same_pc_new_stop_retires_frame_scope_before_value_copy(self):
        frame = self.frames()[1]
        reference = self.source_scope(frame)
        self.broker.stop += 1
        before = len(self.broker.commands)
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.broker.commands[before:], ["status"])
        self.assertFalse(self.adapter.source_frame_ordinals)
        fresh = self.frames()[1]
        self.assertNotEqual(fresh["id"], frame["id"])
        before = len(self.broker.commands)
        self.assertFalse(self.send("evaluate", frameId=frame["id"], context="watch", expression="value")["success"])
        self.assertEqual(self.broker.commands[before:], [])

    def test_stop_changes_during_copy_discard_variable_and_watch_results(self):
        for request in ("variables", "evaluate"):
            with self.subTest(request=request):
                self.setUp()
                frame = self.frames()[1]
                reference = self.source_scope(frame)
                self.broker.after_copy = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
                before = len(self.broker.commands)
                args = {"variablesReference": reference} if request == "variables" else {"frameId": frame["id"], "context": "watch", "expression": "value"}
                result = self.send(request, **args)
                self.assertFalse(result["success"])
                self.assertNotIn("body", result)
                expected = "locals source 1 41 1" if request == "variables" else "print-frame 1 41 1 value"
                self.assertEqual(self.broker.commands[before:], ["status", expected, "status"])
                self.assert_retired()

    def test_reply_stop_mismatch_retires_all_ordinal_labels(self):
        reference = self.source_scope(self.frames()[0])
        self.broker.reply_stop = 42
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assert_retired()

    def test_evaluate_packet_stop_checked_when_both_statuses_are_unchanged(self):
        for context in ("watch", "hover", "variables"):
            with self.subTest(context=context):
                self.setUp()
                frame = self.frames()[1]
                self.broker.reply_stop = 42
                before = len(self.broker.commands)
                reply = self.send("evaluate", frameId=frame["id"], context=context, expression="value")
                self.assertFalse(reply["success"])
                self.assertNotIn("body", reply)
                self.assertEqual(self.broker.commands[before:], ["status", "print-frame 1 41 1 value", "status"])
                self.assert_retired()

    def test_evaluate_malformed_or_unidentified_packets_do_not_publish_values(self):
        bad = ("source local value type=int = i32=11\n", "source-stop 41\n", "source-stop 41\n\n",
               "source-stop 41\n  \n", "source-stop 41\nsource-stop\n",
               "source-stop 41\nsource-stop 41\nsource local value type=int = i32=11\n",
               "source-stop 18446744073709551616\nsource local value type=int = i32=11\n",
               "source-stop 41\r\nsource local value type=int = i32=11\n",
               "source-stop 41\nsource local value type=int = i32=11\x1b\n",
               "source-stop 41\n" + "a" * dap.MAX_REPLY + "\n")
        for packet in bad:
            with self.subTest(packet=packet[:80]):
                self.setUp()
                frame = self.frames()[1]
                self.broker.print_packet = packet
                reply = self.send("evaluate", frameId=frame["id"], context="watch", expression="value")
                self.assertFalse(reply["success"])
                self.assertNotIn("body", reply)
                self.assert_retired()

    def test_evaluate_identity_check_preserves_owned_object_and_origin_text(self):
        frame = self.frames()[1]
        packet = ("source-stop 41\nsource local value type=TupleProbe = object\n"
                  "  .__0 type=int = i32=-9\n"
                  "source-origin stop=41 thread=1 code-offset=4 variable-unit=0 variable-offset=2 "
                  "scope-unit=0 scope-offset=1 type-unit=0 type-offset=3 kind=DW_AT_const_value\n")
        self.broker.print_packet = packet
        reply = self.send("evaluate", frameId=frame["id"], context="watch", expression="value")
        self.assertTrue(reply["success"], reply)
        self.assertEqual(reply["body"]["result"], packet.rstrip("\n"))
        self.assertEqual(reply["body"]["variablesReference"], 0)
        self.assertNotIn("memoryReference", reply["body"])

    def test_evaluate_origin_receipt_must_match_stop_and_participant(self):
        for origin in ("source-origin stop=42 thread=1 kind=DW_AT_const_value\n",
                       "source-origin stop=41 thread=2 kind=DW_AT_const_value\n",
                       "source-origin stop=41 thread=18446744073709551616 kind=DW_AT_const_value\n",
                       "source-origin malformed\n",
                       "source-origin stop=41 thread=1 kind=DW_AT_const_value\n" * 2):
            with self.subTest(origin=origin):
                self.setUp()
                frame = self.frames()[1]
                self.broker.print_packet = "source-stop 41\nsource local value type=int = i32=11\n" + origin
                reply = self.send("evaluate", frameId=frame["id"], context="watch", expression="value")
                self.assertFalse(reply["success"])
                self.assertNotIn("body", reply)
                self.assert_retired()

    def test_resume_and_native_repark_do_not_reuse_source_ordinals(self):
        frame = self.frames()[0]
        reference = self.source_scope(frame)
        self.assertTrue(self.send("continue", threadId=1)["success"])
        self.assert_retired()
        before = len(self.broker.commands)
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.broker.commands[before:], [])
        self.broker.stop += 1
        self.broker.state = "stopped"
        frame = self.frames()[0]
        reference = self.source_scope(frame)
        self.broker.native = True
        self.broker.stop += 1
        before = len(self.broker.commands)
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.broker.commands[before:], ["status"])
        native_frames = self.frames()
        self.assertEqual(len(native_frames), 1)
        self.assertFalse(self.adapter.source_frame_ordinals)
        self.assertEqual([scope["name"] for scope in self.send("scopes", frameId=native_frames[0]["id"])["body"]["scopes"]], ["Registers"])

    def test_success_or_rejected_replace_retires_before_send_without_poll(self):
        for rejected in (False, True):
            with self.subTest(rejected=rejected):
                self.setUp()
                frame = self.frames()[1]
                reference = self.source_scope(frame)
                self.broker.before_replace = self.assert_retired
                if rejected:
                    self.broker.replace_error = "error: control request rejected: replacement body failed WebAssembly validation\n"
                before = len(self.broker.commands)
                result = self.send("evaluate", expression="replace 0 2 1 /host/body", context="repl")
                self.assertEqual(result["success"], not rejected)
                expected = ["replace 0 2 1 /host/body"] + ([] if rejected else ["status"])
                self.assertEqual(self.broker.commands[before:], expected)
                self.assertFalse(self.adapter.source_frame_ordinals)
                before = len(self.broker.commands)
                self.assertFalse(self.send("variables", variablesReference=reference)["success"])
                self.assertFalse(self.send("evaluate", frameId=frame["id"], context="watch", expression="value")["success"])
                self.assertEqual(self.broker.commands[before:], [])

    def test_unavailable_metadata_uses_labels_without_ordinal_watch_permission(self):
        self.broker.frame_error = True
        frames = self.frames()
        self.assertEqual(self.broker.commands, ["status", "bt 1", "frames 1 41", "status"])
        self.assertTrue(frames[0]["name"].startswith("Inlined "))
        self.assertFalse(self.adapter.source_frame_ordinals)
        self.assertEqual(self.send("scopes", frameId=frames[0]["id"])["body"]["scopes"], [])
        before = len(self.broker.commands)
        self.assertFalse(self.send("evaluate", frameId=frames[1]["id"], context="watch", expression="value")["success"])
        self.assertEqual(self.broker.commands[before:], [])

    def test_malformed_rows_stop_bounds_and_order_do_not_leave_partial_frames(self):
        packet = self.broker.frames()
        bad = (packet.replace("stop=41", "stop=42"), packet.replace("thread=1", "thread=2"),
               packet.replace("selected=0", "selected=4"), packet.replace("count=4", "count=129"),
               packet.replace("  frame 1", "  frame 0"), packet.replace("generation=1", "generation=0", 1),
               packet.replace("runtime-epoch=7", "runtime-epoch=8", 1),
               packet.replace("kind=caller", "kind=inline"), packet.replace("variables=caller-unavailable", "variables=current"),
               packet.replace("name=same", "name=same\x1b", 1), packet.replace("source-frames end\n", ""))
        for malformed in bad:
            with self.subTest(packet=malformed):
                self.setUp()
                self.broker.frame_packet = malformed
                self.assertFalse(self.send("stackTrace", threadId=1)["success"])
                self.assert_retired()


if __name__ == "__main__":
    unittest.main()
