#!/usr/bin/env python3
"""Bounded DAP source-value protocol tests; no VM/source authority is mocked in.

The broker below supplies formatter-shaped text only. A separate native/CLI
replay must qualify genuine tickets, copied slots, metadata and generations.
Run this test with the keeper in the Linux test cgroup.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("source_values_dap", ROOT / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


def messages(raw):
    stream, result = io.BytesIO(raw), []
    while line := stream.readline():
        assert line.startswith(b"Content-Length: ") and line.endswith(b"\r\n")
        assert stream.readline() == b"\r\n"
        payload = stream.read(int(line[16:-2]))
        result.append(json.loads(payload))
    return result


class SourceBroker:
    def __init__(self):
        self.commands = []
        self.stop_id = 41
        self.reply_id = None
        self.native = False
        self.status_identified = True
        self.rows = ("source parameter value type=int = i32=5\n"
                     "source local flag type=bool = bool=true\n"
                     "source local fraction type=float = f32 bits=0x7fa00001\n"
                     "source local optimized type=long = unavailable (no location)\n")

    def status(self):
        head = "stopped: native instruction step\n" if self.native else "stopped: selected participant step\n"
        if self.status_identified:
            head += f"stop-id {self.stop_id}\n"
        head += "thread 1 module=0 function=1 byte-offset=4 generation=7\n"
        return head + ("  native-pc=0x1004\n" if self.native else "  source scope.cpp:10:1\n")

    def request(self, command):
        self.commands.append(command)
        if command == "status":
            return self.status()
        if command == "bt 1":
            return self.status() + ("  backtrace unavailable for this stop\n" if self.native else "  #0 module=0 function=1 [demo] helper\n")
        if command == f"frames 1 {self.stop_id}":
            return (f"source-frames stop={self.stop_id} thread=1 selected=0 count=1\n"
                    "  frame 0 kind=physical module=0 function=1 generation=1 runtime-epoch=7 "
                    "scope-unit=0 scope-offset=64 variables=current name=helper\nsource-frames end\n")
        if command == f"locals source 1 {self.stop_id} 0":
            return f"source-stop {self.reply_id if self.reply_id is not None else self.stop_id}\n" + self.rows
        if command.startswith("locals wasm 1 0 "):
            first, count = map(int, command.split()[-2:])
            rows = "local 0 i32 = 5\n" if first == 0 and count else ""
            return f"wasm-stop {self.stop_id}\nWasm state thread=1 module=0 epoch=7 first={first} total=1\n" + rows
        if command == "locals 1":
            return "local 0 i32=5\n"
        if command == "continue":
            return "running\n"
        raise AssertionError(command)


class SourceValuesTests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = SourceBroker()
        self.adapter.broker = self.broker
        self.sequence = 0

    def send(self, command, **arguments):
        self.sequence += 1
        self.adapter.handle({"seq": self.sequence, "type": "request", "command": command, "arguments": arguments})
        result = messages(self.output.getvalue())
        self.output.seek(0)
        self.output.truncate()
        responses = [entry for entry in result if entry["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def scopes(self):
        frames = self.send("stackTrace", threadId=1)["body"]["stackFrames"]
        self.assertEqual(len(frames), 1)
        return self.send("scopes", frameId=frames[0]["id"])["body"]["scopes"]

    def source_reference(self):
        scopes = self.scopes()
        self.assertEqual([row["name"] for row in scopes], ["Source variables", "Wasm locals", "WASIp1 arguments", "WASIp1 environment", "WASIp1 descriptors", "WASIp1 preopens"])
        return scopes[0]["variablesReference"]

    def test_numeric_values_are_read_only_display_leaves(self):
        reference = self.source_reference()
        result = self.send("variables", variablesReference=reference)
        self.assertTrue(result["success"])
        self.assertEqual(self.broker.commands[-3:], ["status", "locals source 1 41 0", "status"])
        values = result["body"]["variables"]
        self.assertEqual([v["value"] for v in values], ["5", "true", "f32 bits=0x7fa00001", "unavailable (no location)"])
        self.assertTrue(all(v["variablesReference"] == 0 and "evaluateName" not in v and "memoryReference" not in v for v in values))
        self.assertEqual(self.send("variables", variablesReference=reference, start=1, count=1)["body"]["variables"], values[1:2])
        self.assertEqual(self.send("variables", variablesReference=reference, filter="indexed")["body"]["variables"], [])

    def test_wasm_scope_remains_independent(self):
        scopes = self.scopes()
        values = self.send("variables", variablesReference=next(row["variablesReference"] for row in scopes if row["name"] == "Wasm locals"))
        self.assertEqual(values["body"]["variables"][0]["value"], "i32=5")
        self.assertEqual(self.broker.commands[-3:], ["status", "locals wasm 1 0 0 1", "status"])

    def test_same_pc_generation_new_pause_retires_source_references(self):
        reference = self.source_reference()
        self.broker.stop_id += 1  # All module/function/PC/code generation fields are unchanged.
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertFalse(any(command.startswith("locals source ") for command in self.broker.commands))
        self.assertEqual(self.adapter.source_scope_stops, {})
        new_reference = self.source_reference()
        self.assertNotEqual(reference, new_reference)
        self.assertTrue(self.send("variables", variablesReference=new_reference)["success"])

    def test_values_reply_must_match_captured_stop(self):
        reference = self.source_reference()
        self.broker.reply_id = 42
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.adapter.scopes, {})

    def test_native_and_unidentified_stops_do_not_offer_source_scopes(self):
        self.broker.status_identified = False
        self.assertEqual([row["name"] for row in self.scopes()], ["Wasm locals"])
        self.broker.status_identified = True
        self.broker.native = True
        self.assertEqual([scope["name"] for scope in self.scopes()], ["Registers"])

    def test_resume_and_malformed_values_retire_all_source_labels(self):
        reference = self.source_reference()
        self.assertTrue(self.send("continue", threadId=1)["success"])
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual((self.adapter.source_frames, self.adapter.source_scope_stops), (set(), {}))
        reference = self.source_reference()
        self.broker.rows = "source local ok type=int = i32=1\nsource local broken\n"
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.adapter.frames, {})

    def test_empty_and_unavailable_values_are_bounded(self):
        self.assertEqual(dap.parse_source_values("source-stop 41\nno active source variables\n", 41), [])
        values = dap.parse_source_values("source-stop 41\nsource locals unavailable: captured stop, source or function generation is stale\n", 41)
        self.assertEqual(values[0]["variablesReference"], 0)
        self.assertEqual(values[0]["type"], "unavailable")

    def test_numeric_width_limits_and_escaped_labels(self):
        rows = ("source local minimum type=int64 = i64=-9223372036854775808\n"
                "source local maximum type=uint64 = u64=18446744073709551615\n"
                "source local escaped\\x1b type=float = f64 bits=0x7ff0000000000001\n")
        values = dap.parse_source_values("source-stop 41\n" + rows, 41)
        self.assertEqual(values[2]["name"], "escaped\\x1b")
        bad = ["source local x type=int8 = i8=128", "source local x type=int8 = i8=-129",
               "source local x type=uint8 = u8=256", "source local x type=uint64 = u64=-1",
               "source local x type=float = f32 bits=0x100000000", "source local x type=int = i32=01",
               "source local x type=int = address=0x1234", "source local x type=int = i32=1\x1b",
               "source local x type=nested type=int = i32=1", "source local x type=int = unavailable\nsource-stop 41",
               "source local " + "x" * 4097 + " type=int = i32=1"]
        for row in bad:
            with self.subTest(row=row[:100]), self.assertRaises(ValueError):
                dap.parse_source_values("source-stop 41\n" + row + "\n", 41)
        for text in ("source local x type=int = i32=1\n", "source-stop 42\nno active source variables\n",
                     "source-stop 0\nno active source variables\n", "source-stop 41\n",
                     "source-stop 41\n" + "source local x type=int = i32=1\n" * 1025):
            with self.subTest(text=text[:100]), self.assertRaises(ValueError):
                dap.parse_source_values(text, 41)

    def test_metadata_source_byte_budget_accepts_formatter_escapes(self):
        labels = ("x" * 4096, r"\xe2" * 4096, r"\\" * 4096,
                  r'\"' * 4096, r"\\x1b" * 1024,
                  "prefix" + r"\x00\x1f\x7f\x80\xff\\\"",
                  "literal... (truncated)", "")
        for label in labels:
            with self.subTest(label=label[:80]):
                values = dap.parse_source_values("source-stop 41\nsource local " + label + " type=int = i32=1\n", 41)
                self.assertEqual(values[0]["name"], label or "<anonymous>")
                values = dap.parse_source_values("source-stop 41\nsource local x type=" + label + " = i32=1\n", 41)
                self.assertEqual(values[0]["type"], label)

    def test_metadata_source_byte_budget_rejects_false_encoded_width(self):
        labels = ("x" * 4097, r"\xe2" * 4097, r"\\" * 4097,
                  r'\"' * 4097, r"\\x1b" * 1025,
                  "x" * 4096 + r"\x80", r"\x80" * 4096 + "x",
                  "unfinished\\", r"\q", r"\x0", r"\xgg", r"\xAF",
                  r"\x20", r"\x41", r"\x5c", 'raw"quote')
        for label in labels:
            for field in ("name", "type"):
                row = ("source local " + label + " type=int = i32=1" if field == "name"
                       else "source local x type=" + label + " = i32=1")
                with self.subTest(label=label[:80], field=field), self.assertRaises(ValueError):
                    dap.parse_source_values("source-stop 41\n" + row + "\n", 41)

    def test_metadata_truncation_marker_requires_complete_input_limit(self):
        marker = "... (truncated)"
        for prefix in ("x" * 4096, r"\xe2" * 4096, r"\\" * 4096, r'\"' * 4096):
            label = prefix + marker
            with self.subTest(prefix=prefix[:20]):
                values = dap.parse_source_values("source-stop 41\nsource local " + label + " type=int = i32=1\n", 41)
                self.assertEqual(values[0]["name"], label)
        # A literal short suffix remains normal display bytes, not a marker
        # escape that excuses a false input-byte count or trailing payload.
        for label in ("x" * 4097 + marker, "x" * 4090 + marker,
                      r"\xe2" * 4097 + marker, "x" * 4096 + marker + "x",
                      "x" * 4096 + marker + marker):
            with self.subTest(label=label[:80]), self.assertRaises(ValueError):
                dap.parse_source_values("source-stop 41\nsource local " + label + " type=int = i32=1\n", 41)

    def test_stop_identity_grammar_and_pagination(self):
        status = self.broker.status()
        self.assertEqual(dap.parse_status(status)[2][0]["stop_id"], 41)
        for changed in (status.replace("stop-id 41", "stop-id 0"), status.replace("stop-id 41", "stop-id 18446744073709551616"),
                        status.replace("stop-id 41\n", "stop-id 41\nstop-id 41\n"), "running\nstop-id 41\n"):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                dap.parse_status(changed)
        for args in ({"start": True}, {"count": -1}, {"count": 1025}, {"start": "0"}, {"filter": "addresses"}):
            reference = self.source_reference()
            self.assertFalse(self.send("variables", variablesReference=reference, **args)["success"])
            self.assertFalse(self.broker.commands[-1].startswith("locals source "))

    def test_complete_bounded_truncation_prefix_is_honest(self):
        rows = "source local first type=int = i32=5\nsource local second type=int = i32=6\n"
        values = dap.parse_source_values("source-stop 41\n" + rows + "source-variables shown=2 total=1024 truncated=true\n", 41)
        self.assertEqual([value["name"] for value in values], ["first", "second", "<source locals truncated>"])
        self.assertIn("1022 additional variables", values[-1]["value"])
        self.assertEqual(values[-1]["variablesReference"], 0)
        for trailer in ("source-variables shown=1 total=3 truncated=true", "source-variables shown=2 total=2 truncated=true",
                        "source-variables shown=2 total=1025 truncated=true", "source-variables shown=2 total=3 truncated=false"):
            with self.assertRaises(ValueError):
                dap.parse_source_values("source-stop 41\n" + rows + trailer + "\n", 41)
        for nonrow in ("no active source variables", "source locals unavailable: stale"):
            with self.assertRaises(ValueError):
                dap.parse_source_values("source-stop 41\n" + nonrow + "\nsource-variables shown=1 total=2 truncated=true\n", 41)

    def test_escaped_metadata_width_and_nonascii_rejection(self):
        name = r"\xe2" * 4096
        values = dap.parse_source_values("source-stop 41\nsource local " + name + " type=int = i32=1\n", 41)
        self.assertEqual(values[0]["name"], name)
        for raw in ("name\x9b", "name\u202e", "name\u0085"):
            with self.assertRaises(ValueError):
                dap.parse_source_values("source-stop 41\nsource local " + raw + " type=int = i32=1\n", 41)


if __name__ == "__main__":
    unittest.main()
