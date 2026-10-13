#!/usr/bin/env python3
"""Detached packet DATA controls; real compiler qualification is a separate runner."""
import unittest
import test_dap_source_objects as objects
dap=objects.dap

class EnumDisplayTests(unittest.TestCase):
 def setUp(self):
  self.fixture=objects.SourceObjectTests()
  self.fixture.setUp()
 def evaluate(self,body,typed=True):return self.fixture.evaluate(body,typed)[1]
 def test_named_enum_preserves_the_complete_parent_tree(self):
  body=objects.BODY.replace('value=3','value=3 (COUNT)')
  result=self.evaluate(body);self.assertTrue(result['success'],result)
  self.assertEqual(result['body']['result'],'Holder')
  children=self.fixture.children(result['body']['variablesReference'])
  self.assertEqual(children[0]['value'],'3 (COUNT)')
  self.assertEqual(children[0]['variablesReference'],0)
  self.assertEqual(children[0]['presentationHint']['attributes'],['readOnly'])
  self.assertNotIn('evaluateName',children[0]);self.assertNotIn('memoryReference',children[0])
  self.assertGreater(children[1]['variablesReference'],0)
 def test_signed_unsigned_and_all_finite_integer_widths(self):
  for size in (1,2,4,8):
   for n in (-(1<<(size*8-1)),0,(1<<(size*8))-1):
    with self.subTest(size=size,value=n):
     body=f'object: type=EnumAlias, offset=0, bytes={size}, value={n} (Name)'
     result=self.evaluate(body);self.assertTrue(result['success'],result)
     self.assertEqual(result['body']['result'],f'{n} (Name)')
     self.assertEqual(result['body']['variablesReference'],0)
     self.assertEqual(result['body']['type'],'EnumAlias')
 def test_complete_name_escapes_are_display_only_even_with_command_spelling(self):
  label=r'Variant;continue\x1b\"\\\xce\xbb'
  body=objects.BODY.replace('value=3','value=3 ('+label+')')
  result=self.evaluate(body);self.assertTrue(result['success'],result)
  first=self.fixture.children(result['body']['variablesReference'])[0]
  self.assertEqual(first['value'],'3 ('+label+')')
  self.assertEqual(sum(c.startswith('print-frame ') for c in self.fixture.broker.commands),1)
  self.assertFalse(any(c=='continue' or c.startswith('read ') for c in self.fixture.broker.commands))
 def test_original_byte_limit_with_expanded_ascii_name(self):
  label=r'\xce'*4096
  result=self.evaluate('object: type=Enum, offset=0, bytes=4, value=3 ('+label+')')
  self.assertTrue(result['success'],result)
  self.assertEqual(result['body']['result'],'3 ('+label+')')
  bad='object: type=Enum, offset=0, bytes=4, value=3 ('+label+r'\xce'+')'
  result=self.evaluate(bad);self.assertFalse(result['success']);self.assertNotIn('body',result)
 def test_invalid_canonical_label_and_enum_extent_publish_no_prefix(self):
  for value in (r'3 (bad\x1B)',r'3 (bad\x41)',r'3 (bad\q)','3 (λ)','3 (bad"quote)',r'3 (bad\x0)','guest:0x20 (Name)'):
   with self.subTest(value=value):
    result=self.evaluate(objects.BODY.replace('value=3','value='+value))
    self.assertFalse(result['success'],result);self.assertNotIn('body',result)
    self.assertFalse(self.fixture.adapter.source_object_views)
  for size,n in ((0,0),(3,3),(1,-129),(1,256),(8,-9223372036854775809),(8,18446744073709551616)):
   result=self.evaluate(f'object: type=Enum, offset=0, bytes={size}, value={n} (Name)')
   self.assertFalse(result['success'],result);self.assertNotIn('body',result)
 def test_unknown_or_truncated_annotation_keeps_whole_opaque_packet(self):
  for value in ('3 ()','01 (Name)','-0 (Name)','true (Name)','3 (Name), omitted-elements=1',
                '3 ('+'a'*4096+'... (truncated))'):
   body=objects.BODY.replace('value=3','value='+value)
   result=self.evaluate(body);self.assertTrue(result['success'],result)
   self.assertEqual(result['body']['result'],objects.packet(body).rstrip('\n'))
   self.assertEqual(result['body']['variablesReference'],0)
 def test_type_capability_and_pagination(self):
  result=self.evaluate(objects.BODY.replace('value=3','value=-1 (Negative)'),typed=False)
  self.assertTrue(result['success'],result);self.assertNotIn('type',result['body'])
  page=self.fixture.children(result['body']['variablesReference'],start=0,count=1,filter='named')
  self.assertEqual(len(page),1);self.assertEqual(page[0]['value'],'-1 (Negative)')
  self.assertNotIn('type',page[0])
 def test_real_stop_change_retires_enum_aggregate(self):
  result=self.evaluate(objects.BODY.replace('value=3','value=3 (COUNT)'))
  ref=result['body']['variablesReference'];self.fixture.broker.stop+=1
  response=self.fixture.send('variables',variablesReference=ref)
  self.assertFalse(response['success']);self.assertFalse(self.fixture.adapter.source_object_views)
 def test_scalar_width_without_annotation_keeps_older_float_compatibility(self):
  result=self.evaluate('object: type=FloatAlias, offset=0, bytes=4, value=4294967296')
  self.assertTrue(result['success'],result)
  self.assertEqual(result['body']['variablesReference'],0)
  self.assertEqual(result['body']['result'],objects.packet('object: type=FloatAlias, offset=0, bytes=4, value=4294967296').rstrip('\n'))

if __name__=='__main__':unittest.main()
