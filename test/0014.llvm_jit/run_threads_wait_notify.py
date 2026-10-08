#!/usr/bin/env python3
"""Actual wait32/wait64/notify CLI syntax, feature gates, traps and Wasmtime comparison.

Real concurrent wakes and memory growth are covered by wasm_execution_domain.cc
and wasm_wait_import_aliases.cc. These bounded guest programs test the frontend,
start section and demand compilation without requiring a nonstandard spawn import.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('uwvm', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--wat2wasm', required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--backend', choices=['jit', 'int', 'tiered'], default='jit')
    a = p.parse_args()
    if a.ros and a.backend == 'tiered':
        p.error('ROS has no tiered backend')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=False)
    a.uwvm = a.uwvm.resolve()
    binary_hash = hashlib.sha256(a.uwvm.read_bytes()).hexdigest()
    rows = []

    def run(label, command, diagnostic=None):
        result = subprocess.run(command, capture_output=True, timeout=45)
        output = (result.stdout + result.stderr).decode(errors='replace')
        (a.output / (label + '.log')).write_text(output)
        passed = result.returncode == 0 if diagnostic is None else result.returncode != 0 and diagnostic in output
        rows.append(dict(case=label, command=command, exit=result.returncode, passed=passed))
        (a.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{label}: exit={result.returncode}, wanted={diagnostic!r}\n{output}')
        return output

    versions = {name: run(name + '-version', [path, '--version']) for name, path in
                [('wat2wasm', a.wat2wasm), ('wasmtime', a.wasmtime)]}
    (a.output / 'toolchain.json').write_text(json.dumps(versions, indent=2) + '\n')
    modes = ['full'] if a.ros else ['full', 'lazy', 'lazy+verification']
    configs = [(mode, []) for mode in modes]
    if a.backend == 'tiered':
        configs = [('all', []), ('no-t0', ['-Rtiered-disable-t0']), ('no-t2', ['-Rtiered-disable-t2']),
                   ('no-t0-no-t2', ['-Rtiered-disable-t0', '-Rtiered-disable-t2'])]
    policies = ['instruction', 'unwind'] if a.backend != 'int' else ['instruction']
    fixtures = []
    for indexed in [False, True]:
        memory = '(memory 1 1) (memory 1 2 shared)' if indexed else '(memory 1 2 shared)'
        index = int(indexed)  # WABT 1.0.41 named atomic memargs assert; numeric indices are equivalent.
        data = f'(data (memory {index}) (i32.const 16) "\\11\\22\\33\\c4\\55\\66\\77\\88")'
        # High-bit expected values test the unsigned native bridge ABI. Negative
        # timeouts only occur on mismatches, so no successful run can hang.
        checks = f'''
    i32.const 13 i64.const 0x9988776655443322 f64.const 7.5
    i32.const 8 i32.const 0xc4332211 i64.const 0 memory.atomic.wait32 {index} offset=8
    i32.const 2 i32.ne if unreachable end
    f64.const 7.5 f64.ne if unreachable end
    i64.const 0x9988776655443322 i64.ne if unreachable end
    i32.const 13 i32.ne if unreachable end
    i32.const 16 i64.const 0x88776655c4332211 i64.const 1 memory.atomic.wait64 {index}
    i32.const 2 i32.ne if unreachable end
    i32.const 16 i32.const 0 i64.const -1 memory.atomic.wait32 {index}
    i32.const 1 i32.ne if unreachable end
    i32.const 16 i64.const 0 i64.const -9223372036854775808 memory.atomic.wait64 {index}
    i32.const 1 i32.ne if unreachable end
    i32.const 8 i32.const 0 memory.atomic.notify {index} offset=8
    i32.eqz if else unreachable end
    i32.const 16 i32.const -1 memory.atomic.notify {index}
    i32.eqz if else unreachable end
    i32.const 65528 i64.const 0 i64.const 0 memory.atomic.wait64 {index}
    i32.const 2 i32.ne if unreachable end
    i32.const 65532 i32.const 0 i64.const 0 memory.atomic.wait32 {index}
    i32.const 2 i32.ne if unreachable end
    i32.const 65532 i32.const -1 memory.atomic.notify {index}
    i32.eqz if else unreachable end'''
        for start in [False, True]:
            entry = '(start $probe) (func (export "_start"))' if start else '(func (export "_start") call $probe)'
            name = f'wait-values-{"indexed" if indexed else "legacy"}-{"start" if start else "entry"}'
            fixtures.append((name, f'(module {memory} {data} (func $probe {checks}) {entry})', indexed, None, None))
    fixtures.append(('notify-unshared', '(module (memory 1 1) (func (export "_start") i32.const 0 i32.const -1 memory.atomic.notify i32.eqz if else unreachable end))', False, None, None))
    for opcode in ['memory.atomic.wait32', 'memory.atomic.wait64', 'memory.atomic.notify']:
        expected = 'i64.const 0' if opcode.endswith('64') else 'i32.const 0'
        operands = 'i32.const 0' if opcode.endswith('notify') else expected + ' i64.const 0'
        cases = [(1, 0, 'unaligned', 'unaligned atomic memory access', 'unaligned atomic'),
                 (65536, 0, 'bounds', 'memory access out of bounds', 'out of bounds'),
                 (0xfffffff8, 16, 'u33', 'memory access out of bounds', 'out of bounds')]
        for address, offset, name, uwvm_error, reference_error in cases:
            wat = f'(module (memory 1 1 shared) (func $test i32.const {address} {operands} {opcode} offset={offset} drop) (func (export "_start") call $test))'
            fixtures.append((opcode.replace('.', '-') + '-' + name, wat, False, uwvm_error, reference_error))
        if not opcode.endswith('notify'):
            wat = f'(module (memory 1 1) (func (export "_start") i32.const 0 {operands} {opcode} drop))'
            fixtures.append((opcode.replace('.', '-') + '-unshared', wat, False, 'atomic wait on non-shared memory', 'non-shared memory'))
    for name, wat, indexed, trap, ref_trap in fixtures:
        source = a.output / (name + '.wat')
        binary = source.with_suffix('.wasm')
        source.write_text(wat + '\n')
        run(name + '-assemble', [a.wat2wasm, '--enable-threads', '--enable-multi-memory', str(source), '-o', str(binary)])
        run(name + '-wasmtime', [a.wasmtime, '-C', 'cache=n', '-W', 'threads=y,shared-memory=y,multi-memory=y', str(binary)], ref_trap)
        features = ['-WFE-threads'] + (['-WFE-multi-memory'] if indexed else [])
        for mode, extra in configs:
            for policy in policies:
                label = f'{name}-{a.backend}-{mode}-{policy}'
                log = a.output / (label + '.compile.log')
                if a.ros:
                    base = ['-Raot' if a.backend == 'jit' else '-Rint']
                elif a.backend == 'tiered':
                    base = ['-Rtiered', *extra]
                else:
                    base = ['-Rcc', a.backend, '-Rcm', mode]
                if a.backend != 'int':
                    base += ['-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy]
                command = [str(a.uwvm), *base, '-Rclog', 'file', str(log), *features, '--run', str(binary)]
                run(label, command, trap)
                # Success must exercise compilation, not stop after parsing. Trap
                # runs may terminate before a buffered compile logger is flushed.
                if trap is None and (not log.is_file() or not any(marker in log.read_text() for marker in (['event=stats.func', 'compile-end'] if a.backend == 'int' else ['compile-end', 'optimize-start']))):
                    raise RuntimeError(f'{label}: missing compilation evidence')
        if trap is None:
            for flag in (['threads', 'multi-memory'] if indexed else ['threads']):
                disabled = [f for f in features if f != '-WFE-' + flag] + ['-WFD-' + flag]
                run(name + '-disabled-' + flag, [str(a.uwvm), '-m', 'validation', *disabled, '--run', str(binary)],
                    '--wasm-feature-enable-' + flag)
    assert binary_hash == hashlib.sha256(a.uwvm.read_bytes()).hexdigest(), 'CLI changed during tests'
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (a.output / 'summary.json').write_text(json.dumps(dict(passed=True, fixtures=len(fixtures), checks=len(rows),
        uwvm_sha256=binary_hash, ros=a.ros, backend=a.backend, configs=configs, policies=policies), indent=2) + '\n')
    print(f'PASS {len(fixtures)} wait/notify guest programs, {len(rows)} CLI/reference/feature checks')


if __name__ == '__main__':
    main()
