#!/usr/bin/env python3
"""Offline controls for the host worker ownership guard, without compilation."""
import importlib.util
from pathlib import Path
import signal
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location("bounded_slot", Path(__file__).with_name("run_core3_component_bounded_slot.py"))
slot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slot)


def row(pid, birth, parent=0, uid=1000):
    return {"pid": pid, "start_time": birth, "parent_pid": parent, "uid": uid,
            "state": "S", "rss_bytes": 4096, "thread_affinities": {str(pid): [16]}}


class OwnershipControls(unittest.TestCase):
    def setUp(self):
        self.root = row(70, 100)
        self.child = row(71, 101, 70)
        self.owned = slot.OwnedWorkerTree(self.root, 1000, Path("/fake/cgroup"), 700)

    def identities(self, pid):
        return ({70: 100, 71: 101}[pid], "S")

    def test_exec_child_environment_is_never_read(self):
        # A non-dumpable compiler still exposes public stat/RSS ancestry.
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, self.child]), \
                patch.object(slot, "validate_membership"), \
                patch.object(slot.os, "pidfd_open", return_value=701, create=True), \
                patch.object(Path, "read_text", return_value="PPid:\t70\nUid:\t1000 1000 1000 1000\n"), \
                patch.object(Path, "read_bytes", side_effect=PermissionError("environ is private")):
            rows = self.owned.refresh()
        self.assertEqual({value["pid"] for value in rows}, {70, 71})
        self.assertEqual(self.owned.pidfds, {70: 700, 71: 701})

    def test_child_parent_change_while_pidfd_is_pinned_is_rejected(self):
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, self.child]), \
                patch.object(slot.os, "pidfd_open", return_value=701, create=True), \
                patch.object(slot.os, "close") as close, \
                patch.object(Path, "read_text", return_value="PPid:\t90\nUid:\t1000 1000 1000 1000\n"):
            with self.assertRaisesRegex(RuntimeError, "ancestry changed"):
                self.owned.refresh()
        close.assert_called_once_with(701)
        self.assertEqual(self.owned.pidfds, {70: 700})

    def test_child_naturally_retired_before_pidfd_is_excluded(self):
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, self.child]), \
                patch.object(slot, "validate_membership"), \
                patch.object(slot.os, "pidfd_open", side_effect=ProcessLookupError(), create=True), \
                patch.object(Path, "exists", return_value=False):
            self.assertEqual(self.owned.refresh(), [self.root])
        self.assertEqual(self.owned.pidfds, {70: 700})

    def test_live_child_pidfd_failure_remains_fatal(self):
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, self.child]), \
                patch.object(slot.os, "pidfd_open", side_effect=ProcessLookupError(), create=True), \
                patch.object(Path, "exists", return_value=True):
            with self.assertRaises(ProcessLookupError):
                self.owned.refresh()

    def test_reparented_previously_proven_child_remains_owned(self):
        self.owned.known[71] = self.child
        self.owned.pidfds[71] = 701
        orphan = self.child | {"parent_pid": 1}
        def identity(pid):
            if pid == 70:
                raise FileNotFoundError()
            return 101, "S"
        with patch.object(slot, "identity", side_effect=identity), \
                patch.object(slot, "tree", return_value=[orphan]), \
                patch.object(slot, "validate_membership"):
            rows = self.owned.refresh()
        self.assertEqual(rows, [orphan])

    def test_new_foreign_parent_is_rejected(self):
        foreign = self.child | {"parent_pid": 90}
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, foreign]):
            with self.assertRaisesRegex(RuntimeError, "no proven parent"):
                self.owned.refresh()

    def test_uid_transition_is_rejected(self):
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", return_value=[self.root, self.child | {"uid": 0}]):
            with self.assertRaisesRegex(RuntimeError, "host UID"):
                self.owned.refresh()

    def test_reused_numeric_pid_is_not_adopted(self):
        with patch.object(slot, "identity", return_value=(200, "S")), \
                patch.object(slot, "tree") as tree, patch.object(slot, "validate_membership"):
            self.assertEqual(self.owned.refresh(), [])
            tree.assert_not_called()

    def test_bootstrap_marker_is_not_read_from_same_uid_peer(self):
        expected = ["python3", "-c", slot.BOOTSTRAP_CODE, "[]"]
        def text(path):
            return "70 71" if path.name == "cgroup.procs" else "Uid:\t1000 1000 1000 1000\n"
        def binary(path):
            if str(path) == "/proc/70/cmdline":
                return b"\0".join(item.encode() for item in expected) + b"\0"
            if str(path) == "/proc/70/environ":
                return (slot.MARKER_NAME + "=own-marker").encode() + b"\0"
            if str(path) == "/proc/71/cmdline":
                return b"clang++\0-cc1\0"
            raise PermissionError("peer environ must not be inspected")
        with patch.object(Path, "read_text", text), patch.object(Path, "read_bytes", binary):
            self.assertEqual(slot.marked_bootstraps(Path("/fake"), "own-marker", 1000, expected), [70])

    def test_public_rss_failure_remains_fatal(self):
        with patch.object(slot, "identity", side_effect=self.identities), \
                patch.object(slot, "tree", side_effect=PermissionError("statm is unavailable")):
            with self.assertRaises(PermissionError):
                self.owned.refresh()

    def test_cleanup_signals_pinned_tasks_even_if_fresh_observation_fails(self):
        self.owned.pidfds[71] = 701
        with patch.object(self.owned, "refresh", side_effect=PermissionError("statm unavailable")), \
                patch.object(slot.signal, "pidfd_send_signal", create=True) as send, \
                patch.object(slot.os, "kill") as numeric_kill:
            errors = self.owned.stop_and_kill()
        self.assertEqual(errors, ["statm unavailable"])
        self.assertEqual([item.args for item in send.call_args_list],
                         [(700, signal.SIGSTOP), (701, signal.SIGSTOP),
                          (700, signal.SIGKILL), (701, signal.SIGKILL)])
        numeric_kill.assert_not_called()


if __name__ == "__main__":
    unittest.main()
