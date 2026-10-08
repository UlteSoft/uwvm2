#!/usr/bin/env python3
"""Read-only WASIp1 DAP DATA tests; actual environment admission has its own C++ fixture."""
import importlib.util
import io
import json
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("wasi_state_dap", Path(__file__).resolve().parents[2] / "tools/debug/dap_adapter.py")
dap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dap)

class Broker:
    def __init__(self): self.stop = 41; self.native = False; self.commands = []; self.after = None; self.packet = None
    def status(self):
        return (("stopped: native instruction step\n" if self.native else "stopped: selected participant step\n") +
            f"stop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation=7\n" +
            ("  native-pc=0x1004\n" if self.native else ""))
    def request(self, command):
        self.commands.append(command)
        if command == "status": return self.status()
        if command.startswith("info wasip1 "):
            packet = self.packet or (f"wasip1-stop {self.stop}\nwasip1 module=0 status=ok epoch=7 applied=0 shared-environment=1 total=2\n"
                '  [0] "KEY=value"\n  [1] "EMPTY="\n')
            if self.after: self.after()
            return packet
        if command.startswith(("set wasip1 ", "unset wasip1 ", "wasm-script ")):
            if self.after: self.after()
            return "wasip1-stop 41\nwasip1 module=0 status=ok epoch=7 applied=1 shared-environment=1 total=2\n"
        raise AssertionError(command)

