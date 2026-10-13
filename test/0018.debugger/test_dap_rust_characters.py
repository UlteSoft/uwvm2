"""Bounded Rust escape syntax and detached display envelopes; CU authority stays in VM."""
import unittest
from test_dap_integer_display import dap
class Tests(unittest.TestCase):
 def test_rust_unicode_syntax(self):
  for text in ("'a'", r"'\u{3bb}'", r"'\u{1f642}' as u32", r"'\u{10_ffff__}'",
               r"leaf_point == '\u{1f642}'", r"(255 as u8) as char", r"'\u{0}'", r"'\u{000000}'"):
   self.assertEqual(dap.validate_source_evaluation_expression(text),text)
  for text in (r"'\u{}'",r"'\u{_1}'",r"'\u{0000000}'",r"'\u{110000}'",r"'\u{d800}'",
               r"'\u{z}'",r"'\u{41}'junk",r"'\u{41}' ;continue",r"'\u{41}'\ncontinue"):
   for prefix in ("","false && ","true ? 1 : "):
    with self.subTest(text=prefix+text),self.assertRaises(ValueError):
     dap.validate_source_evaluation_expression(prefix+text)
 def test_rust_char_envelope(self):
  for value in (0,65,955,128578,0x10ffff):
   text=f"source-value stop=41 name=x\n$expression: type=Rust char, offset=0, bytes=4, value={value}\nsource-value end\n"
   self.assertEqual(dap.source_evaluation_display(text)['type'],'char')
   self.assertEqual(dap.source_evaluation_display(text)['result'],str(value))
  for size,value in ((1,65),(8,65),(4,-1),(4,0xd800),(4,0x110000),(4,0xffffffff)):
   text=f"source-value stop=41 name=x\n$expression: type=Rust char, offset=0, bytes={size}, value={value}\nsource-value end\n"
   with self.assertRaises(ValueError):dap.source_evaluation_display(text)
if __name__=="__main__":unittest.main()
