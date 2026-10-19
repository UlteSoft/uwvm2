#!/usr/bin/env python3
"""Printer configuration cache retirement through DAP framing, using copied DATA."""
import unittest
import test_dap_source_objects as objects


class SourcePrintConfigurationTests(unittest.TestCase):
    send = objects.SourceObjectTests.send
    evaluate = objects.SourceObjectTests.evaluate
    children = objects.SourceObjectTests.children

    def setUp(self):
        objects.SourceObjectTests.setUp(self)
        original = self.broker.request
        self.configuration_reply = 'Pretty-printer custom registered.\n'

        def request(command):
            words = command.split()
            if words[:2] in (['set', 'print'], ['show', 'print']) or words[:1] == ['pretty-printer']:
                self.broker.commands.append(command)
                return self.configuration_reply
            return original(command)
        self.broker.request = request

    def test_configuration_retires_all_source_labels_without_fabricating_a_stop(self):
        for command in ('set print pretty off', 'set print elements 1',
                        'pretty-printer pointer custom Type data', 'pretty-printer disable builtin',
                        'pretty-printer enable custom', 'pretty-printer delete custom'):
            with self.subTest(command=command):
                self.setUp()
                frame, value = self.evaluate()
                reference = value['body']['variablesReference']
                actual_stop = self.broker.stop
                configured = self.send('evaluate', expression=command, context='repl')
                self.assertTrue(configured['success'], configured)
                self.assertEqual(self.broker.stop, actual_stop)
                self.assertFalse(self.adapter.source_object_views)
                self.assertFalse(self.adapter.source_frame_ordinals)
                self.assertFalse(self.send('variables', variablesReference=reference)['success'])
                self.assertFalse(self.send('evaluate', expression='*box', context='watch', frameId=frame['id'])['success'])
                _, fresh = self.evaluate()
                self.assertTrue(fresh['success'], fresh)
                self.assertGreater(fresh['body']['variablesReference'], reference)
                self.assertFalse(any(c in ('continue', 'step', 'pause') for c in self.broker.commands))

    def test_listing_and_show_preserve_a_copied_view(self):
        for command in ('show print pretty', 'show print elements', 'pretty-printer list'):
            with self.subTest(command=command):
                self.setUp()
                frame, value = self.evaluate()
                reference = value['body']['variablesReference']
                self.assertTrue(self.send('evaluate', expression=command, context='repl')['success'])
                self.assertIn(reference, self.adapter.source_object_views)
                self.assertIn(frame['id'], self.adapter.source_frame_ordinals)
                self.assertTrue(self.children(reference))

    def test_failed_or_uncertain_configuration_never_revives_a_source_reference(self):
        _, value = self.evaluate()
        reference = value['body']['variablesReference']
        self.configuration_reply = 'error: invalid pretty-printer command\n'
        self.assertFalse(self.send('evaluate', expression='pretty-printer pointer bad Type payload()', context='repl')['success'])
        self.assertFalse(self.adapter.source_object_views)
        self.configuration_reply = 'Pretty-printer good registered.\n'
        self.assertTrue(self.send('evaluate', expression='pretty-printer pointer good Type data', context='repl')['success'])
        self.assertFalse(self.send('variables', variablesReference=reference)['success'])


if __name__ == '__main__':
    unittest.main()
