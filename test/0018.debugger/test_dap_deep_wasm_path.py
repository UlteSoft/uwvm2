#!/usr/bin/env python3
"""Finite deep GC DAP DATA tests, native owners tested by separate real fixture.

Run only with Linux keeper/cgroup. Fake broker supplies detached protocol DATA,
never capture authority, GC pointers or a VM continuation capability.
"""
import importlib.util
import io
from pathlib import Path
import unittest
SPEC=importlib.util.spec_from_file_location("deep_wasm_dap",Path(__file__).resolve().parents[2]/"tools/debug/dap_adapter.py")
dap=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(dap)


def packet(depth,session=5,handle=7,stop=41):
    return (f"wasm-stop {stop}\nWasm state thread=1 module=0 epoch=7 first=0 total=1\n"
        f"Wasm members object=2 first=1 count=1 path=@{session}:{handle} depth={depth}\n"
        "global 0 (ref null type-index=3 module=0) = struct #1\n"
        "object #2 struct module=0 type=3 members=2 first=1 next=2 more=no\n"
        "  1 i32 = 99 mutable\n"
        "  final member page; earlier members are outside this window\n"
        "object #1 struct module=0 type=3 members=2 first=0 next=0 more=yes\n"
        "  metadata only; expand through the original root and member path\n"
        "Wasm state truncated: rows=1 objects=2\n")


class Broker:
    def __init__(self):
        self.stop=41;self.session=5;self.counter=1;self.paths={};self.commands=[];self.retire_after=None
    def status(self):
        return f"stopped: selected participant step\nstop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation=7\n"
    def request(self,command):
        self.commands.append(command)
        if command=="status":return self.status()
        words=command.split();action=words[1]
        if action=="create":path=tuple(map(int,words[8:]))
        elif action=="extend":
            session,handle=map(int,words[2:4]);assert session==self.session
            path=(*self.paths.pop(handle),*map(int,words[4:]))
        elif action=="members":
            session,handle,first,count=map(int,words[2:]);assert session==self.session and first==count==1
            depth=len(self.paths[handle])
            return (f"wasm-stop {self.stop}\nWasm path session={session} handle={handle} depth={depth} view=3 protocol=1\n"+
                packet(depth,session,handle,self.stop).split("\n",1)[1])
        else:raise AssertionError(command)
        handle=self.counter;self.counter+=1;self.paths[handle]=path
        result=f"wasm-stop {self.stop}\nWasm path session={self.session} handle={handle} depth={len(path)} view=3 protocol=1\n"
        if self.retire_after==action:self.stop+=1
        return result


class DeepProtocol(unittest.TestCase):
    def setup_adapter(self):
        adapter=dap.Adapter(io.BytesIO());adapter.broker=broker=Broker();adapter.step_level="wasm"
        adapter.observe(broker.status(),notify=False);return adapter,broker
    def test_compressed_full_path_metadata_bounded_to_root_and_terminal(self):
        for depth in (17,64,1024,4096):
            with self.subTest(depth=depth):
                copied=dap.parse_wasm_state(packet(depth),"globals",1,0,0,41,
                    member_first=1,member_count=1,path=[0]*depth,path_token=(5,7,depth))
                self.assertEqual(copied["selected_object"],2);self.assertEqual(len(copied["objects"]),2)
                self.assertEqual(copied["objects"][1]["members"][0]["value"],"99")
    def test_compressed_labels_depth_stop_native_token_and_shape_refused(self):
        for changed in (packet(1024).replace("@5:7","@6:7"),packet(1024).replace("depth=1024","depth=1023"),
            packet(1024).replace("wasm-stop 41","wasm-stop 42"),packet(1024).replace("object=2","object=3"),
            packet(1024)+"native owner=0xffff\n"):
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                dap.parse_wasm_state(changed,"globals",1,0,0,41,member_first=1,member_count=1,path=[0]*1024,path_token=(5,7,1024))
        for token in ((0,7,17),(5,0,17),(5,7,16),(True,7,17)):
            with self.subTest(token=token),self.assertRaises(ValueError):
                dap.parse_wasm_state(packet(17),"globals",1,0,0,41,member_first=1,member_count=1,path=[0]*17,path_token=token)
    def test_path_header_protocol_and_view_exact(self):
        good="wasm-stop 41\nWasm path session=5 handle=7 depth=1024 view=3 protocol=1\n"
        data,child=dap.parse_wasm_path(good,41,1024);self.assertEqual(data,{"session":5,"handle":7,"depth":1024});self.assertIsNone(child)
        for changed in (good.replace("view=3","view=2"),good.replace("protocol=1","protocol=2"),
            good.replace("handle=7","handle=0"),good+"native pointer=0xffff\n"):
            with self.subTest(changed=changed),self.assertRaises(ValueError):dap.parse_wasm_path(changed,41,1024)
    def test_deep_query_creates_rewalks_extends_then_pages_and_reuses_data(self):
        adapter,broker=self.setup_adapter();key,copied=adapter.query_wasm_members(1,0,0,"globals",0,0,[0]*33,1,1)
        self.assertEqual(copied["selected_object"],2)
        operations=[command.split()[1] for command in broker.commands if command.startswith("path ")]
        self.assertEqual(operations,["create","extend","extend","members"])
        self.assertEqual(len(broker.paths),1);self.assertEqual(len(adapter.deep_wasm_paths),1)
        before=len(broker.commands);key2,copied2=adapter.query_wasm_members(1,0,0,"globals",0,0,[0]*33,1,1)
        self.assertEqual(key,key2);self.assertEqual(copied,copied2)
        self.assertEqual([c.split()[1] for c in broker.commands[before:] if c.startswith("path ")],["members"])
    def test_retired_stop_during_actual_backend_extension_rejected(self):
        adapter,broker=self.setup_adapter();broker.retire_after="extend"
        with self.assertRaises(ValueError):adapter.query_wasm_members(1,0,0,"globals",0,0,[0]*17,1,1)
        self.assertFalse(adapter.deep_wasm_paths)
    def test_path_4097_and_bool_refused_before_any_backend_command(self):
        for path in ([0]*4097,[True]):
            adapter,broker=self.setup_adapter();before=len(broker.commands)
            with self.assertRaises(ValueError):adapter.query_wasm_members(1,0,0,"globals",0,0,path,1,1)
            self.assertEqual(len(broker.commands),before)
    def test_complete_stop_retirement_clears_paths_and_path_budget(self):
        adapter,broker=self.setup_adapter();adapter.query_wasm_members(1,0,0,"globals",0,0,[0]*17,1,1)
        self.assertTrue(adapter.deep_wasm_paths);adapter.wasm_path_indices=32768
        broker.stop+=1;adapter.observe(broker.status(),notify=False)
        self.assertFalse(adapter.deep_wasm_paths);self.assertEqual(adapter.wasm_path_indices,0)
    def test_standard_variable_expansion_budget_is_bounded(self):
        adapter,_=self.setup_adapter();copied=dap.parse_wasm_state(packet(17),"globals",1,0,0,41,
            member_first=1,member_count=1,path=[0]*17,path_token=(5,7,17))
        value={"name":"child","index":0,"type":"(ref null type-index=3 module=0)","value":"struct #2","object":(2,"struct")}
        adapter.wasm_path_indices=32768
        with self.assertRaises(ValueError):adapter.wasm_variables([value],copied["objects"],adapter.stop_key,(1,0,0,"globals",0),(0,(0,)*17))


if __name__=="__main__":unittest.main()
