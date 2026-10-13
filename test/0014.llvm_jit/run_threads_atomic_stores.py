#!/usr/bin/env python3
"""Narrow new-syntax CLI smoke: every atomic store, feature policy, and demand compilation."""
import argparse
import hashlib
import json
import pathlib
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uwvm', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--wat2wasm', required=True)
    parser.add_argument('--wasmtime', required=True)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    variants = [('i32.atomic.store', 4), ('i64.atomic.store', 8), ('i32.atomic.store8', 1),
                ('i32.atomic.store16', 2), ('i64.atomic.store8', 1),
                ('i64.atomic.store16', 2), ('i64.atomic.store32', 4)]
    modes = ('full',) if args.ros else ('full', 'lazy', 'lazy+verification')
    for instruction, width in variants:
        for indexed in (False, True):
            integer = instruction[:3]
            data = bytes((0x85 + i * 17) % 256 for i in range(8))
            value = int.from_bytes(data[:width], 'little')
            value = (value * 2) % (1 << int(integer[1:]))
            if value >> (int(integer[1:]) - 1):
                value -= 1 << int(integer[1:])
            name = instruction.replace('.', '-') + ('-indexed' if indexed else '-legacy')
            escaped = ''.join('\\%02x' % byte for byte in data)
            extra_memory = '(memory $m 1 1)' if indexed else ''
            target = '1' if indexed else '0'  # WABT 1.0.41 asserts for named atomic memargs.
            input_value = 0x91827364a5b6c7d8 & ((1 << int(integer[1:])) - 1)
            stored = input_value & ((1 << (width * 8)) - 1)
            if stored >> (int(integer[1:]) - 1):
                stored -= 1 << int(integer[1:])
            if input_value >> (int(integer[1:]) - 1):
                input_value -= 1 << int(integer[1:])
            load = instruction.replace('store', 'load') + ('_u' if width * 8 < int(integer[1:]) else '')
            # Live i32 operand survives the store; lazy modes must compile the callee.
            wat = f'''(module
  (memory 1 1) {extra_memory}
  (func $store (param i32 {integer}) (result i32)
    i32.const 87 local.get 0 local.get 1 {instruction} {target} offset=8)
  (func (export "_start")
    i32.const 0 {integer}.const {input_value} call $store i32.const 87 i32.ne
    if unreachable end
    i32.const 0 {load} {target} offset=8 {integer}.const {stored} {integer}.ne
    if unreachable end))\n'''
            source = args.output / (name + '.wat')
            binary = source.with_suffix('.wasm')
            source.write_text(wat)
            subprocess.run([args.wat2wasm, '--enable-threads', '--enable-multi-memory', str(source), '-o', str(binary)], check=True)
            reference = subprocess.run([args.wasmtime, '-C', 'cache=n', str(binary)], capture_output=True, timeout=30)
            if reference.returncode:
                raise RuntimeError(reference.stderr.decode(errors='replace'))
            rows.append({'case': name + '-wasmtime', 'passed': True, 'wasm_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
            for mode in modes:
                for policy in ('instruction', 'unwind'):
                    label = f'{name}-{mode}-{policy}'
                    log = args.output / (label + '.compile.log')
                    base = [str(args.uwvm.resolve())] + (['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', mode])
                    base += ['-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy, '-Rclog', 'file', str(log)]
                    flags = ['-WFE-threads'] + (['-WFE-multi-memory'] if indexed else [])
                    command = base + flags + ['--run', str(binary)]
                    result = subprocess.run(command, capture_output=True, timeout=30)
                    output = (result.stdout + result.stderr).decode(errors='replace')
                    (args.output / (label + '.run.log')).write_text(output)
                    compiled = log.read_text()
                    if result.returncode or ('optimize-start' if mode == 'full' else 'compile-end') not in compiled:
                        raise RuntimeError(f'{label}: exit={result.returncode}\n{output}\n{compiled}')
                    rows.append({'case': label, 'command': command, 'passed': True})
            for flag in (['threads', 'multi-memory'] if indexed else ['threads']):
                features = ['-WFE-threads'] + (['-WFE-multi-memory'] if indexed else [])
                features.remove('-WFE-' + flag)
                features.append('-WFD-' + flag)
                command = [str(args.uwvm.resolve()), '-m', 'validation', *features, '--run', str(binary)]
                result = subprocess.run(command, capture_output=True, timeout=30)
                output = (result.stdout + result.stderr).decode(errors='replace')
                if result.returncode == 0 or '--wasm-feature-enable-' + flag not in output:
                    raise RuntimeError(f'{name} disabled {flag}: {output}')
                rows.append({'case': name + '-disabled-' + flag, 'passed': True})
    (args.output / 'results.json').write_text(json.dumps({'passed': len(rows), 'cases': rows}, indent=2) + '\n')
    print(f'PASS {len(rows)} atomic-store CLI/reference/feature checks')


if __name__ == '__main__':
    main()
