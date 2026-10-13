#!/usr/bin/env python3
"""Floating display agrees with copied IEEE bits; no guest authority is inferred."""
import io
import math
import random
import struct
import unittest
from test_dap_source_values import dap, SourceBroker, messages


class DirectFloatDisplayTests(unittest.TestCase):
    def packet(self, width, bits, spelling, kind='ProducerFloatAlias'):
        value = f'f{width} bits=0x{bits}' + ('' if spelling is None else f' value={spelling}')
        return f'source-stop 41\nsource parameter fraction type={kind} = {value}\n'

    def check(self, width, bits, spelling):
        packet = self.packet(width, bits, spelling)
        rows = dap.parse_source_values(packet, 41)
        body = dap.source_evaluation_display(packet)
        self.assertEqual(rows[0]['value'], spelling)
        self.assertEqual((body['result'], body['type']), (spelling, 'ProducerFloatAlias'))
        for row in (rows[0], body):
            self.assertEqual(row['variablesReference'], 0)
            self.assertEqual(row['presentationHint']['attributes'], ['readOnly'])
            self.assertNotIn('memoryReference', row)
            self.assertNotIn('evaluateName', row)

    def test_real_ieee_edge_cases(self):
        examples = {
            32: [('3fa00000', '1.25'), ('bfa00000', '-1.25'), ('0', '0'), ('80000000', '-0'),
                 ('7f7fffff', '3.4028235e+38'), ('00800000', '1.1754944e-38'),
                 ('1', '1e-45'), ('80000001', '-1e-45'), ('7f800000', 'inf'),
                 ('ff800000', '-inf'), ('7f812345', 'nan'), ('ffc23456', 'nan')],
            64: [('3ff4000000000000', '1.25'), ('bff4000000000000', '-1.25'), ('0', '0'),
                 ('8000000000000000', '-0'), ('7fefffffffffffff', '1.7976931348623157e+308'),
                 ('0010000000000000', '2.2250738585072014e-308'), ('1', '5e-324'),
                 ('8000000000000001', '-5e-324'), ('7ff0000000000000', 'inf'),
                 ('fff0000000000000', '-inf'), ('7ff0123456789abc', '-nan'),
                 ('fff823456789abcd', 'nan')],
        }
        for width, rows in examples.items():
            for bits, spelling in rows:
                with self.subTest(width=width, bits=bits):
                    self.check(width, bits, spelling)

    def test_single_rounding_and_ties_to_even(self):
        # These expected neighbors are fixed IEEE values, independently of the
        # conversion under test. The tiny excess is lost via host binary64 or
        # Decimal's default 28-digit arithmetic context.
        cases = [(32, '3f800000', '1.000000059604644775390625'),
                 (32, '3f800001', '1.000000059604644775390626'),
                 (32, '3f800002', '1.000000178813934326171875'),
                 (64, '4330000000000000', '4503599627370496.5'),
                 (64, '4330000000000001', '4503599627370496.500000000000000000000001'),
                 (64, '4330000000000002', '4503599627370497.5')]
        for width, bits, spelling in cases:
            with self.subTest(width=width, spelling=spelling):
                self.check(width, bits, spelling)

    def test_independent_host_ieee_round_trip(self):
        rng = random.Random(0xF10A7)
        for width in (32, 64):
            for _ in range(512):
                bits = rng.getrandbits(width)
                data = bits.to_bytes(width // 8, 'big')
                number = struct.unpack('!f' if width == 32 else '!d', data)[0]
                if not math.isfinite(number):
                    continue
                spelling = format(number, '.9g' if width == 32 else '.17g')
                with self.subTest(width=width, bits=hex(bits)):
                    self.check(width, data.hex(), spelling)

    def test_contradictory_carriers_and_invalid_decimals_fail_closed(self):
        cases = [(32, '3f800000', '2'), (64, '3ff0000000000000', '1.0001'),
                 (32, '0', '-0'), (64, '8000000000000000', '0'),
                 (32, '7f800000', '-inf'), (64, 'fff0000000000000', 'inf'),
                 (32, '7f800000', 'nan'), (64, '0', '-nan'),
                 (32, '7fc12345', '1'), (64, '7ff0000000000000', '1'),
                 (32, '7f7fffff', '3.5e38'), (64, '1', '1e-9999'),
                 (32, '0', '1e-46'), (64, '0', '1e-324'),
                 (32, '100000000', '1')]
        for spelling in ('', '+1', '01', '1.', '.1', '1_0', 'NaN', 'infinity',
                         '1e10000', '1e', '1e+1x', '1 0', '1\r', '1\x1b', '0' * 65):
            cases.append((32, '3f800000', spelling))
        for width, bits, spelling in cases:
            with self.subTest(width=width, bits=bits, spelling=spelling):
                with self.assertRaises(ValueError):
                    dap.parse_source_values(self.packet(width, bits, spelling), 41)
                with self.assertRaises(ValueError):
                    dap.source_evaluation_display(self.packet(width, bits, spelling))

    def test_legacy_bits_remain_opaque(self):
        for width, bits in ((32, '7fa00001'), (64, '8000000000000000')):
            packet = self.packet(width, bits, None)
            row, = dap.parse_source_values(packet, 41)
            self.assertEqual(row['value'], f'f{width} bits=0x{bits}')
            body = dap.source_evaluation_display(packet)
            self.assertEqual(body['result'], packet.rstrip('\n'))
            self.assertNotIn('type', body)

    def test_complete_stop_packet_required(self):
        packet = self.packet(32, '3f800000', '1')
        for invalid in (packet[:-1], packet.replace('41', '42', 1),
                        packet + 'source-stop 41\n',
                        packet + 'source-origin stop=42 thread=1 kind=DW_AT_const_value\n'):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                dap.validate_source_evaluation_reply(invalid, 41, 1)

    def test_direct_copied_primitive_object_display(self):
        for kind, size, spellings in (("float", 4, ("-0", "1e-45", "inf", "nan")),
                                      ("double", 8, ("-0", "5e-324", "-inf", "-nan")),
                                      ("bool", 1, ("true", "false"))):
            for spelling in spellings:
                packet = f"source-value stop=41 name=fraction\nobject: type={kind}, offset=0, bytes={size}, value={spelling}\nsource-value end\n"
                dap.validate_source_evaluation_reply(packet, 41, 1)
                row = dap.source_evaluation_display(packet)
                self.assertEqual((row["result"], row["type"], row["variablesReference"]), (spelling, kind, 0))
                compound = packet.replace("name=fraction", "name=packet.fraction")
                self.assertEqual(dap.source_evaluation_display(compound)["result"], compound.rstrip("\n"))
        for kind, size in (("float", 8), ("double", 4), ("bool", 4)):
            packet = f"source-value stop=41 name=fraction\nobject: type={kind}, offset=0, bytes={size}, value=0\nsource-value end\n"
            with self.assertRaises(ValueError):
                dap.source_evaluation_display(packet)

    def test_source_variables_and_watch_dispatch_authenticates_stop(self):
        class Broker(SourceBroker):
            def request(self, command):
                if command == 'print-frame 1 41 0 fraction':
                    self.commands.append(command)
                    return 'source-stop 41\nsource parameter fraction type=float = f32 bits=0x80000000 value=-0\n'
                return super().request(command)
        broker = Broker()
        broker.rows = 'source parameter fraction type=float = f32 bits=0x80000000 value=-0\n'
        output = io.BytesIO()
        adapter = dap.Adapter(output); adapter.broker = broker
        adapter.supports_variable_type = True
        seq = 0
        def request(command, **arguments):
            nonlocal seq
            seq += 1; output.seek(0); output.truncate()
            adapter.handle({'seq': seq, 'command': command, 'arguments': arguments})
            return next(x for x in messages(output.getvalue()) if x['type'] == 'response')
        frame = request('stackTrace', threadId=1)['body']['stackFrames'][0]['id']
        scope = request('scopes', frameId=frame)['body']['scopes'][0]['variablesReference']
        rows = request('variables', variablesReference=scope)
        self.assertTrue(rows['success']); self.assertEqual(rows['body']['variables'][0]['value'], '-0')
        body = request('evaluate', expression='fraction', context='watch', frameId=frame)
        self.assertTrue(body['success']); self.assertEqual((body['body']['result'], body['body']['type']), ('-0', 'float'))
        self.assertEqual(broker.commands[-3:], ['status', 'print-frame 1 41 0 fraction', 'status'])
        broker.stop_id += 1
        self.assertFalse(request('evaluate', expression='fraction', context='watch', frameId=frame)['success'])
        self.assertFalse(request('variables', variablesReference=scope)['success'])


if __name__ == '__main__':
    unittest.main()
