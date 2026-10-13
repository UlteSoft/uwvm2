#!/usr/bin/env python3
"""Detached WASIp1 scope protocol DATA; real environment authority tested separately."""
import io
import unittest
import test_dap_wasm_state as state

dap = state.dap
TITLES = {"args":"WASIp1 arguments", "env":"WASIp1 environment", "fds":"WASIp1 descriptors", "preopens":"WASIp1 preopens"}


class Broker(state.Broker):
    def __init__(self):
        super().__init__(); self.total = 160; self.pages = 0; self.hazard = None
        self.epoch = 7; self.after_page = None; self.long_text = False; self.unavailable = False
    def ids(self): return [17 + 3*i for i in range(self.total)]
    def request(self, command):
        if command.startswith("set wasip1 env "):
            self.commands.append(command)
            if self.after_page: self.after_page()
            return f"wasip1-stop {self.stop}\nwasip1 module=0 status=ok epoch=7 applied=1 shared-environment=1 total={self.total}\n"
        if not command.startswith("info wasip1 "): return super().request(command)
        self.commands.append(command); self.pages += 1
        _,_,selected,module,first,count = command.split(); module,first,count = map(int,(module,first,count))
        total,epoch,shared,status = self.total,self.epoch,1,"ok"
        if self.unavailable or self.hazard == "unavailable" and self.pages > 1: status,total = "WASIp1 environment unavailable",0
        if self.pages > 1:
            if self.hazard == "total": total += 1
            if self.hazard == "epoch": epoch += 1
            if self.hazard == "module": module += 1
            if self.hazard == "shared": shared = 0
        result = f"wasip1-stop {self.stop}\nwasip1 module={module} status={status} epoch={epoch} applied=0 shared-environment={shared} total={total}\n"
        if status == "ok":
            indices = list(range(first,min(self.total,first+count))) if selected in ("args","env") else [i for i,fd in enumerate(self.ids()) if fd >= first][:count]
            rows = []; cursor = first; used = 0
            for i in indices:
                text = ("x"*3000 if self.long_text else f"{'KEY=' if selected == 'env' else ''}value{i}")
                if used + len(text) > 4096: break
                if selected in ("args","env"):
                    rows.append(f'  [{i}] "{text}"\n'); cursor = i+1
                else:
                    fd = self.ids()[i]; pre = selected == "preopens"
                    rows.append(f'  fd={fd} storage-kind={"directory" if pre else "file observer"} rights-base=0x2 rights-inheriting=0x0 preopened={int(pre)}' + (f' guest-name="/guest{i}"' if pre else '') + '\n'); cursor = fd+1
                used += len(text)
            more = bool(indices) and indices[len(rows)-1]+1 < self.total if rows else bool(indices)
            if self.pages > 1:
                if self.hazard == "empty": rows=[]; cursor=first; more=True
                if self.hazard == "missing-more": more=False
                if self.hazard == "extra-more": more=True
            result += ''.join(rows) + (f"  more next={cursor}\n" if more else "")
            if self.hazard == "host" and self.pages > 1: result += "  native-handle=0xdeadbeef\n"
        if self.after_page: self.after_page()
        return result


