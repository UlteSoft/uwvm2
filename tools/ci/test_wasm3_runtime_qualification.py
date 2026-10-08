#!/usr/bin/env python3
"""Control-flow regressions for evidence qualification; no VM/compile is run."""

import json
import unittest

from requalify_wasm3_cli_from_provenance import apply_host_stack_probes
from run_wasm3_core3_native_17_modes import tiered_evidence
from wasm3_compile_fatal_policy import negative_classification, pc_evidence, PC_MARKER


class EvidenceQualification(unittest.TestCase):
    def pc(self, **changes):
        value = {"pc": 0x1234,
                 "function": "uwvm2::runtime::details::print_and_terminate_compile_validation_error",
                 "instruction": "ud2", "instruction_bytes": "0f0b",
                 "program_state": "It stopped with signal SIGILL, Illegal instruction."}
        value.update(changes)
        return value

    def transcript(self, value):
        return ("Program received signal SIGILL\n" + PC_MARKER + json.dumps(value) + "\n").encode()

    def test_signal_requires_exact_current_instruction(self):
        accepted = pc_evidence(self.transcript(self.pc()))
        self.assertIsNotNone(accepted)
        for changes in ({"instruction": "nop"}, {"instruction_bytes": "9090"},
                        {"function": "uwvm2::runtime::details::runtime_memory_crash"},
                        {"program_state": "It stopped with signal SIGSEGV"},
                        {"pc": True}, {"pc": 0}):
            self.assertIsNone(pc_evidence(self.transcript(self.pc(**changes))))
        self.assertIsNone(pc_evidence(self.transcript(self.pc()) * 2))

    def test_diagnostic_never_alone_accepts_a_signal(self):
        self.assertEqual(negative_classification(-4, True), ("UNEXPECTED_SIGNAL", False))
        self.assertEqual(negative_classification(-11, True), ("UNEXPECTED_SIGNAL", False))
        self.assertEqual(negative_classification(0, True), ("UNEXPECTED_SUCCESS", False))
        self.assertEqual(negative_classification(1, True), ("EXPECT_COMPILE_DIAGNOSTIC_EXIT", True))
        self.assertEqual(negative_classification(-4, False, {"passed": True, "pc": self.pc()}),
                         ("MISSING_REQUIRED_DIAGNOSTIC", False))
        self.assertEqual(negative_classification(-4, True, {"passed": True, "pc": self.pc()}),
                         ("EXPECT_COMPILE_FATAL_TRAP", True))
        self.assertFalse(negative_classification(-4, True, {"passed": True, "pc": self.pc(instruction="nop")})[1])

    def test_auto_tier_proves_real_small_loop_native_invocation(self):
        compiled = ("[llvm-jit-lazy] tiered-demand-request module=\"x\" fn=0 lane=inline\n"
                    "[llvm-jit-lazy] compile-end module=\"x\" fn=0 state=compiled\n")
        self.assertFalse(tiered_evidence(compiled)["tiered_phase_witnessed"])
        native = tiered_evidence(compiled + "[tiered-lazy] summary tiered_switches=1\n")
        self.assertEqual(native["tiered_phases"], ["T1"])
        self.assertTrue(native["tiered_phase_witnessed"])
        reversed_log = ("[llvm-jit-lazy] compile-end module=\"x\" fn=0 state=compiled\n"
                        "[llvm-jit-lazy] tiered-demand-request module=\"x\" fn=0\n"
                        "[tiered-lazy] summary tiered_switches=1\n")
        self.assertFalse(tiered_evidence(reversed_log)["tiered_phase_witnessed"])

    def test_forced_t1_rejects_an_actual_interpreter_fallback(self):
        interpreter = ("[uwvm-int-lazy] demand-request module=\"x\" fn=0\n"
                       "[uwvm-int-lazy] compile-end module=\"x\" fn=0 state=compiled\n")
        native = ("[llvm-jit-lazy] demand-request module=\"x\" fn=0\n"
                  "[llvm-jit-lazy] compile-end module=\"x\" fn=0 state=compiled\n"
                  "[tiered-lazy] summary tiered_switches=2\n")
        self.assertEqual(tiered_evidence(interpreter)["tiered_phases"], ["T0"])
        self.assertTrue(tiered_evidence(native, forced_t1=True)["tiered_phase_witnessed"])
        self.assertFalse(tiered_evidence(interpreter + native, forced_t1=True)["tiered_phase_witnessed"])

    def test_product_host_probe_flags_are_not_optional(self):
        command = ["clang++", "-O3", "-c", "input.cpp", "-o", "output.o"]
        self.assertEqual(apply_host_stack_probes(command),
                         ["-fstack-clash-protection", "-mstack-probe-size=4096"])
        self.assertEqual(apply_host_stack_probes(command), [])
        for unsafe in (["clang++", "-O3", "-fno-stack-clash-protection"],
                       ["clang++", "-O3", "-mstack-probe-size=8192"],
                       ["clang++", "-O3", "--target=riscv64-linux-gnu"],
                       ["clang++", "-O1"]):
            with self.assertRaises(RuntimeError):
                apply_host_stack_probes(unsafe)


if __name__ == "__main__":
    unittest.main()
