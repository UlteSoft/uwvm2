"""Rust suffix admission is finite syntax DATA; actual CU remains VM authority."""
import json
from pathlib import Path
import unittest
from test_dap_integer_display import dap
class Tests(unittest.TestCase):
 def test_numeric_suffixes(self):
  cases=json.loads(Path(__file__).with_name('dap_rust_numeric_cases.json').read_text())
  for expression,_,_ in cases['valid']:
   with self.subTest(expression=expression):
    self.assertEqual(dap.validate_source_evaluation_expression(expression),expression)
 def test_no_command_or_identifier_tail(self):
  for expression in ('1u8junk','1u8;continue','1u128','1.0u8','1u8_u16','1f32\\ncontinue','0Xffu8','0b2u8'):
   with self.subTest(expression=expression),self.assertRaises(ValueError):
    dap.validate_source_evaluation_expression(expression)
if __name__=='__main__':unittest.main()
