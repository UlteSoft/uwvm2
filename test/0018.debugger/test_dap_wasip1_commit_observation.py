#!/usr/bin/env python3
"""Operation ACK and later stopped observation are separate DAP contracts.

These are protocol DATA doubles, not a VM or checkpoint authority.
"""
import importlib.util
import io
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("commit_observation_state", Path(__file__).with_name("test_dap_wasip1_state.py"))
state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(state)
dap = state.dap
REMINDER = "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n"
CHECKPOINTS = ("saveCheckpoint", "restoreCheckpoint", "dropCheckpoint")
EDITS = ("replaceArgument", "insertArgument", "removeArgument", "setEnvironment", "removeEnvironment", "reduceRights", "createFile", "duplicateDescriptor", "closeDescriptor")
PORTABLE = ("exportPortableCheckpoint", "importPortableCheckpoint", "exportPortableCheckpointGroup", "importPortableCheckpointGroup")
ALL = (*CHECKPOINTS, *EDITS, *PORTABLE)


def arguments(operation, slot=2):
    row = {"operation": operation, "moduleId": 0}
    if operation in CHECKPOINTS:
        row["slot"] = slot
        if operation == "restoreCheckpoint": row["resourceMode"] = "bindings"
    if operation in ("replaceArgument", "insertArgument", "removeArgument"): row["index"] = 1
    if operation in ("replaceArgument", "insertArgument", "setEnvironment"): row["value"] = "x"
    if operation in ("setEnvironment", "removeEnvironment"): row["name"] = "KEY"
    if operation == "createFile": row["valueHex"] = "4100427f"
    if operation in ("reduceRights", "duplicateDescriptor", "closeDescriptor"):
        row.update(descriptor=17, expectedBase=2, expectedInheriting=0)
    if operation == "reduceRights": row.update(newBase=0, newInheriting=0)
    if operation in PORTABLE:
        row["path"] = "/tmp/checkpoint-data-only"
        if operation.endswith("Group"): row["environments"] = [{"moduleId":0}, {"moduleId":1}]
    return row


def packet(operation, slot=2, status="ok", applied=1, metadata=True):
    text = f"wasip1-stop 41\nwasip1 module=0 status={status} epoch=7 applied={applied} shared-environment=1 total=2\n"
    if metadata:
        if operation in CHECKPOINTS:
            text += f"wasip1-checkpoint slot={slot} managed=1 retained-external=3 external-io-rollback=false\n" + REMINDER
        elif operation in ("createFile", "duplicateDescriptor", "closeDescriptor") and applied:
            text += "wasip1-fd affected=17\n"
        elif operation in PORTABLE:
            text += ("wasip1-portable-group environments=2 resources=2 format=1 content=external atomic=true\n"
                     if operation.endswith("Group") else "wasip1-portable resources=2 format=1 content=external\n") + REMINDER
    return text


