#!/usr/bin/env python3
"""Finite write acknowledgement/validation DATA; actual commits tested separately."""
import base64
import io
import unittest
import test_dap_wasm_state as state

dap = state.dap


class Broker(state.Broker):
    def __init__(self):
        super().__init__()
        self.storage = {}; self.limit = None; self.on_mutation = None
        self.packet_transform = None; self.refusal = None; self.after_commit = None
        self.lost_reply = False; self.status_override = None; self.reject_status = False

    def status(self):
        return self.status_override if self.status_override is not None else super().status()

    def request(self, command):
        if command == "status" and self.reject_status:
            self.commands.append(command); raise ConnectionError("modeled later status loss")
        if command.startswith("memory "):
            self.commands.append(command)
            _, module, memory, address, count = command.split()
            module, memory, address, count = map(int, (module, memory, address, count))
            if self.limit is not None and address + count > self.limit:
                return "error: invalid module, memory or byte range\n"
            data = bytes(self.storage.get((module, memory, address+i), 81 if memory == 0 else 167) for i in range(count))
            return "memory:" + ''.join(f" {b:02x}" for b in data) + "\n" if count else "memory: <empty>\n"
        if not command.startswith("set wasm memory "):
            return super().request(command)
        self.commands.append(command)
        _, _, _, module, memory, thread, address, _, spelling = command.split()
        module, memory, thread, address = map(int, (module, memory, thread, address))
        data = bytes.fromhex(spelling)
        if self.on_mutation: self.on_mutation()
        refusal = self.refusal
        if self.limit is not None and address+len(data) > self.limit:
            refusal = "Wasm index or page is out of range"
        if refusal:
            packet = (f"wasm-stop {self.stop}\nWasm mutation v=3 status={refusal} applied=0 reason=none runtime=0 "
                      f"module={module} target=memory index={memory} element={address} bytes=0 address-bytes=0\n")
        else:
            for i, byte in enumerate(data): self.storage[module, memory, address+i] = byte
            self.stop += 1
            packet = (f"wasm-stop {self.stop}\nWasm mutation v=3 status=available applied=1 reason=none runtime=7 "
                      f"module={module} target=memory index={memory} element={address} bytes={len(data)} address-bytes={8 if memory == 1 else 4}\n")
            if self.after_commit: self.after_commit()
            if self.lost_reply: raise ConnectionError("modeled lost commit reply")
        return self.packet_transform(packet) if self.packet_transform else packet


