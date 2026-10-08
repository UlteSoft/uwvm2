#!/usr/bin/env python3
"""Only bounded console-routing/cache DATA; authentic mutation needs C++ runtime fixture."""
import importlib.util
import io
import json
from pathlib import Path
import unittest
SPEC=importlib.util.spec_from_file_location("mutation_dap",Path(__file__).resolve().parents[2]/"tools/debug/dap_adapter.py")
dap=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(dap)
class Broker:
    def __init__(self):self.commands=[]
    def request(self,text):
        self.commands.append(text)
        if text=="status":return "stopped: selected participant step\nstop-id 41\nthread 7 module=3 function=1 byte-offset=4 generation=8\n"
        if text.startswith("set wasm "):
            return "wasm-stop 42\nWasm mutation v=2 status=available applied=1 reason=none runtime=8 module=3 target=global index=4 element=0\n"
        raise AssertionError(text)
class Tests(unittest.TestCase):
    def test_repl_write_retires_all_detached_labels_before_command(self):
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker=Broker()
        adapter.observe(broker.request("status"),notify=False)
        adapter.wasm_object_views[1]=(adapter.stop_key,{},[])
        adapter.deep_wasm_paths[2]=("original-root",[0])
        adapter.wasm_path_indices=1
        adapter.handle({"type":"request","seq":1,"command":"evaluate","arguments":
            {"context":"repl","expression":"set wasm global 3 4 7 bits i32 ffffffff"}})
        self.assertEqual(adapter.wasm_object_views,{})
        self.assertEqual(adapter.deep_wasm_paths,{})
        self.assertEqual(adapter.wasm_path_indices,0)
        self.assertIsNone(adapter.stop_key)
        self.assertEqual(broker.commands,["status","set wasm global 3 4 7 bits i32 ffffffff"])
        data=io.BytesIO(output.getvalue());line=data.readline();self.assertEqual(data.readline(),b"\r\n")
        response=json.loads(data.read(int(line[16:-2])));self.assertTrue(response["success"])
        self.assertEqual(response["body"]["variablesReference"],0)
    def test_deep_handle_repl_routes_same_command_and_retires_views(self):
        # DATA transport only: no real path, stop, capture or GC authority.
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker=Broker()
        adapter.observe(broker.request("status"),notify=False)
        adapter.wasm_object_views[1]=(adapter.stop_key,{},[])
        adapter.deep_wasm_paths[2]=("original-root",[0]*4096)
        adapter.wasm_path_indices=4096
        expression="set wasm table 3 2 4294967301 7 from handle 101 9 path 0 1"
        adapter.handle({"type":"request","seq":1,"command":"evaluate","arguments":
            {"context":"repl","expression":expression}})
        self.assertEqual(broker.commands,["status",expression])
        self.assertEqual(adapter.wasm_object_views,{})
        self.assertEqual(adapter.deep_wasm_paths,{})
        self.assertEqual(adapter.wasm_path_indices,0)
        self.assertIsNone(adapter.stop_key)
        data=io.BytesIO(output.getvalue());line=data.readline();self.assertEqual(data.readline(),b"\r\n")
        response=json.loads(data.read(int(line[16:-2])));self.assertTrue(response["success"])
        self.assertEqual(response["body"]["variablesReference"],0)
    def test_member_repl_retains_exact_original_target_and_source_labels(self):
        # Detached DATA only: fake broker does not qualify native mutation.
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker=Broker()
        adapter.observe(broker.request("status"),notify=False)
        adapter.wasm_object_views[1]=(adapter.stop_key,{},[])
        adapter.deep_wasm_paths[2]=("original-root",[0]*4096)
        adapter.wasm_path_indices=4096
        expression="set wasm member 7 handle 101 9 path 0 1 at 3 from handle 101 10 path 15"
        adapter.handle({"type":"request","seq":1,"command":"evaluate","arguments":
            {"context":"repl","expression":expression}})
        self.assertEqual(broker.commands,["status",expression])
        self.assertEqual(adapter.wasm_object_views,{})
        self.assertEqual(adapter.deep_wasm_paths,{})
        self.assertEqual(adapter.wasm_path_indices,0)
        self.assertIsNone(adapter.stop_key)
        data=io.BytesIO(output.getvalue());line=data.readline();self.assertEqual(data.readline(),b"\r\n")
        response=json.loads(data.read(int(line[16:-2])));self.assertTrue(response["success"])
        self.assertEqual(response["body"]["variablesReference"],0)
    def test_hover_cannot_be_used_as_mutation_transport(self):
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker=Broker()
        adapter.handle({"type":"request","seq":1,"command":"evaluate","arguments":
            {"context":"hover","expression":"set wasm global 3 4 7 null"}})
        self.assertEqual(broker.commands,[])
        data=io.BytesIO(output.getvalue());line=data.readline();self.assertEqual(data.readline(),b"\r\n")
        response=json.loads(data.read(int(line[16:-2])));self.assertFalse(response["success"])
if __name__=="__main__":unittest.main()
