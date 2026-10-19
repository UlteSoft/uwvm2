#!/usr/bin/env python3
"""Copied pretty-printer protocol DATA through DAP framing; live VM tests are separate."""
import unittest
import test_dap_source_objects as objects

dap = objects.dap
SUMMARY = 'size=2 capacity=2 [7, pair {first=3, second=4}]'
BODY = ('object: type=vector<Record>, offset=0, bytes=12, pretty="' + SUMMARY + '"\n'
        '  begin: type=pointer, offset=0, bytes=4, value=guest:0x100\n'
        '  [0]: type=int, offset=256, bytes=4, copied-child, value=7\n'
        '  [1]: type=Pair, offset=4096, bytes=8, copied-child, pretty="pair {first=3, second=4}"\n'
        '    first: type=int, offset=4096, bytes=4, value=3\n'
        '    second: type=int, offset=4100, bytes=4, value=4')


class SourcePrettyTests(unittest.TestCase):
    send = objects.SourceObjectTests.send
    evaluate = objects.SourceObjectTests.evaluate
    children = objects.SourceObjectTests.children

    def setUp(self):
        objects.SourceObjectTests.setUp(self)

    def test_summary_retains_raw_tree_and_independent_copied_children(self):
        nodes = dap.parse_source_object_display(objects.packet(BODY), 41, 1)
        self.assertEqual(nodes[0]['value'], 'vector<Record>')
        self.assertEqual(nodes[0]['pretty'], SUMMARY)
        self.assertEqual(nodes[2]['value'], '7')
        self.assertTrue(nodes[2]['detached_copy'])
        _, reply = self.evaluate(BODY)
        self.assertTrue(reply['success'], reply)
        root = reply['body']
        self.assertEqual(root['result'], SUMMARY)
        self.assertEqual((root['namedVariables'], root['indexedVariables']), (1, 2))
        children = self.children(root['variablesReference'])
        self.assertEqual([row['name'] for row in children], ['begin', '[0]', '[1]'])
        self.assertEqual([row['value'] for row in children], ['guest:0x100', '7', 'pair {first=3, second=4}'])
        pair = self.children(children[2]['variablesReference'])
        self.assertEqual([row['value'] for row in pair], ['3', '4'])
        self.assertEqual(self.children(root['variablesReference'], filter='indexed', start=1, count=1), children[2:])
        self.assertEqual(self.children(root['variablesReference'], filter='named'), children[:1])
        for row in [root, *children, *pair]:
            self.assertEqual(row['presentationHint']['attributes'], ['readOnly'])
            self.assertNotIn('memoryReference', row)
            self.assertNotIn('evaluateName', row)
        self.assertEqual(sum(command.startswith('print-frame ') for command in self.broker.commands), 1)
        self.assertFalse(any(command.startswith(('read ', 'memory ', 'print-frame /')) for command in self.broker.commands))

    def test_quoted_summary_delimiters_and_escapes_are_opaque(self):
        summary = r'x: type=Y, offset=999, bytes=888, text=continue; read 0, value=guest:0x1, pretty=\"nested\" \\ \x1b\x00\xce\xbb'
        body = BODY.replace(SUMMARY, summary)
        _, reply = self.evaluate(body)
        self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['result'], dap._source_pretty_display(summary))
        self.assertEqual(len(self.children(reply['body']['variablesReference'])), 3)
        self.assertNotIn('\x1b', reply['body']['result'])
        self.assertFalse(any(command == 'continue' or command.startswith('read ') for command in self.broker.commands))

    def test_summary_and_legacy_text_are_parsed_independently(self):
        summary = r'string length=3 \"a, text=b\"'
        body = ('object: type=String, offset=0, bytes=8, pretty="' + summary + '", text=a, text=b\n'
                '  length: type=unsigned int, offset=4, bytes=4, value=3')
        nodes = dap.parse_source_object_display(objects.packet(body), 41, 1)
        self.assertEqual(nodes[0]['pretty'], summary)
        self.assertEqual(nodes[0]['value'], '"a, text=b"')
        _, reply = self.evaluate(body)
        self.assertEqual(reply['body']['result'], dap._source_pretty_display(summary))
        self.assertEqual(self.children(reply['body']['variablesReference'])[0]['value'], '3')

    def test_truncated_summary_or_text_keeps_already_copied_elements_expandable(self):
        for suffix in (', omitted-elements=23', ', text=a (truncated)', ', text=a (truncated), omitted-elements=23'):
            body = BODY.replace('pretty="' + SUMMARY + '"', 'pretty="' + SUMMARY + ' ..."' + suffix, 1)
            self.setUp()
            _, reply = self.evaluate(body)
            self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['result'], SUMMARY + ' ...')
            self.assertGreater(reply['body']['variablesReference'], 0)
            self.assertEqual([row['name'] for row in self.children(reply['body']['variablesReference'])], ['begin', '[0]', '[1]'])
        self.setUp()
        _, reply = self.evaluate(BODY.replace(SUMMARY, 'x' * 4096 + '... (truncated)', 1))
        self.assertTrue(reply['success'], reply)
        self.assertGreater(reply['body']['variablesReference'], 0)

    def test_canonical_byte_budget_applies_before_publication(self):
        for summary in ('x' * 4096, r'\x01' * 4096, ''):
            self.setUp()
            _, reply = self.evaluate(BODY.replace(SUMMARY, summary, 1))
            self.assertTrue(reply['success'], reply)
            self.assertEqual(reply['body']['result'], summary)
        for summary in ('x' * 4097, r'\x01' * 4097, r'\x1B', r'\x41', r'\q', 'x\\', 'raw"quote', 'x\x1b', 'λ'):
            self.setUp()
            _, reply = self.evaluate(BODY.replace(SUMMARY, summary, 1))
            self.assertFalse(reply['success'], reply)
            self.assertFalse(self.adapter.source_object_views)

    def test_display_decodes_one_framing_layer_and_keeps_every_byte_inert(self):
        escaped = ''.join(
            chr(byte) if 32 <= byte < 127 and byte not in (34, 92)
            else '\\' + chr(byte) if byte in (34, 92)
            else '\\x%02x' % byte
            for byte in range(256))
        display = 'string length=256 "' + escaped + '"'
        encoded = display.replace('\\', '\\\\').replace('"', '\\"')
        body = BODY.replace(SUMMARY, encoded, 1)
        nodes = dap.parse_source_object_display(objects.packet(body), 41, 1)
        self.assertEqual(nodes[0]['pretty'], encoded)  # Original packet retained.
        _, reply = self.evaluate(body)
        self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['result'], display)
        self.assertTrue(all(32 <= ord(c) < 127 for c in reply['body']['result']))
        root = reply['body']
        self.assertGreater(root['variablesReference'], 0)
        self.assertEqual(len(self.children(root['variablesReference'])), 3)
        self.assertNotIn('memoryReference', root)
        self.assertNotIn('evaluateName', root)
        self.assertEqual(sum(command.startswith('print-frame ') for command in self.broker.commands), 1)
        self.assertFalse(any(command == 'continue' or command.startswith(('read ', 'memory '))
                             for command in self.broker.commands))

    def test_detached_extent_is_local_to_one_copied_root(self):
        bad = (
            BODY.replace('bytes=12, pretty=', 'bytes=12, copied-child, pretty=', 1),
            BODY.replace('offset=4100, bytes=4', 'offset=4104, bytes=4'),
            BODY.replace('offset=256, bytes=4, copied-child', 'offset=256, bytes=4'),
            BODY.replace('offset=4096, bytes=8, copied-child', 'offset=4096, bytes=8'),
            BODY.replace('offset=256, bytes=4, copied-child', 'offset=18446744073709551616, bytes=4, copied-child'),
        )
        for body in bad:
            self.setUp()
            _, reply = self.evaluate(body)
            self.assertFalse(reply['success'], reply)
            self.assertFalse(self.adapter.source_object_views)

    def test_known_flags_have_one_strict_order(self):
        bad = (
            BODY.replace('bytes=4, copied-child, value=7', 'bytes=4, copied-child, copied-child, value=7'),
            BODY.replace('bytes=8, copied-child, pretty=', 'bytes=8, pretty="first", copied-child, pretty='),
            BODY.replace('pretty="' + SUMMARY + '"', 'pretty="' + SUMMARY + '", pretty="again"', 1),
            BODY.replace('bytes=4, copied-child, value=7', 'bytes=4, copied-child=1, value=7'),
            BODY.replace('pretty="' + SUMMARY + '"', 'pretty=' + SUMMARY, 1),
            BODY.replace('pretty="' + SUMMARY + '"', 'pretty="' + SUMMARY + '"junk', 1),
        )
        for body in bad:
            self.setUp()
            _, reply = self.evaluate(body)
            self.assertFalse(reply['success'], reply)
            self.assertFalse(self.adapter.source_object_views)

    def test_summary_reference_retires_before_stale_expansion(self):
        _, reply = self.evaluate(BODY)
        reference = reply['body']['variablesReference']
        prints = sum(command.startswith('print-frame ') for command in self.broker.commands)
        self.broker.stop += 1
        self.assertFalse(self.send('variables', variablesReference=reference)['success'])
        self.assertFalse(self.adapter.source_object_views)
        self.assertEqual(sum(command.startswith('print-frame ') for command in self.broker.commands), prints)


if __name__ == '__main__':
    unittest.main()
