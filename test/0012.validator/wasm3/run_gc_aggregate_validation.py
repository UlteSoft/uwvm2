#!/usr/bin/env python3
"""Core 3 struct/array instruction typing against Wasm-tools and Wasmtime."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


INVALID = {
    'struct-packed-access-mismatch': '''(module
      (type $s (struct (field i32)))
      (func (export "_start")
        i32.const 0 struct.new $s struct.get_s $s 0 drop))''',
    'struct-immutable-set': '''(module
      (type $s (struct (field i32)))
      (func (export "_start")
        i32.const 0 struct.new $s i32.const 1 struct.set $s 0))''',
    'array-immutable-set': '''(module
      (type $a (array i16))
      (func (export "_start")
        i32.const 0 i32.const 1 array.new $a
        i32.const 0 i32.const 2 array.set $a))''',
    'struct-field-index-out-of-range': '''(module
      (type $s (struct (field i32)))
      (func (export "_start")
        i32.const 0 struct.new $s struct.get $s 1 drop))''',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
        parser.error('--uwvm required without --oracle-only')
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    cases = {
        'struct-execution': (True, (root / 'test/0017.runtime/fixtures/gc_struct_execution.wat').read_text()),
        'array-execution': (True, (root / 'test/0017.runtime/fixtures/gc_array_execution.wat').read_text()),
    }
    cases.update((name, (False, wat)) for name, wat in INVALID.items())
    rows = []

    def run(case, phase, command, should_pass):
        result = subprocess.run([str(item) for item in command], capture_output=True, timeout=120)
        output = result.stdout + result.stderr
        (args.out / f'{case}-{phase}.log').write_bytes(output)
        passed = (result.returncode == 0) == should_pass
        rows.append({'case': case, 'phase': phase, 'command': [str(item) for item in command],
                     'exit': result.returncode, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{case} {phase}: exit={result.returncode}\n'
                               f'{output.decode(errors="replace")[-700:]}')

    for case, (valid, wat) in cases.items():
        source = args.out / f'{case}.wat'
        binary = args.out / f'{case}.wasm'
        source.write_text(wat.rstrip() + '\n')
        run(case, 'parse', [args.wasm_tools, 'parse', source, '-o', binary], True)
        run(case, 'wasm-tools-validation',
            [args.wasm_tools, 'validate', '--features', 'all', binary], valid)
        if valid:
            run(case, 'wasmtime-execution',
                [args.wasmtime, '-C', 'cache=n', '-W', 'gc=y', binary], True)
        if not args.oracle_only:
            run(case, 'uwvm-validation',
                [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc', '--run', binary], valid)
            if valid:
                run(case, 'gc-feature-off',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFD-gc', '--run', binary], False)
    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'runner_sha256': sha(Path(__file__)), 'wasm_tools_sha256': sha(args.wasm_tools),
               'wasmtime_sha256': sha(args.wasmtime)}
    if args.uwvm is not None:
        summary['uwvm_sha256'] = sha(args.uwvm)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 aggregate validation: {sum(row["passed"] for row in rows)}/{len(rows)}', flush=True)


if __name__ == '__main__':
    main()
