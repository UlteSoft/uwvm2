#!/usr/bin/env python3
"""Core 3 call_ref/return_call_ref syntax, static typing, traps and deep tail calls.

Run only inside the 64 GiB/20-CPU Linux cgroup. Wasmtime is the independent
reference; --uwvm adds the selected product's real interpreter modes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


CASES = {
    'direct': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41 ref.func $f call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'structural-equivalent': ('ok', '''(module
      (type $a (func (param i32) (result i32)))
      (type $b (func (param i32) (result i32)))
      (func $f (type $a) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41 ref.func $f call_ref $b
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-null': ('null reference', '''(module
      (type $t (func))
      (func (export "_start") ref.null $t call_ref $t))'''),
    'bottom-null': ('null reference', '''(module
      (type $t (func))
      (func (export "_start") ref.null nofunc call_ref $t))'''),
    'tail-null': ('null reference', '''(module
      (type $t (func))
      (func $f (type $t) ref.null $t return_call_ref $t)
      (func (export "_start") call $f))'''),
    'tail-deep': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $loop (type $t)
        local.get 0 i32.eqz if i32.const 17 return end
        local.get 0 i32.const 1 i32.sub ref.func $loop return_call_ref $t)
      (elem declare func $loop)
      (func (export "_start")
        i32.const 100000 call $loop i32.const 17 i32.ne if unreachable end))'''),
    'protected-exception': ('ok', '''(module
      (type $t (func)) (tag $e)
      (func $raise (type $t) throw $e)
      (elem declare func $raise)
      (func (export "_start")
        block $out
          try_table (catch $e $out)
            ref.func $raise call_ref $t
          end
          unreachable
        end))'''),
    'wrong-type': ('validation', '''(module
      (type $a (func (param i32) (result i32)))
      (type $b (func (param i64) (result i32)))
      (func $f (type $b) local.get 0 i32.wrap_i64)
      (elem declare func $f)
      (func (export "_start") i32.const 42 ref.func $f call_ref $a drop))'''),
    'generic-supertype': ('validation', '''(module
      (type $t (func))
      (func (export "_start") ref.null func call_ref $t))'''),
    'wrong-argument': ('validation', '''(module
      (type $t (func (param i32)))
      (func $f (type $t) local.get 0 drop)
      (elem declare func $f)
      (func (export "_start") f64.const 1 ref.func $f call_ref $t))'''),
    'missing-argument': ('validation', '''(module
      (type $t (func (param i32)))
      (func $f (type $t) local.get 0 drop)
      (elem declare func $f)
      (func (export "_start") ref.func $f call_ref $t))'''),
    'unknown-type': ('validation', '''(module
      (func (export "_start") ref.null func call_ref 47))'''),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--only-case', action='append')
    parser.add_argument('--only-mode', action='append')
    parser.add_argument('--all-combine-delay', action='store_true',
                        help='Run every interpreter combine level with delay on and off')
    parser.add_argument('--guard', type=Path, help='Cgroup guard when running from a staged script')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(args.guard or root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    cases = {name: value for name, value in CASES.items() if not args.only_case or name in args.only_case}
    if args.only_case and len(cases) != len(set(args.only_case)):
        parser.error('unknown --only-case')
    modes = ['full'] if args.ros else ['full', 'lazy', 'lazy+verification']
    if args.only_mode:
        if not set(args.only_mode) <= set(modes):
            parser.error('unknown --only-mode')
        modes = args.only_mode
    combinations = [('default', [])]
    if args.all_combine_delay:
        combinations = [(level + ('-no-delay' if no_delay else '-delay'),
                         ['-Rint-op-conbine-level', level] + (['-Rint-no-delay-local'] if no_delay else []))
                        for level in ('disable', 'soft', 'heavy', 'extra') for no_delay in (False, True)]
    rows = []
    for name, (outcome, source) in cases.items():
        wat = args.output / (name + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text(source + '\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        exception_flags = ['-W', 'exceptions=y'] if name == 'protected-exception' else []
        gc_oracle_flags = ['-W', 'gc=y'] if name == 'bottom-null' else []
        gc_product_flags = ['-WFE-gc'] if name == 'bottom-null' else []
        commands = [('wasmtime', [str(args.wasmtime), '-C', 'cache=n', '-W', 'function-references=y', '-W', 'tail-call=y',
                                  *exception_flags, *gc_oracle_flags, str(wasm)])]
        if args.uwvm:
            uwvm = str(args.uwvm.resolve())
            for mode in modes:
                base = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', mode]
                for combination, flags in combinations:
                    label = mode if combination == 'default' else mode + '-' + combination
                    commands.append((label, [uwvm, *base, *flags, '-WFE-function-references', '-WFE-tail-call', *gc_product_flags,
                                             *(['-WFE-exceptions'] if name == 'protected-exception' else []), '--run', str(wasm)]))
            commands.append(('validator', [uwvm, '-m', 'validation', '-WFE-function-references', '-WFE-tail-call', *gc_product_flags,
                                           *(['-WFE-exceptions'] if name == 'protected-exception' else []), '--run', str(wasm)]))
        for mode, command in commands:
            result = subprocess.run(command, capture_output=True, timeout=90)
            log = (result.stdout + result.stderr).decode(errors='replace')
            (args.output / f'{name}-{mode}.log').write_text(log)
            expected_success = outcome == 'ok' or (mode == 'validator' and outcome == 'null reference')
            passed = (result.returncode == 0) == expected_success
            if outcome == 'null reference' and mode != 'validator':
                passed = passed and 'null reference' in log.lower()
            if outcome == 'validation':
                passed = passed and any(x in log.lower() for x in ('validat', 'type mismatch', 'invalid input'))
            rows.append({'case': name, 'mode': mode, 'outcome': outcome, 'passed': passed,
                         'exit': result.returncode, 'command': command})
            (args.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
            if not passed:
                raise RuntimeError(f'{name} {mode}: exit={result.returncode}\n{log}')
    if args.uwvm:
        uwvm = str(args.uwvm.resolve())
        direct = args.output / 'direct.wasm'
        tail = args.output / 'tail-deep.wasm'
        gates = []
        if direct.exists():
            gates.append(('ref-disabled', [uwvm, '-m', 'validation', '-WFD-function-references', '--run', str(direct)]))
        if tail.exists():
            gates.append(('tail-disabled', [uwvm, '-m', 'validation', '-WFE-function-references', '-WFD-tail-call', '--run', str(tail)]))
        for label, command in gates:
            result = subprocess.run(command, capture_output=True)
            log = (result.stdout + result.stderr).decode(errors='replace')
            (args.output / f'{label}.log').write_text(log)
            passed = result.returncode != 0 and ('feature' in log.lower() or 'enable-' in log.lower())
            rows.append({'case': label, 'mode': 'gate', 'passed': passed, 'exit': result.returncode, 'command': command})
            (args.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
            if not passed:
                raise RuntimeError(f'{label}: exit={result.returncode}\n{log}')
    binary_sha = hashlib.sha256(args.uwvm.read_bytes()).hexdigest() if args.uwvm else None
    (args.output / 'summary.json').write_text(json.dumps({'passed': True, 'checks': len(rows),
        'product': 'uwvm2-ros' if args.ros else 'uwvm2', 'binary_sha256': binary_sha}, indent=2) + '\n')
    print(f'PASS Core 3 call_ref/return_call_ref: {len(rows)} checks', flush=True)


if __name__ == '__main__':
    main()
