#!/usr/bin/env python3
"""Native primitive display DATA; live producer/stop qualification is separate."""
import io
import unittest
from test_dap_source_values import dap, SourceBroker, messages


class PrimitiveVariableDisplayTests(unittest.TestCase):
    def packet(self, carrier, name='value', kind='ProducerAlias'):
        return f'source-stop 41\nsource parameter {name} type={kind} = {carrier}\n'

    def check(self, packet, expected, kind='ProducerAlias'):
        row, = dap.parse_source_values(packet, 41)
        body = dap.source_evaluation_display(packet)
        for record, key in ((row, 'value'), (body, 'result')):
            self.assertEqual((record[key], record['type']), (expected, kind))
            self.assertEqual(record['variablesReference'], 0)
            self.assertEqual(record['presentationHint']['attributes'], ['readOnly'])
            self.assertNotIn('evaluateName', record)
            self.assertNotIn('memoryReference', record)

    def test_signed_unsigned_widths_exact_and_consistent(self):
        for width in (8, 16, 32, 64):
            for signed in (True, False):
                tag = ('i' if signed else 'u') + str(width)
                lo, hi = (-(1 << (width-1)), (1 << (width-1))-1) if signed else (0, (1 << width)-1)
                for number in (lo, hi, 0):
                    with self.subTest(tag=tag, number=number):
                        self.check(self.packet(f'{tag}={number}'), str(number))

    def test_boolean_remains_truth_not_integer(self):
        for truth in ('true', 'false'):
            self.check(self.packet('bool='+truth), truth)
        for carrier in ('bool=0', 'bool=1', 'bool=True', 'bool=FALSE'):
            with self.subTest(carrier=carrier), self.assertRaises(ValueError):
                dap.parse_source_values(self.packet(carrier), 41)

    def test_mixed_scalars_opaque_labels_and_legacy_float(self):
        packet = ('source-stop 41\nsource parameter count type=Alias = u64=18446744073709551615\n'
                  'source local enabled type=OpaqueBool = bool=true\n'
                  'source local zero type=OpaqueFloat = f32 bits=0x80000000 value=-0\n'
                  'source local fraction type=OpaqueFloat = f32 bits=0x7fa00001\n'
                  'source local x\\x1b type=Alias\\x1b = i32=3\n'
                  'source local absent type=Alias = unavailable (no location)\n')
        rows = dap.parse_source_values(packet, 41)
        self.assertEqual([r['value'] for r in rows], ['18446744073709551615', 'true', '-0', 'f32 bits=0x7fa00001', '3', 'unavailable (no location)'])
        self.assertEqual(rows[4]['name'], r'x\x1b')
        self.assertEqual(rows[4]['type'], r'Alias\x1b')
        self.assertTrue(all(r['variablesReference']==0 and 'evaluateName' not in r and 'memoryReference' not in r for r in rows))

    def test_no_partial_prefix_when_one_width_is_invalid(self):
        for bad in ('i8=128', 'i16=-32769', 'u32=4294967296', 'u64=-1', 'u8=-0', 'i64=9223372036854775808', 'i32=01'):
            packet = self.packet('i32=1') + f'source local broken type=Alias = {bad}\n'
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                dap.parse_source_values(packet, 41)

    def test_copied_integer_object_primitives_and_widths(self):
        # Formatter-shaped DATA controls. Live named integer producers may use
        # source-stop numeric carriers; these tests do not fabricate an object
        # location, guest memory read or real expression transaction.
        types = [('int', (1,2,4,8), False), ('unsigned int',(4,),True),
                 ('signed integer',(1,2,4,8),False), ('unsigned integer',(1,2,4,8),True),
                 ('signed char',(1,),False), ('unsigned char',(1,),True),
                 ('short',(2,),False), ('unsigned short',(2,),True),
                 ('long',(4,8),False), ('unsigned long',(4,8),True),
                 ('long long',(8,),False), ('unsigned long long',(8,),True)]
        for kind, sizes, unsigned in types:
            for size in sizes:
                lo, hi = (0,(1 << (size*8))-1) if unsigned else (-(1 << (size*8-1)),(1 << (size*8-1))-1)
                for number in (lo,hi):
                    packet = f'source-value stop=41 name=value\nobject: type={kind}, offset=0, bytes={size}, value={number}\nsource-value end\n'
                    dap.validate_source_evaluation_reply(packet, 41, 1)
                    row = dap.source_evaluation_display(packet)
                    self.assertEqual((row['result'],row['type'],row['variablesReference']), (str(number),kind,0))
                    self.assertNotIn('evaluateName',row); self.assertNotIn('memoryReference',row)
                    compound = packet.replace('name=value','name=packet.value')
                    self.assertEqual(dap.source_evaluation_display(compound)['result'],compound.rstrip('\n'))

    def test_copied_integer_object_bad_size_and_bounds_rejected(self):
        for row in ('object: type=int, offset=0, bytes=4, value=2147483648',
                    'object: type=unsigned long long, offset=0, bytes=8, value=-1',
                    'object: type=signed char, offset=0, bytes=2, value=1',
                    'object: type=unsigned short, offset=0, bytes=2, value=65536',
                    'object: type=long, offset=0, bytes=4, value=-2147483649',
                    'object: type=long long, offset=0, bytes=4, value=1',
                    'object: type=unsigned integer, offset=0, bytes=8, value=18446744073709551616',
                    'object: type=int, offset=1, bytes=4, value=1',
                    'object: type=int, offset=0, bytes=4, value=01'):
            packet=f'source-value stop=41 name=value\n{row}\nsource-value end\n'
            with self.subTest(row=row), self.assertRaises(ValueError):
                dap.source_evaluation_display(packet)

    def test_actual_dispatch_preserves_stop_and_no_read_authority(self):
        class Broker(SourceBroker):
            def request(self, command):
                if command=='print-frame 1 41 0 value':
                    self.commands.append(command)
                    return 'source-stop 41\nsource parameter value type=ProducerAlias = u64=18446744073709551615\n'
                return super().request(command)
        broker=Broker();broker.rows='source parameter value type=ProducerAlias = u64=18446744073709551615\n'
        output=io.BytesIO();adapter=dap.Adapter(output);adapter.broker=broker;adapter.supports_variable_type=True
        seq=0
        def request(command,**arguments):
            nonlocal seq
            seq+=1;output.seek(0);output.truncate()
            adapter.handle(dict(seq=seq,command=command,arguments=arguments))
            return next(m for m in messages(output.getvalue()) if m['type']=='response')
        frame=request('stackTrace',threadId=1)['body']['stackFrames'][0]['id']
        scope=request('scopes',frameId=frame)['body']['scopes'][0]['variablesReference']
        self.assertEqual(request('variables',variablesReference=scope)['body']['variables'][0]['value'],'18446744073709551615')
        reply=request('evaluate',expression='value',context='watch',frameId=frame)
        self.assertEqual((reply['body']['result'],reply['body']['type']),('18446744073709551615','ProducerAlias'))
        self.assertEqual(broker.commands[-3:],['status','print-frame 1 41 0 value','status'])
        broker.stop_id+=1
        self.assertFalse(request('evaluate',expression='value',context='watch',frameId=frame)['success'])
        self.assertFalse(request('variables',variablesReference=scope)['success'])


if __name__=='__main__':
    unittest.main()
