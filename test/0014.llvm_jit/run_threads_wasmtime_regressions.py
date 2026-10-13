#!/usr/bin/env python3
"""Execute pinned Wasmtime wait/notify regressions without rewriting their guest code.

wast2json assembles the original official syntax. An appended _start calls each
export and checks integer results; trap checks require the specific diagnostic.
The three pinned files have no imports or state-changing assertion histories.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from run_wasm3_multi_memory_spec import leb, read_leb, constant

COMMIT = '3f3f222b77a198db939863d8769af6092ee547e6'
SOURCES = {
    'atomics_notify.wast': '63804099567c25313be20a9963629dc8f0fe6771a6dd643f6f40f12c38b8fe01',
    'atomics_wait_address.wast': '2dc018477bdbdba947a769761e16488f1cdc152161d3a38d18e7f12a7f7e158c',
    'atomics-end-of-memory.wast': '5f9d9186269b736ffa788a5603b41f3d5962459c4d0327e0a1491b8025fe368e',
}


def wrapper(data, assertion):
    assert data[:8] == b'\0asm\1\0\0\0'
    sections = {}
    pos = 8
    while pos < len(data):
        kind = data[pos]
        size, begin = read_leb(data, pos + 1)
        pos = begin + size
        assert pos <= len(data)
        if kind:
            assert kind not in sections
            sections[kind] = data[begin:pos]
    assert 2 not in sections and 8 not in sections, 'pinned modules have no imports or start section'
    types, pos = read_leb(sections[1], 0)
    sections[1] = leb(types + 1) + sections[1][pos:] + b'\x60\0\0'
    funcs, pos = read_leb(sections[3], 0)
    sections[3] = leb(funcs + 1) + sections[3][pos:] + leb(types)
    exports, pos = read_leb(sections[7], 0)
    original_exports = sections[7][pos:]
    functions = {}
    for _ in range(exports):
        size, pos = read_leb(sections[7], pos)
        name = sections[7][pos:pos + size].decode()
        pos += size
        kind = sections[7][pos]
        index, pos = read_leb(sections[7], pos + 1)
        if kind == 0:
            functions[name] = index
    assert '_start' not in functions
    sections[7] = leb(exports + 1) + original_exports + b'\x06_start\0' + leb(funcs)
    action = assertion['action']
    assert action['type'] == 'invoke' and 'module' not in action
    body = b'\0' + b''.join(constant(v) for v in action['args']) + b'\x10' + leb(functions[action['field']])
    if assertion['type'] == 'assert_trap':
        # An unexpected return must fail with unreachable, never masquerade as the
        # requested alignment/bounds/sharedness trap. Surplus results are polymorphic.
        body += b'\0'
    else:
        assert assertion['type'] == 'assert_return'
        for value in reversed(assertion['expected']):
            body += constant(value) + bytes([0x47 if value['type'] == 'i32' else 0x52]) + b'\x04\x40\0\x0b'
    body += b'\x0b'
    count, pos = read_leb(sections[10], 0)
    assert count == funcs
    sections[10] = leb(count + 1) + sections[10][pos:] + leb(len(body)) + body
    return data[:8] + b''.join(bytes([i]) + leb(len(sections[i])) + sections[i] for i in
        (1, 2, 3, 4, 5, 13, 6, 7, 8, 9, 12, 10, 11) if i in sections)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('spec', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--wast2json', required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--backend', choices=['int', 'jit'], default='jit')
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=False)
    rows = []
    configs = [('wasmtime', [a.wasmtime, '-C', 'cache=n', '-W', 'threads=y,shared-memory=y'])]
    modes = ['full'] if a.ros else ['full', 'lazy', 'lazy+verification']
    for mode in modes:
        for policy in ['instruction', 'unwind'] if a.backend == 'jit' else ['instruction']:
            base = [str(a.uwvm.resolve())] + (['-Raot' if a.backend == 'jit' else '-Rint'] if a.ros else ['-Rcc', a.backend, '-Rcm', mode])
            if a.backend == 'jit':
                base += ['-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy]
            configs.append((f'{a.backend}-{mode}-{policy}', base + ['-WFE-threads', '--run']))
    cases = 0
    for filename, sha in SOURCES.items():
        source = a.spec / filename
        assert hashlib.sha256(source.read_bytes()).hexdigest() == sha, 'source changed: ' + filename
        directory = a.output / source.stem
        directory.mkdir()
        target = directory / 'original.json'
        command = [a.wast2json, '--enable-threads', str(source), '-o', str(target)]
        subprocess.run(command, check=True)
        module = None
        for i, assertion in enumerate(json.loads(target.read_text())['commands']):
            if assertion['type'] == 'module':
                module = (directory / assertion['filename']).read_bytes()
                continue
            assert assertion['type'] in ['assert_return', 'assert_trap']
            fixture = directory / f'assertion-{i}.wasm'
            fixture.write_bytes(wrapper(module, assertion))
            cases += 1
            for name, base in configs:
                command = base + [str(fixture)]
                run = subprocess.run(command, capture_output=True, timeout=45)
                diagnostic = (run.stdout + run.stderr).decode(errors='replace')
                (directory / f'{name}-{i}.log').write_text(diagnostic)
                expected = assertion.get('text')
                if name != 'wasmtime' and expected:
                    expected = {'unaligned atomic': 'unaligned atomic memory access',
                                'out of bounds memory access': 'memory access out of bounds'}.get(expected, expected)
                passed = run.returncode != 0 and expected in diagnostic if expected else run.returncode == 0
                rows.append(dict(file=filename, line=assertion['line'], configuration=name, command=command, exit=run.returncode, passed=passed))
                (a.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
                if not passed:
                    raise RuntimeError(f'{filename}:{assertion["line"]} {name}: {diagnostic}')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (a.output / 'summary.json').write_text(json.dumps(dict(passed=True, assertions=cases, runs=len(rows),
        source_commit=COMMIT, source_sha256=SOURCES, configurations=[x[0] for x in configs]), indent=2) + '\n')
    print(f'PASS {cases} official Wasmtime wait/notify assertions across {len(configs)} configurations ({len(rows)} executions)')


if __name__ == '__main__':
    main()
