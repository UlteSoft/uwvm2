#!/usr/bin/env python3
"""Indexed stores that cross a committed memory boundary must write no prefix."""
import argparse
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--wat2wasm', type=Path, required=True)
p.add_argument('--include-fused-inputs', action='store_true', help='also exercise local-value and same-memory copy store fusion')
p.add_argument('--memory64', action='store_true', help='use i64 addresses and memory64 declarations')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=True)
cases = [
    ('i32.store16', 2, '(i32.const -1)', ''),
    ('i64.store16', 2, '(i64.const -1)', ''),
    ('i32.store', 4, '(i32.const -1)', ''),
    ('i64.store32', 4, '(i64.const -1)', ''),
    ('f32.store', 4, '(f32.const -1)', ''),
    ('i64.store', 8, '(i64.const -1)', ''),
    ('f64.store', 8, '(f64.const -1)', ''),
    ('v128.store', 16, '(v128.const i32x4 -1 -1 -1 -1)', ''),
    ('v128.store16_lane', 2, '(v128.const i32x4 -1 -1 -1 -1)', ' 3'),
    ('v128.store32_lane', 4, '(v128.const i32x4 -1 -1 -1 -1)', ' 2'),
    ('v128.store64_lane', 8, '(v128.const i32x4 -1 -1 -1 -1)', ' 1'),
]
expanded = [(op, width, value, lane, '', '', '') for op, width, value, lane in cases]
if a.include_fused_inputs:
    for op, width, value, lane in cases:
        if op.startswith('v128.'):
            continue
        ty = op.split('.')[0]
        expanded.append((op, width, '(local.get $value)', lane, '-local-value',
                         f'(local $value {ty})', f'(local.set $value {value})'))
    zero_address = '(i64.const 0)' if a.memory64 else '(i32.const 0)'
    for store, width, load in (
        ('i32.store16', 2, 'i32.load16_u'), ('i64.store16', 2, 'i64.load16_u'),
        ('i32.store', 4, 'i32.load'), ('i64.store32', 4, 'i64.load32_u'),
        ('f32.store', 4, 'f32.load'), ('i64.store', 8, 'i64.load'),
        ('f64.store', 8, 'f64.load'), ('i64.store', 8, 'i64.load32_u')):
        expanded.append((store, width, f'({load} 1 {zero_address})', '',
                         '-copy-' + load.replace('.', '-'), '', ''))
rows = []
for op, width, value, lane, suffix, local, setup in expanded:
    # Memory 0 is larger: accidentally selecting it would complete instead of
    # trapping. The memarg alignment is a hint; the address is a runtime input.
    memory_type = ' i64' if a.memory64 else ''
    address_type = 'i64' if a.memory64 else 'i32'
    wat = f'''(module (memory{memory_type} 2 2) (memory{memory_type} 1 2)
      (func (export "run") (param {address_type}) (result i32) {local}
        {setup} local.get 0 {value} {op} 1{lane} i32.const 0))'''
    path = a.output / (op.replace('.', '-') + suffix + ('-m64' if a.memory64 else '') + '.wat')
    path.write_text(wat + '\n')
    binary = path.with_suffix('.wasm')
    command = [str(a.wat2wasm), '--enable-multi-memory']
    if a.memory64:
        command.append('--enable-memory64')
    subprocess.run([*command, str(path), '-o', str(binary)], check=True)
    rows.append(dict(opcode=op + suffix, store_opcode=op, width=width,
                     memory64=a.memory64, wasm=str(binary.resolve())))
(a.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
