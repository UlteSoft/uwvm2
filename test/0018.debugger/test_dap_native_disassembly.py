#!/usr/bin/env python3
"""Bounded native-disassembly protocol only; fake text never mints runtime authority."""
import importlib.util
import io
import json
from pathlib import Path
import unittest

ADAPTER_PATH = Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py"
SPEC = importlib.util.spec_from_file_location("uwvm_dap_disassembly", ADAPTER_PATH)
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)
STATUS = ("stopped: native instruction step\nstop-id 17\n"
          "thread 3 module=0 function=2 byte-offset=4 generation=8\n  native-pc=0x0000000000001234\n")
HEADER = "native-disassembly stop=17 thread=3 module=0 function=2 function-generation=2 runtime-epoch=8\n"
ROWS = "  instruction 0 pc=0x0000000000001234 bytes=48 83 c0 01  addq $1, %rax\n"
END = "native-disassembly-end\n"

class Broker:
    def __init__(self):
        self.commands = []
        self.status = STATUS
        self.rows = ROWS
        self.header = HEADER
        self.after = None
    def request(self, text):
        self.commands.append(text)
        if text == "status" or text == "bt 3": return self.status
        if text.startswith("disassemble 3 17 "):
            result = self.header + self.rows + END
            if self.after is not None: self.status = self.after
            return result
        if text == "continue": self.status = "running\n"; return self.status
        raise AssertionError(text)

class Tests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO()
        self.adapter = dap.Adapter(self.output)
        self.broker = Broker()
        self.adapter.broker = self.broker
        self.sequence = 0
    def send(self, command, **arguments):
        self.sequence += 1; self.output.seek(0); self.output.truncate()
        self.adapter.handle({"seq": self.sequence, "command": command, "arguments": arguments})
        stream = io.BytesIO(self.output.getvalue()); result = []
        while prefix := stream.readline():
            self.assertTrue(prefix.startswith(b"Content-Length: "))
            count = int(prefix[16:-2]); self.assertEqual(stream.readline(), b"\r\n")
            result.append(json.loads(stream.read(count)))
        return next(row for row in result if row["type"] == "response")
    def frame(self):
        response = self.send("stackTrace", threadId=3); self.assertTrue(response["success"], response)
        return response["body"]["stackFrames"][0]
    def test_current_stop_owned_query_and_exact_bytes(self):
        frame = self.frame(); self.broker.commands.clear()
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)
        self.assertTrue(response["success"], response)
        self.assertEqual(self.broker.commands, ["status", "bt 3", "disassemble 3 17 1", "status"])
        self.assertEqual(response["body"]["instructions"], [{"address": "0x1234", "instructionBytes": "48 83 c0 01",
                         "instruction": "addq $1, %rax", "presentationHint": "normal"}])
    def test_exact_invalid_filler_at_actual_boundary(self):
        frame = self.frame()
        self.broker.rows = ROWS + "  instruction 1 pc=0x1238 unavailable\n  instruction 2 pc=0x1238 unavailable\n"
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=3)
        self.assertTrue(response["success"], response)
        rows = response["body"]["instructions"]
        self.assertEqual(len(rows), 3)
        self.assertEqual([row["presentationHint"] for row in rows], ["normal", "invalid", "invalid"])
        self.assertTrue(all(row["address"] == "-1" and "instructionBytes" not in row for row in rows[1:]))
        self.assertNotIn("instructionBytes", rows[1]); self.assertEqual(rows[1]["address"], "-1")
    def test_addresses_wasm_inline_offsets_counts_symbols_rejected_before_command(self):
        for args in [{"memoryReference": value, "instructionCount": 1} for value in
                     ("0x1234", "wasm:0:2:4", "uwvm-native-stop:0", "uwvm-native-stop:999999999999", "uwvm-native-stop:1;quit")]+[
                     {"memoryReference": "$CURRENT", "instructionCount": value} for value in (0, 513, True, "1")]+[
                     {"memoryReference": "$CURRENT", "instructionCount": 1, name: value}
                     for name, value in (("offset", 65537), ("offset", -65537), ("offset", True),
                                         ("instructionOffset", -8193), ("instructionOffset", "0"), ("resolveSymbols", "yes"))]:
            with self.subTest(args=args):
                # Recreate the real protocol display state after each fail-closed retirement.
                frame = self.frame(); args = dict(args)
                if args.get("memoryReference") == "$CURRENT":
                    args["memoryReference"] = frame["instructionPointerReference"]
                old = list(self.broker.commands)
                self.assertFalse(self.send("disassemble", **args)["success"])
                self.assertEqual(self.broker.commands, old)
    def test_native_display_without_stop_label_has_no_query(self):
        self.broker.status = STATUS.replace("stop-id 17\n", "")
        frame = self.frame(); old = list(self.broker.commands)
        self.assertFalse(self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)["success"])
        self.assertEqual(self.broker.commands, old)
    def test_invisible_resume_same_pc_new_stop_retires_reference(self):
        frame = self.frame(); self.broker.status = STATUS.replace("stop-id 17", "stop-id 18")
        self.assertFalse(self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)["success"])
        self.assertFalse(any(command.startswith("disassemble ") for command in self.broker.commands))
    def test_resume_after_copy_declines_old_output(self):
        frame = self.frame(); self.broker.after = STATUS.replace("stop-id 17", "stop-id 18")
        self.assertFalse(self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)["success"])
        self.assertFalse(self.adapter.native_frames)
    def test_wrong_stop_owner_epoch_and_malformed_rows_fail(self):
        headers = [HEADER.replace("stop=17", "stop=18"), HEADER.replace("thread=3", "thread=4"),
                   HEADER.replace("module=0", "module=1"), HEADER.replace("function=2", "function=3"),
                   HEADER.replace("runtime-epoch=8", "runtime-epoch=7"), HEADER.replace("function-generation=2", "function-generation=0")]
        rows = [ROWS.replace("0x0000000000001234", "0x1235"), ROWS.replace("instruction 0", "instruction 1"),
                ROWS.replace("48 83", "4 83"), ROWS.replace("  addq", "  \x1baddq"), ROWS + ROWS,
                ROWS.replace("\n", "\r\n"), ""]
        for header, row in [(header, ROWS) for header in headers] + [(HEADER, row) for row in rows]:
            with self.subTest(header=header,row=row):
                frame = self.frame(); self.broker.header, self.broker.rows = header, row
                self.assertFalse(self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)["success"])
                self.broker.header, self.broker.rows = HEADER, ROWS
    def test_native_read_memory_still_has_no_address_authority(self):
        frame = self.frame(); old = list(self.broker.commands)
        self.assertFalse(self.send("readMemory", memoryReference=frame["instructionPointerReference"], count=1)["success"])
        self.assertEqual(self.broker.commands, old)
    def test_filler_cannot_restart_decoding_or_move_to_fabricated_address(self):
        location = dap.parse_status(STATUS)[2][0]
        for rows in ("  instruction 0 pc=0x1234 unavailable\n  instruction 1 pc=0x1234 bytes=90  nop\n",
                     "  instruction 0 pc=0x1234 unavailable\n  instruction 1 pc=0x1235 unavailable\n"):
            with self.subTest(rows=rows):
                with self.assertRaises(ValueError): dap.parse_native_disassembly(HEADER+rows+END, location, 2)

if __name__ == "__main__": unittest.main()