class Wasip1Protocol(unittest.TestCase):
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output); self.adapter.broker = self.broker = Broker()
        self.adapter.observe(self.broker.status(), notify=False)
    def send(self, command_name, **args):
        self.output.seek(0); self.output.truncate()
        self.adapter.handle({"type":"request", "seq":1, "command":command_name, "arguments":args})
        stream=io.BytesIO(self.output.getvalue()); responses=[]
        while line:=stream.readline():
            if not line.startswith(b"Content-Length: ") or stream.readline()!=b"\r\n": raise AssertionError("DAP frame")
            payload=json.loads(stream.read(int(line[16:-2])))
            if payload["type"]=="response": responses.append(payload)
        self.assertEqual(len(responses),1); return responses[0]
    def test_independent_environment_query_is_detached_read_only(self):
        reply=self.send("uwvm/wasip1State",selection="env",moduleId=0)
        self.assertTrue(reply["success"],reply); body=reply["body"]
        self.assertTrue(body["sharedEnvironment"]); self.assertEqual(body["total"],2)
        self.assertEqual(self.broker.commands,["status","info wasip1 env 0 0 64","status"])
        self.assertEqual([v["value"] for v in body["variables"]],['"KEY=value"','"EMPTY="'])
        self.assertTrue(all(v["variablesReference"]==0 and "memoryReference" not in v for v in body["variables"]))
    def test_sorted_guest_fd_metadata_not_native_handle(self):
        packet=('wasip1-stop 41\nwasip1 module=0 status=ok epoch=7 applied=0 shared-environment=0 total=3\n'
            '  fd=17 storage-kind=file observer rights-base=0x2 rights-inheriting=0x0 preopened=0\n  more next=18\n')
        copied=dap.parse_wasip1_state(packet,"fds",0,0,1,41)
        self.assertTrue(copied["more"]); self.assertEqual(copied["next"],18); self.assertEqual(copied["variables"][0]["descriptor"],17)
        for altered in (packet.replace("next=18","next=58"),packet.replace("fd=17","fd=2147483648"),packet.replace("rights-base=0x2","rights-base=0x10000000000000000")):
            with self.assertRaises(ValueError): dap.parse_wasip1_state(altered,"fds",0,0,1,41)
    def test_stale_native_and_mid_copy_refuse(self):
        self.broker.after=lambda:setattr(self.broker,"stop",self.broker.stop+1)
        self.assertFalse(self.send("uwvm/wasip1State",selection="env")["success"])
        self.broker.after=None; self.broker.native=True; self.adapter.observe(self.broker.status(),notify=False)
        before=len(self.broker.commands); self.assertFalse(self.send("uwvm/wasip1State",selection="env")["success"])
        self.assertEqual(self.broker.commands[before:],["status"])
    def test_no_mutation_or_unbounded_page_from_read_request(self):
        for args in ({"selection":"set env"},{"selection":"env","moduleId":True},{"selection":"env","count":65},{"selection":"env","start":-1}):
            before=len(self.broker.commands);self.assertFalse(self.send("uwvm/wasip1State",**args)["success"])
            self.assertEqual(self.broker.commands[before:],[])
    def test_canonical_escaping_and_row_bounds(self):
        packet='wasip1-stop 41\nwasip1 module=0 status=ok epoch=7 applied=0 shared-environment=0 total=1\n  [0] "A=\\x20"\n'
        self.assertEqual(dap.parse_wasip1_state(packet,"env",0,0,64,41)["variables"][0]["value"],'"A=\\x20"')
        for altered in (packet.replace("[0]","[1]"),packet.replace("applied=0","applied=1"),packet.replace("A=\\x20","A= "),packet.replace("wasip1-stop 41","wasip1-stop 42"),packet+'host handle=0xffff\n'):
            with self.assertRaises(ValueError):dap.parse_wasip1_state(altered,"env",0,0,64,41)
    def test_environment_mutation_retires_all_cached_labels_before_send(self):
        self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        reply=self.send("evaluate",context="repl",expression="set wasip1 env 0 4b4559 7632")
        self.assertTrue(reply["success"],reply);self.assertEqual(self.adapter.wasm_object_views,{})
        self.assertIsNone(self.adapter.stop_key)

    def test_script_mutation_retires_labels_before_broker_write(self):
        self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        self.broker.after=lambda:self.assertEqual(self.adapter.wasm_object_views,{})
        reply=self.send("evaluate",context="repl",expression="wasm-script set wasip1 env 0 4b4559 7632")
        self.assertTrue(reply["success"],reply)
    def test_edit_request_encodes_text_and_preserves_commit_after_resume(self):
        self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        def after():
            self.assertEqual(self.adapter.wasm_object_views,{})
            self.broker.stop=42
        self.broker.after=after
        reply=self.send("uwvm/wasip1Edit",operation="setEnvironment",name="KEY",value="v2",stopId=41)
        self.assertTrue(reply["success"],reply)
        self.assertTrue(reply["body"]["applied"])
        self.assertFalse(reply["body"]["stopCurrent"])
        self.assertEqual(self.broker.commands,["status","set wasip1 env 0 4b4559 7632 if-stop 41","status"])
    def test_edit_bounds_rejected_before_sending(self):
        for args in ({"operation":"replaceArgument","index":True,"value":"x"},
                     {"operation":"setEnvironment","name":"A=","value":"x"},
                     {"operation":"replaceArgument","index":0,"valueHex":"00"},
                     {"operation":"replaceArgument","index":0,"value":"a","valueHex":"61"},
                     {"operation":"replaceArgument","index":0,"value":"a"*4097}):
            before=len(self.broker.commands); self.assertFalse(self.send("uwvm/wasip1Edit",**args)["success"])
            self.assertEqual(self.broker.commands[before:],[])
    def test_long_edit_and_explicit_unavailable_status(self):
        reply=self.send("uwvm/wasip1Edit",operation="replaceArgument",index=1,value="z"*4096)
        self.assertTrue(reply["success"],reply)
        self.assertEqual(self.broker.commands[1],"set wasip1 arg 0 1 "+"7a"*4096+" if-stop 41")
        self.broker.packet="wasip1-stop 41\nwasip1 module=0 status=host activity is busy or untracked epoch=7 applied=0 shared-environment=0 total=0\n"
        reply=self.send("uwvm/wasip1State",selection="env")
        self.assertTrue(reply["success"],reply);self.assertFalse(reply["body"]["available"])

