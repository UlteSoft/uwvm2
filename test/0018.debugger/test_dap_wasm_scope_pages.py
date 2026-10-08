#!/usr/bin/env python3
"""Standard typed scope pagination over bounded, detached protocol DATA."""
import io
import unittest
import test_dap_wasm_state as state

dap = state.dap
TITLES = {
    "locals": "Typed Wasm locals", "operands": "Wasm operand stack (last safepoint; may differ from native state)",
    "saved": "Wasm saved if parameters", "controls": "Wasm control stack", "handlers": "Wasm handler clauses",
    "globals": "Wasm globals", "table": "Wasm table 0",
}


def row(selected, i):
    if selected == "controls":
        return f"control {i} kind={'function' if i == 0 else 'block'} entry=0 end=500 height=0 saved-first=0 saved-count=0 params=0 results=0\n"
    if selected == "handlers":
        return f"handler {i} catch=catch-all tag-index=0 target-control=0 target-offset=500 params=0\n"
    if selected == "table": return f"table 0 element {i} (ref null func) = function module=0 index=9\n"
    prefix = {"locals": "local", "operands": "operand", "saved": "saved-parameter", "globals": "global"}[selected]
    return f"{prefix} {i} {'i64' if 1000+i >= 1<<31 else 'i32'} = {1000+i}\n"


class Broker(state.Broker):
    def __init__(self):
        super().__init__(); self.total = 160; self.pages = 0; self.hazard = None
        self.frame = 0; self.module = 0; self.epoch = 7; self.after_page = None; self.unavailable_probe = False
    def request(self, command):
        words = command.split()
        selected = "locals" if words[:2] == ["locals", "wasm"] else words[0]
        if selected not in TITLES: return super().request(command)
        self.commands.append(command); self.pages += 1
        if self.unavailable_probe:
            return f"wasm-stop {self.stop}\nWasm state unavailable: actual GC root and store borrowing is unavailable\n"
        first, count = map(int, words[-2:]); total = self.total
        module, epoch = self.module, self.epoch
        if self.pages > 1:
            if self.hazard == "total": total += 1
            elif self.hazard == "module": module += 1
            elif self.hazard == "epoch": epoch += 1
            elif self.hazard == "unavailable":
                return f"wasm-stop {self.stop}\nWasm state unavailable: actual GC root and store borrowing is unavailable\n"
        end = min(first+count, total)
        if self.hazard == "short" and self.pages > 1: end -= 1
        result = f"wasm-stop {self.stop}\nWasm state thread=1 module={module} epoch={epoch} first={first} total={total}\n"
        if selected == "operands" and not (self.hazard == "note" and self.pages > 1): result += dap._WASM_OPERAND_SNAPSHOT_NOTE + "\n"
        result += ''.join(row(selected, i) for i in range(first, end))
        if end < total:
            result += f"Wasm layout page next={end}\n" if selected in ("controls", "handlers") else f"Wasm state truncated: rows={end-first} objects=0\n"
        if self.after_page is not None: self.after_page()
        return result


