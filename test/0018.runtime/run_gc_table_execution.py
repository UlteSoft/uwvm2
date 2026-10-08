#!/usr/bin/env python3
"""Run Core 3 GC table syntax and execution across both VM backends."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


FIXTURES = ('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
            'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
            'gc_struct_into_anyref_table',
            'i31_table64', 'gc_table_initializer_execution')
MODES = {
    'int-full': ['-Rcc', 'int', '-Rcm', 'full'],
    'int-lazy': ['-Rcc', 'int', '-Rcm', 'lazy'],
    'int-lazy-verified': ['-Rcc', 'int', '-Rcm', 'lazy+verification'],
    'jit-full': ['-Rcc', 'jit', '-Rcm', 'full'],
    'jit-lazy': ['-Rcc', 'jit', '-Rcm', 'lazy'],
    'jit-lazy-verified': ['-Rcc', 'jit', '-Rcm', 'lazy+verification'],
    'tiered-lazy': ['-Rcc', 'tiered', '-Rcm', 'lazy', '-Rct', '0'],
    'tiered-lazy-verified': ['-Rcc', 'tiered', '-Rcm', 'lazy+verification', '-Rct', '0'],
}
ROS_MODES = {'int-full': ['-Rint'], 'jit-full': ['-Raot']}
ENABLED = ['-WFE-gc', '-WFE-reference-types', '-WFE-table-instructions',
           '-WFE-bulk-memory']
# Core 3 ref.i31/struct.new element initializers are GC constant instructions;
# they do not depend on the separate extended-const numeric-arithmetic feature.
INDEPENDENT_OFF = ['-WFD-exceptions', '-WFD-function-references',
                   '-WFD-threads', '-WFD-extended-const']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--mode', action='append', default=[])
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    modes = ROS_MODES if args.ros else MODES
    selected = args.mode or list(modes)
    if any(mode not in modes for mode in selected):
        parser.error('unsupported mode for this product')
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []

    def check(name, command, expected, feature=None):
        result = subprocess.run([str(part) for part in command], capture_output=True, timeout=120)
        output = result.stdout + result.stderr
        (args.out / f'{name}.log').write_bytes(output)
        plain = re.sub(rb'\x1b\[[0-9;]*m', b'', output).decode(errors='replace').lower()
        passed = (result.returncode == 0) == expected
        if feature is not None and not expected:
            passed &= feature in plain
        rows.append({'case': name, 'command': [str(part) for part in command],
                     'exit': result.returncode, 'expected_success': expected,
                     'feature_diagnostic': feature, 'passed': bool(passed)})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            print(f'FAIL {name}: {plain[-900:]}', flush=True)
        return passed

    fixture_root = Path(__file__).resolve().parent / 'fixtures'
    binaries = {}
    for name in FIXTURES:
        wat = fixture_root / f'{name}.wat'
        wasm = args.out / f'{name}.wasm'
        binaries[name] = wasm
        if not check(f'{name}-parse', [args.wasm_tools, 'parse', wat, '-o', wasm], True):
            raise SystemExit(1)
        if not check(f'{name}-validate', [args.wasm_tools, 'validate', wasm], True):
            raise SystemExit(1)
        oracle_flags = ['-W', 'gc=y']
        if name in ('i31_element', 'gc_table_initializer_execution'):
            oracle_flags += ['-W', 'extended-const=n']
        if not check(f'{name}-wasmtime', [args.wasmtime, 'run', '-C', 'cache=n',
                                          *oracle_flags, wasm], True):
            raise SystemExit(1)

    for mode in selected:
        base = [args.uwvm, *modes[mode], *ENABLED, *INDEPENDENT_OFF]
        for name in FIXTURES:
            flags = ['-WFE-table64'] if name == 'i31_table64' else []
            if name == 'gc_table_initializer_execution':
                flags.append('-WFE-table-initializer')
            if not check(f'{mode}-{name}', [*base, *flags, '--run', binaries[name]], True):
                continue
            for feature in ('gc', 'reference-types', 'table-instructions'):
                replacement = [f'-WFD-{feature}' if flag == f'-WFE-{feature}' else flag
                               for flag in ENABLED]
                check(f'{mode}-{name}-{feature}-off',
                      [args.uwvm, *modes[mode], *replacement, *INDEPENDENT_OFF,
                       *flags, '--run', binaries[name]], False, feature)
            if name == 'i31_table64':
                check(f'{mode}-{name}-table64-off',
                      [*base, '-WFD-table64', '--run', binaries[name]], False, 'table64')
            if name == 'gc_table_initializer_execution':
                check(f'{mode}-{name}-table-initializer-off',
                      [*base, '-WFD-table-initializer', '--run', binaries[name]],
                      False, 'table-initializer')
            if name == 'i31_element':
                replacement = [f'-WFD-bulk-memory' if flag == '-WFE-bulk-memory' else flag
                               for flag in ENABLED]
                check(f'{mode}-{name}-bulk-memory-off',
                      [args.uwvm, *modes[mode], *replacement, *INDEPENDENT_OFF,
                       '--run', binaries[name]], False, 'bulk-memory')

    summary = {'passed': all(row['passed'] for row in rows),
               'checks': len(rows), 'product_sha256': digest(args.uwvm),
               'wasm_tools_sha256': digest(args.wasm_tools),
               'wasmtime_sha256': digest(args.wasmtime),
               'runner_sha256': digest(Path(__file__)),
               'source_root': str(root), 'mode': selected, 'ros': args.ros}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 GC tables: {sum(row["passed"] for row in rows)}/{len(rows)}')
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
