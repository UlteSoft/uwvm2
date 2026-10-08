#!/usr/bin/env python3
"""Core 3 exception-reference execution and rejection cases against Wasmtime.

These are current-standard WAT fixtures, including reachable throw_ref and
retained catch_ref/catch_all_ref. With --uwvm they also qualify the selected
runtime backend against Wasmtime, including invalid type rejection.
https://webassembly.github.io/spec/core/valid/instructions.html#valid-throw-ref
https://webassembly.github.io/spec/core/exec/instructions.html#exec-throw-ref
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CASES = {
    'null-exn-throws-trap': ('trap', '(module (func (export "_start") ref.null exn throw_ref))'),
    'null-noexn-throws-trap': ('trap', '(module (func (export "_start") ref.null noexn throw_ref))'),
    'null-exn-nonnull-trap': ('trap', '(module (func (export "_start") ref.null exn ref.as_non_null throw_ref))'),
    'tagged-catch-ref-rethrow': ('return', '''(module
      (tag $t (param i32))
      (func (export "_start")
        block $outer (result i32)
          try_table (catch $t $outer)
            block $inner (result i32 (ref exn))
              try_table (catch_ref $t $inner)
                i32.const 17 throw $t
              end
              unreachable
            end
            throw_ref
          end
          unreachable
        end
        i32.const 17 i32.ne if unreachable end))'''),
    'catch-all-ref-rethrow': ('return', '''(module
      (tag $t)
      (func (export "_start")
        block $done
          try_table (catch_all $done)
            block $caught (result (ref exn))
              try_table (catch_all_ref $caught)
                throw $t
              end
              unreachable
            end
            throw_ref
          end
          unreachable
        end))'''),
    'invalid-throw-ref-i32': ('invalid', '(module (func (export "_start") i32.const 0 throw_ref))'),
    'invalid-throw-ref-funcref': ('invalid', '(module (func (export "_start") ref.null func throw_ref))'),
    'invalid-catch-ref-wrong-label': ('invalid', '''(module (tag $t (param i32))
      (func $bad (result i32 externref)
        try_table (catch_ref $t 0) unreachable end unreachable)
      (func (export "_start") call $bad drop drop))'''),
    'invalid-catch-all-ref-wrong-label': ('invalid', '''(module
      (func $bad (result funcref)
        try_table (catch_all_ref 0) unreachable end unreachable)
      (func (export "_start") call $bad drop))'''),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--wasm-tools', required=True, type=Path)
    parser.add_argument('--wasmtime', required=True, type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--wrapper-arg', action='append', default=[],
                        help='repeat an exact QEMU or target launcher argument before --uwvm')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--backend', choices=('int', 'llvm'), default='int')
    parser.add_argument('--mode', choices=('full', 'lazy', 'lazy+verification'), action='append')
    parser.add_argument('--trace', choices=('instruction', 'unwind'), action='append')
    args = parser.parse_args()
    if args.wrapper_arg and not args.uwvm:
        parser.error('--wrapper-arg requires --uwvm')
    modes = args.mode or (['full'] if args.ros else ['full', 'lazy', 'lazy+verification'])
    if args.ros and modes != ['full']:
        parser.error('ROS supports full only')
    if args.trace and args.backend != 'llvm':
        parser.error('--trace requires --backend llvm')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    ansi = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
    configs = []
    if args.uwvm:
        for mode in modes:
            for trace in (args.trace or ['instruction']) if args.backend == 'llvm' else [None]:
                if args.backend == 'int':
                    flags = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', mode]
                else:
                    flags = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', mode]
                    flags += ['-Rllvm-call-stack', trace, '-Rllvm-cache-path', 'disable']
                configs.append((mode + ('-' + trace if trace else ''), flags))
    for name, (outcome, source) in CASES.items():
        wat = args.out / f'{name}.wat'
        wasm = wat.with_suffix('.wasm')
        wat.write_text(source + '\n')
        parse = subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)],
                               capture_output=True, timeout=30)
        (args.out / f'{name}-parse.log').write_bytes(parse.stdout + parse.stderr)
        if parse.returncode and outcome != 'invalid':
            raise RuntimeError(f'{name}: valid Core 3 WAT did not parse:\n{parse.stderr.decode(errors="replace")}')
        if parse.returncode:
            rows.append(dict(name=name, outcome=outcome, stage='parse', exit=parse.returncode, passed=True))
            continue
        run = subprocess.run([str(args.wasmtime), '-C', 'cache=n', '-W', 'exceptions=y', str(wasm)],
                             capture_output=True, timeout=30)
        output = (run.stdout + run.stderr).decode(errors='replace')
        (args.out / f'{name}-wasmtime.log').write_text(output)
        passed = (run.returncode == 0 if outcome == 'return' else
                  run.returncode != 0 and 'wasm trap: null reference' in output if outcome == 'trap' else
                  run.returncode != 0 and 'wasm trap:' not in output and
                  ('failed to compile' in output or 'type mismatch' in output))
        rows.append(dict(name=name, outcome=outcome, stage='execute', exit=run.returncode, passed=passed))
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{name}: expected {outcome}, got exit {run.returncode}:\n{output}')
        for label, flags in configs:
            command = [*args.wrapper_arg, str(args.uwvm), *flags, '-Rct', '0', '-WFE-function-references',
                       '-WFE-exceptions', '--run', str(wasm)]
            product = subprocess.run(command, capture_output=True, timeout=60)
            raw = product.stdout + product.stderr
            (args.out / f'{name}-{label}-uwvm.log').write_bytes(raw)
            product_output = ansi.sub('', raw.decode(errors='replace'))
            if outcome == 'return':
                product_passed = product.returncode == 0
            elif outcome == 'trap':
                product_passed = (product.returncode != 0 and
                                  '[fatal] Runtime crash (' in product_output and
                                  'null reference' in product_output.lower())
            else:
                product_passed = (product.returncode != 0 and '[error]' in product_output and
                                  'Runtime crash (' not in product_output and
                                  'Cannot resolve entry function' not in product_output)
            rows.append(dict(name=name, outcome=outcome, stage='uwvm', config=label,
                             exit=product.returncode, passed=product_passed, command=command))
            (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
            if not product_passed:
                raise RuntimeError(f'{name}-{label}: expected {outcome}, got exit {product.returncode}:\n{product_output}')
    (args.out / 'summary.json').write_text(json.dumps(dict(
        passed=True, cases=len(CASES), runs=len(rows), backend=args.backend, modes=modes,
        wrapper_args=args.wrapper_arg,
        sha256={key: hashlib.sha256(getattr(args, key).read_bytes()).hexdigest()
                for key in ('wasm_tools', 'wasmtime', 'uwvm') if getattr(args, key)}), indent=2) + '\n')
    print(f'PASS Core 3 exnref reference/product: {len(CASES)} new-syntax cases, {len(rows)} runs')


if __name__ == '__main__':
    main()
