#!/usr/bin/env python3
"""Detached scalar display bounds; actual frame/capture proof needs live replay."""
import io
import unittest
from test_dap_source_values import dap, SourceBroker, messages


class DirectNumericDisplayTests(unittest.TestCase):
    def packet(self, value, kind='i32', name='seed', origin=''):
        return f'source-stop 41\nsource parameter {name} type={kind} = {value}\n' + origin

    def test_integer_widths_and_producer_types(self):
        for width in (8, 16, 32, 64):
            for signed in (True, False):
                tag = ('i' if signed else 'u') + str(width)
                limits = (-(1 << (width - 1)), (1 << (width - 1)) - 1) if signed else (0, (1 << width) - 1)
                for number in (*limits, 0):
                    with self.subTest(tag=tag, number=number):
                        row = dap.source_evaluation_display(self.packet(f'{tag}={number}', 'ProducerAlias'))
                        self.assertEqual((row['result'], row['type'], row['variablesReference']), (str(number), 'ProducerAlias', 0))
                        self.assertEqual(row['presentationHint']['attributes'], ['readOnly'])
                        self.assertNotIn('memoryReference', row)
                        self.assertNotIn('evaluateName', row)

    def test_boolean_truth_is_not_an_integer(self):
        for truth in ('true', 'false'):
            row = dap.source_evaluation_display(self.packet('bool=' + truth, 'bool'))
            self.assertEqual((row['result'], row['type']), (truth, 'bool'))

    def test_invalid_carriers_and_stop_packets_are_rejected(self):
        for value in ('i8=128', 'i8=-129', 'u8=-1', 'u8=256', 'i64=9223372036854775808',
                      'u64=18446744073709551616', 'i32=01', 'i32=+1', 'bool=1', 'bool=True'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                dap.source_evaluation_display(self.packet(value))
        for packet in (self.packet('i32=1').replace('41', '0', 1),
                       self.packet('i32=1', origin='source-stop 41\n'),
                       self.packet('i32=1', origin='source-origin stop=42 thread=1 kind=DW_AT_const_value\n'),
                       self.packet('i32=1', name='seed\x1b')):
            with self.subTest(packet=packet), self.assertRaises(ValueError):
                dap.source_evaluation_display(packet)

    def test_origin_receipt_and_compound_display(self):
        packet = self.packet('i32=3', origin='source-origin stop=41 thread=1 kind=DW_AT_const_value\n')
        self.assertEqual(dap.source_evaluation_display(packet)['result'], '3')
        packet = self.packet('i32=3') + 'source local other type=i32 = i32=4\n'
        self.assertEqual(dap.source_evaluation_display(packet)['result'], packet.rstrip('\n'))

    def test_actual_adapter_dispatch_keeps_display_detached(self):
        class Broker(SourceBroker):
            def request(self, command):
                if command == 'print-frame 1 41 0 seed':
                    self.commands.append(command)
                    return 'source-stop 41\nsource parameter seed type=i32 = i32=3\n'
                return super().request(command)
        output = io.BytesIO(); adapter = dap.Adapter(output); adapter.broker = Broker()
        adapter.supports_variable_type = True
        adapter.handle({'seq': 1, 'command': 'stackTrace', 'arguments': {'threadId': 1}})
        frame = next(x for x in messages(output.getvalue()) if x['type'] == 'response')['body']['stackFrames'][0]
        output.seek(0); output.truncate()
        adapter.handle({'seq': 2, 'command': 'evaluate', 'arguments': {'expression': 'seed', 'context': 'watch', 'frameId': frame['id']}})
        reply = next(x for x in messages(output.getvalue()) if x['type'] == 'response')
        self.assertTrue(reply['success']); self.assertEqual((reply['body']['result'], reply['body']['type']), ('3', 'i32'))
        self.assertEqual(adapter.broker.commands[-3:], ['status', 'print-frame 1 41 0 seed', 'status'])


if __name__ == '__main__':
    unittest.main()