class Wasip1CheckpointProtocol(unittest.TestCase):
    setUp = Wasip1Protocol.setUp
    send = Wasip1Protocol.send
    def test_binary_managed_factory(self):
        original = self.broker.request
        def request(command):
            text = original(command)
            return text + ("wasip1-fd affected=4\n" if command.startswith("set wasip1 file") else "")
        self.broker.request = request
        reply = self.send("uwvm/wasip1Edit", operation="createFile", valueHex="410042")
        self.assertTrue(reply["success"], reply)
        self.assertEqual(reply["body"]["descriptor"], 4)
        self.assertEqual(self.broker.commands[1], "set wasip1 file 0 410042 if-stop 41")
    def test_checkpoint_reminder_and_explicit_resource_mode(self):
        original = self.broker.request
        def request(command):
            text = original(command)
            if command.startswith("set wasip1 checkpoint"):
                text += "wasip1-checkpoint slot=1 managed=1 retained-external=3 external-io-rollback=false\n" + \
                    "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n"
            return text
        self.broker.request = request
        reply = self.send("uwvm/wasip1Edit", operation="saveCheckpoint", slot=1)
        self.assertTrue(reply["success"], reply)
        self.assertTrue(reply["body"]["wasmCheckpointRequired"])
        self.assertEqual(reply["body"]["managedResources"], 1)
        self.assertFalse(reply["body"]["externalIORollback"])
        before = len(self.broker.commands)
        reply = self.send("uwvm/wasip1Edit", operation="restoreCheckpoint", slot=1)
        self.assertFalse(reply["success"])
        self.assertEqual(len(self.broker.commands), before)
    def test_checkpoint_bounds_before_transport(self):
        for args in ({"operation":"saveCheckpoint", "slot":8}, {"operation":"saveCheckpoint", "slot":True},
                     {"operation":"restoreCheckpoint", "slot":1, "resourceMode":"all"},
                     {"operation":"closeDescriptor", "descriptor":2147483648, "expectedBase":0, "expectedInheriting":0}):
            reply = self.send("uwvm/wasip1Edit", **args)
            self.assertFalse(reply["success"], reply)
        self.assertEqual(self.broker.commands, [])
    def test_extra_acknowledgement_is_not_ignored(self):
        original = self.broker.request
        self.broker.request = lambda command: original(command) + ("garbage\n" if command.startswith("set wasip1 ") else "")
        reply = self.send("uwvm/wasip1Edit", operation="setEnvironment", name="KEY", value="x")
        self.assertFalse(reply["success"])

