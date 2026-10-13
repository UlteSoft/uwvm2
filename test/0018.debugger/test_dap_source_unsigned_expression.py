#!/usr/bin/env python3
"""Unsigned-int copied display controls; real C/C++ stops use the tuple runner."""
import unittest
import test_dap_source_objects as source


class UnsignedExpressionTests(unittest.TestCase):
    def setUp(self):
        self.case = source.SourceObjectTests()
        self.case.setUp()

    def evaluate(self, value, *, size=4, kind='unsigned int', label='$expression', typed=True):
        return self.case.evaluate(f'{label}: type={kind}, offset=0, bytes={size}, value={value}', typed=typed)[1]

    def test_unsigned_range_and_explicit_type_are_copied_readonly(self):
        for value in ('0', '33', '2147483648', '4294967295'):
            with self.subTest(value=value):
                reply = self.evaluate(value)
                self.assertTrue(reply['success'], reply)
                self.assertEqual(reply['body'], dict(result=value, type='unsigned int', variablesReference=0,
                                                    presentationHint={'attributes': ['readOnly']}))
                self.assertFalse(self.case.adapter.source_object_views)
                self.assertFalse(any(c == 'continue' or c.startswith(('read ', 'set ', 'replace '))
                                     for c in self.case.broker.commands))

    def test_type_capability_does_not_change_unsigned_value(self):
        reply = self.evaluate('4294967295', typed=False)
        self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['result'], '4294967295')
        self.assertNotIn('type', reply['body'])
        self.assertEqual(reply['body']['variablesReference'], 0)

    def test_invalid_width_range_or_decimal_refuses_the_whole_packet(self):
        for size, value in ((0, '1'), (1, '1'), (2, '1'), (3, '1'), (8, '1'),
                            (4, '-1'), (4, '4294967296'), (4, '01'), (4, '-0'), (4, '+1')):
            with self.subTest(size=size, value=value):
                reply = self.evaluate(value, size=size)
                self.assertFalse(reply['success'], reply)
                self.assertNotIn('body', reply)
                self.assertFalse(self.case.adapter.source_object_views)
                self.assertFalse(self.case.adapter.source_frame_ordinals)

    def test_expression_type_cannot_relabel_a_go_builtin(self):
        for label in ('$len', '$cap'):
            reply = self.evaluate('1', label=label)
            self.assertFalse(reply['success'], reply)
            self.assertNotIn('body', reply)

    def test_unknown_type_names_keep_the_complete_opaque_display(self):
        for kind in ('UIntAlias', 'unsigned int const', 'unsigned long const', 'unsigned int*'):
            reply = self.evaluate('33', kind=kind)
            self.assertTrue(reply['success'], reply)
            self.assertIn(f'type={kind}, offset=0, bytes=4, value=33', reply['body']['result'])
            self.assertEqual(reply['body']['variablesReference'], 0)
            self.assertNotIn('type', reply['body'])


if __name__ == '__main__':
    unittest.main()
