#!/usr/bin/env python3
"""Finite DAP uncaught packet tests; these fixtures do not qualify runtime capture."""
import io
import unittest
import test_dap_wasm_state as state

dap = state.dap
TRAP = 'stopped: Uncaught Wasm exception before unwind; continue propagates\nuncaught-thread 1'
NOTE = 'Note: Uncaught Wasm snapshot; stack has not unwound.'
SNAPSHOT = 'Note: Last Wasm safepoint snapshot; may differ from current native state.'

def packet(stop=41):
    return (f'wasm-stop {stop}\n{NOTE}\n'
            'Wasm state thread=1 module=0 epoch=7 first=0 total=1\n'
            f'{SNAPSHOT}\noperand 0 i64 = 1234\n')

class Broker(state.Broker):
    def status(self):
        return f'{TRAP}\nstop-id {self.stop}\nthread 1 module=0 function=1 byte-offset=4 generation=7\n'
    def request(self, command):
        result = super().request(command)
        if command.startswith('operands 1 0 '):
            result = result.replace('\nWasm state thread=', '\n' + NOTE + '\nWasm state thread=', 1)
        return result

class UncaughtPackets(unittest.TestCase):
    send = state.StateProtocol.send
    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output)
        self.adapter.step_level = 'wasm'; self.adapter.broker = self.broker = Broker()
        self.adapter.observe(self.broker.status(), notify=False); self.sequence = 0
    def test_exception_event_has_no_native_authority(self):
        output = io.BytesIO(); adapter = dap.Adapter(output)
        adapter.observe(self.broker.status())
        event = state.messages(output.getvalue())[0]
        self.assertEqual(event['event'], 'stopped')
        self.assertEqual(event['body']['reason'], 'exception')
        self.assertNotIn('native_pc', adapter.threads[0])
        self.assertEqual(adapter.native_frames, set())
    def test_exact_inputs_and_both_snapshot_notices(self):
        copied = dap.parse_wasm_state(packet(), 'operands', 1, 0, 0, 41)
        self.assertEqual([(r['type'], r['value']) for r in copied['rows']], [('i64', '1234')])
        self.assertEqual(copied['snapshotNote'], NOTE + '\n' + SNAPSHOT)
        reply = self.send('uwvm/wasmState', threadId=1, selection='operands')
        self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['snapshotNote'], NOTE + '\n' + SNAPSHOT)
    def test_scope_title_and_read_only_inputs(self):
        frame = self.send('stackTrace', threadId=1)['body']['stackFrames'][0]
        reply = self.send('scopes', frameId=frame['id']); self.assertTrue(reply['success'], reply)
        scopes = reply['body']['scopes']
        scope = next(s for s in scopes if s['name'] == 'Wasm uncaught snapshot (before unwind)')
        self.assertFalse(any(s['name'] == 'Registers' for s in scopes))
        reply = self.send('variables', variablesReference=scope['variablesReference'])
        self.assertTrue(reply['success'], reply)
        rows = reply['body']['variables']; self.assertEqual(len(rows), 1)
        self.assertEqual((rows[0]['type'], rows[0]['value']), ('i64', '1234'))
        self.assertEqual(rows[0]['presentationHint']['attributes'], ['readOnly'])
        self.assertNotIn('memoryReference', rows[0])
    def test_empty_inputs_have_no_result(self):
        empty = packet().replace('total=1', 'total=0').replace('operand 0 i64 = 1234\n', '')
        copied = dap.parse_wasm_state(empty, 'operands', 1, 0, 0, 41)
        self.assertEqual(copied['rows'], []); self.assertEqual(copied['total'], 0)
        self.assertIn(NOTE, copied['snapshotNote'])
    def test_unavailable_is_not_an_empty_successful_preview(self):
        text = f'wasm-stop 41\n{NOTE}\nWasm state unavailable: requires a current cooperative Wasm stop\n'
        copied = dap.parse_wasm_state(text, 'operands', 1, 0, 0, 41)
        self.assertEqual(copied['unavailable'], 'requires a current cooperative Wasm stop')
        self.assertEqual(copied['snapshotNote'], NOTE)
        self.assertEqual(copied['rows'], [])
        plain = text.replace(NOTE + '\n', '')
        self.assertNotIn('snapshotNote', dap.parse_wasm_state(plain, 'operands', 1, 0, 0, 41))
    def test_ambiguous_notes_and_stale_stop_rejected(self):
        for text in (packet(42), packet().replace(NOTE, NOTE + '\n' + NOTE),
                     packet().replace(NOTE, 'Note: Current native stack.'),
                     packet().replace(SNAPSHOT, SNAPSHOT + '\n' + SNAPSHOT)):
            with self.subTest(text=text), self.assertRaises(ValueError):
                dap.parse_wasm_state(text, 'operands', 1, 0, 0, 41)
        with self.assertRaises(ValueError):
            dap.parse_wasm_state(packet().replace('operand 0 ', 'local 0 '), 'locals', 1, 0, 0, 41)
    def test_stop_changes_during_copy_invalidate_values(self):
        self.broker.after_copy = lambda: setattr(self.broker, 'stop', self.broker.stop + 1)
        self.assertFalse(self.send('uwvm/wasmState', threadId=1, selection='operands')['success'])
        self.assertEqual(self.adapter.wasm_object_views, {})

    def test_uncaught_participant_is_required_and_bound(self):
        for text in (self.broker.status().replace('uncaught-thread 1\n', ''),
                     self.broker.status().replace('uncaught-thread 1', 'uncaught-thread 2'),
                     self.broker.status().replace('uncaught-thread 1', 'uncaught-thread 1\nuncaught-thread 1')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                dap.parse_status(text)
        self.assertTrue(dap.parse_status(self.broker.status())[2][0]['uncaught_exception'])
    def test_other_participant_keeps_generic_snapshot_scope(self):
        old_status = self.broker.status
        self.broker.status = lambda: old_status() + "thread 2 module=0 function=1 byte-offset=4 generation=7\n"
        self.adapter.observe(self.broker.status(), notify=False)
        frame_id = self.adapter.new_reference()
        self.adapter.frames[frame_id] = (2, 0, 0, 1)
        reply = self.send("scopes", frameId=frame_id)
        self.assertTrue(reply["success"], reply)
        names = [s["name"] for s in reply["body"]["scopes"]]
        self.assertIn("Wasm operand stack (last safepoint; may differ from native state)", names)
        self.assertNotIn("Wasm uncaught snapshot (before unwind)", names)
        self.assertNotIn("Wasm pre-trap inputs (no instruction result)", names)

    def test_terminal_notes_cannot_be_combined(self):
        combined = packet().replace(NOTE, NOTE + '\n' + dap._WASM_PRETRAP_INPUT_NOTE)
        with self.assertRaises(ValueError):
            dap.parse_wasm_state(combined, 'operands', 1, 0, 0, 41)

if __name__ == '__main__':
    unittest.main()
