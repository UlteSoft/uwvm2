#!/usr/bin/env python3
"""WASIp1 mutation reply contracts; broker is a transport double, not a VM."""
import argparse
import importlib.util
from pathlib import Path
import re
import sys
import unittest

spec=importlib.util.spec_from_file_location("edit_state_tests",Path(__file__).with_name("test_dap_wasip1_state.py"))
state=importlib.util.module_from_spec(spec);spec.loader.exec_module(state)
OPERATIONS=("replaceArgument","insertArgument","removeArgument","setEnvironment","removeEnvironment",
    "reduceRights","createFile","duplicateDescriptor","closeDescriptor")
SCENARIOS=0
PACKETS=None
def arguments(operation):
    row={"operation":operation}
    if operation in ("replaceArgument","insertArgument","removeArgument"):row["index"]=1
    if operation in ("replaceArgument","insertArgument","setEnvironment"):row["value"]="x"
    if operation in ("setEnvironment","removeEnvironment"):row["name"]="KEY"
    if operation=="createFile":row["valueHex"]="410042"
    if operation in ("reduceRights","duplicateDescriptor","closeDescriptor"):
        row.update(descriptor=17,expectedBase=2,expectedInheriting=0)
    if operation=="reduceRights":row.update(newBase=0,newInheriting=0)
    return row
def packet(operation,status="ok",applied=1):
    text=f"wasip1-stop 41\nwasip1 module=0 status={status} epoch=7 applied={applied} shared-environment=1 total=2\n"
    if operation in OPERATIONS[6:] and applied:text+="wasip1-fd affected=17\n"
    return text

class EditAcknowledgements(unittest.TestCase):
    setUp=state.Wasip1Protocol.setUp
    send=state.Wasip1Protocol.send
    def edit(self,response,operation,resumed=False):
        global SCENARIOS
        SCENARIOS+=1;self.setUp();self.adapter.wasm_object_views[1]=(self.adapter.stop_key,{},[])
        original=self.broker.request
        def request(command):
            if command=="status":return original(command)
            self.broker.commands.append(command);self.assertEqual(self.adapter.wasm_object_views,{})
            self.assertIsNone(self.adapter.stop_key)
            if resumed:self.broker.stop=42
            return response
        self.broker.request=request
        reply=self.send("uwvm/wasip1Edit",**arguments(operation))
        self.assertEqual(self.broker.commands[0],"status")
        self.assertTrue(self.broker.commands[1].endswith(" if-stop 41"))
        self.assertEqual(self.adapter.wasm_object_views,{})
        return reply
    def rejected(self,response,operation):
        reply=self.edit(response,operation);self.assertFalse(reply["success"],reply)
        self.assertEqual(len(self.broker.commands),2);self.assertIsNone(self.adapter.stop_key)
    def test_status_and_applied_agree_for_every_edit(self):
        for operation in OPERATIONS:
            for status in sorted(state.dap._WASIP1_STATUS):
                for applied in (0,1):
                    response=packet(operation,status,applied)
                    if (status=="ok")!=bool(applied):self.rejected(response,operation)
                    else:
                        reply=self.edit(response,operation);self.assertTrue(reply["success"],reply)
                        self.assertEqual(reply["body"]["available"],status=="ok")
                        self.assertEqual(reply["body"]["applied"],bool(applied))
                        self.assertEqual(len(self.broker.commands),3)
    def test_success_after_resume_keeps_committed_result(self):
        for operation in OPERATIONS:
            reply=self.edit(packet(operation),operation,True)
            self.assertTrue(reply["success"] and reply["body"]["applied"] and reply["body"]["available"],reply)
            self.assertFalse(reply["body"]["stopCurrent"])
            self.assertEqual(len(self.broker.commands),3)
    def test_invalid_identity_or_extra_data_is_not_success(self):
        for operation in OPERATIONS:
            good=packet(operation)
            for response in (good.replace("wasip1-stop 41","wasip1-stop 42"),good.replace("module=0","module=1"),
                    good.replace("epoch=7","epoch=18446744073709551616"),good.replace("total=2","total=65537"),
                    good.replace("status=ok","status=unknown"),good+"garbage\n"):
                self.rejected(response,operation)
        for operation in OPERATIONS[6:]:
            good=packet(operation)
            for response in (good.split("wasip1-fd")[0],good.replace("affected=17","affected=2147483648"),
                    good.replace("affected=17","affected=-1"),good+"wasip1-fd affected=18\n"):
                self.rejected(response,operation)
    def test_native_formatter_packets(self):
        self.assertIsNotNone(PACKETS,"supply --packets; native interoperability is mandatory")
        rows=re.findall(r"^PACKET operation=([0-8]) variant=([0-3]) hex=([0-9a-f]+)\r?$",PACKETS.read_text(),re.M)
        self.assertEqual(len(rows),36);self.assertEqual({(int(o),int(v)) for o,v,_ in rows},{(o,v) for o in range(9) for v in range(4)})
        for index,variant,hexadecimal in rows:
            operation=OPERATIONS[int(index)];response="wasip1-stop 41\n"+bytes.fromhex(hexadecimal).decode("ascii")
            if int(variant) in (1,2):self.rejected(response,operation)
            else:
                reply=self.edit(response,operation);self.assertTrue(reply["success"],reply)
                self.assertEqual(reply["body"]["applied"],int(variant)==0)
                self.assertEqual(reply["body"]["runtimeEpoch"],(1<<64)-1)
                if int(index)>=6 and int(variant)==0:self.assertEqual(reply["body"]["descriptor"],0x7fffffff)
                self.assertEqual(len(self.broker.commands),3)

def baseline():
    checks=EditAcknowledgements()
    for operation in OPERATIONS:
        reply=checks.edit(packet(operation,applied=0),operation)
        checks.assertTrue(reply["success"] and reply["body"]["available"] and not reply["body"]["applied"],reply)
        checks.assertEqual(len(checks.broker.commands),3)
    print(f"edit acknowledgement old bug reproduced scenarios={SCENARIOS}")
if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--packets",type=Path);parser.add_argument("--expect-old-bug",action="store_true")
    options=parser.parse_args();PACKETS=options.packets
    if options.expect_old_bug:baseline()
    else:
        result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(EditAcknowledgements))
        print(f"edit acknowledgement scenarios={SCENARIOS} passed={int(result.wasSuccessful())}")
        sys.exit(0 if result.wasSuccessful() else 1)
