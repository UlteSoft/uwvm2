"""Finite UTF C++ syntax and detached canonical result envelopes only."""
import unittest
from test_dap_integer_display import dap

class Tests(unittest.TestCase):
 def test_utf_single_code_unit_syntax(self):
  for text in ("u8'a'", "u8'\\xff'", "u'\\u03bb'", "u'\\xD800'", "U'\\U0001F642'",
               "U'\\xffffffff'", "sizeof(u'\\u03bb')", "1 ? u'a' : u'\\u03bb'",
               "static_cast<char16_t>(955)", "char32_t(128578)", "sizeof(U')' + U'(')"):
   self.assertEqual(dap.validate_source_evaluation_expression(text),text)
  for text in ("u8'\\u0080'","u'\\U0001F642'","U'\\U00110000'","u'\\uD800'",
               "u8'\\x100'","u'\\x10000'","U'\\x100000000'","u'ab'","L'a'","u'\\u03b'"):
   for prefix in ("","0 && ","1 ? 7 : "):
    with self.subTest(text=prefix+text),self.assertRaises(ValueError):
     dap.validate_source_evaluation_expression(prefix+text)

 def test_utf_display_requires_actual_width_and_unsigned_range(self):
  for kind,size in (("char8_t",1),("char16_t",2),("char32_t",4)):
   for n in (0,127,(1<<(size*8))-1):
    text=f"source-value stop=41 name=x\n$expression: type={kind}, offset=0, bytes={size}, value={n}\nsource-value end\n"
    self.assertEqual(dap.source_evaluation_display(text),dict(result=str(n),type=kind,variablesReference=0,presentationHint=dict(attributes=['readOnly'])))
   for wrong_size,n in ((8,1),(size,-1),(size,1<<(size*8))):
    text=f"source-value stop=41 name=x\n$expression: type={kind}, offset=0, bytes={wrong_size}, value={n}\nsource-value end\n"
    with self.assertRaises(ValueError):dap.source_evaluation_display(text)

if __name__=="__main__":unittest.main()
