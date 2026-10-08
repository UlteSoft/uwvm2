#!/usr/bin/env python3
"""Checkpoint reply contracts; optional packets come from actual native formatters.

python3 test_dap_wasip1_checkpoint_acknowledgement.py [--packets native.log]
The broker is a transport double. This does not run a VM or restore a resource.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import sys
import unittest

spec = importlib.util.spec_from_file_location("checkpoint_state_tests", Path(__file__).with_name("test_dap_wasip1_state.py"))
state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(state)
REMINDER = "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n"
OPERATIONS = ("saveCheckpoint", "restoreCheckpoint", "dropCheckpoint")
SCENARIOS = 0
PACKETS = None

def arguments(operation, slot):
    result = {"operation": operation, "slot": slot}
    if operation == "restoreCheckpoint":
        result["resourceMode"] = "strict"
    return result

def packet(slot, status="ok", applied=1, managed=1, external=3):
    return (f"wasip1-stop 41\nwasip1 module=0 status={status} epoch=7 applied={applied} shared-environment=1 total=2\n"
        f"wasip1-checkpoint slot={slot} managed={managed} retained-external={external} external-io-rollback=false\n" + REMINDER)

class CheckpointAcknowledgements(unittest.TestCase):
    setUp = state.Wasip1Protocol.setUp
    send = state.Wasip1Protocol.send

    def edit(self, response, operation, slot, resumed=False):
        global SCENARIOS
        SCENARIOS += 1
        self.setUp()
        self.adapter.wasm_object_views[1] = (self.adapter.stop_key, {}, [])
        original = self.broker.request
        def request(command):
            if command != "status":
                self.broker.commands.append(command)
                self.assertEqual(self.adapter.wasm_object_views, {})
                self.assertIsNone(self.adapter.stop_key)
                if resumed:
                    self.broker.stop = 42
                return response
            return original(command)
        self.broker.request = request
        reply = self.send("uwvm/wasip1Edit", **arguments(operation, slot))
        verb = "unset" if operation == "dropCheckpoint" else "set"
        noun = "restore" if operation == "restoreCheckpoint" else "checkpoint"
        spelling = f"{verb} wasip1 {noun} 0 {slot}" + (" strict" if operation == "restoreCheckpoint" else "") + " if-stop 41"
        self.assertEqual(self.broker.commands[:2], ["status", spelling])
        self.assertEqual(self.adapter.wasm_object_views, {})
        return reply

    def rejected(self, response, operation, slot):
        reply = self.edit(response, operation, slot)
        self.assertFalse(reply["success"], reply)
        self.assertEqual(len(self.broker.commands), 2)  # no retry or trusting follow-up status
        self.assertIsNone(self.adapter.stop_key)

    def test_success_requires_complete_metadata_and_reminder(self):
        for operation in OPERATIONS:
            for slot in range(8):
                good = packet(slot)
                variants = (good.split("wasip1-checkpoint")[0], good.replace(REMINDER, ""),
                    good.replace(REMINDER, REMINDER[:-1]), good.replace("Wasm and WASIp1", "WASIp1"),
                    good.replace(f"slot={slot}", f"slot={(slot + 1) % 8}"), good.replace(f"slot={slot}", "slot=8"),
                    good.replace("managed=1", "managed=65534"), good.replace("managed=1", "managed=100000"),
                    good.replace("managed=1", "managed=-1"), good.replace("rollback=false", "rollback=true"),
                    good + "host-handle=42\n", good + REMINDER, good.replace("wasip1-stop 41", "wasip1-stop 42"),
                    good.replace("module=0", "module=1"))
                for altered in variants:
                    with self.subTest(operation=operation, slot=slot, packet=altered):
                        self.rejected(altered, operation, slot)

    def test_ok_requires_applied_operation(self):
        for operation in OPERATIONS:
            for slot in range(8):
                for response in (packet(slot, applied=0), packet(slot, applied=0).split("wasip1-checkpoint")[0]):
                    with self.subTest(operation=operation, slot=slot):
                        self.rejected(response, operation, slot)

    def test_valid_metadata_resource_limits_and_commit_after_resume(self):
        for operation in OPERATIONS:
            for slot in range(8):
                for managed, external in ((0, 0), (32768, 32768), (65536, 0), (0, 65536)):
                    reply = self.edit(packet(slot, managed=managed, external=external), operation, slot)
                    self.assertTrue(reply["success"], reply)
                    body = reply["body"]
                    self.assertTrue(body["applied"] and body["available"] and body["stopCurrent"] and body["wasmCheckpointRequired"])
                    self.assertFalse(body["externalIORollback"])
                    self.assertEqual((body["slot"], body["managedResources"], body["retainedExternalResources"]), (slot, managed, external))
                    self.assertEqual(len(self.broker.commands), 3)
                reply = self.edit(packet(slot), operation, slot, resumed=True)
                self.assertTrue(reply["success"] and reply["body"]["applied"] and reply["body"]["wasmCheckpointRequired"], reply)
                self.assertFalse(reply["body"]["stopCurrent"])
                self.assertEqual(len(self.broker.commands), 3)

    def test_refusals_remain_available_as_nonapplied_results(self):
        for operation in OPERATIONS:
            for slot in range(8):
                for status in ("entry not found", "resource rollback unavailable", "requires current cooperative stop"):
                    response = packet(slot, status=status, applied=0)
                    for tail in (response, response.split("wasip1-checkpoint")[0]):
                        reply = self.edit(tail, operation, slot)
                        self.assertTrue(reply["success"], reply)
                        self.assertFalse(reply["body"]["applied"] or reply["body"]["available"])
                        self.assertEqual(reply["body"]["status"], status)
                        self.assertEqual(len(self.broker.commands), 3)
                    self.rejected(packet(slot, status=status, applied=1), operation, slot)

    def test_native_formatter_packets(self):
        if PACKETS is None:
            self.skipTest("supply --packets for native formatter interoperability")
        data = PACKETS.read_text()
        rows = re.findall(r"^PACKET slot=([0-7]) variant=([0-4]) hex=([0-9a-f]+)\r?$", data, re.M)
        self.assertEqual(len(rows), 40)
        self.assertEqual({(int(s), int(v)) for s, v, _ in rows}, {(s, v) for s in range(8) for v in range(5)})
        for slot, variant, hexadecimal in rows:
            response = "wasip1-stop 41\n" + bytes.fromhex(hexadecimal).decode("ascii")
            for operation in OPERATIONS:
                reply = self.edit(response, operation, int(slot))
                self.assertTrue(reply["success"], reply)
                self.assertEqual(reply["body"]["applied"], int(variant) < 3)
                self.assertEqual(reply["body"]["available"], int(variant) < 3)
                self.assertEqual(reply["body"]["runtimeEpoch"], (1 << 64) - 1)
                self.assertTrue(reply["body"]["wasmCheckpointRequired"])
                self.assertFalse(reply["body"]["externalIORollback"])

def baseline():
    """Demonstrate old success-without-reminder/noncommit behavior, without writes."""
    checks = CheckpointAcknowledgements()
    for operation in OPERATIONS:
        for slot in range(8):
            for malformed in (packet(slot).split("wasip1-checkpoint")[0],
                    packet(slot, applied=0).split("wasip1-checkpoint")[0], packet(slot, applied=0)):
                reply = checks.edit(malformed, operation, slot)
                checks.assertTrue(reply["success"] and reply["body"]["available"], reply)
    print(f"checkpoint acknowledgement old bug reproduced scenarios={SCENARIOS}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packets", type=Path)
    parser.add_argument("--expect-old-bug", action="store_true")
    options = parser.parse_args()
    PACKETS = options.packets
    if options.expect_old_bug:
        baseline()
    else:
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(CheckpointAcknowledgements))
        print(f"checkpoint acknowledgement scenarios={SCENARIOS} passed={int(result.wasSuccessful())}")
        sys.exit(0 if result.wasSuccessful() else 1)
