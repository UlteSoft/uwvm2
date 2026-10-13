#!/usr/bin/env python3
"""Qualify dead and adjacent-null Core 3 throw_ref, not retained exnref support.

Official rule: https://webassembly.github.io/spec/core/valid/instructions.html#valid-throw-ref
Run only in the remote test cgroup. Invalid concrete operands in dead code must
still fail validation. Both execution backends recognize adjacent live-null
sequences as null-reference traps. Retained/non-null exception references are
outside this bounded suite.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CHECK = 'i32.const 42 i32.ne if unreachable end'


def fixtures():
    # name, normative outcome, source.
    yield 'branch', 'return', '(module (func (export "_start") block br 0 throw_ref end))'
    yield 'prefix', 'return', f'(module (func (export "_start") i32.const 42 block br 0 throw_ref end {CHECK}))'
    yield 'return', 'return', f'''(module
      (func $f (result i32) i32.const 42 return throw_ref)
      (func (export "_start") call $f {CHECK}))'''
    yield 'nested', 'return', '''(module (func (export "_start")
      block $out loop block br $out throw_ref end end end))'''
    yield 'unknown-select', 'return', '(module (func (export "_start") block br 0 select throw_ref end))'
    yield 'reference-bottom', 'return', '(module (func (export "_start") block br 0 ref.as_non_null throw_ref end))'
    yield 'branch-reference-bottom', 'return', '(module (func (export "_start") block br 0 br_on_null 0 throw_ref end))'
    # Keep a live value from an actual call below a delayed local.get. Soft add
    # and heavy multiply use distinct compiled delay opfunc families; the dead
    # instruction must preserve their result across its enclosing block.
    for name, seed, argument, opcode in [('soft', 2, 40, 'i32.add'), ('heavy', 6, 7, 'i32.mul')]:
        yield 'delay-local-' + name, 'return', f'''(module
          (func $seed (result i32) i32.const {seed})
          (func $calc (param i32) (result i32)
            call $seed local.get 0 {opcode} block br 0 throw_ref end)
          (func (export "_start") i32.const {argument} call $calc {CHECK}))'''
    yield 'after-local-throw', 'return', f'''(module (tag $t (param i32))
      (func (export "_start") block $out (result i32)
        try_table (catch $t $out) i32.const 42 throw $t throw_ref end unreachable
      end {CHECK}))'''
    yield 'after-unreachable', 'trap', '(module (func (export "_start") unreachable throw_ref))'
    for name, value in [('i32', 'i32.const 0'), ('i64', 'i64.const 0'),
                        ('f32', 'f32.const 0'), ('f64', 'f64.const 0'),
                        ('v128', 'v128.const i32x4 0 0 0 0'),
                        ('funcref', 'ref.null func'), ('externref', 'ref.null extern')]:
        yield 'invalid-dead-' + name, 'invalid', f'(module (func (export "_start") unreachable {value} throw_ref))'
    yield 'invalid-live-underflow', 'invalid', '(module (func (export "_start") throw_ref))'
    yield 'invalid-live-i32', 'invalid', '(module (func (export "_start") i32.const 0 throw_ref))'
    yield 'live-null-exn', 'null-trap', '(module (func (export "_start") ref.null exn throw_ref))'
    yield 'live-null-noexn', 'null-trap', '(module (func (export "_start") ref.null noexn throw_ref))'
    yield 'live-null-protected', 'null-trap', '''(module (func (export "_start")
      block $outer try_table (catch_all $outer) ref.null exn throw_ref end end))'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--wasm-tools', required=True, type=Path)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--wrapper-arg', action='append', default=[],
                        help='repeat an exact QEMU or target launcher argument before --uwvm')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--backend', choices=['int', 'llvm'], default='int')
    parser.add_argument('--mode', choices=['full', 'lazy', 'lazy+verification'], action='append')
    parser.add_argument('--trace', choices=['instruction', 'unwind', 'none'], action='append')
    parser.add_argument('--all-combine-delay', action='store_true')
    args = parser.parse_args()
    if not args.wasmtime and not args.uwvm:
        parser.error('at least one execution engine is required')
    if args.wrapper_arg and not args.uwvm:
        parser.error('--wrapper-arg requires --uwvm')
    modes = args.mode or (['full'] if args.ros else ['full', 'lazy', 'lazy+verification'])
    if args.ros and modes != ['full']:
        parser.error('ROS supports full only')
    if args.trace and args.backend != 'llvm':
        parser.error('--trace requires --backend llvm')
    if args.all_combine_delay and args.backend != 'int':
        parser.error('--all-combine-delay requires --backend int')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    ansi = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')

    def run(label, command, outcome, reference=False):
        command = [str(part) for part in command]
        result = subprocess.run(command, capture_output=True, timeout=60)
        raw = result.stdout + result.stderr
        (args.out / (label + '.log')).write_bytes(raw)
        text = ansi.sub('', raw.decode(errors='replace'))
        if outcome == 'return':
            passed = result.returncode == 0
        elif outcome == 'disabled':
            passed = result.returncode != 0 and '--wasm-feature-enable-exceptions' in text
        elif outcome == 'disabled-functions':
            passed = result.returncode != 0 and '--wasm-feature-enable-function-references' in text
        elif outcome == 'null-trap':
            passed = result.returncode != 0 and (
                'wasm trap: null reference' in text if reference else
                'Runtime crash (' in text and 'null reference' in text.lower())
        elif outcome == 'trap' or (reference and outcome == 'unsupported'):
            passed = result.returncode != 0 and ('wasm trap:' in text if reference else 'uwvm: [fatal] Runtime crash (' in text)
            if outcome == 'trap':
                passed = passed and 'unreachable' in text
        else:
            # A signal, runtime trap, or uncaught exception is never evidence of validation rejection.
            passed = result.returncode != 0 and 'wasm trap:' not in text and 'Runtime crash (' not in text
            passed = passed and 'Uncaught WebAssembly exception' not in text
            passed = passed and (('failed to compile' in text or 'type mismatch' in text) if reference else '[error]' in text)
        rows.append(dict(label=label, command=command, outcome=outcome, reference=reference,
                         exit=result.returncode, passed=passed))
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{label}: expected {outcome}, exit={result.returncode}\n{text}')

    policies = args.trace or ['instruction']
    combinations = [('default', [])]
    if args.all_combine_delay:
        combinations = [(level + ('-no-delay' if no_delay else '-delay'),
                         ['-Rint-op-conbine-level', level] + (['-Rint-no-delay-local'] if no_delay else []))
                        for level in ['disable', 'soft', 'heavy', 'extra'] for no_delay in [False, True]]
    configs = []
    if args.uwvm:
        for mode in modes:
            for combination, flags in combinations:
                for policy in policies if args.backend == 'llvm' else [None]:
                    if args.backend == 'llvm':
                        base = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', mode]
                        base += ['-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable']
                    else:
                        base = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', mode]
                    label = mode + '-' + combination + ('-' + policy if policy else '')
                    configs.append((label, [*args.wrapper_arg, args.uwvm] + base + flags + ['-Rct', '0', '-WFE-function-references']))

    cases = list(fixtures())
    for name, outcome, source in cases:
        wat = args.out / (name + '.wat')
        wat.write_text(source + '\n')
        wasm = wat.with_suffix('.wasm')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        if args.wasmtime:
            run(name + '-wasmtime', [args.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', wasm], outcome, True)
        for label, base in configs:
            run(name + '-' + label, base + ['-WFE-exceptions', '--run', wasm], outcome)
            if name in ['branch', 'reference-bottom', 'branch-reference-bottom', 'live-null-exn', 'live-null-noexn', 'live-null-protected']:
                run(name + '-' + label + '-disabled', base + ['-WFD-exceptions', '--run', wasm], 'disabled')
            if name in ['reference-bottom', 'branch-reference-bottom']:
                function_refs_disabled = [flag for flag in base if flag != '-WFE-function-references']
                run(name + '-' + label + '-function-references-disabled',
                    function_refs_disabled + ['-WFD-function-references', '-WFE-exceptions', '--run', wasm],
                    'disabled-functions')
        print('PASS', name, flush=True)
    hashes = {name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()
              for name in ['wasm_tools', 'wasmtime', 'uwvm'] if getattr(args, name)}
    (args.out / 'summary.json').write_text(json.dumps(dict(passed=True, cases=len(cases), runs=len(rows),
        backend=args.backend, modes=modes, wrapper_args=args.wrapper_arg, sha256=hashes,
        scope='dead throw_ref plus adjacent null-exn/noexn traps; retained exnref is covered by the separate exception-reference suite'), indent=2) + '\n')
    print('PASS dead throw_ref:', len(cases), 'cases,', len(rows), 'commands')


if __name__ == '__main__':
    main()
