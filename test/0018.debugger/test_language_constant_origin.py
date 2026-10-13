#!/usr/bin/env python3
"""Finite official-text/receipt join tests; never a real producer/product PASS."""
import importlib.util
from pathlib import Path
import sys
import unittest
RUNTIME = Path(__file__).resolve().parents[1] / '0017.runtime'
sys.path.insert(0, str(RUNTIME))
spec = importlib.util.spec_from_file_location('language_experience_origin', RUNTIME / 'run_debug_language_experience_cli.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

ORACLE = '''.debug_info contents:
0x00000000: Compile Unit: length = 0x00000060, version = 0x0005, unit_type = DW_UT_compile
0x0000000c: DW_TAG_compile_unit
              DW_AT_name [DW_FORM_string] ("fixture.c")
0x00000010:   DW_TAG_subprogram
                DW_AT_name [DW_FORM_string] ("leaf")
0x00000020:     DW_TAG_variable
                  DW_AT_name [DW_FORM_string] ("leaf_positive")
                  DW_AT_type [DW_FORM_ref4] (0x00000050 "int")
                  DW_AT_const_value [DW_FORM_udata] (23)
0x00000030:     DW_TAG_lexical_block
0x00000040:       DW_TAG_variable
                    DW_AT_name [DW_FORM_string] ("leaf_positive")
                    DW_AT_type [DW_FORM_ref4] (0x00000050 "int")
                    DW_AT_const_value [DW_FORM_udata] (99)
0x00000048:       NULL
0x00000049:     NULL
0x00000050:   DW_TAG_base_type
                DW_AT_name [DW_FORM_string] ("int")
                DW_AT_encoding [DW_FORM_data1] (DW_ATE_signed)
                DW_AT_byte_size [DW_FORM_data1] (4)
0x00000060:   NULL
'''
ORIGIN = (b'source-stop 9\nsource local leaf_positive type=int = i32=23\n'
          b'source-origin stop=9 thread=1 code-offset=12 variable-unit=0 variable-offset=32 '
          b'scope-unit=0 scope-offset=16 type-unit=0 type-offset=80 kind=DW_AT_const_value\n')
POSITION = {'stop_id': 9, 'code_offset': 12}

class OriginJoin(unittest.TestCase):
    def join(self, reply=ORIGIN, oracle=ORACLE, position=POSITION):
        return module.direct_constant_receipt(oracle, reply, position, 1, 'leaf_positive')
    def test_exact_direct_concrete_identity(self):
        result = self.join()
        self.assertEqual(result['variable'], [0, 32])
        self.assertEqual(result['scope'], [0, 16])
        self.assertEqual(result['type'], [0, 80])
    def test_inventory_does_not_qualify_copy(self):
        self.assertEqual(module.scalar_constant_names(ORACLE), ('leaf_positive',))
        self.assertIsNone(self.join(reply=ORIGIN.split(b'source-origin')[0]))
    def test_wrong_scope_same_name_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(reply=ORIGIN.replace(b'variable-offset=32', b'variable-offset=64'))
    def test_wrong_type_identity_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(reply=ORIGIN.replace(b'type-offset=80', b'type-offset=16'))
    def test_different_real_stop_same_pc_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(position={'stop_id': 10, 'code_offset': 12})
    def test_different_real_code_pc_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(position={'stop_id': 9, 'code_offset': 13})
    def test_explicit_empty_location_conflict_rejected(self):
        oracle = ORACLE.replace('                  DW_AT_const_value [DW_FORM_udata] (23)',
                                '                  DW_AT_const_value [DW_FORM_udata] (23)\n                  DW_AT_location [DW_FORM_sec_offset] (0x00000000)')
        with self.assertRaises(AssertionError):
            self.join(oracle=oracle)
    def test_duplicate_attribute_dump_rejected(self):
        oracle = ORACLE.replace('                  DW_AT_const_value [DW_FORM_udata] (23)',
                                '                  DW_AT_const_value [DW_FORM_udata] (23)\n                  DW_AT_const_value [DW_FORM_udata] (99)')
        with self.assertRaises(AssertionError):
            self.join(oracle=oracle)
    def test_receipt_without_direct_attribute_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(oracle=ORACLE.replace('                  DW_AT_const_value [DW_FORM_udata] (23)\n', ''))
    def test_duplicate_receipt_rejected(self):
        with self.assertRaises(AssertionError):
            self.join(reply=ORIGIN + ORIGIN.splitlines(keepends=True)[-1])

if __name__ == '__main__':
    unittest.main()
