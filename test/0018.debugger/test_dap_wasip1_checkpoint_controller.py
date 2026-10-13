#!/usr/bin/env python3
"""Controller packet interoperability; transport double does not run a VM."""
import argparse
import importlib.util
from pathlib import Path
import re
import sys
import unittest

spec=importlib.util.spec_from_file_location("controller_ack_tests",Path(__file__).with_name("test_dap_wasip1_checkpoint_acknowledgement.py"))
ack=importlib.util.module_from_spec(spec);spec.loader.exec_module(ack)
PACKETS=None
SCENARIOS=0

class ControllerPackets(ack.CheckpointAcknowledgements):
    def edit(self,*args,**kwargs):
        global SCENARIOS
        SCENARIOS+=1
        return super().edit(*args,**kwargs)
    def test_controller_packets_keep_recorded_epoch(self):
        self.assertIsNotNone(PACKETS)
        rows=re.findall(r"^PACKET operation=([0-2]) slot=([0-7]) epoch-index=([0-2]) epoch=([0-9]+) hex=([0-9a-f]+)\r?$",PACKETS.read_text(),re.M)
        self.assertEqual(len(rows),72)
        self.assertEqual({(int(o),int(s),int(e)) for o,s,e,_,_ in rows},{(o,s,e) for o in range(3) for s in range(8) for e in range(3)})
        for operation,slot,epoch_index,epoch,hexadecimal in rows:
            expected=(1,7,(1<<64)-1)[int(epoch_index)]
            self.assertEqual(int(epoch),expected)
            response="wasip1-stop 41\n"+bytes.fromhex(hexadecimal).decode("ascii")
            for resumed in (False,True):
                reply=self.edit(response,ack.OPERATIONS[int(operation)],int(slot),resumed)
                self.assertTrue(reply["success"],reply)
                body=reply["body"]
                self.assertTrue(body["available"] and body["applied"] and body["wasmCheckpointRequired"])
                self.assertEqual(body["runtimeEpoch"],expected)
                self.assertEqual(body["stopCurrent"],not resumed)
                self.assertEqual((body["managedResources"],body["retainedExternalResources"]),(2,3))
    def test_zero_epoch_success_is_not_a_checkpoint(self):
        for operation in ack.OPERATIONS:
            for slot in range(8):
                self.rejected(ack.packet(slot).replace("epoch=7","epoch=0"),operation,slot)
    def test_unavailable_metadata_can_have_zero_epoch(self):
        for operation in ack.OPERATIONS:
            for slot in range(8):
                for status in ("allocation failed","entry not found","resource rollback unavailable"):
                    reply=self.edit(ack.packet(slot,status=status,applied=0).replace("epoch=7","epoch=0"),operation,slot)
                    self.assertTrue(reply["success"],reply);self.assertFalse(reply["body"]["applied"] or reply["body"]["available"])

def baseline():
    test=ControllerPackets()
    for operation in ack.OPERATIONS:
        for slot in range(8):
            reply=test.edit(ack.packet(slot).replace("epoch=7","epoch=0"),operation,slot)
            test.assertTrue(reply["success"] and reply["body"]["available"] and reply["body"]["runtimeEpoch"]==0,reply)
    print(f"controller zero-epoch old bug reproduced scenarios={SCENARIOS}")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--packets",type=Path);parser.add_argument("--expect-old-bug",action="store_true")
    options=parser.parse_args();PACKETS=options.packets
    if options.expect_old_bug:baseline()
    else:
        # Only this class's new cases: inherited checkpoint regressions have
        # their own complete runner with the original 40 formatter packets.
        names=("test_controller_packets_keep_recorded_epoch","test_zero_epoch_success_is_not_a_checkpoint","test_unavailable_metadata_can_have_zero_epoch")
        result=unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(ControllerPackets(name) for name in names))
        print(f"controller acknowledgement scenarios={SCENARIOS} passed={int(result.wasSuccessful())}")
        sys.exit(0 if result.wasSuccessful() else 1)
