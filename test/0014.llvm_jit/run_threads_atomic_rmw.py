#!/usr/bin/env python3
"""Batch every new RMW text opcode in two modules for fast development-mode CLI checks."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def literal(value, bits):
    value &= (1 << bits) - 1
    return str(value - (1 << bits) if value >> (bits - 1) else value)


def fixture(indexed):
    functions, checks = [], []
    memory = '1' if indexed else '0'
    for family in ('add', 'sub', 'and', 'or', 'xor', 'xchg', 'cmpxchg'):
        for bits, width in ((32, 4), (64, 8), (32, 1), (32, 2), (64, 1), (64, 2), (64, 4)):
            integer = f'i{bits}'
            suffix = str(width * 8) if width * 8 != bits else ''
            instruction = f'{integer}.atomic.rmw{suffix}.{family}' + ('_u' if suffix else '')
            store = f'{integer}.atomic.store{suffix}'
            load = f'{integer}.atomic.load{suffix}' + ('_u' if suffix else '')
            mask = (1 << (width * 8)) - 1
            initial = 0x8574639281abedcf & mask
            operand = 0x9eab0172cadcdbf8 & ((1 << bits) - 1)
            for matched in ((True, False) if family == 'cmpxchg' else (True,)):
                expected = initial ^ (~mask if matched else 1)
                extra = f'{integer}.const {literal(expected, bits)}' if family == 'cmpxchg' else ''
                expression = {
                    'add': initial + operand, 'sub': initial - operand, 'and': initial & operand,
                    'or': initial | operand, 'xor': initial ^ operand, 'xchg': operand,
                    'cmpxchg': operand if matched else initial,
                }[family] & mask
                name = f'f{len(functions)}'
                functions.append(f'''(func ${name} (param i32) (result {integer})
  local.get 0 {extra} {integer}.const {literal(operand, bits)} {instruction} {memory} offset=8)''')
                checks.append(f'''i32.const 0 {integer}.const {literal(initial, bits)} {store} {memory} offset=8
  i32.const 0 call ${name} {integer}.const {literal(initial, bits)} {integer}.ne if unreachable end
  i32.const 0 {load} {memory} offset=8 {integer}.const {literal(expression, bits)} {integer}.ne if unreachable end''')
    body = '\n'.join(functions)
    main = '\n'.join(checks)
    return f'(module (memory 1 1) {"(memory 1 1)" if indexed else ""}\n{body}\n(func (export "_start")\n{main}))\n', len(functions)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('uwvm', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--backend', choices=('jit', 'int', 'tiered'), default='jit')
    p.add_argument('--wat2wasm', required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--tiered-variant', choices=('all', 'no-t0', 'no-t2', 'no-t0-no-t2'), default='all')
    a = p.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.output.mkdir(parents=True, exist_ok=True)
    rows = []
    modes = ('full',) if a.ros else (('lazy', 'lazy+verification') if a.backend == 'tiered' else ('full', 'lazy', 'lazy+verification'))
    if a.ros and a.backend == 'tiered':
        p.error('ROS supports full backends only')
    policies = ('instruction', 'unwind') if a.backend == 'jit' else ('instruction',)
    for indexed in (False, True):
        name = 'indexed' if indexed else 'legacy'
        wat, cases = fixture(indexed)
        source = a.output / (name + '.wat')
        wasm = source.with_suffix('.wasm')
        source.write_text(wat)
        subprocess.run([a.wat2wasm, '--enable-threads', '--enable-multi-memory', str(source), '-o', str(wasm)], check=True)
        reference = subprocess.run([a.wasmtime, '-C', 'cache=n', str(wasm)], capture_output=True, timeout=60)
        if reference.returncode:
            raise RuntimeError(reference.stderr.decode(errors='replace'))
        rows.append({'case': name + '-wasmtime', 'assertion_pairs': cases, 'passed': True, 'sha256': hashlib.sha256(wasm.read_bytes()).hexdigest()})
        features = ['-WFE-threads'] + (['-WFE-multi-memory'] if indexed else [])
        for mode in modes:
            for policy in policies:
                label = f'{name}-{a.backend}-{mode}-{policy}'
                log = a.output / (label + '.compile.log')
                command = [str(a.uwvm.resolve())]
                command += (['-Raot' if a.backend == 'jit' else '-Rint'] if a.ros else ['-Rcc', a.backend, '-Rcm', mode])
                if a.backend != 'int':
                    command += ['-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy]
                if a.backend == 'tiered':
                    if 'no-t0' in a.tiered_variant:
                        command += ['-Rtiered-disable-t0']
                    if 'no-t2' in a.tiered_variant:
                        command += ['-Rtiered-disable-t2']
                command += ['-Rclog', 'file', str(log), *features, '--run', str(wasm)]
                result = subprocess.run(command, capture_output=True, timeout=90)
                output = (result.stdout + result.stderr).decode(errors='replace')
                (a.output / (label + '.run.log')).write_text(output)
                compiled = log.read_text()
                marker = ('optimize-start' if mode == 'full' else 'compile-end') if a.backend == 'jit' else 'uwvm-int'
                if a.backend == 'tiered':
                    marker = '[llvm-jit-lazy] compile-end' if 'no-t0' in a.tiered_variant else 'uwvm-int'
                if result.returncode or marker not in compiled:
                    raise RuntimeError(f'{label}: exit={result.returncode}\n{output}\n{compiled[-1500:]}')
                rows.append({'case': label, 'assertion_pairs': cases, 'command': command, 'passed': True})
        for flag in ('threads', 'multi-memory') if indexed else ('threads',):
            selected = [f for f in features if f != '-WFE-' + flag] + ['-WFD-' + flag]
            result = subprocess.run([str(a.uwvm.resolve()), '-m', 'validation', *selected, '--run', str(wasm)], capture_output=True, timeout=30)
            output = (result.stdout + result.stderr).decode(errors='replace')
            if result.returncode == 0 or '--wasm-feature-enable-' + flag not in output:
                raise RuntimeError(f'{name} disabled {flag}: {output}')
            rows.append({'case': name + '-disabled-' + flag, 'passed': True})
    (a.output / 'results.json').write_text(json.dumps({'passed': len(rows), 'cases': rows}, indent=2) + '\n')
    print(f'PASS {len(rows)} batched RMW CLI/reference/feature checks, 112 new-syntax execution cases per configuration')


if __name__ == '__main__':
    main()