class Wasip1PortableProtocol(unittest.TestCase):
    setUp = Wasip1Protocol.setUp
    send = Wasip1Protocol.send
    def ack(self, status="ok", applied=True, diagnostic=""):
        original=self.broker.request
        def request(command):
            packet=original(command)
            if command.startswith(("set wasip1 export", "set wasip1 import")):
                packet=("wasip1-stop 41\nwasip1 module=0 status="+status+" epoch=7 applied="+str(int(applied))+" shared-environment=1 total=4\n"
                    "wasip1-portable resources=4 format=1 content=external\n"
                    "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n")
                if diagnostic: packet+='diagnostic="'+diagnostic+'"\n'
            return packet
        self.broker.request=request
    def test_export_path_hex_and_paired_checkpoint_reminder(self):
        self.ack()
        self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        reply=self.send("uwvm/wasip1Edit",operation="exportPortableCheckpoint",path="/tmp/a b")
        self.assertTrue(reply["success"],reply)
        self.assertEqual(self.broker.commands,["status","set wasip1 export 0 2f746d702f612062 if-stop 41","status"])
        self.assertEqual(self.adapter.wasm_object_views,{})
        self.assertTrue(reply["body"]["wasmCheckpointRequired"])
        self.assertEqual(reply["body"]["contentMode"],"external")
        self.assertFalse(reply["body"]["externalIORollback"])
    def test_import_explicit_rebindings_and_refusal_diagnostic(self):
        self.ack("resource rollback unavailable",False,"resource=2\\x20missing\\x20mount")
        reply=self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",pathHex="2f746d702f61",bindings=[{"resource":2,"descriptor":91}])
        self.assertTrue(reply["success"],reply)
        self.assertFalse(reply["body"]["applied"])
        self.assertEqual(reply["body"]["diagnostic"],"resource=2\\x20missing\\x20mount")
        self.assertEqual(self.broker.commands[1],"set wasip1 import 0 2f746d702f61 2=91 if-stop 41")
    def test_invalid_paths_bindings_and_bounds_before_transport(self):
        for extra in ({"path":""},{"pathHex":"00"},{"path":"x","bindings":[{"resource":True,"descriptor":1}]},
                      {"path":"x","bindings":[{"resource":2,"descriptor":1},{"resource":2,"descriptor":3}]},
                      {"path":"x","bindings":[{"resource":0,"descriptor":2147483648}]},
                      {"path":"x","bindings":[{"resource":65536,"descriptor":1}]},
                      {"path":"x","bindings":[{"resource":0,"descriptor":1,"nativeHandle":42}]},
                      {"path":"x","bindings":[{"resource":0,"descriptor":1}]*65}):
            self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",**extra)["success"])
        self.assertFalse(self.send("uwvm/wasip1Edit",operation="exportPortableCheckpoint",path="x",bindings=[{"resource":0,"descriptor":1}])["success"])
        self.assertEqual(self.broker.commands,[])
    def test_success_requires_metadata_acknowledgement(self):
        reply=self.send("uwvm/wasip1Edit",operation="exportPortableCheckpoint",path="x")
        self.assertFalse(reply["success"],reply)
    def test_success_claim_requires_applied_operation(self):
        self.ack(applied=False)
        reply=self.send("uwvm/wasip1Edit",operation="exportPortableCheckpoint",path="x")
        self.assertFalse(reply["success"],reply)
        self.assertIsNone(self.adapter.stop_key)
    def test_malformed_utf8_filename_never_reaches_transport(self):
        for path in ("ff", "c0af", "eda080", "f4908080", "e282"):
            self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",pathHex=path)["success"])
            self.assertEqual(self.broker.commands,[])
    def test_resource_count_matches_original_acknowledgement(self):
        self.ack()
        original=self.broker.request
        self.broker.request=lambda command: original(command).replace("resources=4", "resources=5")
        reply=self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",path="x")
        self.assertFalse(reply["success"],reply)
        self.assertIsNone(self.adapter.stop_key)
    def test_combined_path_and_binding_size_before_transport(self):
        mappings=[{"resource":n,"descriptor":2147483647} for n in range(64)]
        self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",path="x"*4096,bindings=mappings)["success"])
        self.assertEqual(self.broker.commands,[])
    def test_retired_stop_and_native_stop_never_send_import(self):
        self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",path="x",stopId=42)["success"])
        self.broker.native=True
        self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpoint",path="x")["success"])
        self.assertEqual(self.broker.commands,["status","status"])


