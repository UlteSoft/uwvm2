#!/usr/bin/env python3
"""Compare registry matching with real Wasmtime cross-module import instantiation.

The UWVM side exercises the new type registry API, not the unfinished VM linker.
The oracle instantiates original function declarations, recursive groups and imports.
"""
import argparse
import json
from pathlib import Path
import subprocess
from run_recursive_type_spec import type_section

CASES = [
    ('same-group', '(rec (type $f (func)) (type (struct (field (ref $f)))))',
     '(rec (type $f (func)) (type (struct (field (ref $f)))))', 0, 0, True),
    ('projection-order', '(rec (type $f (func)) (type (struct)))',
     '(rec (type (struct)) (type $f (func)))', 0, 1, False),
    ('group-size', '(rec (type $f (func)) (type (struct)))',
     '(type $f (func))', 0, 0, False),
    ('finality', '(type $f (sub (func)))', '(type $f (func))', 0, 0, False),
    ('declared-subtype', '(type $base (sub (func))) (type $f (sub $base (func)))',
     '(type $f (sub (func)))', 1, 0, True),
    ('undeclared-shape', '(type $f (sub (func)))',
     '(type $base (sub (func))) (type $f (sub $base (func)))', 0, 1, False),
    ('closed-external', '(type $s (struct (field (ref null $s)))) (type $f (func (param (ref $s))))',
     '(type (array i64)) (type $s (struct (field (ref null $s)))) (type $f (func (param (ref $s))))', 1, 2, True),
    ('recursive-variance', '(rec (type $base (sub (func (param (ref $child))))) (type $child (sub $base (func (param (ref $base)))))) (type $f (sub $child (func (param (ref $base)))))',
     '(rec (type $f (sub (func (param (ref $child))))) (type $child (sub $f (func (param (ref $f))))))', 2, 0, True),
    ('mutable-field', '(type $s (struct (field (mut i32)))) (type $f (func (param (ref $s))))',
     '(type $s (struct (field i32))) (type $f (func (param (ref $s))))', 1, 1, False),
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--wasm-tools', required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--validator', required=True)
    p.add_argument('--out', required=True, type=Path)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.out.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, provider, consumer, pi, ci, expected in CASES:
        directory = args.out / name
        directory.mkdir(exist_ok=True)
        for role, types, fn in (
            ('provider', provider, '(func (export "f") (type $f) unreachable)'),
            ('consumer', consumer, '(func (import "M" "f") (type $f)) (func (export "_start"))'),
        ):
            wat = directory / f'{role}.wat'
            wasm = directory / f'{role}.wasm'
            wat.write_text('(module ' + types + ' ' + fn + ')\n')
            proc = subprocess.run([args.wasm_tools, 'parse', str(wat), '-o', str(wasm)], capture_output=True)
            (directory / f'{role}.parse.log').write_bytes(proc.stderr)
            assert proc.returncode == 0, (name, role, proc.stderr)
            # Check each module independently first: mismatches must be linker/type-identity failures.
            proc = subprocess.run([args.wasm_tools, 'validate', str(wasm)], capture_output=True)
            assert proc.returncode == 0, (name, role, proc.stderr)
            payload, _, _ = type_section(wasm.read_bytes())
            (directory / f'{role}.payload').write_bytes(payload)
        check = subprocess.run([args.validator, '--match', str(directory / 'provider.payload'), str(pi),
                                str(directory / 'consumer.payload'), str(ci)], capture_output=True, timeout=60)
        oracle = subprocess.run([args.wasmtime, 'run', '-C', 'cache=n', '-W', 'gc=y,function-references=y',
                                 '--preload', 'M=' + str(directory / 'provider.wasm'),
                                 str(directory / 'consumer.wasm')], capture_output=True, timeout=60)
        for label, proc in (('uwvm-registry', check), ('wasmtime-linker', oracle)):
            (directory / f'{label}.stdout').write_bytes(proc.stdout)
            (directory / f'{label}.stderr').write_bytes(proc.stderr)
        row = {'case': name, 'compatible': expected, 'registry_exit': check.returncode, 'wasmtime_exit': oracle.returncode}
        rows.append(row)
        (args.out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert check.returncode == (0 if expected else 1) and oracle.returncode == (0 if expected else 1), row
        if not expected:
            assert b'incompatible import type' in oracle.stderr, oracle.stderr
    print(f'PASS recursive registry / Wasmtime real linker: {len(rows)} cross-module cases')


if __name__ == '__main__':
    main()
