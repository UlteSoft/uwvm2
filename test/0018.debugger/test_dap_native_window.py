#!/usr/bin/env python3
"""Fake controller formatting only, NEVER runtime ownership/IDE qualification."""
import importlib.util
import io
import json
from pathlib import Path
import unittest

PATH = Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py"
SPEC = importlib.util.spec_from_file_location("uwvm_dap_native_window", PATH)
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)
COOP = "stopped: breakpoint\nstop-id 17\nthread 3 module=0 function=2 byte-offset=4 generation=8\n"
NATIVE = COOP.replace("breakpoint", "native instruction step") + "  native-pc=0x1100\n"

class Broker:
    def __init__(self, native=False):
        self.status = NATIVE if native else COOP
        self.commands = []
        self.generation = 2
        self.after_page = None
        self.fail_query = False
        self.pages = 0
        self.header_change = None
        self.hide_current = False
        self.hide_tail_in_first_page = False
    def request(self, command):
        self.commands.append(command)
        if command == "status": return self.status
        if command == "bt 3":
            return self.status + ("" if "native-pc" in self.status else "  #0 module=0 function=2\n")
        if command == "frames 3 17":
            return "error: source debug info unavailable\n"  # Guest safepoint without DWARF.
        if command.startswith("disassemble-range "):
            if self.fail_query: return "error: native disassembly unavailable\n"
            _, thread, stop, count, byte_offset, offset, symbols = command.split()
            count, byte_offset, offset, symbols = map(int, (count, byte_offset, offset, symbols))
            generation = self.generation + (self.header_change is not None and self.pages >= self.header_change)
            header = (f"native-disassembly-range stop={stop} thread={thread} module=0 function=2 function-generation={generation} runtime-epoch=8 "
                      f"reference-pc=0x1100 owner-begin=0x1000 owner-end=0x1600 byte-offset={byte_offset} instruction-offset={offset} resolve-symbols={symbols}\n")
            if symbols: header += "native-function-name demo\\x1Bname\n"
            rows = ""
            for index in range(count):
                pc = 0x1100 + byte_offset + offset + index
                if self.hide_current or (self.hide_tail_in_first_page and offset == 0 and index >= 1) or not 0x1000 <= 0x1100 + byte_offset < 0x1600 or not 0x1000 <= pc < 0x1600:
                    rows += f"  instruction {index} unavailable\n"
                else: rows += f"  instruction {index} pc=0x{pc:x} bytes=90  nop\n"
            self.pages += 1
            if self.after_page is not None and self.pages >= self.after_page:
                self.status = self.status.replace("stop-id 17", "stop-id 18")
            return header + rows + "native-disassembly-end\n"
        if command == "continue": self.status = "running\n"; return self.status
        raise AssertionError(command)