class PortableGroupProtocol(unittest.TestCase):
    # These are parser/acknowledgement tests only. The separate live test uses
    # real original-compiler owners and the authenticated console broker.
    setUp = Wasip1Protocol.setUp
    send = Wasip1Protocol.send
    def ack(self, status="ok", applied=1, diagnostic=""):
        original = self.broker.request
        def request(command):
            if command.startswith("set wasip1 "):
                self.broker.commands.append(command)
                if self.broker.after: self.broker.after()
                return (f"wasip1-stop 41\nwasip1 module=0 status={status} epoch=7 applied={applied} shared-environment=0 total=4\n"
                    "wasip1-portable-group environments=2 resources=4 format=1 content=external atomic=true\n"
                    "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n" +
                    (f'diagnostic="{diagnostic}"\n' if diagnostic else ""))
            return original(command)
        self.broker.request = request
    def test_group_canonical_export_and_import(self):
        self.ack()
        for operation, environments, expected in (
            ("exportPortableCheckpointGroup", [{"moduleId":0},{"moduleId":1}], "export-group 78 0 1"),
            ("importPortableCheckpointGroup", [{"moduleId":0,"bindings":[{"resource":0,"descriptor":2}]},{"moduleId":1,"bindings":[{"resource":1,"descriptor":3}]}], "import-group 78 0:0=2 1:1=3")):
            reply=self.send("uwvm/wasip1Edit", operation=operation, path="x", environments=environments, stopId=41)
            self.assertTrue(reply["success"],reply);self.assertTrue(reply["body"]["applied"])
            self.assertEqual(reply["body"]["environmentCount"],2);self.assertTrue(reply["body"]["groupAtomic"])
            self.assertTrue(reply["body"]["wasmCheckpointRequired"])
            self.assertIn("set wasip1 "+expected+" if-stop 41",self.broker.commands)
    def test_group_negative_ack_preserves_diagnostic(self):
        self.ack("resource rollback unavailable",0,r"request=1\x20missing\x20mount")
        reply=self.send("uwvm/wasip1Edit",operation="importPortableCheckpointGroup",path="x",environments=[{"moduleId":0},{"moduleId":1}])
        self.assertTrue(reply["success"],reply);self.assertFalse(reply["body"]["applied"])
        self.assertIn("request=1",reply["body"]["diagnostic"])
    def test_group_shapes_fail_before_transport(self):
        cases=[[],[{"moduleId":True}],[{"moduleId":-1}],[{"moduleId":0},{"moduleId":0}],
            [{"moduleId":n} for n in range(17)],[{"moduleId":0,"extra":1}],[{"moduleId":0,"bindings":None}],
            [{"moduleId":0,"bindings":[{"resource":0,"descriptor":True}]}],
            [{"moduleId":0,"bindings":[{"resource":0,"descriptor":1},{"resource":0,"descriptor":2}]}]]
        for environments in cases:
            self.assertFalse(self.send("uwvm/wasip1Edit",operation="importPortableCheckpointGroup",path="x",environments=environments)["success"])
            self.assertEqual(self.broker.commands,[])
        for extra in ({"moduleId":2},{"bindings":[]},{"pathHex":"ff","path":None}):
            args={"operation":"importPortableCheckpointGroup","path":"x","environments":[{"moduleId":0}],**extra}
            self.assertFalse(self.send("uwvm/wasip1Edit",**args)["success"]);self.assertEqual(self.broker.commands,[])
        self.assertFalse(self.send("uwvm/wasip1Edit",operation="exportPortableCheckpointGroup",path="x",environments=[{"moduleId":0,"bindings":[{"resource":0,"descriptor":2}]}])["success"])
        self.assertEqual(self.broker.commands,[])
    def test_group_ack_count_and_atomicity_are_exact(self):
        self.ack();original=self.broker.request
        for old,new in (("environments=2","environments=1"),("resources=4","resources=5"),("atomic=true","atomic=false"),("applied=1","applied=0"),("status=ok","status=invalid request")):
            self.broker.request=lambda command,old=old,new=new:original(command).replace(old,new)
            self.assertFalse(self.send("uwvm/wasip1Edit",operation="exportPortableCheckpointGroup",path="x",environments=[{"moduleId":0},{"moduleId":1}])["success"])
            self.assertIsNone(self.adapter.stop_key)
    def test_group_stop_cache_and_commit_after_resume(self):
        self.ack();self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        def after():
            self.assertEqual(self.adapter.wasm_object_views,{})
            self.broker.stop=42
        self.broker.after=after
        reply=self.send("uwvm/wasip1Edit",operation="importPortableCheckpointGroup",path="x",environments=[{"moduleId":0},{"moduleId":1}])
        self.assertTrue(reply["success"],reply);self.assertTrue(reply["body"]["applied"]);self.assertFalse(reply["body"]["stopCurrent"])

if __name__=="__main__":unittest.main()
