#!/usr/bin/env python3
"""Protocol/lifetime regression. Fake replies do not establish JIT authority."""
import importlib.util
import io
import json
from pathlib import Path
import unittest
p=Path(__file__).resolve().parents[2]/"tools/debug/dap_adapter.py"
s=importlib.util.spec_from_file_location("dap_regs",p); dap=importlib.util.module_from_spec(s); s.loader.exec_module(dap)
STATUS="stopped: native instruction step\nstop-id 17\nthread 3 module=0 function=2 byte-offset=4 generation=8\n  native-pc=0x0000000000001234\n"
NAMES=["rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp"]+[f"r{i}" for i in range(8,16)]+["rip","rflags"]+[f"xmm{i}" for i in range(16)]+[f"st{i}" for i in range(8)]+["fcw","fsw","ftw","fop","mxcsr","mxcsr_mask"]
REPLY="native-registers stop=17 thread=3 frame=0 module=0 function=2 generation=2 epoch=8 architecture=x86_64\n"+"".join(f"  {name}="+({"rax":"0x????????12345678","rip":"0x0000000000001234","xmm0":"0x????????????????????????3f800000"}.get(name,"unavailable"))+"\n" for name in NAMES)+"native-registers end\n"
class Broker:
    def __init__(self): self.commands=[]; self.status=STATUS; self.reply=REPLY; self.after=None; self.fail=False
    def request(self,text):
        self.commands.append(text)
        if text in ("status","bt 3"): return self.status
        if text == "info registers 3 17 all":
            if self.after: self.status=self.after
            return self.reply
        if text in ("ni","nexti","ni 3"):
            if self.fail: raise RuntimeError("lost step reply")
            self.status=STATUS.replace("17\n","18\n"); return self.status
        if text == "continue": self.status="running\n"; return self.status
        raise AssertionError(text)
