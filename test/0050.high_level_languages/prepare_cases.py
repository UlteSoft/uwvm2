#!/usr/bin/env python3
"""Bind the finite qualification matrix to unchanged producer artifacts."""
import argparse
import json
from pathlib import Path
import re

from build_products import digest

LANGUAGES = frozenset({'c', 'cpp', 'rust', 'go', 'tinygo', 'zig', 'java', 'csharp',
                       'fsharp', 'vb', 'python', 'javascript', 'assemblyscript', 'moonbit', 'kotlin'})


def append_manifests(cases, sources):
    """Append actual producer cases; never replace a built-in case or its output."""
    names = {row['name'] for row in cases}
    assert len(names) == len(cases), 'duplicate built-in case names'
    for source in sources:
        rows = json.loads(source.read_text())
        assert isinstance(rows, list) and rows, f'nonempty producer case list required: {source}'
        for row in rows:
            assert isinstance(row, dict), f'producer case must be an object: {source}'
            name = row.get('name')
            assert isinstance(name, str) and re.fullmatch(r'[A-Za-z0-9_.-]+', name), 'unsafe added case name'
            assert name not in names, f'duplicate/additional case would replace an existing case: {name}'
            assert isinstance(row.get('wasm'), str) and row['wasm'], f'added Wasm path required: {name}'
            wasm = Path(row['wasm'])
            assert wasm.is_absolute() and wasm.is_file(), f'absolute existing producer Wasm path required: {name}'
            expected = row.get('wasm_sha256')
            assert isinstance(expected, str) and re.fullmatch(r'[0-9a-f]{64}', expected), f'producer SHA256 required: {name}'
            assert digest(wasm) == expected, f'producer Wasm SHA256 mismatch: {name}'
            with wasm.open('rb') as stream:
                assert stream.read(8) == b'\0asm\x01\0\0\0', f'core Wasm producer artifact required: {name}'
            assert isinstance(row.get('expected_stdout'), str), f'exact expected_stdout required: {name}'
            for option in ('args', 'load_args', 'features'):
                if option in row:
                    assert isinstance(row[option], list) and all(isinstance(v, str) for v in row[option]), (
                        f'{option} must be a string list: {name}')
            if 'language' in row:
                assert row['language'] in LANGUAGES, f'unknown producer language: {name}'
            if 'skip_debug' in row:
                assert isinstance(row['skip_debug'], bool), f'skip_debug must be boolean: {name}'
            if 'expected_exit' in row:
                assert (isinstance(row['expected_exit'], int) and not isinstance(row['expected_exit'], bool)
                        or row['expected_exit'] == 'nonzero'), f'invalid expected_exit: {name}'
            for option in ('expected_stderr', 'expected_pattern'):
                if option in row:
                    assert isinstance(row[option], str), f'{option} must be a string: {name}'
            for option in ('generated_source', 'oracle'):
                checksum = option + '_sha256'
                if checksum in row:
                    assert isinstance(row.get(option), str) and isinstance(row[checksum], str), (
                        f'{option} path and SHA256 required together: {name}')
                    assert re.fullmatch(r'[0-9a-f]{64}', row[checksum]), f'invalid {checksum}: {name}'
                    assert digest(row[option]) == row[checksum], f'{option} SHA256 mismatch: {name}'
            # All producer options and exact expectations remain as declared.
            # This is manifest validation, not an execution or debugger PASS.
            cases.append(row)
            names.add(name)
    return cases


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--additional-manifest', type=Path, action='append', default=[],
                   help='append real producer ordinary/DBG cases after the unchanged default cases')
    a = p.parse_args()
    other = a.arena / 'other/artifacts'
    java = a.arena / 'java'
    csharp = a.arena / 'csharp'
    cases = []

    def add(name, wasm, stdout, **options):
        assert wasm.is_file(), wasm
        cases.append({'name': name, 'wasm': str(wasm), 'wasm_sha256': digest(wasm), 'expected_stdout': stdout, **options})

    java_stdout = (java / 'expected.stdout').read_text()
    add('java-full', java / 'target/wasi/java-semantics.wasm', java_stdout)
    add('java-simple', java / 'target/wasi-debug/java-semantics.wasm', java_stdout)
    add('java-perf', java / 'target/perf/java-perf.wasm',
        (java / 'perf-expected-1000000.stdout').read_text())
    for row in json.loads((csharp / 'src/cases.json').read_text()):
        add('csharp-' + row['name'], csharp / 'build/AppBundle/highlevel-csharp.wasm', row['stdout'],
            args=row['args'], expected_exit=row['exit_code'], expected_stderr=row['stderr'],
            skip_debug=row['name'] != 'default', ordinary_timeout=900, initial_prompt_timeout=1200,
            cold_command_timeout=1200, require_parallel_objects=True)
    common = 'uwvm:high-level checksum=264715'
    add('assemblyscript', other / 'assemblyscript/assemblyscript-debug.wasm', f'assemblyscript {common}\n')
    add('assemblyscript-release', other / 'assemblyscript/assemblyscript-release.wasm', f'assemblyscript {common}\n')
    add('javascript-small', other / 'javascript/javascript-rounds-50.wasm',
        'javascript uwvm:high-level checksum=265424 caught=true\n')
    add('javascript', other / 'javascript/javascript-debug.wasm', f'javascript {common} caught=true\n', skip_debug=True)
    for backend in ['wasm', 'wasm-gc']:
        add('moonbit-' + backend, other / f'moonbit/moonbit-{backend}-debug.wasm', f'moonbit {common} caught=true\n')
    add('moonbit-upstream-host-unavailable', other / 'moonbit/moonbit-wasm-gc-upstream-stdio.wasm', '',
        expected_exit=127, expected_pattern=r'Missing module dependency.*spectest', skip_debug=True)
    add('java-implicit-arithmetic-known-gap', java / 'target/known-gap/java-implicit-arithmetic.wasm', '',
        expected_exit='nonzero', expected_pattern=r'(?:integer.*(?:divide|division).*zero|div.*zero)', skip_debug=True)
    add('java-gc-host-unavailable', java / 'target/wasm-gc/java-semantics.wasm', '',
        expected_exit=127, expected_pattern=r'Missing module dependency.*teavm', skip_debug=True)
    append_manifests(cases, a.additional_manifest)
    a.out.write_text(json.dumps(cases, indent=2) + '\n')


if __name__ == '__main__':
    main()
