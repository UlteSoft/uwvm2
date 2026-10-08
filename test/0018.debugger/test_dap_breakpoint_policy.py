"""Breakpoint policy DATA regression. Real hit behavior is qualified by the CLI test."""
import importlib.util
import io
import json
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("policy_dap", Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


class Broker:
    def __init__(self):
        self.commands = []
        self.reject = False

    def request(self, command):
        self.commands.append(command)
        if command.startswith("delete "):
            return "breakpoint deleted\n"
        if command.startswith(("break ", "break-source ")):
            return "error: invalid breakpoint target\n" if self.reject else "breakpoint 17 registered at executable Wasm expression byte offset\n"
        raise AssertionError(command)


class Tests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker

    def send(self, command, **arguments):
        self.adapter.handle({"seq": 1, "type": "request", "command": command, "arguments": arguments})
        raw = self.output.getvalue()
        head, body = raw.split(b"\r\n\r\n", 1)
        self.assertEqual(int(head.split(b": ")[1]), len(body))
        self.output.seek(0); self.output.truncate()
        return json.loads(body)

    def points(self, **policy):
        return self.send("setBreakpoints", source={"path": "source file.cpp"}, breakpoints=[{"line": 9, **policy}])["body"]["breakpoints"]

    def test_thresholds_use_one_atomic_command(self):
        self.assertTrue(self.points(hitCondition=">=4")[0]["verified"])
        self.assertEqual(self.broker.commands, ["break-source 0 source file.cpp:9 ignore 3"])
        self.assertTrue(self.send("setInstructionBreakpoints", breakpoints=[{"instructionReference": "wasm:0:1:2", "hitCondition": ">3"}])["body"]["breakpoints"][0]["verified"])
        self.assertEqual(self.broker.commands[-1], "break 0 1 2 ignore 3")

    def test_language_conditions_are_one_atomic_command(self):
        self.assertTrue(self.points(condition="counter == 4", hitCondition=">=2")[0]["verified"])
        self.assertEqual(self.broker.commands, ["break-source 0 source file.cpp:9 ignore 1 if counter == 4"])
        result = self.send("setInstructionBreakpoints", breakpoints=[{"instructionReference": "wasm:0:1:2", "condition": "parameter > 0"}])
        self.assertTrue(result["body"]["breakpoints"][0]["verified"])
        self.assertEqual(self.broker.commands[-1], "break 0 1 2 if parameter > 0")

    def test_condition_bounds_and_backend_syntax_admission(self):
        for condition in ["a"*257, "\\x1b", None, 4, "x\\ncontinue"]:
            if condition == "\\x1b": condition = "\x1b"
            if condition == "x\\ncontinue": condition = "x\ncontinue"
            with self.subTest(condition=condition):
                self.assertFalse(self.points(condition=condition)[0]["verified"])
                self.assertEqual(self.broker.commands, [])
        self.broker.reject = True
        self.assertFalse(self.points(condition="run()")[0]["verified"])
        self.assertEqual(self.broker.commands, ["break-source 0 source file.cpp:9 if run()"])

    def test_zero_and_uint64_bounds(self):
        for condition, count in [(">=0", 0), (">=1", 0), (">0", 0), (">=18446744073709551615", (1 << 64)-2)]:
            self.assertEqual(dap.breakpoint_ignore_count({"hitCondition": condition}), count)

    def test_unsupported_policies_never_install_unconditional_breakpoints(self):
        for policy in [{"condition": "counter\ncontinue"}, {"logMessage": "counter={counter}"}, {"condition": None},
                       {"hitCondition": "4"}, {"hitCondition": "==4"}, {"hitCondition": "%4"},
                       {"hitCondition": ">18446744073709551616"}, {"hitCondition": ">3\ncontinue"}, {"hitCondition": 4}]:
            with self.subTest(policy=policy):
                self.assertFalse(self.points(**policy)[0]["verified"])
                self.assertEqual(self.broker.commands, [])
        with self.assertRaises(ValueError):
            dap.breakpoint_ignore_count({"hitCondition": ">18446744073709551615"})

    def test_instruction_policies_also_reject_before_registration(self):
        for policy in [{"condition": "x\ncontinue"}, {"logMessage": "x"}, {"hitCondition": "3"}]:
            result = self.send("setInstructionBreakpoints", breakpoints=[{"instructionReference": "wasm:0:1:2", **policy}])
            self.assertFalse(result["body"]["breakpoints"][0]["verified"])
        self.assertEqual(self.broker.commands, [])

    def test_empty_fields_preserve_plain_breakpoint(self):
        self.assertTrue(self.points(condition="", logMessage="", hitCondition="")[0]["verified"])
        self.assertEqual(self.broker.commands, ["break-source 0 source file.cpp:9"])

    def test_failed_atomic_registration_is_not_verified(self):
        self.broker.reject = True
        self.assertFalse(self.points(hitCondition=">=4")[0]["verified"])
        self.assertEqual(self.broker.commands, ["break-source 0 source file.cpp:9 ignore 3"])
        self.assertEqual(self.adapter.breakpoints, {})

    def test_capabilities_are_explicit(self):
        body = self.send("initialize")["body"]
        self.assertTrue(body["supportsHitConditionalBreakpoints"])
        self.assertTrue(body["supportsConditionalBreakpoints"])
        self.assertFalse(body["supportsLogPoints"])


if __name__ == "__main__":
    unittest.main()