class WasmMemoryWrites(unittest.TestCase):
    send = state.StateProtocol.send

    def setUp(self):
        self.output = io.BytesIO(); self.adapter = dap.Adapter(self.output)
        self.adapter.broker = self.broker = Broker(); self.sequence = 0

    def write(self, data=b'\x00\x01\x7f\xff', module=0, memory=0, address=17, offset=0, **extra):
        return self.send("writeMemory", memoryReference=f"wasm-memory:{module}:{memory}:{address}",
                         offset=offset, data=base64.b64encode(data).decode(), **extra)

    def events(self):
        return [m for m in state.messages(self.output.getvalue()) if m['type'] == 'event']

    def test_initialize_announces_writes_and_negotiates_events(self):
        reply = self.send("initialize", supportsMemoryEvent=True, supportsInvalidatedEvent=True)
        self.assertTrue(reply['success']); self.assertTrue(reply['body']['supportsWriteMemoryRequest'])
        self.assertTrue(reply['body']['supportsReadMemoryRequest']); self.assertEqual(self.broker.commands, [])
        self.assertTrue(self.adapter.supports_memory_event); self.assertTrue(self.adapter.supports_invalidated_event)
        self.send("initialize"); self.assertFalse(self.adapter.supports_memory_event)
        self.assertFalse(self.adapter.supports_invalidated_event)

    def test_capability_types_are_not_truthiness(self):
        for flag in ('supportsMemoryEvent', 'supportsInvalidatedEvent'):
            for value in (None, 0, 1, 'true', [], {}):
                self.setUp(); self.assertFalse(self.send('initialize', **{flag:value})['success'])
                self.assertEqual(self.broker.commands, [])

    def test_binary_payloads_boundaries_memory64_and_single_commit(self):
        for memory in (0, 1):
            for count in (1, 2, 3, 128, 255, 256):
                self.setUp(); data = bytes(i & 255 for i in range(count))
                reply = self.write(data, memory=memory, address=64, offset=-47, allowPartial=False)
                self.assertTrue(reply['success'], reply)
                self.assertEqual(reply['body'], {'offset':-47, 'bytesWritten':count})
                self.assertEqual(self.broker.commands, ['status', f'set wasm memory 0 {memory} 1 17 bytes {data.hex()}'])
                self.assertEqual(bytes(self.broker.storage[0, memory, 17+i] for i in range(count)), data)
                self.assertIsNone(self.adapter.stop_key); self.assertEqual(self.events(), [])

    def test_invalid_payloads_offsets_ranges_and_partial_never_reach_broker(self):
        invalid = [{'data':v} for v in (None, {}, [], True, 1, b'AQ==', '!', 'AQ==\n', 'AQ===', 'AR==', 'éAAA', base64.b64encode(bytes(257)).decode())]
        invalid += [{'offset':v} for v in (True, 1.5, '1', None, 1<<53, -(1<<53))]
        invalid += [{'allowPartial':v} for v in (True, 0, 1, None, 'false')]
        invalid += [{'memoryReference':v} for v in (None, {}, '$rsp', '0x1234', 'wasm:0:1:2', 'uwvm-native-stop:1',
            'wasm-memory:0:0:-1', 'wasm-memory:18446744073709551616:0:0', 'wasm-memory:0:4294967296:0',
            'wasm-memory:0:0:18446744073709551616', 'wasm-memory:0:0:18446744073709551615')]
        invalid += [{'memoryReference':'wasm-memory:0:0:0', 'offset':-1}]
        for extra in invalid:
            self.setUp(); args = {'memoryReference':'wasm-memory:0:0:17', 'data':'AA=='}; args.update(extra)
            reply = self.send('writeMemory', **args)
            self.assertFalse(reply['success'], (extra, reply)); self.assertNotIn('body', reply)
            self.assertEqual(self.broker.commands, []); self.assertEqual(self.broker.storage, {})

    def test_running_unidentified_empty_and_native_refuse_before_commit(self):
        for raw in ('running\n', 'prepared; no Wasm instruction executed\n', 'guest exited: 0\n',
                    'stopped: pause\nstop-id 41\n', 'stopped: pause\nthread 1 module=0 function=1 byte-offset=4 generation=7\n'):
            self.setUp(); self.broker.status_override = raw
            self.assertFalse(self.write()['success']); self.assertEqual(self.broker.commands, ['status'])
            self.assertEqual(self.broker.storage, {})
        self.setUp(); self.broker.native = True
        self.assertFalse(self.write()['success']); self.assertEqual(self.broker.commands, ['status'])

    def test_noncanonical_cohort_is_refused_for_reads_and_writes(self):
        base = self.broker.status()
        invalid = [base.replace('thread 1 ', 'thread 0 '), base.replace('generation=7', 'generation=0'),
            base.replace('generation=7', 'generation=18446744073709551616'), base + base.splitlines()[-1]+'\n',
            base.replace('module=0', 'module=18446744073709551616'),
            base.replace('thread 1 ', 'thread 18446744073709551616 '),
            base + ''.join(f'thread {i} module=0 function=1 byte-offset=4 generation=7\n' for i in range(2, 258))]
        for raw in invalid:
            for command in ('writeMemory', 'readMemory'):
                self.setUp(); self.broker.status_override = raw
                reply = self.send(command, memoryReference='wasm-memory:0:0:17', data='AA==', count=1)
                self.assertFalse(reply['success'], reply); self.assertEqual(self.broker.commands, ['status'])

    def test_mixed_native_cohort_has_no_write_authority(self):
        self.broker.status_override = self.broker.status()+'thread 2 module=0 function=1 byte-offset=4 generation=7\n  native-pc=0x1234\n'
        self.assertFalse(self.write()['success']); self.assertEqual(self.broker.commands, ['status'])
        self.assertEqual(self.broker.storage, {})

    def test_cohort_participant_selection_is_deterministic_and_bounded(self):
        self.broker.status_override = self.broker.status().replace('thread 1 ', 'thread 8 ')+self.broker.status().splitlines()[-1]+'\n'
        self.assertTrue(self.write()['success'])
        self.assertEqual(self.broker.commands[-1], 'set wasm memory 0 0 1 17 bytes 00017fff')

    def test_zero_bytes_validate_real_guest_range_without_write_or_events(self):
        self.send('initialize', supportsMemoryEvent=True, supportsInvalidatedEvent=True)
        self.broker.limit = 17
        reply = self.write(b''); self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body'], {'offset':0, 'bytesWritten':0})
        self.assertEqual(self.broker.commands, ['status', 'status', 'memory 0 0 17 0', 'status'])
        self.assertEqual(self.broker.storage, {}); self.assertEqual(self.events(), [])
        self.setUp(); self.broker.limit = 16
        self.assertFalse(self.write(b'')['success']); self.assertFalse(any(c.startswith('set ') for c in self.broker.commands))

    def test_exact_commit_acknowledgement_is_required_without_retry(self):
        transforms = [('v=3', 'v=2'), ('wasm-stop 42', 'wasm-stop 0'), ('wasm-stop 42', 'wasm-stop 41'),
            ('wasm-stop 42', 'wasm-stop 18446744073709551616'), ('runtime=7', 'runtime=0'),
            ('runtime=7', 'runtime=18446744073709551616'), ('module=0', 'module=1'), ('index=0', 'index=1'),
            ('index=0', 'index=4294967296'), ('element=17', 'element=18'), ('target=memory', 'target=global'),
            ('bytes=4', 'bytes=3'), ('address-bytes=4', 'address-bytes=0'), ('applied=1', 'applied=0'),
            ('reason=none', 'reason=global is immutable'), ('status=available', 'status=unknown')]
        for before, after in transforms:
            self.setUp(); self.broker.packet_transform = lambda packet:packet.replace(before, after)
            reply = self.write(); self.assertFalse(reply['success'], (before, after, reply))
            self.assertNotIn('body', reply); self.assertIn('unconfirmed', reply['message'])
            self.assertEqual(len(self.broker.commands), 2); self.assertIsNone(self.adapter.stop_key)
            self.assertEqual(self.events(), [])
        for change in (lambda p:p+'host-address=0xffff\n', lambda p:p.replace('\n', '\r\n'), lambda p:p[:-1]):
            self.setUp(); self.broker.packet_transform = change; self.assertFalse(self.write()['success'])
            self.assertEqual(len(self.broker.commands), 2)

    def test_genuine_protocol_refusals_are_nonapplied_and_no_retry(self):
        for reason in sorted(dap._WASM_STATE_UNAVAILABLE):
            self.setUp(); self.broker.refusal = reason
            reply = self.write(); self.assertFalse(reply['success'], reply)
            self.assertIn('write refused', reply['message']); self.assertEqual(self.broker.storage, {})
            self.assertEqual(len(self.broker.commands), 2); self.assertIsNone(self.adapter.stop_key)

    def test_whole_range_refusal_never_writes_the_valid_prefix(self):
        self.broker.limit = 19; reply = self.write()
        self.assertFalse(reply['success'], reply); self.assertEqual(self.broker.storage, {})
        self.assertEqual(self.broker.stop, 41); self.assertEqual(len(self.broker.commands), 2)

    def test_old_frame_and_value_caches_retire_before_even_refused_write(self):
        frame = self.send('stackTrace', threadId=1)['body']['stackFrames'][0]
        self.send('scopes', frameId=frame['id'])
        def check():
            self.assertIsNone(self.adapter.stop_key)
            for name in ('frames', 'scopes', 'wasm_local_scope_stops', 'source_scope_stops', 'wasm_scope_stops',
                         'wasip1_scope_stops', 'wasm_object_views', 'native_code_refs', 'deep_wasm_paths'):
                self.assertFalse(getattr(self.adapter, name), name)
        self.broker.on_mutation = check; self.broker.refusal = 'Wasm index or page is out of range'
        self.assertFalse(self.write()['success']); self.assertFalse(self.send('scopes', frameId=frame['id'])['success'])
        self.broker.refusal = None; self.assertTrue(self.write()['success'])
        fresh = self.send('stackTrace', threadId=1)['body']['stackFrames'][0]; self.assertGreater(fresh['id'], frame['id'])

    def test_commit_is_reported_even_if_the_next_status_is_lost(self):
        self.broker.after_commit = lambda:setattr(self.broker, 'reject_status', True)
        reply = self.write(); self.assertTrue(reply['success'], reply)
        self.assertEqual(reply['body']['bytesWritten'], 4); self.assertEqual(len(self.broker.commands), 2)
        self.assertEqual(bytes(self.broker.storage[0,0,17+i] for i in range(4)), b'\x00\x01\x7f\xff')
        self.assertFalse(self.send('readMemory', memoryReference='wasm-memory:0:0:17', count=4)['success'])
        self.assertIsNone(self.adapter.stop_key)

    def test_lost_commit_reply_is_unconfirmed_and_never_replayed(self):
        self.broker.lost_reply = True; reply = self.write()
        self.assertFalse(reply['success'], reply); self.assertIn('unconfirmed', reply['message'])
        self.assertEqual(len(self.broker.commands), 2); self.assertIsNone(self.adapter.stop_key)
        self.assertEqual(self.broker.stop, 42); self.assertEqual(self.events(), [])

    def test_events_are_negotiated_and_follow_the_committed_response(self):
        for memory, invalidated in ((False, False), (True, False), (False, True), (True, True)):
            self.setUp(); self.send('initialize', supportsMemoryEvent=memory, supportsInvalidatedEvent=invalidated)
            reply = self.write(address=64, offset=-47); self.assertTrue(reply['success'], reply)
            messages = state.messages(self.output.getvalue()); self.assertEqual(messages[0]['type'], 'response')
            expected = []
            if memory: expected.append(('memory', {'memoryReference':'wasm-memory:0:0:64', 'offset':-47, 'count':4}))
            if invalidated: expected.append(('invalidated', {'areas':['stacks', 'variables']}))
            self.assertEqual([(m['event'], m['body']) for m in self.events()], expected)
        self.setUp(); self.send('initialize', supportsMemoryEvent=True, supportsInvalidatedEvent=True)
        self.broker.refusal = 'Wasm index or page is out of range'; self.assertFalse(self.write()['success'])
        self.assertEqual(self.events(), [])

    def test_large_guest_indices_and_addresses_are_not_native_or_narrowed(self):
        module, memory, address = (1<<64)-1, (1<<32)-1, (1<<63)+17
        reply = self.write(b'\xff', module=module, memory=memory, address=address, offset=-1)
        self.assertTrue(reply['success'], reply)
        self.assertEqual(self.broker.commands[-1], f'set wasm memory {module} {memory} 1 {address-1} bytes ff')
        self.assertEqual(reply['body'], {'offset':-1, 'bytesWritten':1})


if __name__ == '__main__': unittest.main()