class CommitObservation(unittest.TestCase):
    def run_edit(self, operation, *, slot=2, reply=None, post=None, lost=None, negotiated=False, before=None):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output); self.broker = state.Broker()
        self.adapter.broker = self.broker; self.adapter.observe(self.broker.status(), notify=False)
        self.adapter.supports_invalidated_event = negotiated
        self.adapter.wasm_object_views[17] = (self.adapter.stop_key, {}, [])
        calls = []; statuses = 0
        def request(command):
            nonlocal statuses
            calls.append(command)
            if command == "status":
                statuses += 1
                if statuses == 1:
                    if before is not None:
                        if isinstance(before, Exception): raise before
                        return before
                    return self.broker.status()
                if isinstance(post, Exception): raise post
                return self.broker.status() if post is None else post
            self.assertIsNone(self.adapter.stop_key)
            self.assertEqual(self.adapter.wasm_object_views, {})
            if lost is not None: raise lost
            return packet(operation, slot) if reply is None else reply
        self.broker.request = request
        self.adapter.handle({"type":"request", "seq":1, "command":"uwvm/wasip1Edit", "arguments":arguments(operation, slot)})
        stream = io.BytesIO(self.output.getvalue()); messages = []
        while prefix := stream.readline():
            self.assertTrue(prefix.startswith(b"Content-Length: ")); self.assertEqual(stream.readline(), b"\r\n")
            messages.append(json.loads(stream.read(int(prefix[16:-2]))))
        responses = [m for m in messages if m["type"] == "response"]
        self.assertEqual(len(responses), 1)
        return responses[0], messages, calls

    def confirmed(self, response, applied=True, observed=False):
        self.assertTrue(response["success"], response)
        body = response["body"]
        self.assertEqual(body["applied"], applied); self.assertEqual(body["available"], applied)
        self.assertEqual(body["stopObservationAvailable"], observed)
        if not observed:
            self.assertFalse(body["stopCurrent"])
            self.assertIn("outcome is confirmed", body["stopObservationDiagnostic"])
            self.assertIsNone(self.adapter.stop_key)
        self.assertEqual(self.adapter.wasm_object_views, {})

    def test_all_checkpoint_slots_preserve_ack_after_transport_failure(self):
        for operation in CHECKPOINTS:
            for slot in range(8):
                for error in (ConnectionError("lost status"), OSError("transport closed"), RuntimeError("error status"), ValueError("invalid status")):
                    with self.subTest(operation=operation, slot=slot, error=type(error).__name__):
                        response, _, calls = self.run_edit(operation, slot=slot, post=error)
                        self.confirmed(response); self.assertEqual(len(calls), 3)
                        self.assertEqual(response["body"]["slot"], slot)
                        self.assertTrue(response["body"]["wasmCheckpointRequired"])
                        self.assertFalse(response["body"]["externalIORollback"])

    def test_all_other_edits_preserve_confirmed_outcome(self):
        for operation in EDITS:
            with self.subTest(operation=operation):
                response, _, calls = self.run_edit(operation, post=ConnectionError("private status detail"))
                self.confirmed(response); self.assertEqual(len(calls), 3)
                self.assertNotIn("private status detail", json.dumps(response))
                if operation in EDITS[-3:]: self.assertEqual(response["body"]["descriptor"], 17)

    def test_portable_metadata_survives_observation_failure(self):
        for operation in PORTABLE:
            response, _, calls = self.run_edit(operation, post=OSError("lost status"))
            self.confirmed(response); self.assertEqual(len(calls), 3)
            self.assertFalse(response["body"]["externalIORollback"])
            self.assertEqual(response["body"]["formatVersion"], 1)
            if operation.endswith("Group"): self.assertTrue(response["body"]["groupAtomic"])

    def test_known_nonapplied_refusals_remain_nonapplied(self):
        for operation in CHECKPOINTS:
            for status in ("entry not found", "resource rollback unavailable", "requires current cooperative stop"):
                for metadata in (False, True):
                    response, _, calls = self.run_edit(operation, reply=packet(operation, status=status, applied=0, metadata=metadata), post=OSError("lost status"))
                    self.confirmed(response, applied=False); self.assertEqual(response["body"]["status"], status)
                    self.assertEqual(len(calls), 3)

    def test_unrecognized_observation_is_unavailable_not_running_proof(self):
        for post in ("", "garbage\n", "running\nextra\n", "guest exited: 0\nextra\n", "prepared; old unidentified data\n"):
            response, _, calls = self.run_edit("restoreCheckpoint", post=post)
            self.confirmed(response); self.assertEqual(len(calls), 3)

    def test_incomplete_or_unbounded_stopped_observation_cannot_publish_views(self):
        good = state.Broker().status()
        variants = (good.replace("stop-id 41\n", ""), good.replace("stop-id 41", "stop-id 0"),
                    good.replace("thread 1", "thread 0"), good.replace("generation=7", "generation=0"),
                    good.replace("module=0", "module=18446744073709551616"),
                    good + "thread 1 module=0 function=1 byte-offset=4 generation=7\n",
                    good + "stopped: selected participant step\n", "stopped: pause\nstop-id 41\n",
                    "stopped: selected participant step\nstop-id 41\n" + "".join(f"thread {i} module=0 function=1 byte-offset=4 generation=7\n" for i in range(1,258)))
        for post in variants:
            response, _, _ = self.run_edit("saveCheckpoint", post=post); self.confirmed(response)

    def test_known_fresh_running_native_and_exited_observations_preserve_commit(self):
        native = state.Broker(); native.native = True
        for post in ("running\n", native.status(), "guest exited: 0\n", "debug domain closed\n"):
            response, _, calls = self.run_edit("saveCheckpoint", post=post)
            self.confirmed(response, observed=True); self.assertFalse(response["body"]["stopCurrent"])
            self.assertNotIn("stopObservationDiagnostic", response["body"]); self.assertEqual(len(calls), 3)

    def test_stop_current_requires_the_original_complete_cohort(self):
        good = state.Broker().status()
        for post in (good, good.replace("stop-id 41", "stop-id 42"), good.replace("generation=7", "generation=8"),
                     good + "thread 2 module=0 function=1 byte-offset=4 generation=7\n"):
            response, _, _ = self.run_edit("saveCheckpoint", post=post)
            self.confirmed(response, observed=True); self.assertEqual(response["body"]["stopCurrent"], post == good)

    def test_invalid_full_ack_fails_before_any_observation(self):
        good = packet("restoreCheckpoint")
        for reply in (good.replace("slot=2", "slot=3"), good.replace("epoch=7", "epoch=0"),
                      good.replace(REMINDER, ""), good.replace("applied=1", "applied=0"), good + "host-handle=42\n"):
            response, messages, calls = self.run_edit("restoreCheckpoint", reply=reply, post=OSError("unused"), negotiated=True)
            self.assertFalse(response["success"]); self.assertNotIn("body", response); self.assertEqual(len(calls), 2)
            self.assertFalse(any(m.get("event") == "invalidated" for m in messages)); self.assertIsNone(self.adapter.stop_key)

    def test_lost_ack_is_unconfirmed_with_no_retry_or_observation(self):
        for operation in ALL:
            response, messages, calls = self.run_edit(operation, lost=ConnectionError("withheld committed ACK"), negotiated=True)
            self.assertFalse(response["success"]); self.assertNotIn("body", response); self.assertIn("unconfirmed", response["message"])
            self.assertEqual(len(calls), 2); self.assertIsNone(self.adapter.stop_key)
            self.assertFalse(any(m.get("event") == "invalidated" for m in messages))

    def test_negotiated_refresh_follows_confirmed_response_even_without_status(self):
        for operation in ALL:
            for negotiated in (False, True):
                response, messages, _ = self.run_edit(operation, post=OSError("status unavailable"), negotiated=negotiated)
                self.confirmed(response)
                self.assertEqual([m["type"] for m in messages], ["response", "event"] if negotiated else ["response"])
                if negotiated: self.assertEqual(messages[1]["body"], {"areas":["stacks", "variables"]})

    def test_refusal_does_not_emit_mutation_refresh(self):
        response, messages, _ = self.run_edit("restoreCheckpoint", reply=packet("restoreCheckpoint", status="entry not found", applied=0), post=OSError("lost status"), negotiated=True)
        self.confirmed(response, applied=False); self.assertEqual([m["type"] for m in messages], ["response"])

    def test_preoperation_failure_does_not_claim_unknown_mutation(self):
        for before in (ConnectionError("admission unavailable"), "running\n"):
            response, _, calls = self.run_edit("restoreCheckpoint", before=before)
            self.assertFalse(response["success"]); self.assertNotIn("unconfirmed", response["message"])
            self.assertEqual(calls, ["status"])


if __name__ == "__main__": unittest.main()
