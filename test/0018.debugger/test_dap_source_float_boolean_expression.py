#!/usr/bin/env python3
"""Authenticated copied float/bool display only; actual bits use the live runner."""
import unittest
import test_dap_source_objects as source


class FloatBooleanExpressionTests(unittest.TestCase):
    def setUp(self):
        self.case = source.SourceObjectTests()
        self.case.setUp()

    def evaluate(self, kind, size, value, *, typed=True, label='$expression'):
        return self.case.evaluate(f'{label}: type={kind}, offset=0, bytes={size}, value={value}', typed=typed)[1]

    def check_display(self, kind, size, value):
        reply = self.evaluate(kind, size, value)
        self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body'], dict(result=value, type=kind, variablesReference=0,
                                            presentationHint={'attributes': ['readOnly']}))
        self.assertFalse(self.case.adapter.source_object_views)
        self.assertFalse(any(c == 'continue' or c.startswith(('read ', 'set ', 'replace '))
                             for c in self.case.broker.commands))

    def test_boolean_truth_is_copied_without_queries_or_expansion(self):
        for value in ('true', 'false'):
            self.check_display('bool', 1, value)

    def test_float_finite_width_rounding_and_signed_zero_spelling(self):
        cases = ((4, ('0', '-0', '1.25', '-1.25', '3.4028235e+38', '-3.4028235e38',
                      '1.1754944e-38', '1e-45', '-1e-45')),
                 (8, ('0', '-0', '1.25', '-1.25', '1.7976931348623157e308',
                      '-1.7976931348623157E+308', '2.2250738585072014e-308', '5e-324', '-5e-324')))
        for size, values in cases:
            for value in values:
                with self.subTest(size=size, value=value):
                    self.check_display('float' if size == 4 else 'double', size, value)

    def test_ieee_special_spelling_is_preserved_without_nan_payload_inference(self):
        for kind, size in (('float', 4), ('double', 8)):
            for value in ('inf', '-inf', 'nan', '-nan'):
                self.check_display(kind, size, value)

    def test_type_capability_only_controls_the_type_field(self):
        for kind, size, value in (('bool', 1, 'false'), ('float', 4, '-0'), ('double', 8, '5e-324')):
            reply = self.evaluate(kind, size, value, typed=False)
            self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['result'], value)
            self.assertNotIn('type', reply['body'])
            self.assertEqual(reply['body']['variablesReference'], 0)

    def test_wrong_width_or_go_builtin_label_refuses_the_whole_packet(self):
        for kind, size, value in (('bool', 1, 'true'), ('float', 4, '1.25'), ('double', 8, '1.25')):
            for bad_size in {0, 1, 2, 3, 4, 8, 16} - {size}:
                reply = self.evaluate(kind, bad_size, value)
                self.assertFalse(reply['success'], reply)
                self.assertNotIn('body', reply)
            for label in ('$len', '$cap'):
                reply = self.evaluate(kind, size, value, label=label)
                self.assertFalse(reply['success'], reply)
                self.assertNotIn('body', reply)

    def test_malformed_truth_and_decimal_or_nonzero_underflow_refuses_and_retires(self):
        bad = [('bool', 1, value) for value in ('0', '1', 'True', 'FALSE', 'true, memory=0x1')]
        for kind, size in (('float', 4), ('double', 8)):
            bad += [(kind, size, value) for value in ('01', '+1', '.5', '1.', ' 1', '1 ', '0x1',
                    'NaN', '+inf', 'nan(payload)', '1e10000', '1e9999', '1e-9999', '9' * 65,
                    '1, memory=0x1', '1\nsource-value end')]
        bad += [('float', 4, '3.4028236e38'), ('float', 4, '1e-46'),
                ('double', 8, '1.7976931348623159e308'), ('double', 8, '1e-324')]
        for kind, size, value in bad:
            with self.subTest(kind=kind, size=size, value=value):
                reply = self.evaluate(kind, size, value)
                self.assertFalse(reply['success'], reply)
                self.assertNotIn('body', reply)
                self.assertFalse(self.case.adapter.source_object_views)
                self.assertFalse(self.case.adapter.source_frame_ordinals)

    def test_unknown_typedef_qualifier_pointer_or_plain_char_remains_opaque(self):
        for kind in ('FloatAlias', 'float const', 'double volatile', 'bool const', 'float*', 'char'):
            reply = self.evaluate(kind, 4, '1.25')
            self.assertTrue(reply['success'], reply)
            self.assertIn(f'type={kind}, offset=0, bytes=4, value=1.25', reply['body']['result'])
            self.assertNotIn('type', reply['body'])
            self.assertEqual(reply['body']['variablesReference'], 0)


if __name__ == '__main__':
    unittest.main()
