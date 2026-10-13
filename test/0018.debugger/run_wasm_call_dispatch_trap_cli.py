#!/usr/bin/env python3
"""Real indirect/ref call-dispatch failure inputs; verified Linux cgroup only."""
import sys
from pathlib import Path

source_root = Path(sys.argv[sys.argv.index('--source-root') + 1])
sys.path.insert(0, str(source_root / 'test/0018.debugger'))
import run_wasm_trap_stops_cli as traps


def dispatch_examples():
    prefix = ['i64 = -991', 'f32 = bits=0x80000000']
    arguments = ['i32 = -17', 'i64 = -19', 'f32 = bits=0x7fc12345',
                 'f64 = bits=0x8000000000000000',
                 'v128 = bytes=000102030405060708090a0b0c0d0e0f', r'\(ref null extern\) = null']
    declarations = ('(global $p (mut i64) (i64.const -991)) '
                    '(global $z (mut f32) (f32.const -0)) '
                    '(type $f (func (param i32 i64 f32 f64 v128 externref))) '
                    '(type $wrong (func (param i32))) ')
    inputs = ('global.get $p global.get $z i32.const -17 i64.const -19 '
              'f32.const nan:0x412345 f64.const -0 '
              'v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ref.null extern ')
    cases = []
    for tail in (False, True):
        for wide in (False, True):
            width = 'i64' if wide else 'i32'
            for failure, selector, diagnostic in (
                ('bounds', 4294967296 if wide else 1, 'table index out of bounds'),
                ('null', 0, 'uninitialized element'),
                ('mismatch', 0, 'signature mismatch')):
                opcode = 'return_call_indirect' if tail else 'call_indirect'
                name = f'dispatch-{opcode}-{width}-{failure}'
                table = '(table $t ' + ('i64 ' if wide else '') + '1 funcref) '
                element = (f'(elem ({width}.const 0) func $bad)' if failure == 'mismatch' else '')
                tables = [dict(index=0, values=[r'\(ref null func\) = ' +
                          ('function module=0 index=1' if failure == 'mismatch' else 'null')])]
                point = dict(function=0, ordinal=0, values=prefix + arguments + [f'{width} = {selector}'],
                             caller=None, locals_page=None, frame_total=1, tables=tables, members=[])
                cases.append(dict(name=name, expected=[point],
                    diagnostic='Runtime crash (call_indirect: ' + diagnostic + ')',
                    wat='(module ' + declarations + table +
                        '(func (export "_start") ' + inputs + f'{width}.const {selector} nop ' +
                        opcode + ' $t (type $f) unreachable) '
                        '(func $bad (type $wrong)) ' + element + ')'))
        opcode = 'return_call_ref' if tail else 'call_ref'
        point = dict(function=0, ordinal=0,
                     values=prefix + arguments + [r'\(ref null type-index=0 module=0\) = null'],
                     caller=None, locals_page=None, frame_total=1, tables=[], members=[])
        cases.append(dict(name='dispatch-' + opcode + '-null', expected=[point],
            diagnostic='Runtime crash (null reference)',
            wat='(module ' + declarations + '(func (export "_start") ' + inputs +
                'ref.null $f nop ' + opcode + ' $f unreachable))'))
    return cases


original_inspect = traps.containers.inspect

def inspect_dispatch_frame(console, thread, frame, expected, observations):
    original_inspect(console, thread, frame, expected, observations)
    page = traps.inspect_frame_page(console, thread, observations[0]['stop_id'], 1)
    assert page['rows'][0]['function'] == 0, page
    observations[0]['dispatch_frame_page'] = page


traps.trap_examples = dispatch_examples
traps.containers.inspect = inspect_dispatch_frame
if __name__ == '__main__':
    traps.main()
