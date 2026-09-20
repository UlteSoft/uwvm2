#!/usr/bin/env python3
"""Selection regressions for actual-header and named-module opfunc spellings."""
import unittest
from check_existing_int_add_codegen import inspect, select


class SymbolSelectionTests(unittest.TestCase):
    def test_header_and_module_spellings(self):
        # Module annotations follow both the function name and the enum name
        # in real Clang output (checked with a separately compiled tiny module).
        cases = [
            ("uwvmint_f32_binop", "float_binop", 0, "f32-add"),
            ("uwvmint_f64_binop", "float_binop", 0, "f64-add"),
            ("uwvmint_simd_v128_binop", "v128_binop", 8, "f32x4-add"),
            ("uwvmint_simd_full_binop", "op_simd", 228, "f32x4-add"),
            ("uwvmint_simd_full_binop", "op_simd", 240, "f64x2-add"),
        ]
        for annotation in ("", "@uwvm_int_symbol_probe", "@uwvm2.test:partition"):
            for function, enum, value, group in cases:
                with self.subTest(annotation=annotation, function=function, value=value):
                    symbol = f"void ns::{function}{annotation}<(ns::{enum}{annotation}){value}, 0>()"
                    self.assertEqual(select(symbol)[0], group)

    def test_non_add_and_unrelated_symbols_are_not_selected(self):
        for symbol in (
            "void ns::uwvmint_f32_binop<(ns::float_binop)1, 0>()",
            "void ns::uwvmint_simd_full_binop@test<(ns::op_simd@test)229, 0>()",
            "void ns::unrelated<(ns::float_binop)0, 0>()",
            "initializer for module test",
        ):
            with self.subTest(symbol=symbol):
                self.assertIsNone(select(symbol))


class InstructionInspectionTests(unittest.TestCase):
    def test_native_add_and_indirect_dispatch(self):
        body = [("vaddps", "%xmm0, %xmm1, %xmm1"), ("jmpq", "*0x8(%rax)")]
        self.assertTrue(inspect(body, {"vaddps"})["passed"])

    def test_implicit_stack_and_false_tail_dispatch_are_rejected(self):
        body = [("vaddps", "%xmm0, %xmm1, %xmm1"), ("jmpq", "*0x8(%rax)")]
        for op in ("pushq", "popq", "retq", "enter", "leave"):
            with self.subTest(op=op):
                report = inspect([(op, "%rax")] + body, {"vaddps"})
                self.assertFalse(report["passed"])
                self.assertEqual(report["implicit_stack_ops"], [op])
        self.assertFalse(inspect(body[:1] + [("jmp", "0x1234")], {"vaddps"})["passed"])
        self.assertFalse(inspect([], {"vaddps"})["passed"])
        self.assertFalse(inspect([("callq", "helper")] + body, {"vaddps"})["passed"])
        self.assertFalse(inspect([("movq", "(%rsp), %rax")] + body, {"vaddps"})["passed"])


if __name__ == "__main__":
    unittest.main()
