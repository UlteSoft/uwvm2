#!/usr/bin/env python3
"""Verify terminal traps and exact pre-trap inputs for every cold GC access opcode."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '0018.debugger'))
import run_wasm_trap_stops_cli as traps


def examples():
    cases = []
    prefix = ['i64 = -991', 'f32 = bits=0x80000000']
    declarations = '(type $s (struct (field (mut i32)))) (type $a (array (mut i32)))'
    packed = '(type $s (struct (field (mut i8)))) (type $a (array (mut i8)))'
    struct_null = r'\(ref null type-index=0 module=0\) = null'
    array_null = r'\(ref null type-index=1 module=0\) = null'

    def add(name, types, inputs, opcode, values, diagnostic='null reference'):
        point = dict(function=0, ordinal=0, values=prefix + values, caller=None,
                     locals_page=None, frame_total=None, tables=[], members=[])
        cases.append(dict(name=name, expected=[point], diagnostic='Runtime crash (' + diagnostic + ')',
            wat='(module (global $p (mut i64) (i64.const -991)) '
                '(global $z (mut f32) (f32.const -0)) ' + types +
                ' (func (export "_start") global.get $p global.get $z ' + inputs +
                ' nop ' + opcode + ' unreachable))'))

    for suffix in ('', '_s', '_u'):
        types = packed if suffix else declarations
        add('gc-trap-struct-get' + suffix, types, 'ref.null $s', 'struct.get' + suffix + ' $s 0', [struct_null])
        add('gc-trap-array-get' + suffix, types, 'ref.null $a i32.const 2',
            'array.get' + suffix + ' $a', [array_null, 'i32 = 2'])
    add('gc-trap-struct-set', declarations, 'ref.null $s i32.const 37',
        'struct.set $s 0', [struct_null, 'i32 = 37'])
    add('gc-trap-array-set', declarations, 'ref.null $a i32.const 2 i32.const 37',
        'array.set $a', [array_null, 'i32 = 2', 'i32 = 37'])
    add('gc-trap-array-len', declarations, 'ref.null $a', 'array.len', [array_null])
    array = r'\(ref type-index=1 module=0\) = array #1'
    add('gc-trap-array-get-bounds', declarations, 'i32.const 0 i32.const 2 array.new $a i32.const 2',
        'array.get $a', [array, 'i32 = 2'], 'array access out of bounds')
    add('gc-trap-array-set-bounds', declarations, 'i32.const 0 i32.const 2 array.new $a i32.const 2 i32.const 37',
        'array.set $a', [array, 'i32 = 2', 'i32 = 37'], 'array access out of bounds')
    return cases


if __name__ == '__main__':
    traps.trap_examples = examples
    traps.main()
