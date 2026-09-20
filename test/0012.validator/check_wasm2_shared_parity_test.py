#!/usr/bin/env python3
"""Semantic tests for the paired source-parity guard, using synthetic trees.

These test the checker, not Wasm semantics. No product source is modified.
"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECKER = Path(__file__).with_name("check_wasm2_shared_parity.py")
SPEC = importlib.util.spec_from_file_location("paired_guard", CHECKER)
GUARD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GUARD)


class PairedGuardTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="uwvm-parity-checker-")
        self.addCleanup(self.temporary.cleanup)
        self.products = [Path(self.temporary.name) / name for name in ("main", "ros")]
        self.roots = [path / "src/uwvm2" for path in self.products]
        files = set(GUARD.COMMON_FILES) | {GUARD.CONTROL}
        files.update(tree + "/probe.h" for tree in GUARD.COMMON_TREES)
        for index, root in enumerate(self.roots):
            for relative in files:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                data = "int stable_common;\n"
                if relative == GUARD.CONTROL and index == 0:
                    data += GUARD.TIERED_OUTPUT
                if relative == GUARD.MEMORY:
                    data += GUARD.MEMORY_COMMENTS[index]
                path.write_text(data)
        for relative in GUARD.FULL_ONLY:
            path = self.roots[0] / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.suffix:
                path.write_text("full-only fixture\n")
            else:
                path.mkdir()

    def check(self, expected):
        result = subprocess.run([sys.executable, CHECKER, "--main", self.products[0],
            "--ros", self.products[1]], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(bool(report["failures"]), bool(expected))
        return report

    def test_only_declared_differences_are_accepted(self):
        self.check(0)

    def test_interpreter_memory_body_drift_is_rejected(self):
        path = self.roots[1] / GUARD.MEMORY
        path.write_text(path.read_text().replace("stable_common", "changed_memory_code"))
        report = self.check(1)
        self.assertIn({"file": GUARD.MEMORY, "error": "shared implementation drift"}, report["failures"])

    def test_linear_memory_body_drift_is_rejected(self):
        path = self.roots[1] / "object/memory/linear/probe.h"
        path.write_text("int changed_reservation;\n")
        self.check(1)

    def test_runtime_api_module_owner_drift_is_rejected(self):
        path = self.roots[1] / "runtime/lib/uwvm_runtime.cppm"
        path.write_text("export module wrong_owner;\n")
        self.check(1)

    def test_network_service_lifetime_drift_is_rejected(self):
        path = self.roots[1] / "uwvm/crtmain/uwvm.h"
        path.write_text("int changed_network_lifetime;\n")
        self.check(1)

    def test_missing_linear_memory_member_is_rejected(self):
        # Deliberately alter a test-owned temporary fixture, never a product file.
        (self.roots[1] / "object/memory/linear/probe.h").unlink()
        self.check(1)

    def test_restored_ros_mode_is_rejected(self):
        path = self.roots[1] / GUARD.FULL_ONLY[0]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("unexpected ROS mode\n")
        self.check(1)

    def test_unrecognized_memory_comment_is_rejected(self):
        path = self.roots[1] / GUARD.MEMORY
        path.write_text(path.read_text().replace("LLVM AOT backend", "unknown backend"))
        self.check(1)

    def test_tiered_output_in_ros_is_rejected(self):
        path = self.roots[1] / GUARD.CONTROL
        path.write_text(path.read_text() + GUARD.TIERED_OUTPUT)
        self.check(1)

    def test_missing_common_file_is_rejected(self):
        (self.roots[1] / GUARD.COMMON_FILES[0]).unlink()
        self.check(1)

    def test_same_product_roots_are_rejected(self):
        result = subprocess.run([sys.executable, CHECKER, "--main", self.products[0],
            "--ros", self.products[0]], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertIn("two distinct", result.stderr)


if __name__ == "__main__":
    unittest.main()
