#!/usr/bin/env python3
"""Exercise the sparse-memory watchdog without a Linux benchmark cgroup."""

import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import preflight_high_memory as target


class BoundedRunTest(unittest.TestCase):
    def test_successful_child_remains_eligible(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "memory.current").write_text("0\n")
            actual_path = Path

            def path_for_test(value):
                return root if value == "/sys/fs/cgroup" else actual_path(value)

            with mock.patch.object(target, "Path", side_effect=path_for_test), \
                    mock.patch.object(target, "proc_memory_kib",
                                      return_value={"VmRSS": 1024}):
                row = target.bounded_run(
                    [sys.executable, "-c", "print('checked')"],
                    "success", root, os.environ.copy())
            self.assertEqual(row["exit"], 0)
            self.assertFalse(row["rss_limited"])
            self.assertFalse(row["timed_out"])
            self.assertEqual((root / "success.log").read_text(), "checked\n")

    def test_rss_limit_kills_a_live_child(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "memory.current").write_text("0\n")
            actual_path = Path

            def path_for_test(value):
                return root if value == "/sys/fs/cgroup" else actual_path(value)

            with mock.patch.object(target, "Path", side_effect=path_for_test), \
                    mock.patch.object(target, "proc_memory_kib",
                                      return_value={"VmRSS": target.MAX_RSS_KIB + 1}):
                row = target.bounded_run(
                    [sys.executable, "-c", "import time; time.sleep(10)"],
                    "over-rss", root, os.environ.copy())
            self.assertEqual(row["exit"], 125)
            self.assertTrue(row["rss_limited"])
            self.assertFalse(row["timed_out"])
            self.assertFalse(row["cgroup_memory_limited"])
            self.assertLess(row["elapsed_ns"], 2_000_000_000)

    def test_transient_high_water_mark_still_kills_child(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "memory.current").write_text("0\n")
            actual_path = Path

            def path_for_test(value):
                return root if value == "/sys/fs/cgroup" else actual_path(value)

            with mock.patch.object(target, "Path", side_effect=path_for_test), \
                    mock.patch.object(target, "proc_memory_kib",
                                      return_value={"VmRSS": 1024,
                                                    "VmHWM": target.MAX_RSS_KIB + 1}):
                row = target.bounded_run(
                    [sys.executable, "-c", "import time; time.sleep(10)"],
                    "over-hwm", root, os.environ.copy())
            self.assertEqual(row["exit"], 125)
            self.assertTrue(row["rss_limited"])


if __name__ == "__main__":
    unittest.main()
