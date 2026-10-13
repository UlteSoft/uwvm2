#!/usr/bin/env python3
"""Bounded DAP transport/cache DATA; no broker authentication or runtime authority."""
import importlib.util
import io
import json
from pathlib import Path
import unittest
SPEC=importlib.util.spec_from_file_location('memory_mutation_dap',Path(__file__).resolve().parents[2]/'tools/debug/dap_adapter.py')
dap=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(dap)
class Transport:
    def __init__(self):self.sent=[]
    def send_raw(self,data):self.sent.append(data)
    def receive_raw(self):return b'bounded transport DATA\n'
class Broker:
    def __init__(self):self.commands=[]
    def request(self,text):
        self.commands.append(text)
        if text=='status':return 'stopped: selected participant step\nstop-id 41\nthread 7 module=3 function=1 byte-offset=4 generation=8\n'
        if text.startswith('set wasm memory '):return 'wasm-stop 42\nWasm mutation v=3 status=available applied=1 reason=none runtime=8 module=3 target=memory index=4 element=4294967304 bytes=256 address-bytes=8\n'
        raise AssertionError(text)
class Tests(unittest.TestCase):
    def test_both_transport_caps_preserve_maximum_owned_hex(self):
        text='set wasm memory 3 4 7 4294967304 bytes '+bytes(range(256)).hex()
        self.assertGreater(len(text.encode()),512)
        for cls in (dap.UnixBroker,dap.WindowsBroker):
            channel=Transport();self.assertEqual(cls.request(channel,text),'bounded transport DATA\n')
            self.assertEqual(channel.sent,[text.encode()])
            with self.assertRaises(ValueError):cls.request(channel,'x'*(dap.MAX_COMMAND+1))
            self.assertEqual(channel.sent,[text.encode()])
    def test_memory_repl_forwards_full_literal_and_discards_detached_labels(self):
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker=Broker()
        adapter.observe(broker.request('status'),notify=False)
        adapter.wasm_object_views[1]=(adapter.stop_key,{},[])
        adapter.deep_wasm_paths[2]=('original-root',[0]);adapter.wasm_path_indices=1
        text='set wasm memory 3 4 7 4294967304 bytes '+bytes(range(256)).hex()
        adapter.handle({'type':'request','seq':1,'command':'evaluate','arguments':{'context':'repl','expression':text}})
        self.assertEqual(broker.commands,['status',text]);self.assertIsNone(adapter.stop_key)
        self.assertEqual(adapter.wasm_object_views,{});self.assertEqual(adapter.deep_wasm_paths,{})
        data=io.BytesIO(output.getvalue());line=data.readline();self.assertEqual(data.readline(),b'\r\n')
        reply=json.loads(data.read(int(line[16:-2])));self.assertTrue(reply['success'])
        self.assertEqual(reply['body']['variablesReference'],0)
if __name__=='__main__':unittest.main()
