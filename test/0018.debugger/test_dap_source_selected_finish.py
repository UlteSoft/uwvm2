#!/usr/bin/env python3
"""DATA-only adapter stop/reference lifecycle checks, not runtime authority.

The fake broker deliberately triggers a callback BEFORE its reply. The oracle
requires previous adapter-owned frame/scope labels to be retired at that point;
waiting for status polling or the later response cannot satisfy this check.
Keeper alone runs these components in the original Linux cgroup. This source
file has not been imported/executed or compiled on the development host.
"""
import importlib.util
from pathlib import Path
import unittest

# Reuse actual existing formatter fixtures/test transport. These strings are
# inert DATA, never pause tickets, a canonical capture or a VM memory permission.
FIXTURE_PATH = Path(__file__).resolve().with_name('test_dap_source_frames.py')
SPEC = importlib.util.spec_from_file_location('uwvm_dap_finish_data_fixtures', FIXTURE_PATH)
fixtures = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixtures)

class SelectedFinishBroker(fixtures.Broker):
    def __init__(self):
        super().__init__()
        self.before_finish = None
        self.finish_error = None
        self.finish_exited = False

    def request(self, command):
        words = command.split()
        if words and words[0] in ('finish', 'fin'):
            self.commands.append(command)
            if self.before_finish is not None:
                self.before_finish()
            if self.finish_error is not None:
                raise self.finish_error
            self.stop += 1
            if self.finish_exited:
                self.state = 'guest exited: 0'
            return self.status()
        return super().request(command)

class SelectedFinishTests(fixtures.SourceFrameTests):
    def setUp(self):
        super().setUp()
        self.broker = SelectedFinishBroker()
        self.adapter.broker = self.broker

    def exercise_retirement(self, spelling, exited=False, failure=None):
        frames = self.frames()
        scope = self.source_scope(frames[0])
        frame = frames[0]['id']
        key = self.adapter.stop_key
        observed = []
        def before_finish():
            self.assert_retired()
            observed.append('retired before broker send/reply')
        self.broker.before_finish = before_finish
        self.broker.finish_exited = exited
        self.broker.finish_error = failure
        reply = self.send('evaluate', expression=spelling, context='repl')
        self.assertEqual(observed, ['retired before broker send/reply'])
        if failure is not None:
            self.assertFalse(reply['success'])
            self.assert_retired()
        elif exited:
            self.assertTrue(reply['success'], reply)
            self.assertEqual(self.adapter.state, 'exited')
            self.assertTrue(self.adapter.finished)
            self.assertFalse(self.adapter.frames)
        else:
            self.assertTrue(reply['success'], reply)
            self.assertEqual(self.adapter.state, 'stopped')
            self.assertNotEqual(self.adapter.stop_key, key)
            self.assertIsNotNone(self.adapter.stop_key)
            self.assertFalse(self.adapter.frames)
        events = [item for item in fixtures.messages(self.output.getvalue()) if item['type'] == 'event']
        if failure is None:
            self.assertTrue(any(event['event'] == ('terminated' if exited else 'stopped') for event in events), events)
        self.assertFalse(self.send('scopes', frameId=frame)['success'])
        self.assertFalse(self.send('variables', variablesReference=scope)['success'])
        self.assertFalse(self.adapter.source_scope_stops)

    def test_explicit_finish_retires_before_send_and_observes_new_formatter_stop(self):
        self.exercise_retirement('finish 1')

    def test_explicit_fin_retires_before_send_and_observes_new_formatter_stop(self):
        self.exercise_retirement('fin 1')

    def test_fin_transport_failure_never_resurrects_previous_labels(self):
        self.exercise_retirement('fin 1', failure=ConnectionError('controlled DATA-only transport failure'))

    def test_selected_finish_guest_exit_retires_labels_and_observes_exit_event(self):
        self.exercise_retirement('finish 1', exited=True)

    def test_ordinary_dap_stepout_still_routes_unselected_current_source_out(self):
        self.adapter.step_level = 'source'
        self.frames()
        observed = []
        original = self.broker.request
        def request(command):
            if command == 'step source 1 out':
                self.assert_retired()
                observed.append(command)
                self.broker.stop += 1
                return self.broker.status()
            return original(command)
        self.broker.request = request
        result = self.send('stepOut', threadId=1, granularity='statement')
        self.assertTrue(result['success'], result)
        self.assertEqual(observed, ['step source 1 out'])
        self.assertNotIn('finish 1', self.broker.commands)
        self.assertNotIn('fin 1', self.broker.commands)

if __name__ == '__main__':
    unittest.main()
