#!/usr/bin/env python3
"""Finite canonical copied integer results, not arbitrary source type names."""
import unittest
import test_dap_source_objects as source


class IntegerExpressionTests(unittest.TestCase):
    TYPES = (('signed char', 1, False), ('unsigned char', 1, True),
             ('short', 2, False), ('unsigned short', 2, True),
             ('long', 4, False), ('long', 8, False),
             ('unsigned long', 4, True), ('unsigned long', 8, True),
             ('long long', 8, False), ('unsigned long long', 8, True))

    def setUp(self):
        self.case = source.SourceObjectTests()
        self.case.setUp()

    def evaluate(self, kind, size, value, *, typed=True, label='$expression'):
        return self.case.evaluate(f'{label}: type={kind}, offset=0, bytes={size}, value={value}', typed=typed)[1]

    def test_canonical_minimum_zero_maximum_without_expansion_or_commands(self):
        for kind, size, unsigned in self.TYPES:
            bits = size * 8
            limits = (0, (1 << bits) - 1) if unsigned else (-(1 << (bits - 1)), (1 << (bits - 1)) - 1)
            for value in set((*limits, 0)):
                with self.subTest(kind=kind, size=size, value=value):
                    reply = self.evaluate(kind, size, value)
                    self.assertTrue(reply['success'], reply)
                    self.assertEqual(reply['body'], dict(result=str(value), type=kind, variablesReference=0,
                                                        presentationHint={'attributes': ['readOnly']}))
                    self.assertFalse(self.case.adapter.source_object_views)
                    self.assertFalse(any(c == 'continue' or c.startswith(('read ', 'set ', 'replace '))
                                         for c in self.case.broker.commands))

    def test_each_actual_width_rejects_both_overflow_directions(self):
        for kind, size, unsigned in self.TYPES:
            bits = size * 8
            low, high = (0, (1 << bits) - 1) if unsigned else (-(1 << (bits - 1)), (1 << (bits - 1)) - 1)
            for value in (low - 1, high + 1):
                with self.subTest(kind=kind, size=size, value=value):
                    reply = self.evaluate(kind, size, value)
                    self.assertFalse(reply['success'], reply)
                    self.assertNotIn('body', reply)
                    self.assertFalse(self.case.adapter.source_object_views)
                    self.assertFalse(self.case.adapter.source_frame_ordinals)

    def test_primitive_width_mismatch_refuses_the_whole_reply(self):
        for kind, _, _ in self.TYPES:
            allowed = {size for name, size, _ in self.TYPES if name == kind}
            for size in {0, 1, 2, 3, 4, 8, 16} - allowed:
                with self.subTest(kind=kind, size=size):
                    reply = self.evaluate(kind, size, '1')
                    self.assertFalse(reply['success'], reply)
                    self.assertNotIn('body', reply)

    def test_noncanonical_decimal_is_refused_for_every_primitive(self):
        for kind, size, _ in self.TYPES:
            for value in ('01', '-0', '+1', ' 1', '1 ', '0x1', '1.0', '1\nsource-value end', '9' * 21):
                with self.subTest(kind=kind, size=size, value=value):
                    reply = self.evaluate(kind, size, value)
                    self.assertFalse(reply['success'], reply)
                    self.assertNotIn('body', reply)

    def test_type_capability_does_not_change_the_copied_result(self):
        for kind, size, unsigned in self.TYPES:
            value = (1 << (size * 8)) - 1 if unsigned else -(1 << (size * 8 - 1))
            reply = self.evaluate(kind, size, value, typed=False)
            self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['result'], str(value))
            self.assertNotIn('type', reply['body'])
            self.assertEqual(reply['body']['variablesReference'], 0)

    def test_c_primitive_cannot_relabel_a_go_builtin(self):
        for kind, size, _ in self.TYPES:
            for label in ('$len', '$cap'):
                reply = self.evaluate(kind, size, '1', label=label)
                self.assertFalse(reply['success'], reply)
                self.assertNotIn('body', reply)

    def test_unknown_typedef_qualifier_pointer_and_char_stays_opaque(self):
        for kind in ('IntegerAlias', 'long const', 'long long*', 'unsigned short int',
                     'signed short', 'char', 'bool const', 'float const', 'double const', 'unsigned __int128'):
            reply = self.evaluate(kind, 4, '1')
            self.assertTrue(reply['success'], reply)
            self.assertIn(f'type={kind}, offset=0, bytes=4, value=1', reply['body']['result'])
            self.assertEqual(reply['body']['variablesReference'], 0)
            self.assertNotIn('type', reply['body'])


if __name__ == '__main__':
    unittest.main()
