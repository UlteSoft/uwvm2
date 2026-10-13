#!/usr/bin/env python3
"""Identical legacy and Core 3 indexed-memory loops for runtime/assembly comparison.

The aligned scalar and SIMD kernels span 256 KiB. The unaligned scalar kernel
also crosses Wasm pages, exercising the production store preflight instead of
timing only its constant-folded aligned special case. Every load contributes to
the returned checksum. WAT conversion and validation run in the Linux cgroup.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def pattern_byte(index):
    value = (index + 0x9e3779b9) & 0xffffffff
    value = ((value ^ (value >> 16)) * 0x7feb352d) & 0xffffffff
    value = ((value ^ (value >> 15)) * 0x846ca68b) & 0xffffffff
    return (value ^ (value >> 16)) & 255


def checksum(kernel, count):
    expected = 0
    for value in range(count):
        expected ^= value
        if kernel.startswith('load-'):
            offset = ((value * 16) & 262128) + (13 if 'unaligned' in kernel else 0)
            for lane in range(4 if kernel.startswith('load-simd-') else 1):
                for byte in range(4):
                    expected ^= pattern_byte(offset + 4 * lane + byte) << (8 * byte)
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--wat2wasm', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, help='independent reference for all generated kernels')
    parser.add_argument('--include-independent-loads', action='store_true',
                        help='add disjoint initialized reads that cannot forward from the preceding store')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.output.mkdir(parents=True, exist_ok=True)
    reference = []
    kernels = ['scalar-aligned', 'scalar-unaligned', 'simd-aligned']
    if args.include_independent_loads:
        kernels += ['load-' + kernel for kernel in kernels.copy()]
    data = ''.join('\\' + format(pattern_byte(i), '02x') for i in range(5 * 65536)) if args.include_independent_loads else ''
    for kernel in kernels:
        independent = kernel.startswith('load-')
        shape = kernel.removeprefix('load-')
        for indexed in (False, True):
            memory = '1' if indexed else '0'
            pages = 10 if independent else 5
            memories = f'(memory {pages} {pages})' * (2 if indexed else 1)
            # The fifth page keeps the last unaligned access valid, while every
            # other 64 KiB boundary is crossed by the unaligned kernel.
            address = '(i32.and (i32.shl (local.get $i) (i32.const 4)) (i32.const 262128))'
            if shape == 'scalar-unaligned':
                address = f'(i32.add {address} (i32.const 13))'
            if shape == 'simd-aligned':
                access = f'''(v128.store {memory} (local.get $address) (i32x4.splat (local.get $i)))
                (local.set $sum (i32.xor (local.get $sum)
                    (i32x4.extract_lane 0 (v128.load {memory} (local.get $address)))))'''
            else:
                access = f'''(i32.store {memory} (local.get $address) (local.get $i))
                (local.set $sum (i32.xor (local.get $sum) (i32.load {memory} (local.get $address))))'''
            locals_extra = ''
            segment = ''
            if independent:
                read_address = '(i32.add (local.get $address) (i32.const 327680))'
                segment = f'(data (memory {memory}) (i32.const 327680) "{data}")'
                if shape == 'simd-aligned':
                    locals_extra = '(local $read v128)'
                    folded = '(i32.xor (i32.xor (i32x4.extract_lane 0 (local.get $read)) (i32x4.extract_lane 1 (local.get $read))) (i32.xor (i32x4.extract_lane 2 (local.get $read)) (i32x4.extract_lane 3 (local.get $read))))'
                    access = f'''(v128.store {memory} (local.get $address) (i32x4.splat (local.get $i)))
                        (local.set $read (v128.load {memory} {read_address}))
                        (local.set $sum (i32.xor (local.get $sum) (i32.xor (local.get $i) {folded})))'''
                else:
                    access = f'''(i32.store {memory} (local.get $address) (local.get $i))
                        (local.set $sum (i32.xor (local.get $sum)
                            (i32.xor (local.get $i) (i32.load {memory} {read_address}))))'''
            wat = f'''(module
              {memories}
              {segment}
              (func (export "run") (param $count i32) (result i32)
                (local $i i32) (local $address i32) (local $sum i32) {locals_extra}
                (loop $again
                  (local.set $address {address})
                  {access}
                  (local.set $i (i32.add (local.get $i) (i32.const 1)))
                  (br_if $again (i32.lt_u (local.get $i) (local.get $count))))
                (local.get $sum)))
            '''
            name = kernel + ('-indexed' if indexed else '-legacy')
            path = args.output / (name + '.wat')
            path.write_text(wat)
            subprocess.run([str(args.wat2wasm), '--enable-multi-memory', str(path),
                            '-o', str(path.with_suffix('.wasm'))], check=True)
            if args.wasmtime:
                # Nonzero, independently calculated checksums detect a missing
                # loop or discarded memory result. Include an actual Wasm-page
                # crossing in the unaligned kernel and a working-set wrap.
                for count in (1003, 20003):
                    expected = checksum(kernel, count)
                    if independent and (expected == 0 or expected == checksum(shape, count)):
                        raise RuntimeError('independent read must affect the reference checksum')
                    command = [str(args.wasmtime), '-C', 'cache=n', '--invoke', 'run',
                               str(path.with_suffix('.wasm')), str(count)]
                    run = subprocess.run(command, capture_output=True, text=True, timeout=60)
                    passed = run.returncode == 0 and (int(run.stdout.strip()) & 0xffffffff) == expected
                    reference.append(dict(fixture=name, count=count, expected=expected, command=command,
                        exit=run.returncode, stdout=run.stdout, stderr=run.stderr, passed=passed,
                        wasm_sha256=hashlib.sha256(path.with_suffix('.wasm').read_bytes()).hexdigest()))
                    (args.output / 'reference.json').write_text(json.dumps(reference, indent=2) + '\n')
                    if not passed:
                        raise RuntimeError(f'independent reference mismatch: {name}, count={count}')
    if reference:
        print(f'PASS {len(reference)} independent Wasmtime memory-kernel executions')


if __name__ == '__main__':
    main()
