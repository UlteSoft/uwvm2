#!/usr/bin/env python3
"""Detached display/deadline unit tests; real broker acceptance is separate."""
import importlib.util, io, sys, unittest
from pathlib import Path
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


dap = load(root / 'tools/debug/dap_adapter.py', 'integer_display_dap')
server = load(root / 'tools/debug/secure_server.py', 'integer_display_server')


def packet(row, stop=41):
    return f'source-value stop={stop} name=len(box.Numbers)\n{row}\nsource-value end\n'


class Channel:
    def __init__(self): self.timeouts = []; self.closed = False
    def settimeout(self, value): self.timeouts.append(value)
    def fileno(self): return -1 if self.closed else 1
    def close(self): self.closed = True


class Tests(unittest.TestCase):
    def test_initialize_negotiates_hover_and_type_display(self):
        import test_dap_source_frames as frames
        for capability in (False, True):
            output = io.BytesIO(); adapter = dap.Adapter(output); adapter.broker = frames.Broker()
            request = lambda seq, command, args: adapter.handle(dict(seq=seq, type='request', command=command, arguments=args))
            request(1, 'initialize', dict(supportsVariableType=capability))
            initialize = [r for r in frames.messages(output.getvalue()) if r.get('command') == 'initialize'][0]
            self.assertTrue(initialize['body']['supportsEvaluateForHovers'])
            output.seek(0); output.truncate()
            request(2, 'stackTrace', dict(threadId=1))
            stack = [r for r in frames.messages(output.getvalue()) if r.get('command') == 'stackTrace'][0]
            frame = stack['body']['stackFrames'][1]
            adapter.broker.print_packet = packet('$len: type=int, offset=0, bytes=4, value=3')
            output.seek(0); output.truncate()
            request(3, 'evaluate', dict(frameId=frame['id'], context='hover', expression='len(box.Numbers)'))
            result = [r for r in frames.messages(output.getvalue()) if r.get('command') == 'evaluate'][0]
            self.assertTrue(result['success'], result)
            self.assertEqual(result['body']['result'], '3')
            self.assertEqual('type' in result['body'], capability)
        output = io.BytesIO(); adapter = dap.Adapter(output)
        adapter.handle(dict(seq=1, type='request', command='initialize', arguments=dict(supportsVariableType=1)))
        self.assertFalse(frames.messages(output.getvalue())[0]['success'])

    def test_finite_integer_roots_and_actual_widths(self):
        for size in (1, 2, 4, 8):
            for label, kind, bounds in (
                    ('$len', 'int', (0, (1 << (size*8-1))-1)),
                    ('$cap', 'int', (0, (1 << (size*8-1))-1)),
                    ('$expression', 'int', (-(1 << (size*8-1)), (1 << (size*8-1))-1)),
                    ('$expression', 'signed integer', (-(1 << (size*8-1)), (1 << (size*8-1))-1)),
                    ('$expression', 'unsigned integer', (0, (1 << (size*8))-1))):
                for number in bounds:
                    text = packet(f'{label}: type={kind}, offset=0, bytes={size}, value={number}')
                    dap.validate_source_evaluation_reply(text, 41, 7)
                    self.assertEqual(dap.source_evaluation_display(text),
                                     dict(result=str(number), type=kind, variablesReference=0,
                                          presentationHint=dict(attributes=['readOnly'])))

    def test_invalid_integer_root_does_not_publish_a_scalar(self):
        for row in ('$len: type=int, offset=0, bytes=4, value=-1',
                    '$cap: type=int, offset=0, bytes=4, value=2147483648',
                    '$expression: type=signed integer, offset=0, bytes=8, value=9223372036854775808',
                    '$expression: type=unsigned integer, offset=0, bytes=8, value=18446744073709551616',
                    '$expression: type=unsigned integer, offset=0, bytes=4, value=-1',
                    '$len: type=int, offset=0, bytes=4, value=01',
                    '$cap: type=int, offset=0, bytes=4, value=-0',
                    '$len: type=int, offset=1, bytes=4, value=3',
                    '$cap: type=unsigned integer, offset=0, bytes=4, value=3',
                    '$len: type=signed integer, offset=0, bytes=4, value=3',
                    '$cap: type=int, offset=0, bytes=16, value=3',
                    '$len: type=int, offset=0, bytes=4, value=3, memory=0x1'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                dap.source_evaluation_display(packet(row))

    def test_unknown_and_compound_display_grants_no_new_authority(self):
        for text in ('source-stop 41\ni32=3\n',
                     packet('value: type=MyType, offset=0, bytes=4, value=3'),
                     packet('$len: type=int, offset=0, bytes=4, value=3\n  field: unavailable')):
            result = dap.source_evaluation_display(text)
            self.assertEqual(result['result'], text.rstrip('\n'))
            self.assertNotIn('type', result)
            self.assertNotIn('memoryReference', result)
            self.assertEqual(result['variablesReference'], 0)
        for text in (packet('$len: type=int, offset=0, bytes=4, value=3', 42),
                     packet('$len: type=int, offset=0, bytes=4, value=3').replace('source-value end', 'source-value end\nsource-value end')):
            with self.assertRaises(ValueError): dap.validate_source_evaluation_reply(text, 41, 7)

    def test_client_only_first_request_has_startup_wait(self):
        broker = object.__new__(dap.UnixBroker); broker.channel = Channel(); broker.first_request = True
        broker.send_raw = lambda data: None; broker.receive_raw = lambda: b'ok\n'
        with patch.object(dap.sys, 'platform', 'linux'):
            self.assertEqual(broker.request('status'), 'ok\n'); broker.request('pause')
        self.assertEqual(broker.channel.timeouts, [125, 8, 8, 8])
        broker.receive_raw = lambda: (_ for _ in ()).throw(TimeoutError('late packet'))
        with self.assertRaises(TimeoutError): broker.request('status')
        self.assertTrue(broker.channel.closed)

    def test_server_startup_allowance_is_launch_owned_across_reconnects(self):
        pending = [True]; channel = Channel()
        guest = type('Guest', (), {'poll': lambda self: None})()
        sent = []
        for expected in (120, 6):
            replies = iter((b'c'*32, b'pause', b'ok\n', b'disconnect'))
            with patch.object(server, 'checked_packet', side_effect=lambda *a: next(replies)), \
                 patch.object(server, 'send_packet', side_effect=lambda target, data: sent.append(data)):
                self.assertFalse(server.serve_client(None, None, None, channel, b'c'*32, guest, pending))
            self.assertEqual(channel.timeouts[-2:], [expected, 6])
        self.assertFalse(pending[0])
        unauthorized = [True]
        with patch.object(server, 'checked_packet', return_value=b'x'*32), self.assertRaises(RuntimeError):
            server.serve_client(None, None, None, channel, b'c'*32, guest, unauthorized)
        self.assertTrue(unauthorized[0])

    def test_vm_transport_failure_cannot_return_to_client_accept_loop(self):
        # These are detached protocol DATA. Real credentials, original VM exec
        # and OS retirement are exercised by run_dap_tinygo_collections.py.
        guest = type('Guest', (), {'poll': lambda self: None})()
        for response in (None, server._TIMEOUT):
            channel = Channel(); pending = [True]
            replies = iter((b'c'*32, b'pause', response))
            with patch.object(server, 'checked_packet', side_effect=lambda *a: next(replies)), \
                 patch.object(server, 'send_packet'), self.assertRaises(server.VMChannelFailure):
                server.serve_client(None, None, None, channel, b'c'*32, guest, pending)
            self.assertFalse(pending[0])
            self.assertEqual(channel.timeouts, [120, 6])
        channel = Channel(); pending = [False]
        replies = iter((b'c'*32, b'pause'))
        def send(target, data):
            if target is channel: raise BrokenPipeError('closed VM endpoint')
        with patch.object(server, 'checked_packet', side_effect=lambda *a: next(replies)), \
             patch.object(server, 'send_packet', side_effect=send), self.assertRaises(server.VMChannelFailure):
            server.serve_client(None, None, None, channel, b'c'*32, guest, pending)
        self.assertEqual(channel.timeouts, [6, 6])

    def test_fatal_vm_channel_retires_original_launcher_and_listener(self):
        import os, socket, tempfile
        from types import SimpleNamespace
        class Guest:
            pid = 123456789  # detached DATA only; no signal uses this number
            returncode = None
            def poll(self): return self.returncode
            def terminate(self): self.returncode = -15
            def wait(self, timeout=None):
                if self.returncode is None: raise AssertionError('original owner was not retired')
                return self.returncode
            def kill(self): raise AssertionError('unnecessary force kill')
        guest = Guest(); left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        peerfd = os.pidfd_open(os.getpid())
        with tempfile.TemporaryDirectory(prefix='dap-integer-unit-', dir='/tmp') as name:
            folder = Path(name)
            args = SimpleNamespace(socket_dir=str(folder), uwvm=Path('/detached-test-data'), vm_args=['--', '-m', 'run'])
            try:
                with patch.object(server.subprocess, 'Popen', return_value=guest), \
                     patch.object(server, 'accept_client', side_effect=[(left, (os.getpid(), os.getuid(), os.getgid()), peerfd),
                                                                       AssertionError('accepted a new client after VM transport loss')]), \
                     patch.object(server, 'serve_client', side_effect=server.VMChannelFailure('late unsequenced reply')), \
                     self.assertRaises(server.VMChannelFailure):
                    server.serve(args)
                self.assertEqual(guest.returncode, -15)
                self.assertFalse((folder / 'control.sock').exists())
                self.assertFalse((folder / 'capability').exists())
            finally:
                left.close(); right.close()
                # serve owns and closes the actual descriptor returned by its
                # accepted peer; this unit never signals that live PIDFD.


if __name__ == '__main__':
    unittest.main()