class ScopePages(unittest.TestCase):
    send = state.StateProtocol.send
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output); self.adapter.step_level = "wasm"
        self.broker = Broker(); self.adapter.broker = self.broker; self.sequence = 0
    def scope(self, selected):
        frame = self.send("stackTrace", threadId=1)["body"]["stackFrames"][0]
        scopes = self.send("scopes", frameId=frame["id"])["body"]["scopes"]
        return next(s["variablesReference"] for s in scopes if s["name"] == TITLES[selected])
    def page(self, selected, arguments, indices):
        ref = self.scope(selected); reply = self.send("variables", variablesReference=ref, **arguments)
        self.assertTrue(reply["success"], reply)
        expected = [row(selected, i).split(" ", 2) for i in indices]
        rows = reply["body"]["variables"]
        if selected == "table": names = [f"table 0 element {i}" for i in indices]
        else: names = [' '.join(r[:2]) for r in expected]
        self.assertEqual([r["name"] for r in rows], names)
        if selected in ("locals", "operands", "saved", "globals"):
            self.assertEqual([r["value"] for r in rows], [str(1000+i) for i in indices])
        self.assertTrue(all(r["variablesReference"] == 0 and "memoryReference" not in r and
                            r["presentationHint"]["attributes"] == ["readOnly"] for r in rows))
        self.assertTrue(all(int(c.split()[-1]) <= 64 for c in self.broker.commands if c != "status" and c.split()[0] in TITLES))
        return reply
    def test_all_seven_scopes_cross_boundary_tail_and_empty(self):
        for selected in TITLES:
            for first, count in ((0,1), (63,80), (129,8), (159,64), (160,4), ((1<<64)-1,1)):
                with self.subTest(selected=selected, first=first):
                    self.setUp(); self.page(selected, {"start":first,"count":count,"filter":"indexed"}, range(first,min(first+count,160)))
    def test_count_zero_and_omitted_cover_all_remaining(self):
        for selected in TITLES:
            for args in ({}, {"start":60,"count":0}):
                with self.subTest(selected=selected, args=args):
                    self.setUp(); self.page(selected, args, range(args.get("start",0),160))
    def test_empty_and_unavailable_scope_remain_distinct_read_only_results(self):
        for selected in TITLES:
            self.setUp(); self.broker.total = 0; self.page(selected, {}, [])
            self.setUp(); ref = self.scope(selected); self.broker.unavailable_probe = True
            reply = self.send("variables",variablesReference=ref)
            self.assertTrue(reply["success"],reply); rows = reply["body"]["variables"]
            self.assertEqual(len(rows),1); self.assertEqual(rows[0]["type"],"unavailable")
            self.assertEqual(rows[0]["variablesReference"],0); self.assertNotIn("memoryReference",rows[0])
            self.assertEqual(rows[0].get("presentationHint",{}).get("attributes"),["readOnly"])
    def test_named_filter_refreshes_stop_without_a_state_borrow(self):
        for selected in TITLES:
            self.setUp(); ref = self.scope(selected); begin = len(self.broker.commands)
            reply = self.send("variables",variablesReference=ref,filter="named")
            self.assertTrue(reply["success"],reply); self.assertEqual(reply["body"]["variables"],[])
            self.assertEqual(self.broker.commands[begin:],["status"])
    def test_invalid_arguments_rejected_before_broker_io(self):
        invalid = [{"start":x} for x in (-1, True, 1.5, "1", 1<<64)] + [{"count":x} for x in (-1, True, 1.5, "1", 1025)]
        invalid += [{"filter":x} for x in ("all", True, [], {})]
        for selected in TITLES:
            for args in invalid:
                with self.subTest(selected=selected,args=args):
                    self.setUp(); ref = self.scope(selected); before = list(self.broker.commands)
                    self.assertFalse(self.send("variables",variablesReference=ref,**args)["success"])
                    self.assertEqual(self.broker.commands,before)
    def test_inconsistent_or_short_later_page_never_publishes_prefix(self):
        for selected in TITLES:
            for hazard in ("total", "module", "epoch", "short", "unavailable", "note"):
                if hazard == "note" and selected != "operands": continue
                with self.subTest(selected=selected,hazard=hazard):
                    self.setUp(); ref = self.scope(selected); self.broker.hazard = hazard
                    reply = self.send("variables",variablesReference=ref,start=63,count=80)
                    self.assertFalse(reply["success"],reply); self.assertNotIn("body",reply)
                    self.assertFalse(self.adapter.wasm_scope_stops)
    def test_real_stop_key_change_after_first_copy_refuses_whole_response(self):
        for selected in TITLES:
            self.setUp(); ref = self.scope(selected)
            def advance():
                if self.broker.pages == 2: self.broker.stop += 1
            self.broker.after_page = advance
            reply = self.send("variables",variablesReference=ref,start=63,count=80)
            self.assertFalse(reply["success"],reply); self.assertNotIn("body",reply)
    def test_old_scope_never_rebinds_to_a_new_or_native_stop(self):
        for selected in TITLES:
            for native in (False,True):
                self.setUp(); ref = self.scope(selected)
                self.broker.native = native; self.broker.stop += int(not native)
                reply = self.send("variables",variablesReference=ref)
                self.assertFalse(reply["success"],reply); self.assertEqual(self.broker.pages,0)
    def test_probe_module_epoch_and_count_are_checked_even_for_empty_window(self):
        for selected in TITLES:
            for field in ("module", "epoch"):
                self.setUp(); ref = self.scope(selected); setattr(self.broker,field,getattr(self.broker,field)+1)
                reply = self.send("variables",variablesReference=ref,start=160,count=1)
                self.assertFalse(reply["success"],reply); self.assertNotIn("body",reply)
    def test_large_cardinality_needs_explicit_page_without_narrowing_indices(self):
        for selected in TITLES:
            self.setUp(); self.broker.total = 2000; ref = self.scope(selected)
            reply = self.send("variables",variablesReference=ref)
            self.assertFalse(reply["success"],reply); self.assertNotIn("body",reply)
            self.setUp(); self.broker.total = (1<<32)+160
            self.page(selected,{"start":(1<<32)+63,"count":80},range((1<<32)+63,(1<<32)+143))


if __name__ == "__main__": unittest.main()
