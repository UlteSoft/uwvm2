#!/usr/bin/env python3
"""Bounded memory protocol DATA; real memory64/multi-memory tested separately."""
import base64
import io
import unittest
import test_dap_wasm_state as state

dap=state.dap


class Broker(state.Broker):
    def __init__(self):
        super().__init__();self.pages=0;self.after_page=None;self.status_override=None
        self.fault=None;self.fail_page=2;self.extra_thread=False;self.epoch=7;self.limit=None
    def status(self):
        if self.status_override is not None:return self.status_override
        result=super().status().replace("generation=7",f"generation={self.epoch}")
        if self.extra_thread:result+="thread 2 module=0 function=2 byte-offset=8 generation=7\n"
        return result
    def request(self,command):
        if not command.startswith("memory "):return super().request(command)
        self.commands.append(command);self.pages+=1
        _,module,memory,address,count=command.split();module,memory,address,count=map(int,(module,memory,address,count))
        if self.limit is not None and address+count>self.limit:return "error: memory module, index or byte range unavailable\n"
        data=bytes((module*17+memory*31+address+i)&255 for i in range(count))
        result="memory:"+''.join(f" {b:02x}" for b in data)+"\n" if count else "memory: <empty>\n"
        if self.pages==self.fail_page and self.fault:
            if self.fault=="short":result=result[:-4]+"\n"
            elif self.fault=="extra":result+="host-memory: 0xffff\n"
            elif self.fault=="newline":result=result.replace(" ","\n",1)
            elif self.fault=="nonhex":result=result.replace("memory: ","memory: zz ",1)
            elif self.fault=="CR":result=result.replace("\n","\r\n")
            elif self.fault=="wide":result=result.replace("memory: ","memory: 100 ",1)
            elif self.fault=="unavailable":result="error: memory module, index or byte range unavailable\n"
            elif self.fault=="lost":raise ConnectionError("modeled transport loss")
            elif self.fault=="zero":result="memory: \n"
        if self.after_page:self.after_page()
        return result