class Tests(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output)
        self.adapter.step_level = "native"; self.broker = Broker(); self.adapter.broker = self.broker
        self.seq = 0
    def send(self, command, **arguments):
        self.seq += 1; self.output.seek(0); self.output.truncate()
        self.adapter.handle({"seq": self.seq, "command": command, "arguments": arguments})
        stream = io.BytesIO(self.output.getvalue()); messages = []
        while line := stream.readline():
            count = int(line[16:-2]); self.assertEqual(stream.readline(), b"\r\n")
            messages.append(json.loads(stream.read(count)))
        return next(value for value in messages if value["type"] == "response")
    def frame(self):
        response = self.send("stackTrace", threadId=3); self.assertTrue(response["success"], response)
        return response["body"]["stackFrames"][0]
    def test_native_top_physical_ref_requires_actual_query_and_fresh_same_stop(self):
        frame = self.frame(); self.assertTrue(frame["instructionPointerReference"].startswith("uwvm-native-code:"))
        self.assertEqual(self.broker.commands, ["status", "bt 3", "frames 3 17", "status", "disassemble-range 3 17 1 0 0 0", "status"])
        self.assertEqual(len(self.adapter.native_code_refs), 1)
        old = list(self.broker.commands)
        self.assertFalse(self.send("readMemory", memoryReference=frame["instructionPointerReference"], count=1)["success"])
        self.assertEqual(self.broker.commands, old)
    def test_hidden_current_retains_bounded_display_reference(self):
        self.broker.hide_current = True
        frame = self.frame()
        self.assertTrue(frame["instructionPointerReference"].startswith("uwvm-native-code:"))
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=3)
        self.assertTrue(response["success"], response)
        self.assertTrue(all(row["presentationHint"] == "invalid" and "instructionBytes" not in row
                            for row in response["body"]["instructions"]))

    def test_newly_hidden_row_keeps_paged_reply_valid_and_hides_later_pages(self):
        frame = self.frame(); self.broker.commands.clear()
        self.broker.hide_tail_in_first_page = True
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=64)
        self.assertTrue(response["success"], response)
        rows = response["body"]["instructions"]
        self.assertEqual(len(rows), 64)
        self.assertEqual(rows[0]["presentationHint"], "normal")
        self.assertTrue(all(row["presentationHint"] == "invalid" and row["address"] == "-1" and
                            "instructionBytes" not in row for row in rows[1:]))
        self.assertEqual(len([value for value in self.broker.commands if value.startswith("disassemble-range ")]), 2)

    def test_actual_vscode_400_rows_minus200_symbols_true_in_13_small_pages(self):
        frame = self.frame(); self.broker.commands.clear(); self.broker.pages = 0
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=400,
                             instructionOffset=-200, resolveSymbols=True)
        self.assertTrue(response["success"], response); rows = response["body"]["instructions"]
        self.assertEqual(len(rows), 400); self.assertEqual(rows[0]["address"], "0x1038")
        self.assertEqual(rows[-1]["address"], "0x11c7")
        self.assertTrue(all(row["symbol"] == "demo\\x1Bname" for row in rows))
        queries = [value for value in self.broker.commands if value.startswith("disassemble-range ")]
        self.assertEqual(len(queries), 13)
        self.assertEqual([int(value.split()[3]) for value in queries], [32]*12+[16])
        self.assertEqual([int(value.split()[5]) for value in queries], list(range(-200,200,32)))
    def test_prefix_and_suffix_fillers_have_minus_one_no_invented_address(self):
        frame = self.frame()
        for offset in (-260, 1276):
            response = self.send("disassemble", memoryReference=frame["instructionPointerReference"],
                                 instructionCount=8, instructionOffset=offset, resolveSymbols=True)
            self.assertTrue(response["success"], response)
            rows = response["body"]["instructions"]; self.assertEqual(len(rows), 8)
            self.assertEqual(sum(row["presentationHint"]=="invalid" for row in rows), 4)
            for row in rows:
                if row["presentationHint"]=="invalid":
                    self.assertEqual(row["address"], "-1"); self.assertNotIn("instructionBytes", row)
    def test_native_stop_accepts_real_owned_offsets_and_symbols(self):
        self.broker = Broker(native=True); self.adapter.broker = self.broker
        frame = self.frame()
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"],
                             instructionCount=40, offset=-1, instructionOffset=-2, resolveSymbols=True)
        self.assertTrue(response["success"], response)
        self.assertEqual(response["body"]["instructions"][0]["address"], "0x10fd")
    def test_unqualified_top_never_mints_code_and_source_level_never_queries_native(self):
        self.broker.fail_query = True
        self.assertFalse(self.send("stackTrace", threadId=3)["success"]); self.assertFalse(self.adapter.native_code_refs)
        self.broker.fail_query = False; self.adapter.step_level = "source"; self.broker.commands.clear()
        frame = self.frame(); self.assertTrue(frame["instructionPointerReference"].startswith("wasm:"))
        self.assertFalse(any(value.startswith("disassemble-range ") for value in self.broker.commands))
    def test_external_same_site_repark_and_resume_retire_independent_code_ids(self):
        frame = self.frame(); reference = frame["instructionPointerReference"]
        self.broker.status = COOP.replace("stop-id 17", "stop-id 18")
        self.assertFalse(self.send("disassemble", memoryReference=reference, instructionCount=1)["success"])
        self.assertFalse(self.adapter.native_code_refs)
        self.broker.status = COOP; frame = self.frame()
        self.send("continue")
        self.assertFalse(self.send("disassemble", memoryReference=frame["instructionPointerReference"], instructionCount=1)["success"])
    def test_late_page_generation_change_rejects_the_whole_result(self):
        frame = self.frame(); self.broker.pages = 0; self.broker.header_change = 1
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"],
                             instructionCount=64, instructionOffset=-10, resolveSymbols=True)
        self.assertFalse(response["success"]); self.assertNotIn("body", response); self.assertFalse(self.adapter.native_code_refs)
    def test_invisible_resume_during_page_discards_all_copied_rows(self):
        frame = self.frame(); self.broker.pages = 0; self.broker.after_page = 2
        response = self.send("disassemble", memoryReference=frame["instructionPointerReference"],
                             instructionCount=64, instructionOffset=-10, resolveSymbols=True)
        self.assertFalse(response["success"]); self.assertNotIn("body", response)
    def test_user_budgets_types_raw_addresses_and_retired_ids_reject_before_query(self):
        for arguments in ({"instructionCount":513},{"instructionCount":True},{"offset":-65537},
                          {"offset":True},{"instructionOffset":8193},{"instructionOffset":-8193},
                          {"instructionOffset":"-1"},{"resolveSymbols":1}):
            frame = self.frame(); old = list(self.broker.commands)
            request = {"memoryReference":frame["instructionPointerReference"],"instructionCount":1}; request.update(arguments)
            self.assertFalse(self.send("disassemble", **request)["success"]); self.assertEqual(old,self.broker.commands)
        for reference in ("0x1000","wasm:0:2:4","uwvm-native-code:0","uwvm-native-code:999","uwvm-native-code:1;quit"):
            old = list(self.broker.commands)
            self.assertFalse(self.send("disassemble", memoryReference=reference, instructionCount=400, resolveSymbols=True)["success"])
            self.assertEqual(old,self.broker.commands)
    def test_display_name_controls_rows_and_owner_boundary_failclosed(self):
        location = dap.parse_status(NATIVE)[2][0]
        text = Broker(native=True).request("disassemble-range 3 17 1 0 0 1")
        for malformed in (text.replace("demo\\x1Bname","demo\x1Bname"),
                          text.replace("owner-end=0x1600","owner-end=0x1100"),
                          text.replace("bytes=90","bytes=90 90").replace("pc=0x1100","pc=0x15ff"),
                          text.replace("reference-pc=0x1100","reference-pc=0x1101"),
                          text.replace("byte-offset=0","byte-offset=1")):
            with self.subTest(text=malformed):
                with self.assertRaises(ValueError):
                    dap.parse_native_disassembly_range(malformed,location,1,0,0,True)
if __name__=="__main__": unittest.main()
