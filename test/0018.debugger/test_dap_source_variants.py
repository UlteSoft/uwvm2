#!/usr/bin/env python3
"""Detached variant formatter DATA controls; actual Rust stops have a separate runner."""
import unittest
import test_dap_source_objects as objects

BODY = ('object: type=Packet, offset=0, bytes=16\n'
        '  seed: type=i32, offset=0, bytes=4, value=5\n'
        '  payload: type=Payload, offset=4, bytes=8\n'
        '    <variant-part>: type=unavailable, offset=4, bytes=8, variant-part\n'
        '      tag: type=i8, offset=4, bytes=1, value=-3\n'
        '      Negative: type=unavailable, offset=4, bytes=8, active-variant\n'
        '        signed: type=i32, offset=8, bytes=4, value=-3\n'
        '  option: type=Option, offset=12, bytes=4\n'
        '    <variant-part>: type=unavailable, offset=12, bytes=4, variant-part\n'
        '      Some: type=unavailable, offset=12, bytes=4, active-variant, default\n'
        '        __0: type=u32, offset=12, bytes=4, value=13')


class SourceVariantTests(unittest.TestCase):
    def control(self, body=BODY):
        c = objects.SourceObjectTests(); c.setUp()
        frame, reply = c.evaluate(body)
        return c, frame, reply

    def test_selected_payload_and_default_are_readonly_copied_views(self):
        c, _, reply = self.control(); self.assertTrue(reply['success'], reply)
        fields = c.children(reply['body']['variablesReference'])
        self.assertEqual([r['name'] for r in fields], ['seed', 'payload', 'option'])
        group = c.children(fields[1]['variablesReference'])[0]
        self.assertEqual(group['value'], 'variant-part')
        branches = c.children(group['variablesReference'])
        self.assertEqual([r['name'] for r in branches], ['tag', 'Negative'])
        self.assertEqual(branches[1]['value'], 'active variant')
        payload = c.children(branches[1]['variablesReference'])
        self.assertEqual(payload[0]['value'], '-3')
        niche = c.children(c.children(fields[2]['variablesReference'])[0]['variablesReference'])
        self.assertEqual(niche[0]['value'], 'active variant (default)')
        value = c.children(niche[0]['variablesReference'])
        self.assertEqual(value[0]['value'], '13')
        for row in [reply['body'], *fields, group, *branches, *payload, *niche, *value]:
            self.assertEqual(row['presentationHint']['attributes'], ['readOnly'])
            self.assertNotIn('evaluateName', row); self.assertNotIn('memoryReference', row)
        self.assertEqual(sum(x.startswith('print-frame ') for x in c.broker.commands), 1)

    def test_unavailable_selection_keeps_known_discriminant_and_siblings(self):
        body = BODY.split('      Negative:')[0].rstrip() + '\n  sibling: type=i32, offset=12, bytes=4, value=7'
        for reason in ('variant discriminant or selector is unavailable', 'variant selectors are ambiguous'):
            c, _, reply = self.control(body.replace(', variant-part', ', variant-part, unavailable=' + reason))
            self.assertTrue(reply['success'], reply)
            fields = c.children(reply['body']['variablesReference'])
            self.assertEqual(fields[-1]['value'], '7')
            group = c.children(fields[1]['variablesReference'])[0]
            self.assertIn('unavailable (' + reason + ')', group['value'])
            self.assertEqual([r['name'] for r in c.children(group['variablesReference'])], ['tag'])
        c, _, reply = self.control(BODY.replace(', active-variant\n', ', active-variant, unavailable=nested variant layout is unavailable\n'))
        self.assertTrue(reply['success'], reply)
        fields = c.children(reply['body']['variablesReference'])
        branch = c.children(c.children(fields[1]['variablesReference'])[0]['variablesReference'])[1]
        self.assertIn('nested variant layout is unavailable', branch['value'])
        self.assertEqual(c.children(branch['variablesReference'])[0]['value'], '-3')

    def test_inconsistent_active_branch_never_publishes_a_prefix(self):
        cases = [BODY.replace('      Negative:', '    Negative:'),
                 BODY.replace('Negative: type=unavailable, offset=4, bytes=8', 'Negative: type=unavailable, offset=4, bytes=7'),
                 BODY.replace('bytes=8, variant-part', 'bytes=7, variant-part'),
                 BODY.replace(', variant-part\n', ', variant-part, unavailable=variant selectors are ambiguous\n'),
                 BODY.replace('  option:', '      Other: type=unavailable, offset=4, bytes=8, active-variant\n  option:')]
        for body in cases:
            with self.subTest(body=body):
                c, _, reply = self.control(body)
                self.assertFalse(reply['success'], reply); self.assertNotIn('body', reply)
                self.assertFalse(c.adapter.source_object_views)

    def test_unknown_or_type_only_variant_formats_keep_whole_packet_opaque(self):
        for body in (BODY.replace(', active-variant', ', variant'),
                     BODY.replace(', variant-part', ', variant-part, default'),
                     BODY.replace(', active-variant\n', ', active-variant, value=7\n'),
                     BODY.replace(', variant-part', ', variant-part, unavailable=unknown extension'),
                     BODY.replace('type=unavailable', 'type=Other'),
                     BODY + '\nsource-object output-truncated shown=11 total=12'):
            c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['variablesReference'], 0)
            self.assertEqual(reply['body']['result'], objects.packet(body, c.broker.stop).rstrip('\n'))
            self.assertFalse(c.adapter.source_object_views)

    def test_variant_labels_do_not_grant_queries_and_stops_retire_views(self):
        c, _, reply = self.control(BODY.replace('Negative:', 'x;continue\\x1b:'))
        self.assertTrue(reply['success'], reply)
        ref = reply['body']['variablesReference']
        fields = c.children(ref)
        branch = c.children(c.children(fields[1]['variablesReference'])[0]['variablesReference'])[1]
        self.assertEqual(branch['name'], 'x;continue\\x1b')
        self.assertFalse(any(x == 'continue' or x.startswith('read ') for x in c.broker.commands))
        c.broker.stop += 1
        self.assertFalse(c.send('variables', variablesReference=branch['variablesReference'])['success'])
        self.assertFalse(c.adapter.source_object_views)

    def test_case_and_complete_copied_payload_summary_keeps_original_views(self):
        c, _, reply = self.control(); self.assertTrue(reply['success'], reply)
        fields = c.children(reply['body']['variablesReference'])
        self.assertEqual(fields[1]['value'], 'Negative { signed: -3 }')
        self.assertEqual(fields[2]['value'], 'Some { __0: 13 }')
        self.assertEqual(fields[1]['type'], 'Payload')
        self.assertGreater(fields[1]['variablesReference'], 0)
        self.assertEqual(sum(x.startswith('print-frame ') for x in c.broker.commands), 1)

    def test_same_name_wrapper_and_nested_owned_fields_are_display_only(self):
        body = BODY.replace('        signed: type=i32, offset=8, bytes=4, value=-3',
                            '        Negative: type=Negative, offset=4, bytes=8\n'
                            '          signed: type=i32, offset=8, bytes=4, value=-3')
        body = body.replace('        __0: type=u32, offset=12, bytes=4, value=13',
                            '        Some: type=Some, offset=12, bytes=4\n'
                            '          __0: type=NonZero, offset=12, bytes=4\n'
                            '            __0: type=Inner, offset=12, bytes=4\n'
                            '              __0: type=u32, offset=12, bytes=4, value=13')
        c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
        fields = c.children(reply['body']['variablesReference'])
        self.assertEqual(fields[1]['value'], 'Negative { signed: -3 }')
        self.assertEqual(fields[2]['value'], 'Some { __0.__0.__0: 13 }')
        branch = c.children(c.children(fields[1]['variablesReference'])[0]['variablesReference'])[1]
        wrapper = c.children(branch['variablesReference'])[0]
        self.assertEqual(wrapper['name'], 'Negative')
        self.assertEqual(c.children(wrapper['variablesReference'])[0]['value'], '-3')
        for row in [*fields, branch, wrapper]:
            self.assertNotIn('evaluateName', row); self.assertNotIn('memoryReference', row)
        self.assertEqual(sum(x.startswith('print-frame ') for x in c.broker.commands), 1)

    def test_empty_case_and_unavailable_values_do_not_invent_payload(self):
        body = ('option: type=Option, offset=0, bytes=4\n'
                '  <variant-part>: type=unavailable, offset=0, bytes=4, variant-part\n'
                '    None: type=unavailable, offset=0, bytes=4, active-variant\n'
                '      None: type=None, offset=0, bytes=0')
        c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['result'], 'None')
        self.assertGreater(reply['body']['variablesReference'], 0)
        for body, expected in ((BODY.replace('value=-3\n  option:', 'unavailable=unsupported type\n  option:'), 'Negative'),
                               (BODY.replace(', active-variant\n', ', active-variant, unavailable=nested variant layout is unavailable\n'), 'Payload')):
            c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
            fields = c.children(reply['body']['variablesReference'])
            self.assertEqual(fields[1]['value'], expected)

    def test_summary_budgets_keep_case_and_entire_payload_tree(self):
        prefix = ('object: type=Large, offset=0, bytes=4\n'
                  '  <variant-part>: type=unavailable, offset=0, bytes=4, variant-part\n'
                  '    Big: type=unavailable, offset=0, bytes=4, active-variant\n')
        bodies = [prefix + '\n'.join(f'      n{i}: type=u32, offset=0, bytes=4, value={i}' for i in range(33)),
                  prefix + '      word: type=Enum, offset=0, bytes=4, value=0 (' + 'x'*257 + ')',
                  prefix + '\n'.join('  '*(i+3) + f'n{i}: type=Wrapper, offset=0, bytes=4' for i in range(9)) +
                  '\n' + '  '*12 + 'leaf: type=u32, offset=0, bytes=4, value=13']
        for body in bodies:
            c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['result'], 'Big')
            nodes = objects.dap.parse_source_object_display(objects.packet(body), 41, 1)
            self.assertEqual(len(nodes), len(body.splitlines()))
            self.assertGreater(reply['body']['variablesReference'], 0)

    def test_unambiguous_labels_and_single_variant_owner_required_for_summary(self):
        body = BODY.replace('signed:', 'signed.x;continue\\x1b:')
        c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
        fields = c.children(reply['body']['variablesReference'])
        self.assertEqual(fields[1]['value'], 'Negative')
        branch = c.children(c.children(fields[1]['variablesReference'])[0]['variablesReference'])[1]
        self.assertEqual(c.children(branch['variablesReference'])[0]['name'], 'signed.x;continue\\x1b')
        body = BODY.replace('    <variant-part>: type=unavailable, offset=4, bytes=8, variant-part',
                            '    common: type=i32, offset=8, bytes=4, value=7\n'
                            '    <variant-part>: type=unavailable, offset=4, bytes=8, variant-part')
        c, _, reply = self.control(body); self.assertTrue(reply['success'], reply)
        self.assertEqual(c.children(reply['body']['variablesReference'])[1]['value'], 'Payload')
        self.assertFalse(any(x == 'continue' or x.startswith('read ') for x in c.broker.commands))


if __name__ == '__main__': unittest.main()
