#!/usr/bin/env python3
"""Core 3 i31 binary/validation oracle, executed only in the Linux test cgroup."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


INVALID = {
    'i31-ref-from-i64': '''(module
      (func (export "_start")
        i64.const 1 ref.i31 drop))''',
    'i31-get-from-funcref': '''(module
      (func (export "_start")
        ref.null func i31.get_u drop))''',
}
VALID = '''(module
  (func (export "_start")
    i32.const -1 ref.i31 i31.get_s
    i32.const -1 i32.ne if unreachable end
    i32.const -1 ref.i31 i31.get_u
    i32.const 2147483647 i32.ne if unreachable end))'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--oracle-only', action='store_true')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if not args.oracle_only and args.uwvm is None:
        parser.error('--uwvm is required unless --oracle-only is set')
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    cases = {'i31-execution': (True, VALID)}
    cases.update({name: (False, source) for name, source in INVALID.items()})
    rows = []

    def run(case, phase, command, succeeds, marker=None):
        result = subprocess.run([str(part) for part in command], capture_output=True, timeout=120)
        output = result.stdout + result.stderr
        (args.out / f'{case}-{phase}.log').write_bytes(output)
        passed = (result.returncode == 0) if succeeds else (result.returncode > 0)
        if marker:
            passed = passed and marker in output.lower()
        rows.append({'case': case, 'phase': phase, 'command': [str(part) for part in command],
                     'exit': result.returncode, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{case} {phase}: exit={result.returncode}\n{output.decode(errors="replace")[-700:]}')

    for case, (valid, source) in cases.items():
        wat = args.out / f'{case}.wat'
        wasm = args.out / f'{case}.wasm'
        wat.write_text(source.rstrip() + '\n')
        run(case, 'parse', [args.wasm_tools, 'parse', wat, '-o', wasm], True)
        run(case, 'wasm-tools-validation',
            [args.wasm_tools, 'validate', '--features', 'all', wasm], valid)
        if valid:
            run(case, 'wasmtime-execution',
                [args.wasmtime, '-C', 'cache=n', '-W', 'gc=y', wasm], True)
        if not args.oracle_only:
            run(case, 'uwvm-validation',
                [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc', '--run', wasm], valid)
            if valid:
                # With GC disabled, CLI dispatch can select the legacy validator before
                # the Core 3-specific diagnostic path. Rejection is the policy invariant.
                run(case, 'gc-feature-off',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFD-gc', '--run', wasm],
                    False)
    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'runner_sha256': digest(Path(__file__)),
               'wasm_tools_sha256': digest(args.wasm_tools), 'wasmtime_sha256': digest(args.wasmtime)}
    if args.uwvm is not None:
        summary['uwvm_sha256'] = digest(args.uwvm)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 i31 validation: {sum(row["passed"] for row in rows)}/{len(rows)}', flush=True)
    if not summary['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
