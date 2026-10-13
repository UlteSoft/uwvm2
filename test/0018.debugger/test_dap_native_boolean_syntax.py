#!/usr/bin/env python3
"""Bounded Zig keyword syntax; language authority remains in the backend."""
import unittest
import test_dap_source_objects as source

dap = source.dap
class NativeBooleanSyntaxTests(unittest.TestCase):
    def test_keyword_boundaries_and_nested_lazy_syntax(self):
        for expression in ('true and false', 'false or true', '(a > b) and flag',
                           'flag and !false or false', 'false and (1 / 0 > 0)',
                           'true or missing', 'android == ordinary',
                           'android and ordinary', 'ordinary or android',
                           'object.android == object.ordinary'):
            with self.subTest(expression=expression):
                self.assertEqual(dap.validate_source_evaluation_expression(expression), expression)
    def test_keywords_do_not_authorize_calls_assignment_or_unbounded_input(self):
        for expression in ('true and f()', 'flag or value = 1', 'false or true;continue',
                           'true android false', 'false ordinary true',
                           'true and', 'or true', '(' * 40 + 'true' + ')' * 40):
            with self.subTest(expression=expression):
                with self.assertRaises(ValueError):
                    dap.validate_source_evaluation_expression(expression)
if __name__ == '__main__': unittest.main()
