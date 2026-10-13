#!/usr/bin/env python3
"""Core 3 aggregate type-section conformance, inside the Linux test cgroup."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


CASES = {
    'struct-fields': ('''(module
      (type $pair (struct (field (mut i32)) (field i8)))
      (func (export "_start")))''', bytes([0x5f])),
    'array-packed': ('''(module
      (type $numbers (array (mut i16)))
      (func (export "_start")))''', bytes([0x5e])),
    'rec-mixed-aggregates': ('''(module
      (rec (type $node (struct (field (ref null $node))))
           (type $numbers (array (mut i16))))
      (func (export "_start")))''', bytes([0x4e, 0x02])),
    'defined-field-reference': ('''(module
      (type $inner (struct (field i32)))
      (type $outer (struct (field (ref null $inner))))
      (func (export "_start")))''', bytes([0x5f])),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def aggregate_as_function(binary):
    """Change the one-byte function typeidx from its function slot to struct slot 0."""
    data = bytearray(binary)
    assert data[:8] == b'\0asm\x01\0\0\0'

    def uleb(offset):
        value = 0
        shift = 0
        while True:
            assert offset < len(data) and shift <= 28
            byte = data[offset]
            offset += 1
            value |= (byte & 0x7f) << shift
            if byte < 0x80:
                return value, offset
            shift += 7

    offset = 8
    while offset < len(data):
        section_id = data[offset]
        size, body = uleb(offset + 1)
        end = body + size
        assert end <= len(data)
        if section_id == 3:
            count, type_index_offset = uleb(body)
            assert count == 1 and data[type_index_offset] == 1
            data[type_index_offset] = 0
            return bytes(data)
        offset = end
    raise RuntimeError('fixture has no function section')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--oracle-only', action='store_true')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if not args.oracle_only and args.uwvm is None:
        parser.error('--uwvm is required without --oracle-only')
    subprocess.run(['bash', str(args.source_root.resolve(strict=True) /
                               'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []

    def run(name, phase, command, expected_success):
        result = subprocess.run([str(item) for item in command], capture_output=True, timeout=120)
        (args.out / f'{name}-{phase}.log').write_bytes(result.stdout + result.stderr)
        passed = (result.returncode == 0) == expected_success
        rows.append({'case': name, 'phase': phase, 'command': [str(item) for item in command],
                     'exit': result.returncode, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{name} {phase}: exit={result.returncode}\n'
                               f'{(result.stdout + result.stderr).decode(errors="replace")[-700:]}')

    for name, (wat, signature) in CASES.items():
        source = args.out / f'{name}.wat'
        binary = args.out / f'{name}.wasm'
        source.write_text(wat + '\n')
        run(name, 'parse', [args.wasm_tools, 'parse', source, '-o', binary], True)
        if signature not in binary.read_bytes():
            raise RuntimeError(f'{name}: binary omitted expected Core 3 type encoding')
        run(name, 'wasm-tools-validation', [args.wasm_tools, 'validate', '--features', 'all', binary], True)
        run(name, 'wasmtime-execution', [args.wasmtime, '-C', 'cache=n', '-W', 'gc=y', binary], True)
        if not args.oracle_only:
            run(name, 'uwvm-validation',
                [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc', '--run', binary], True)
            run(name, 'gc-feature-off',
                [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFD-gc', '--run', binary], False)
    bad_name = 'aggregate-as-function-type'
    bad_binary = args.out / f'{bad_name}.wasm'
    bad_binary.write_bytes(aggregate_as_function((args.out / 'struct-fields.wasm').read_bytes()))
    run(bad_name, 'wasm-tools-validation',
        [args.wasm_tools, 'validate', '--features', 'all', bad_binary], False)
    run(bad_name, 'wasmtime-validation',
        [args.wasmtime, '-C', 'cache=n', '-W', 'gc=y', bad_binary], False)
    if not args.oracle_only:
        run(bad_name, 'uwvm-validation',
            [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc', '--run', bad_binary], False)
    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'runner_sha256': sha(Path(__file__)), 'wasm_tools_sha256': sha(args.wasm_tools),
               'wasmtime_sha256': sha(args.wasmtime)}
    if args.uwvm is not None:
        summary['uwvm_sha256'] = sha(args.uwvm)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 aggregate type section: {sum(row["passed"] for row in rows)}/{len(rows)}', flush=True)


if __name__ == '__main__':
    main()