class WasmMemoryPages(unittest.TestCase):
    send=state.StateProtocol.send
    def setUp(self):
        self.output=io.BytesIO();self.adapter=dap.Adapter(self.output);self.broker=Broker();self.adapter.broker=self.broker;self.sequence=0
    def read(self,module=0,memory=0,address=17,offset=0,count=4):
        return self.send("readMemory",memoryReference=f"wasm-memory:{module}:{memory}:{address}",offset=offset,count=count)
    def verify(self,args):
        reply=self.read(**args);self.assertTrue(reply["success"],reply);body=reply["body"]
        module,memory,address=args.get("module",0),args.get("memory",0),args.get("address",17)+args.get("offset",0)
        count=args.get("count",4)
        self.assertEqual(body,{"address":str(address),"data":base64.b64encode(bytes((module*17+memory*31+address+i)&255 for i in range(count))).decode(),"unreadableBytes":0})
        commands=[c for c in self.broker.commands if c.startswith("memory ")]
        self.assertEqual(len(commands),max(1,(count+255)//256));self.assertTrue(all(0<=int(c.split()[-1])<=256 for c in commands))
        for i,c in enumerate(commands):self.assertEqual(c,f"memory {module} {memory} {address+256*i} {min(256,count-256*i)}")
        return reply
    def test_page_boundaries_negative_offset_and_64k_response(self):
        for memory in (0,1):
            for count in (0,1,128,256,257,4096,65536):
                self.setUp();self.verify({"module":2,"memory":memory,"address":64,"offset":-47,"count":count})
    def test_fresh_identified_stop_surrounds_every_actual_copy(self):
        self.verify({"count":513});expected=["status"]
        for i in range(3):expected += ["status",f"memory 0 0 {17+256*i} {min(256,513-256*i)}","status"]
        self.assertEqual(self.broker.commands,expected)
    def test_invalid_inputs_never_reach_broker(self):
        invalid=[{"count":v} for v in (-1,True,1.5,"1",None,65537)]+[{"offset":v} for v in (True,1.5,"1",None)]
        invalid += [{"memoryReference":v} for v in ("0x1234","$rsp","uwvm-native-stop:1","wasm:0:1:2",None,{},"wasm-memory:0:0:-1","wasm-memory:18446744073709551616:0:0","wasm-memory:0:4294967296:0","wasm-memory:0:0:18446744073709551616")]
        invalid += [{"memoryReference":"wasm-memory:0:0:0","offset":-1},{"memoryReference":"wasm-memory:0:0:18446744073709551615"},{"memoryReference":"wasm-memory:0:0:18446744073709551615","offset":1,"count":0}]
        for args in invalid:
            self.setUp();request={"memoryReference":"wasm-memory:0:0:17","count":4};request.update(args)
            self.assertFalse(self.send("readMemory",**request)["success"]);self.assertEqual(self.broker.commands,[])
    def test_noncooperative_unidentified_and_empty_cohorts_refuse_before_copy(self):
        for raw in ("running\n","prepared; no Wasm instruction executed\n","guest exited: 0\n","stopped: pause\nstop-id 41\n","stopped: pause\nthread 1 module=0 function=1 byte-offset=4 generation=7\n"):
            self.setUp();self.broker.status_override=raw;reply=self.read()
            self.assertFalse(reply["success"],reply);self.assertEqual(self.broker.pages,0)
        self.setUp();self.broker.native=True;self.assertFalse(self.read()["success"]);self.assertEqual(self.broker.pages,0)
    def test_same_pc_new_stop_after_copy_never_publishes_old_bytes(self):
        self.broker.after_page=lambda:setattr(self.broker,"stop",self.broker.stop+1)
        reply=self.read();self.assertFalse(reply["success"],reply);self.assertNotIn("body",reply)
    def test_mid_page_and_final_page_stop_changes_refuse_whole_response(self):
        for at in (1,2,3):
            self.setUp()
            def change():
                if self.broker.pages==at:self.broker.stop+=1
            self.broker.after_page=change;reply=self.read(count=513)
            self.assertFalse(reply["success"],reply);self.assertNotIn("body",reply);self.assertEqual(self.broker.pages,at)
    def test_generation_and_participant_cohort_changes_are_not_same_stop(self):
        for field,value in (("epoch",8),("extra_thread",True)):
            self.setUp();self.broker.after_page=lambda:setattr(self.broker,field,value)
            reply=self.read(count=513);self.assertFalse(reply["success"],reply);self.assertNotIn("body",reply)
    def test_malformed_or_unavailable_later_page_never_publishes_prefix(self):
        for fault in ("short","extra","newline","nonhex","CR","wide","unavailable","lost"):
            self.setUp();self.broker.fault=fault;reply=self.read(count=513)
            self.assertFalse(reply["success"],(fault,reply));self.assertNotIn("body",reply);self.assertEqual(self.broker.pages,2)
    def test_failed_copy_retires_all_prior_frame_and_value_references(self):
        frame=self.send("stackTrace",threadId=1)["body"]["stackFrames"][0];self.send("scopes",frameId=frame["id"])
        self.broker.fault="short";reply=self.read(count=513)
        self.assertFalse(reply["success"],reply);self.assertIsNone(self.adapter.stop_key)
        for attr in ("frames","scopes","source_scope_stops","wasm_scope_stops","wasip1_scope_stops","wasm_object_views","native_code_refs"):
            self.assertFalse(getattr(self.adapter,attr),attr)
    def test_new_logical_selector_refreshes_after_external_stop(self):
        self.adapter.observe(self.broker.status(),notify=False);self.broker.stop+=1
        self.verify({});self.assertEqual(self.adapter.threads[0]["stop_id"],42)
    def test_zero_count_has_empty_data_and_real_runtime_bounds_check(self):
        self.broker.limit=17;self.verify({"count":0})
        self.setUp();self.broker.limit=16;reply=self.read(count=0)
        self.assertFalse(reply["success"],reply);self.assertEqual(self.broker.pages,1)
        self.setUp();self.broker.fault="zero";self.broker.fail_page=1
        self.assertFalse(self.read(count=0)["success"])
    def test_partial_guest_range_never_becomes_successful_prefix(self):
        self.broker.limit=300;reply=self.read(address=0,count=513)
        self.assertFalse(reply["success"],reply);self.assertNotIn("body",reply);self.assertEqual(self.broker.pages,2)
    def test_guest_u64_offset_is_not_narrowed_or_returned_as_host_address(self):
        self.verify({"module":(1<<64)-1,"memory":(1<<32)-1,"address":(1<<63)+17,"offset":-1,"count":257})
    def test_mixed_native_participant_refuses_cohort_without_memory_borrow(self):
        self.broker.status_override=self.broker.status()+"thread 2 module=0 function=2 byte-offset=8 generation=7\n  native-pc=0x1234\n"
        self.assertFalse(self.read()["success"]);self.assertEqual(self.broker.pages,0)


if __name__=="__main__":unittest.main()