class Wasip1Scopes(unittest.TestCase):
    send = state.StateProtocol.send
    def setUp(self):
        self.output=io.BytesIO(); self.adapter=dap.Adapter(self.output); self.adapter.step_level="wasm"
        self.broker=Broker(); self.adapter.broker=self.broker; self.sequence=0
    def scopes(self):
        frame=self.send("stackTrace",threadId=1)["body"]["stackFrames"][0]
        return self.send("scopes",frameId=frame["id"])["body"]["scopes"]
    def scope(self,selected): return next(s["variablesReference"] for s in self.scopes() if s["name"] == TITLES[selected])
    def page(self,selected,args,indices):
        ref=self.scope(selected); reply=self.send("variables",variablesReference=ref,**args)
        self.assertTrue(reply["success"],reply); values=reply["body"]["variables"]
        names=[f"[{i}]" if selected in ("args","env") else f"fd {self.broker.ids()[i]}" for i in indices]
        self.assertEqual([v["name"] for v in values],names)
        self.assertTrue(all(v["variablesReference"] == 0 and "memoryReference" not in v and "evaluateName" not in v and v["presentationHint"]["attributes"] == ["readOnly"] for v in values))
        self.assertTrue(all(int(c.split()[-1]) <= 64 for c in self.broker.commands if c.startswith("info wasip1 ")))
        return reply
    def test_four_scopes_on_source_and_wasm_physical_frames(self):
        for level in ("source","wasm"):
            self.setUp(); self.adapter.step_level=level
            rows=self.scopes(); wasi=[s for s in rows if s["name"] in TITLES.values()]
            self.assertEqual([s["name"] for s in wasi],list(TITLES.values())); self.assertTrue(all(s["expensive"] for s in wasi))
            self.assertEqual(self.broker.pages,0)
    def test_dense_fd_ordinals_cross_64_boundary_tail_and_empty(self):
        for selected in TITLES:
            for first,count in ((0,1),(63,80),(129,8),(159,64),(160,4),((1<<64)-1,1)):
                with self.subTest(selected=selected,first=first):
                    self.setUp(); self.page(selected,{"start":first,"count":count,"filter":"indexed"},range(first,min(first+count,160)))
    def test_count_zero_and_omitted_read_remaining(self):
        for selected in TITLES:
            for args in ({},{"start":60,"count":0}):
                self.setUp(); self.page(selected,args,range(args.get("start",0),160))
    def test_legitimate_text_budget_short_pages_continue(self):
        for selected in ("args","env"):
            self.setUp(); self.broker.long_text=True
            self.page(selected,{"start":63,"count":80},range(63,143)); self.assertEqual(self.broker.pages,81)
    def test_reply_byte_budget_rejects_entire_result(self):
        self.broker.total=400; self.broker.long_text=True; ref=self.scope("args")
        reply=self.send("variables",variablesReference=ref)
        self.assertFalse(reply["success"],reply); self.assertIn("byte budget",reply["message"]); self.assertNotIn("body",reply)
    def test_empty_and_unavailable_are_distinct(self):
        for selected in TITLES:
            self.setUp(); self.broker.total=0; self.page(selected,{},[])
            self.setUp(); ref=self.scope(selected); self.broker.unavailable=True
            reply=self.send("variables",variablesReference=ref); self.assertTrue(reply["success"],reply)
            self.assertEqual(reply["body"]["variables"][0]["type"],"unavailable")
    def test_named_filter_only_refreshes_stop(self):
        for selected in TITLES:
            self.setUp(); ref=self.scope(selected); before=len(self.broker.commands)
            reply=self.send("variables",variablesReference=ref,filter="named")
            self.assertTrue(reply["success"],reply); self.assertEqual(reply["body"]["variables"],[]); self.assertEqual(self.broker.commands[before:],["status"])
    def test_invalid_arguments_fail_before_broker_io(self):
        invalid=[{"start":v} for v in (-1,True,1.5,"1",1<<64)]+[{"count":v} for v in (-1,True,1.5,"1",1025)]+[{"filter":v} for v in (True,[],{},"all")]
        for selected in TITLES:
            for args in invalid:
                self.setUp(); ref=self.scope(selected); before=list(self.broker.commands)
                self.assertFalse(self.send("variables",variablesReference=ref,**args)["success"]); self.assertEqual(self.broker.commands,before)
    def test_changed_or_malformed_page_never_publishes_prefix(self):
        for selected in TITLES:
            for hazard in ("total","module","epoch","shared","empty","missing-more","extra-more","unavailable","host"):
                self.setUp(); ref=self.scope(selected); self.broker.hazard=hazard
                reply=self.send("variables",variablesReference=ref,start=159 if hazard == "extra-more" else 63,count=80)
                self.assertFalse(reply["success"],(selected,hazard,reply)); self.assertNotIn("body",reply); self.assertFalse(self.adapter.wasip1_scope_stops)
    def test_original_scope_never_rebinds_after_external_step_or_native_stop(self):
        for selected in TITLES:
            for native in (False,True):
                self.setUp(); ref=self.scope(selected); self.broker.native=native; self.broker.stop+=1
                reply=self.send("variables",variablesReference=ref); self.assertFalse(reply["success"],reply); self.assertEqual(self.broker.pages,0)
    def test_mid_copy_stop_change_rejects_whole_result(self):
        for selected in TITLES:
            self.setUp(); ref=self.scope(selected)
            def change():
                if self.broker.pages == 2: self.broker.stop+=1
            self.broker.after_page=change
            reply=self.send("variables",variablesReference=ref,start=63,count=80)
            self.assertFalse(reply["success"],reply); self.assertNotIn("body",reply)
    def test_probe_epoch_and_count_zero_budget(self):
        for selected in TITLES:
            self.setUp(); ref=self.scope(selected); self.broker.epoch=8
            self.assertFalse(self.send("variables",variablesReference=ref,start=160,count=1)["success"])
            self.setUp(); self.broker.total=2000; ref=self.scope(selected)
            self.assertFalse(self.send("variables",variablesReference=ref)["success"])
            self.setUp(); self.broker.total=2000; self.page(selected,{"start":1500,"count":80},range(1500,1580))
    def test_native_frames_never_offer_wasip1_scopes(self):
        self.broker.native=True
        self.assertFalse(any(s["name"] in TITLES.values() for s in self.scopes()))
    def test_scope_lifetime_retires_before_environment_mutation(self):
        ref=self.scope("env")
        self.broker.after_page=lambda:self.assertFalse(self.adapter.wasip1_scope_stops)
        reply=self.send("uwvm/wasip1Edit",operation="setEnvironment",name="KEY",value="new")
        self.assertTrue(reply["success"],reply); self.assertTrue(reply["body"]["applied"])
        self.assertFalse(self.send("variables",variablesReference=ref)["success"]); self.assertEqual(self.broker.pages,0)
    def test_expected_stop_guard_prevents_a_new_environment_query(self):
        ref=self.scope("args"); expected=self.adapter.stop_key; self.broker.stop+=1; before=len(self.broker.commands)
        with self.assertRaises(ValueError): self.adapter.query_wasip1_state("args",0,0,1,expected_stop=expected)
        self.assertEqual(self.broker.commands[before:],["status"])


if __name__ == "__main__": unittest.main()
