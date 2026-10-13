#!/usr/bin/env python3
"""Detached canonical copied-text DATA; real TinyGo evidence uses Linux runner."""
import unittest
import test_dap_source_objects as objects
dap=objects.dap
BODY=('object: type=string, offset=0, bytes=8, text=a\\xce\\xbb\\xf0\\x9f\\x99\\x82\n'
      '  ptr: type=pointer, offset=0, bytes=4, value=guest:0x20\n'
      '  len: type=uintptr, offset=4, bytes=4, value=7')
class SourceTextTests(unittest.TestCase):
 send=objects.SourceObjectTests.send
 evaluate=objects.SourceObjectTests.evaluate
 children=objects.SourceObjectTests.children
 def setUp(self):objects.SourceObjectTests.setUp(self)
 def test_canonical_unicode_byte_display_and_copied_carriers(self):
  _,reply=self.evaluate(BODY);body=reply['body']
  self.assertEqual(body['result'],'"a\\xce\\xbb\\xf0\\x9f\\x99\\x82"');self.assertEqual(body['type'],'string')
  self.assertGreater(body['variablesReference'],0);rows=self.children(body['variablesReference'])
  self.assertEqual([r['value'] for r in rows],['guest:0x20','7'])
  for r in [body,*rows]:
   self.assertEqual(r['presentationHint']['attributes'],['readOnly']);self.assertNotIn('memoryReference',r);self.assertNotIn('evaluateName',r)
  self.children(body['variablesReference']);self.assertEqual(sum(c.startswith('print-frame ') for c in self.broker.commands),1)
 def test_empty_text_and_client_type_capability(self):
  _,reply=self.evaluate(BODY.replace('text=a\\xce\\xbb\\xf0\\x9f\\x99\\x82','text='),typed=False)
  self.assertEqual(reply['body']['result'],'""');self.assertNotIn('type',reply['body'])
  self.assertTrue(all('type' not in r for r in self.children(reply['body']['variablesReference'])))
 def test_text_delimiters_and_command_spelling_are_only_display(self):
  text='x: type=Y, offset=999, bytes=888, text=continue; read 0, value=guest:0x1'
  _,reply=self.evaluate(BODY.replace('a\\xce\\xbb\\xf0\\x9f\\x99\\x82',text))
  self.assertTrue(reply['success'],reply);self.assertEqual(reply['body']['result'],'"'+text+'"')
  self.children(reply['body']['variablesReference'])
  self.assertFalse(any(c=='continue' or c.startswith('read ') for c in self.broker.commands))
 def test_pointer_text_remains_a_display_leaf(self):
  _,reply=self.evaluate('object: type=pointer, offset=0, bytes=4, value=guest:0x20, text=hi\\\\\\"\\x00')
  self.assertEqual(reply['body']['result'],'guest:0x20 "hi\\\\\\"\\x00"');self.assertEqual(reply['body']['variablesReference'],0)
  self.assertEqual(len(self.adapter.source_pointer_views),0);self.assertEqual(sum(c.startswith('print-frame ') for c in self.broker.commands),1)
 def test_truncated_or_ambiguous_text_is_complete_opaque(self):
  for text in ('hello (truncated)','a'*4096+' (truncated)','a'*4096+'... (truncated)','hello, omitted-elements=2'):
   self.setUp();body=BODY.replace('a\\xce\\xbb\\xf0\\x9f\\x99\\x82',text)
   _,reply=self.evaluate(body);self.assertTrue(reply['success'],reply)
   self.assertEqual(reply['body']['result'],objects.packet(body).rstrip('\n'));self.assertEqual(reply['body']['variablesReference'],0)
   self.assertFalse(self.adapter.source_object_views)
 def test_bad_escapes_bytes_and_extents_do_not_publish_prefix(self):
  for text in ('a\\x1B','a\\x41','a\\q','a\\','a"','a\x1b','aλ','a'*4097):
   self.setUp();_,reply=self.evaluate(BODY.replace('a\\xce\\xbb\\xf0\\x9f\\x99\\x82',text))
   self.assertFalse(reply['success'],reply);self.assertFalse(self.adapter.source_object_views)
  self.setUp();_,reply=self.evaluate(BODY.replace('offset=4, bytes=4','offset=7, bytes=4'))
  self.assertFalse(reply['success'],reply);self.assertFalse(self.adapter.source_object_views)
 def test_original_byte_budget_not_escaped_width(self):
  _,reply=self.evaluate('object: type=NamedString, offset=0, bytes=8, text='+'\\x01'*4096)
  self.assertTrue(reply['success'],reply);self.assertEqual(reply['body']['result'],'"'+'\\x01'*4096+'"')
  self.setUp();_,reply=self.evaluate('object: type=NamedString, offset=0, bytes=8, text='+'\\x01'*4097)
  self.assertFalse(reply['success'],reply)
 def test_text_views_retire_on_actual_status_change(self):
  _,reply=self.evaluate(BODY);ref=reply['body']['variablesReference'];before=len(self.broker.commands)
  self.broker.stop+=1;self.assertFalse(self.send('variables',variablesReference=ref)['success'])
  self.assertFalse(self.adapter.source_object_views);self.assertFalse(any(c.startswith('print-frame ') for c in self.broker.commands[before:]))
if __name__=='__main__':unittest.main()