class Tests(unittest.TestCase):
    def setUp(self): self.out=io.BytesIO(); self.a=dap.Adapter(self.out); self.b=Broker(); self.a.broker=self.b; self.seq=0
    def send(self,command,**args):
        self.seq+=1; self.out.seek(0); self.out.truncate(); self.a.handle({"seq":self.seq,"command":command,"arguments":args})
        stream=io.BytesIO(self.out.getvalue()); rows=[]
        while prefix:=stream.readline():
            size=int(prefix[16:-2]); stream.readline(); rows.append(json.loads(stream.read(size)))
        return next(row for row in rows if row["type"]=="response")
    def scope(self):
        frame=self.send("stackTrace",threadId=3)["body"]["stackFrames"][0]["id"]
        result=self.send("scopes",frameId=frame); self.assertTrue(result["success"],result)
        scopes=result["body"]["scopes"]; self.assertEqual([row["name"] for row in scopes],["Registers"])
        return frame,scopes[0]["variablesReference"]
    def test_positive_partial_values_paging_and_aliases(self):
        frame,ref=self.scope(); result=self.send("variables",variablesReference=ref,start=0,count=1)
        self.assertTrue(result["success"],result); value=result["body"]["variables"][0]
        self.assertEqual(value["value"],"0x????????12345678"); self.assertNotIn("memoryReference",value)
        for expression,expected in [("$rax","0x????????12345678"),("$pc","0x0000000000001234"),("sp","unavailable"),("st(0)","unavailable")]:
            result=self.send("evaluate",context="watch",frameId=frame,expression=expression)
            self.assertTrue(result["success"],result); self.assertEqual(result["body"]["result"],expected)
    def test_current_word_bits_header_and_aarch64_simd(self):
        self.b.reply=REPLY.replace("architecture=x86_64", "architecture=x86_64 word-bits=64")
        frame,ref=self.scope()
        self.assertTrue(self.send("variables",variablesReference=ref)["success"])
        gp=[f"x{i}" for i in range(29)]+["fp","lr","sp","pc","cpsr"]
        names=gp+[f"v{i}" for i in range(32)]
        self.b.reply="native-registers stop=17 thread=3 frame=0 module=0 function=2 generation=2 epoch=8 architecture=aarch64 word-bits=64\n"+"".join(
            f"  {name}="+{"pc":"0x0000000000001234","v0":"0x????????????????????????3f800000"}.get(name,"unavailable")+"\n" for name in names)+"native-registers end\n"
        result=self.send("evaluate",context="watch",frameId=frame,expression="$v0")
        self.assertTrue(result["success"],result)
        self.assertEqual(result["body"]["result"],"0x????????????????????????3f800000")
        self.assertNotIn("memoryReference",result["body"])
    def test_32_bit_pc_and_aliases(self):
        names=["eax","ebx","ecx","edx","esi","edi","ebp","esp","eip","eflags"]+[f"xmm{i}" for i in range(8)]
        self.b.reply="native-registers stop=17 thread=3 frame=0 module=0 function=2 generation=2 epoch=8 architecture=i686 word-bits=32\n"+"".join(
            f"  {name}="+{"eip":"0x00001234","eax":"0x12345678","xmm0":"0x????????????????????????3f800000"}.get(name,"unavailable")+"\n" for name in names)+"native-registers end\n"
        frame,_=self.scope()
        for expression,expected in (("$pc","0x00001234"),("$eax","0x12345678"),("sp","unavailable"),("fp","unavailable")):
            result=self.send("evaluate",context="watch",frameId=frame,expression=expression)
            self.assertTrue(result["success"],result); self.assertEqual(result["body"]["result"],expected)
        self.b.reply=self.b.reply.replace("eax=0x12345678","eax=0x0000000012345678")
        self.assertFalse(self.send("evaluate",context="watch",frameId=frame,expression="$eax")["success"])
    def test_architecture_control_registers_and_target_width_are_rejected(self):
        for architecture,forbidden in (("powerpc","r2"),("mips64","k0"),("riscv64","tp"),
                                       ("loongarch64","r21"),("sparc64","npc"),("s390x","r14"),("arm","lr")):
            gp,fp,_,pc,widths=dap.native_register_layout(architecture)
            width=widths[-1]; digits=width//4
            header=f"native-registers stop=17 thread=3 frame=0 module=0 function=2 generation=2 epoch=8 architecture={architecture} word-bits={width}\n"
            reply=header+"".join(f"  {name}="+(f"0x{0x1234:0{digits}x}" if name==pc else "unavailable")+"\n" for name in gp+fp)+"native-registers end\n"
            location={"stop_id":17,"id":3,"module":0,"function":2,"generation":8,"native_pc":"0x1234"}
            self.assertEqual(dap.parse_native_registers(reply,location)[0],architecture)
            with self.assertRaises(ValueError):
                dap.parse_native_registers(reply.replace(f"{forbidden}=unavailable",f"{forbidden}=0x{1:0{digits}x}"),location)
        for reply in (REPLY.replace("architecture=x86_64","architecture=x86_64 word-bits=32"),
                      REPLY.replace("architecture=x86_64","architecture=unknown word-bits=64")):
            self.setUp(); _,ref=self.scope(); self.b.reply=reply
            self.assertFalse(self.send("variables",variablesReference=ref)["success"])
    def test_same_pc_new_stop_retires_scope(self):
        _,ref=self.scope(); self.b.after=STATUS.replace("17\n","18\n")
        self.assertFalse(self.send("variables",variablesReference=ref)["success"])
        self.assertNotIn(ref,self.a.native_register_scopes)
    def test_ni_aliases_and_lost_reply_retire(self):
        for command in ("ni","nexti"):
            for fail in (False,True):
                self.setUp(); _,ref=self.scope(); self.b.fail=fail
                self.send("evaluate",expression=command,context="repl")
                self.assertFalse(self.send("variables",variablesReference=ref)["success"])
    def test_dap_native_next_retires_references_before_broker_and_on_lost_reply(self):
        for fail in (False, True):
            self.setUp(); self.a.step_level="native"; frame,ref=self.scope()
            request=self.b.request
            def next_request(text):
                if text=="ni 3":
                    self.assertNotIn(frame,self.a.frames)
                    self.assertNotIn(ref,self.a.native_register_scopes)
                    self.b.commands.append(text)
                    if fail: raise RuntimeError("lost native next reply")
                    # Cancel/no movement can return the same display stop;
                    # it must not resurrect old opaque inspection references.
                    return STATUS
                return request(text)
            self.b.request=next_request
            result=self.send("next",threadId=3)
            self.assertEqual(result["success"],not fail)
            self.assertEqual(self.b.commands[-1],"ni 3")
            self.assertFalse(self.send("variables",variablesReference=ref)["success"])
            self.assertFalse(self.send("evaluate",context="watch",frameId=frame,expression="$pc")["success"])

    def test_native_step_timeout_diagnostic_and_real_pause_retire_before_io(self):
        for command, wire in (("stepIn", "step asm 3"), ("next", "ni 3"), ("stepOut", "finish asm 3")):
            self.setUp(); self.a.step_level="native"; frame,ref=self.scope()
            original=self.b.request; events=[]; emit=self.a.event
            def event(name,body=None):
                events.append((name,body)); emit(name,body)
            self.a.event=event
            pause=STATUS.replace("stopped: native instruction step", "stopped: pause").replace("17\n","18\n")
            pause=pause.replace("  native-pc=0x0000000000001234\n","")
            diagnostic="pause/step timed out; execution state below is the current actual snapshot\n"
            def request(text):
                if text==wire:
                    self.assertNotIn(frame,self.a.frames)
                    self.assertNotIn(ref,self.a.native_register_scopes)
                    self.b.status=pause
                    return diagnostic+pause
                return original(text)
            self.b.request=request
            result=self.send(command,threadId=3,granularity="instruction")
            self.assertTrue(result["success"],result)
            self.assertIn(("output",{"category":"console","output":diagnostic}),events)
            self.assertIn(("stopped",{"reason":"pause","threadId":3,"allThreadsStopped":True}),events)
            self.assertEqual(self.a.state,"stopped")
            self.assertNotIn("native_pc",self.a.threads[0])
            self.assertEqual(self.a.threads[0]["stop_id"],18)
            self.assertFalse(self.send("variables",variablesReference=ref)["success"])
            self.assertFalse(self.send("evaluate",context="watch",frameId=frame,expression="$pc")["success"])

    def test_console_status_and_wait_observe_actual_reply_without_idle_poll(self):
        for command in ("status", " wait "):
            for state, reply in (("running", "running\n"), ("exited", "guest exited: 7\n"),
                                 ("stopped", STATUS.replace("17\n", "18\n"))):
                self.setUp(); frame,ref=self.scope()
                original=self.b.request; events=[]; emit=self.a.event
                def event(name,body=None):
                    events.append((name,body)); emit(name,body)
                self.a.event=event
                def request(text):
                    if text==command:
                        # Readonly inspection keeps the old handles until the
                        # actual reply establishes that their stop has retired.
                        self.assertIn(frame,self.a.frames)
                        self.assertIn(ref,self.a.native_register_scopes)
                        self.b.status=reply
                        return reply
                    return original(text)
                self.b.request=request
                result=self.send("evaluate",context="repl",expression=command)
                self.assertTrue(result["success"],result)
                self.assertEqual(self.a.state,state)
                if state=="exited":
                    self.assertEqual(events,[("exited",{"exitCode":7}),("terminated",None)])
                    self.assertTrue(self.a.finished)
                self.assertNotIn(frame,self.a.frames)
                self.assertNotIn(ref,self.a.native_register_scopes)
                self.assertFalse(self.send("variables",variablesReference=ref)["success"])

    def test_invalid_native_expressions_never_fall_back(self):
        for expression in ("*$rax","$rax+8","0x1234","$sp[0]","info registers","$rax;continue"):
            self.setUp(); frame,_=self.scope(); before=len(self.b.commands)
            self.assertFalse(self.send("evaluate",context="watch",frameId=frame,expression=expression)["success"])
            self.assertEqual(len(self.b.commands),before)
    def test_native_stack_and_memory_references_never_grant_access(self):
        frame,ref=self.scope()
        self.assertEqual(self.a.frames[frame][1],-1)
        pc=self.send("evaluate",context="watch",frameId=frame,expression="$pc")
        self.assertTrue(pc["success"],pc)
        self.assertNotIn("memoryReference",pc["body"])
        self.assertEqual(pc["body"]["variablesReference"],0)
        before=len(self.b.commands)
        for reference in (f"uwvm-native-stop:{frame}",f"uwvm-native-code:{frame}",f"native-stack:{frame}:0",
                          "0x1234","$rsp","$rbp","wasm-memory:0:0:-1",
                          "wasm-memory:0:0:18446744073709551615"):
            self.assertFalse(self.send("readMemory",memoryReference=reference,count=8)["success"])
            self.assertFalse(self.send("writeMemory",memoryReference=reference,data="AAAA")["success"])
        self.assertFalse(self.send("setVariable",variablesReference=ref,name="rsp",value="0x1234")["success"])
        self.assertEqual(len(self.b.commands),before)
        # Refused native-memory/edit requests also retire cached inspection
        # references; the old frame must not become a read capability later.
        self.assertFalse(self.send("evaluate",context="watch",frameId=frame,expression="$pc")["success"])
    def test_native_stack_cannot_reuse_cooperative_caller_data(self):
        original=self.b.request
        self.b.request=lambda text: STATUS+"  #0 module=0 function=2 [guest] stale-caller\n" if text=="bt 3" else original(text)
        result=self.send("stackTrace",threadId=3)
        self.assertFalse(result["success"])
        self.assertNotIn("stackFrames",result.get("body",{}))
    def test_malformed_or_host_payloads_rejected(self):
        for reply in (REPLY.replace("stop=17","stop=18"),REPLY.replace("rsp=unavailable","rsp=0x13579bdf2468ace0"),
                      REPLY.replace("st0=unavailable","st0=0x0000000000000000"),REPLY.replace("rip=0x0000000000001234","rip=0x0000000000005678"),
                      REPLY.replace("rax=0x????????12345678","rax=0x1234"),REPLY.replace("rbx=unavailable","rax=unavailable")):
            self.setUp(); _,ref=self.scope(); self.b.reply=reply
            self.assertFalse(self.send("variables",variablesReference=ref)["success"])
if __name__=="__main__": unittest.main()
