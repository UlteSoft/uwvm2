#!/usr/bin/env python3
"""Finite typed Wasm DAP protocol tests, not runtime capture qualification.

Run with the Linux keeper in the original cgroup. This broker returns DATA
only; the native two-participant fixture separately qualifies actual opaque
capture owners, SAME current cohort, host closure, N GC leases and graph copy.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("typed_wasm_dap", Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)


def graph(stop=41, first=0):
    return (f"wasm-stop {stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=2\n"
            "global 0 (ref null type-index=3 module=0) = struct #1\n"
            "global 1 (ref null exn) = exception #2\n"
            "object #1 struct module=0 type=3 members=3\n"
            "  0 (ref null type-index=3 module=0) = struct #1\n"
            "  1 i32 = packed-u8=255\n"
            "  2 v128 = bytes=0102030405060708090a0b0c0d0e0f10\n"
            "object #2 exception tag-module=0 tag=1 members=3\n"
            "  0 f32 = bits=0x7fc12345\n"
            "  1 i64 = -1\n"
            "  2 (ref i31) = i31 signed=-1 unsigned=2147483647\n")


def member_graph(stop=41, root=0, first=0, count=64, path=()):
    """Detached formatting oracle only, never capture/GC authority."""
    total = 3
    if root == 0:
        root_value = "(ref null type-index=3 module=0) = struct #1"
        label = "struct module=0 type=3"
        fields = ["(ref null type-index=3 module=0) = struct #1 mutable", "i32 = packed-u8=255 immutable", "v128 = bytes=0102030405060708090a0b0c0d0e0f10 mutable"]
    elif root == 1 and not path:
        root_value = "(ref null exn) = exception #1"; label = "exception tag-module=0 tag=1"
        fields = ["f32 = bits=0x7fc12345 immutable", "i64 = -1 immutable", "(ref i31) = i31 signed=-1 unsigned=2147483647 immutable"]
    else:
        raise AssertionError((root, path))
    last = min(total, first + count)
    more = last != total
    header = (f"wasm-stop {stop}\nWasm state thread=1 module=0 epoch=7 first={root} total=2\n"
              f"Wasm members object=1 first={first} count={count} path={','.join(map(str,path)) if path else '-'}\n"
              f"global {root} {root_value}\nobject #1 {label} members={total} first={first} next={last} more={'yes' if more else 'no'}\n")
    rows = ''.join(f"  {index} {fields[index]}\n" for index in range(first, last))
    marker = "  member page; continue with original root/path and next\n" if more else "  final member page; earlier members are outside this window\n" if first else ""
    return header + rows + marker + ("Wasm state truncated: rows=1 objects=1\n" if first or more else "")


def array_page(stop=41, first=960, count=64, path=(4,)):
    """Selected object2 precedes metadata1; every index remains original."""
    last = min(1024, first + count)
    return (f"wasm-stop {stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=2\n"
            f"Wasm members object=2 first={first} count={count} path={','.join(map(str,path))}\n"
            "global 0 (ref null type-index=3 module=0) = struct #1\n"
            f"object #2 array module=0 type=7 members=1024 first={first} next={last} more={'yes' if last < 1024 else 'no'}\n" +
            ''.join(f"  {index} i32 = {index} mutable\n" for index in range(first, last)) +
            ("  member page; continue with original root/path and next\n" if last < 1024 else "  final member page; earlier members are outside this window\n") +
            "object #1 struct module=0 type=3 members=8 first=0 next=0 more=yes\n"
            "  metadata only; expand through the original root and member path\nWasm state truncated: rows=1 objects=2\n")


class Broker:
    def __init__(self):
        self.stop = 41
        self.native = False
        self.commands = []
        self.packet = None
        self.member_packet = None
        self.after_copy = None

    def status(self):
        label = "native instruction step" if self.native else "selected participant step"
        return (f"stopped: {label}\nstop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation=7\n" +
                ("  native-pc=0x1004\n" if self.native else ""))

    def request(self, command):
        self.commands.append(command)
        if command == "status": return self.status()
        if command == "bt 1": return self.status() + ("  backtrace unavailable for this stop\n" if self.native else "  #0 module=0 function=1 [demo] run\n")
        if command == f"frames 1 {self.stop}":
            return "error: source debug info unavailable\n"  # This formatter fixture has no DWARF.
        if command.startswith("frames wasm 1 "):
            words = command.split(); first = min(int(words[-2]), 1)
            count = 1 - first
            row = "  frame 0 kind=physical module=0 function=1 generation=1 runtime-epoch=7 scope-unit=0 scope-offset=0 variables=current name=Wasm activation\n" if count else ""
            return f"wasm-frames stop={self.stop} thread=1 selected=0 count={count} total=1 first={first} physical=0\n" + row + "wasm-frames end\n"
        if command.startswith("members globals 1 0 0 0 "):
            tokens = command.split(); root, first, count = map(int, tokens[6:9]); path = tuple(map(int, tokens[9:]))
            result = member_graph(self.stop, root, first, count, path) if self.member_packet is None else self.member_packet
        elif command.startswith("globals 1 0 "):
            result = graph(self.stop) if self.packet is None else self.packet
        elif command.startswith("locals wasm 1 0 "):
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\nlocal 0 (ref null type-index=3 module=0) = struct #1\nobject #1 struct module=0 type=3 members=0\n"
        elif command.startswith("controls 1 0 "):
            first, count = map(int, command.split()[-2:]); end = min(first+count, 2)
            rows = ["control 0 kind=function entry=0 end=50 height=0 saved-first=0 saved-count=0 params=0 results=1\n",
                    "control 1 kind=if-else entry=7 end=40 height=1 saved-first=0 saved-count=1 params=1 results=1\n"]
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first={first} total=2\n" + ''.join(rows[first:end])
            if end < 2: result += f"Wasm layout page next={end}\n"
        elif command.startswith("handlers 1 0 "):
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\nhandler 0 catch=catch-ref tag-index=8 target-control=0 target-offset=50 params=1\n"
        elif command == "control-params 1 0 1 128 2":
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=128 total=1024\ncontrol 1 parameter 128 (ref type-index=4294967295 module=7)\ncontrol 1 parameter 129 v128\nWasm layout page next=130\n"
        elif command.startswith("saved 1 0 "):
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\nsaved-parameter 0 (ref null type-index=3 module=0) = struct #1\nobject #1 struct module=0 type=3 members=0\n"
        elif command.startswith("operands 1 0 "):
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\nNote: Last Wasm safepoint snapshot; may differ from current native state.\noperand 0 i64 = 1234\n"
        elif command == "table 1 0 2 4294967301 64":
            result = f"wasm-stop {self.stop}\nWasm state thread=1 module=0 epoch=7 first=4294967301 total=4294967302\ntable 2 element 4294967301 (ref null func) = function module=0 index=9\n"
        elif command == "locals 1": return "local 0 i32=5\n"
        elif command == "continue": return "running\n"
        else: raise AssertionError(command)
        if self.after_copy is not None: self.after_copy()
        return result


def messages(data):
    stream, result = io.BytesIO(data), []
    while line := stream.readline():
        if not line.startswith(b"Content-Length: ") or stream.readline() != b"\r\n": raise AssertionError("invalid test DAP output")
        result.append(json.loads(stream.read(int(line[16:-2]))))
    return result


class StateProtocol(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output)
        self.adapter.broker = self.broker = Broker(); self.adapter.step_level = "wasm"
        self.adapter.observe(self.broker.status(), notify=False); self.sequence = 0

    def send(self, command, **args):
        self.output.seek(0); self.output.truncate(); self.sequence += 1
        self.adapter.handle({"type": "request", "seq": self.sequence, "command": command, "arguments": args})
        responses = [m for m in messages(self.output.getvalue()) if m["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0]

    def test_control_and_handler_metadata_queries_are_fresh_read_only(self):
        result = self.send("uwvm/wasmState", threadId=1, selection="controls")
        self.assertTrue(result["success"],result)
        self.assertEqual(result["body"]["variables"][1]["name"],"control 1")
        self.assertIn("saved-count=1",result["body"]["variables"][1]["value"])
        self.assertTrue(all(v["variablesReference"]==0 for v in result["body"]["variables"]))
        result = self.send("uwvm/wasmState", threadId=1, selection="handlers")
        self.assertTrue(result["success"],result)
        self.assertIn("catch=catch-ref",result["body"]["variables"][0]["value"])
        self.assertEqual(self.broker.commands,["status","controls 1 0 0 64","status","status","handlers 1 0 0 64","status"])

    def test_exact_large_declared_tuple_page_and_saved_values(self):
        result=self.send("uwvm/wasmState",threadId=1,selection="control-params",index=1,start=128,count=2)
        self.assertTrue(result["success"],result)
        self.assertEqual(result["body"]["total"],1024)
        self.assertTrue(result["body"]["truncated"])
        self.assertEqual(result["body"]["variables"][0]["type"],"(ref type-index=4294967295 module=7)")
        self.assertEqual(result["body"]["variables"][0]["name"],"control 1 parameter 128")
        result=self.send("uwvm/wasmState",threadId=1,selection="saved")
        self.assertTrue(result["success"],result)
        self.assertEqual(result["body"]["variables"][0]["name"],"saved-parameter 0")

    def test_lexical_metadata_missing_rows_invalid_width_and_host_address_refused(self):
        packet="wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=2\ncontrol 0 kind=function entry=0 end=50 height=0 saved-first=0 saved-count=0 params=0 results=1\ncontrol 1 kind=if-else entry=7 end=40 height=1 saved-first=0 saved-count=1 params=1 results=1\n"
        for bad in (packet.replace("control 1 kind", "control 0 kind"),packet.replace("end=40","end=6"),
                    packet.replace("height=1","height=18446744073709551616"),packet.replace("params=1","native-pc=0x42"),
                    packet[:packet.rfind("control 1")],packet.replace("saved-count=1","saved-count=2"),
                    packet.replace("saved-first=0 saved-count=1", "saved-first=18446744073709551615 saved-count=1")):
            with self.assertRaises(ValueError):dap.parse_wasm_layout(bad,"controls",1,0,0,41)
        typed="wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=128 total=1024\ncontrol 1 parameter 128 i32\ncontrol 1 parameter 129 v128\nWasm layout page next=130\n"
        for bad in (typed.replace("next=130","next=131"),typed.replace("parameter 128","parameter 0"),typed.replace("128 i32","128 unknown"),
                    typed.replace("128 i32","128 (ref type-index=4294967296 module=7)")):
            with self.assertRaises(ValueError):dap.parse_wasm_layout(bad,"control-params",1,0,128,41,index=1,count=2)

    def test_control_scope_retired_stop_and_native_trap_refuse_before_query(self):
        frame=self.send("stackTrace",threadId=1)["body"]["stackFrames"][0]
        scopes=self.send("scopes",frameId=frame["id"])["body"]["scopes"]
        control=next(s for s in scopes if s["name"]=="Wasm control stack")
        result=self.send("variables",variablesReference=control["variablesReference"])
        self.assertTrue(result["success"],result)
        self.broker.stop+=1
        self.assertFalse(self.send("variables",variablesReference=control["variablesReference"])["success"])
        self.adapter.observe(self.broker.status(),notify=False);self.broker.native=True
        self.assertFalse(self.send("uwvm/wasmState",threadId=1,selection="controls")["success"])

    def test_metadata_stop_change_during_copy_and_index_caps_refused(self):
        self.broker.after_copy=lambda:setattr(self.broker,"stop",self.broker.stop+1)
        self.assertFalse(self.send("uwvm/wasmState",threadId=1,selection="controls")["success"])
        self.broker.after_copy=None;self.adapter.observe(self.broker.status(),notify=False)
        before=len(self.broker.commands)
        for args in ({"index":1<<64},{"start":1<<64},{"count":65}):
            self.assertFalse(self.send("uwvm/wasmState",threadId=1,selection="control-params",**args)["success"])
        self.assertEqual(len(self.broker.commands),before)

    def test_pretrap_note_is_operand_data_and_preserves_regular_parsers(self):
        note = "Note: Pre-trap Wasm inputs; no instruction result.\n"
        header = "wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=1\n"
        row = "operand 0 i64 = 1234\n"
        packet = "wasm-stop 41\n" + note + header.split("\n", 1)[1] + row
        copied = dap.parse_wasm_state(packet, "operands", 1, 0, 0, 41)
        self.assertEqual(copied["snapshotNote"], note.rstrip("\n"))
        self.assertEqual(copied["rows"], dap.parse_wasm_state(header + row, "operands", 1, 0, 0, 41)["rows"])
        both = packet.replace(row, dap._WASM_OPERAND_SNAPSHOT_NOTE + "\n" + row)
        self.assertEqual(dap.parse_wasm_state(both, "operands", 1, 0, 0, 41)["snapshotNote"],
                         note + dap._WASM_OPERAND_SNAPSHOT_NOTE)
        for selection in ("locals", "globals", "saved", "table"):
            with self.subTest(selection=selection), self.assertRaises(ValueError):
                dap.parse_wasm_state(packet, selection, 1, 0, 0, 41)
        for bad in (packet.replace(note, note + note), packet.replace(note, note.replace("inputs", "values"))):
            with self.assertRaises(ValueError): dap.parse_wasm_state(bad, "operands", 1, 0, 0, 41)
        with self.assertRaises(ValueError): dap.parse_wasm_layout(packet, "controls", 1, 0, 0, 41)

    def test_true_reference_kinds_alias_cycles_and_payload_display(self):
        reply = self.send("uwvm/wasmState", threadId=1, selection="globals", moduleId=0)
        self.assertTrue(reply["success"], reply)
        rows = reply["body"]["variables"]
        self.assertEqual(rows[0]["value"], "struct #1")
        self.assertEqual(rows[1]["value"], "exception #2")
        self.assertEqual(self.broker.commands, ["status", "globals 1 0 0 64", "status"])
        members = self.send("variables", variablesReference=rows[0]["variablesReference"])
        self.assertTrue(members["success"], members)
        self.assertEqual([v["value"] for v in members["body"]["variables"]], ["struct #1", "packed-u8=255", "bytes=0102030405060708090a0b0c0d0e0f10"])
        exception = self.send("variables", variablesReference=rows[1]["variablesReference"])
        self.assertEqual([v["value"] for v in exception["body"]["variables"]], ["bits=0x7fc12345", "-1", "i31 signed=-1 unsigned=2147483647"])
        self.assertTrue(all("memoryReference" not in v and "evaluateName" not in v and v["presentationHint"]["attributes"] == ["readOnly"] for v in rows))

    def test_actual_expansion_command_retains_original_root_and_cycle_path(self):
        rows = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"]
        fields = self.send("variables", variablesReference=rows[0]["variablesReference"])["body"]["variables"]
        self.assertEqual(self.broker.commands[-4:], ["status", "status", "members globals 1 0 0 0 0 0 3", "status"])
        self.assertTrue(fields[0]["variablesReference"])
        self.assertTrue(self.send("variables", variablesReference=fields[0]["variablesReference"], start=1, count=1)["success"])
        self.assertEqual(self.broker.commands[-2], "members globals 1 0 0 0 0 1 1 0")

    def test_member_array_middle_tail_and_empty_pages_use_original_indices(self):
        for first, count in ((128, 64), (960, 64), (1024, 64)):
            with self.subTest(first=first):
                self.broker.member_packet = array_page(first=first, count=count)
                reply = self.send("uwvm/wasmMembers", threadId=1, selection="globals", moduleId=0, root=0, path=[4], start=first, count=count)
                self.assertTrue(reply["success"], reply)
                body = reply["body"]
                self.assertEqual((body["total"], body["first"], body["next"], body["more"]), (1024, first, min(first + count, 1024), first + count < 1024))
                self.assertEqual([v["name"] for v in body["variables"]], [f"[{i}]" for i in range(first, min(first + count, 1024))])
                self.assertEqual(self.broker.commands[-3:], ["status", f"members globals 1 0 0 0 0 {first} {count} 4", "status"])

    def test_member_parser_exact_window_path_and_dense_metadata(self):
        packet = array_page()
        copied = dap.parse_wasm_state(packet, "globals", 1, 0, 0, 41, member_first=960, member_count=64, path=[4])
        self.assertEqual(copied["selected_object"], 2)
        self.assertEqual([o["id"] for o in copied["objects"]], [1, 2])
        self.assertTrue(copied["objects"][1]["members"][0]["mutable"])
        for changed in (packet.replace("path=4", "path=5"), packet.replace("object=2", "object=1"),
                        packet.replace("next=1024", "next=1023"), packet.replace("more=no", "more=yes"),
                        packet.replace("  960 i32", "  959 i32"), packet.replace("object #1 struct", "object #3 struct"),
                        packet.replace(" mutable", " externally-mutable"), packet + "native owner=0xffff\n"):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                dap.parse_wasm_state(changed, "globals", 1, 0, 0, 41, member_first=960, member_count=64, path=[4])

    def test_member_request_bounds_refuse_before_query(self):
        for changes in ({"path": [0] * 4097}, {"path": [True]}, {"root": True}, {"start": -1}, {"count": 65},
                        {"count": 0}, {"frame": 1}, {"table": 1}, {"selection": "native"}):
            with self.subTest(changes=changes):
                before = len(self.broker.commands)
                args = {"threadId": 1, "selection": "globals", "root": 0}; args.update(changes)
                self.assertFalse(self.send("uwvm/wasmMembers", **args)["success"])
                self.assertEqual(self.broker.commands[before:], [])

    def test_mid_member_copy_stale_stop_retains_no_expansion(self):
        rows = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"]
        self.broker.after_copy = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
        self.assertFalse(self.send("variables", variablesReference=rows[0]["variablesReference"])["success"])
        self.assertEqual(self.adapter.wasm_object_views, {})

    def test_table64_original_index_is_not_narrowed(self):
        reply = self.send("uwvm/wasmState", threadId=1, selection="table", moduleId=0, table=2, start=4294967301)
        self.assertTrue(reply["success"], reply)
        self.assertEqual(reply["body"]["variables"][0]["name"], "table 2 element 4294967301")
        self.assertEqual(reply["body"]["variables"][0]["value"], "function module=0 index=9")

    def test_wasm_level_standard_scopes_use_typed_packets(self):
        frame = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]
        scopes = self.send("scopes", frameId=frame["id"])["body"]["scopes"]
        self.assertEqual([s["name"] for s in scopes], ["Wasm locals", "Typed Wasm locals", "Wasm operand stack (last safepoint; may differ from native state)", "Wasm saved if parameters", "Wasm control stack", "Wasm handler clauses", "Wasm globals", "Wasm table 0", "WASIp1 arguments", "WASIp1 environment", "WASIp1 descriptors", "WASIp1 preopens"])
        scope = next(s for s in scopes if s["name"] == "Wasm operand stack (last safepoint; may differ from native state)")
        reply = self.send("variables", variablesReference=scope["variablesReference"])
        self.assertTrue(reply["success"], reply)
        self.assertEqual(reply["body"]["variables"][0]["value"], "1234")
        typed = self.send("uwvm/wasmState", threadId=1, selection="operands")
        self.assertTrue(typed["success"], typed)
        self.assertEqual(typed["body"]["snapshotNote"], dap._WASM_OPERAND_SNAPSHOT_NOTE)

    def test_operand_notice_preserves_member_pages_and_strict_packet_validation(self):
        old = member_graph().replace("global 0 ", "operand 0 ")
        start = old.index("Wasm members object=")
        packet = old[:start] + dap._WASM_OPERAND_SNAPSHOT_NOTE + "\n" + old[start:]
        copied = dap.parse_wasm_state(packet, "operands", 1, 0, 0, 41, member_count=64)
        self.assertEqual(copied["snapshotNote"], dap._WASM_OPERAND_SNAPSHOT_NOTE)
        self.assertEqual(copied["selected_object"], 1)
        self.assertEqual(dap.parse_wasm_state(old, "operands", 1, 0, 0, 41, member_count=64)["total"], 2)
        for bad in (packet.replace("safepoint snapshot", "current native stack"),
                    packet.replace(dap._WASM_OPERAND_SNAPSHOT_NOTE, dap._WASM_OPERAND_SNAPSHOT_NOTE + "\n" + dap._WASM_OPERAND_SNAPSHOT_NOTE)):
            with self.assertRaises(ValueError):
                dap.parse_wasm_state(bad, "operands", 1, 0, 0, 41, member_count=64)
        with self.assertRaises(ValueError):
            dap.parse_wasm_state(packet.replace("operand 0 ", "global 0 "), "globals", 1, 0, 0, 41, member_count=64)

    def test_same_pc_new_stop_and_resume_retire_graph_references(self):
        rows = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"]
        reference = rows[0]["variablesReference"]
        self.broker.stop += 1
        self.assertFalse(self.send("variables", variablesReference=reference)["success"])
        self.assertEqual(self.adapter.wasm_object_views, {})
        self.adapter.observe(self.broker.status(), notify=False)
        rows = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"]
        self.assertNotEqual(reference, rows[0]["variablesReference"])
        self.send("continue", threadId=1)
        self.assertFalse(self.send("variables", variablesReference=rows[0]["variablesReference"])["success"])

    def test_mid_copy_stop_change_or_native_stop_refuses_values(self):
        self.broker.after_copy = lambda: setattr(self.broker, "stop", self.broker.stop + 1)
        self.assertFalse(self.send("uwvm/wasmState", threadId=1, selection="globals")["success"])
        self.assertEqual(self.adapter.wasm_object_views, {})
        self.broker.after_copy = None; self.broker.native = True
        self.adapter.observe(self.broker.status(), notify=False)
        before = len(self.broker.commands)
        self.assertFalse(self.send("uwvm/wasmState", threadId=1, selection="globals")["success"])
        self.assertEqual(self.broker.commands[before:], ["status"])

    def test_formatter_packet_strict_bounded_validation(self):
        dap.parse_wasm_state(graph(), "globals", 1, 0, 0, 41)
        for altered in [graph(42), graph().replace("global 1", "global 2"), graph().replace("struct #1", "struct #0"),
                        graph().replace("exception #2", "struct #2"), graph().replace("type=3", "type=4294967296"),
                        graph().replace("packed-u8=255", "packed-u8=256"), graph().replace("members=3", "members=2"),
                        graph().replace("unsigned=2147483647", "unsigned=0"), graph().replace("epoch=7", "epoch=0"),
                        graph().replace("bits=0x7fc12345", "bits=0x123456789"), graph().replace("i64 = -1", "i64 = 9223372036854775808"),
                        graph().replace("struct #1", "0x12345678"), graph() + "host memory 0xffff\n"]:
            with self.subTest(altered=altered), self.assertRaises(ValueError):
                dap.parse_wasm_state(altered, "globals", 1, 0, 0, 41)

    def test_wrapper_only_cycle_is_refused_without_rejecting_gc_cycles(self):
        packet = ("wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=1\n"
                  "global 0 (ref null extern) = extern-wrapper #1\n"
                  "object #1 extern-wrapper members=1\n  0 (ref null extern) = extern-wrapper #1\n")
        with self.assertRaises(ValueError):
            dap.parse_wasm_state(packet, "globals", 1, 0, 0, 41)
        dap.parse_wasm_state(graph(), "globals", 1, 0, 0, 41)

    def test_truncated_external_wrapper_keeps_explicit_unavailability(self):
        packet = ("wasm-stop 41\nWasm state thread=1 module=0 epoch=7 first=0 total=1\n"
                  "global 0 (ref null extern) = extern-wrapper #1\n"
                  "object #1 extern-wrapper members=1\n  members truncated\n")
        copied = dap.parse_wasm_state(packet, "globals", 1, 0, 0, 41)
        self.assertTrue(copied["objects"][0]["truncated"])
        self.assertEqual(copied["objects"][0]["members"], [])

    def test_indexed_copy_filter_has_no_native_authority(self):
        rows = self.send("uwvm/wasmState", threadId=1, selection="globals")["body"]["variables"]
        reference = rows[0]["variablesReference"]
        self.assertEqual(self.send("variables", variablesReference=reference, filter="named")["body"]["variables"], [])
        before = len(self.broker.commands)
        self.assertFalse(self.send("variables", variablesReference=reference, filter="arbitrary")["success"])
        self.assertEqual(self.broker.commands[before:], [])

    def test_unavailable_and_host_payload_remain_explicit(self):
        self.broker.packet = "wasm-stop 41\nWasm state unavailable: actual GC root and store borrowing is unavailable\n"
        reply = self.send("uwvm/wasmState", threadId=1, selection="globals")
        self.assertTrue(reply["success"])
        self.assertEqual(reply["body"]["variables"], [])
        self.assertEqual(reply["body"]["unavailable"], "actual GC root and store borrowing is unavailable")
        with self.assertRaises(ValueError):
            dap.parse_wasm_state(self.broker.packet.replace("unavailable\n", "host address=0xff\n"), "globals", 1, 0, 0, 41)

    def test_request_bounds_fail_before_query(self):
        for changes in [{"moduleId": True}, {"frame": -1}, {"count": 65}, {"count": True}, {"start": 1 << 64}, {"table": -1}]:
            with self.subTest(changes=changes):
                self.adapter.observe(self.broker.status(), notify=False)
                before = len(self.broker.commands)
                reply = self.send("uwvm/wasmState", threadId=1, selection="globals", **changes)
                self.assertFalse(reply["success"])
                self.assertEqual(self.broker.commands[before:], [])

if __name__ == "__main__": unittest.main()
