#!/usr/bin/env python3
"""Require actual tiered execution for numeric Core 3 exception fixtures.

Ordinary uwvm2 only. No performance claims: compiler logging is required and
this runner records which native tiers the logs actually prove. The default is
a bounded T1 selection; OSR/T2 workloads and larger mode/policy matrices must be
requested explicitly. Reuses the current cross-function exception fixtures.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess

from run_exception_cross_cli import CHECK, fixtures


DEFAULT_CASES = ['caller-prefix', 'indirect-parameters', 'large-eh-only-tuple',
                 'loop-mixed-parameters', 'tail-bypass-direct', 'tail-bypass-indirect',
                 'scratch-frame-repeat', 'middle-tag-mismatch', 'same-frame-tag-mismatch',
                 'uncaught-direct', 'uncaught-mismatch']
PROFILES = {'all': [], 'no-t0': ['-Rtiered-disable-t0'],
            'no-t2': ['-Rtiered-disable-t2'],
            'no-t0-no-t2': ['-Rtiered-disable-t0', '-Rtiered-disable-t2']}


def osr_fixtures():
    for indirect, tail in [(False, False), (True, False), (False, True), (True, True)]:
        name = 'osr-' + ('tail-' if tail else '') + ('indirect' if indirect else 'direct')
        edge = ('i32.const 0 return_call_indirect (type $sig)' if indirect else 'return_call $leaf') if tail else (
            'i32.const 0 call_indirect (type $sig)' if indirect else 'call $leaf')
        # The two normal warm calls complete an actual OSR before the third
        # activation throws. A tail call must retire the misleading inner catch.
        suffix = f'''block $wrong (result i32)
          try_table (result i32) (catch $tag $wrong) {edge} end
          end drop i32.const 13''' if tail else edge
        source = f'''(module
          (type $sig (func (result i32))) (tag $tag (param i32))
          (table 1 funcref) (elem (i32.const 0) $leaf)
          (func $leaf (type $sig) i32.const 42 throw $tag)
          (func $hot (param $throw i32) (result i32) (local $n i32)
            i32.const 1000000 local.set $n
            loop $again {'nop ' * 2000}
              local.get $n i32.const 1 i32.sub local.tee $n br_if $again
            end
            local.get $throw i32.eqz if i32.const 42 return end
            {suffix})
          (func (export "_start")
            i32.const 0 call $hot {CHECK}
            i32.const 0 call $hot {CHECK}
            i32.const 42 block $out (result i32)
              try_table (catch $tag $out) i32.const 1 call $hot drop end unreachable
            end {CHECK} {CHECK}))'''
        yield name, source, None, False
        warm = f'''i32.const 0 call $hot {CHECK}
            i32.const 0 call $hot {CHECK}'''
        # This actual OSR activation exits exceptionally, without a previous normal return.
        escape = source.replace(warm, '')
        yield name.replace('osr-', 'osr-escape-'), escape, None, False
        uncaught = escape.replace(f'''i32.const 42 block $out (result i32)
              try_table (catch $tag $out) i32.const 1 call $hot drop end unreachable
            end {CHECK} {CHECK}''', 'i32.const 1 call $hot drop')
        yield name.replace('osr-', 'osr-uncaught-'), uncaught, 'uncaught', False



def tier2_fixtures():
    for indirect, tail in [(False, False), (True, False), (False, True), (True, True)]:
        name = 't2-' + ('tail-' if tail else '') + ('indirect' if indirect else 'direct')
        edge = ('i32.const 0 return_call_indirect (type $sig)' if indirect else 'return_call $leaf') if tail else (
            'i32.const 0 call_indirect (type $sig)' if indirect else 'call $leaf')
        # A short T0 driver intentionally has no emitted OSR poll. Repeated normal
        # native returns trigger T2; only after that workload does the same target
        # throw. The runtime's actual-address entry log must prove T2 execution.
        tag = '$tag' if tail else '$wrong_tag'
        source = f'''(module
          (type $sig (func (param i32) (result i32)))
          (tag $tag (param i32)) (tag $wrong_tag (param i32))
          (table 1 funcref) (elem (i32.const 0) $leaf)
          (func $leaf (type $sig) local.get 0 if i32.const 42 throw $tag end i32.const 42)
          (func $middle (param i32) (result i32)
            block $wrong (result i32) try_table (result i32) (catch {tag} $wrong)
              local.get 0 {edge} end end drop i32.const 13)
          (func (export "_start") (local $n i32)
            i32.const 4000000 local.set $n
            loop $warm i32.const 0 call $middle drop
              local.get $n i32.const 1 i32.sub local.tee $n br_if $warm end
            i32.const 8 local.set $n
            loop $throws i32.const 42 block $out (result i32)
              try_table (catch $tag $out) i32.const 1 call $middle drop end unreachable
            end {CHECK} {CHECK}
            local.get $n i32.const 1 i32.sub local.tee $n br_if $throws end))'''
        yield name, source, None, False
        last_throws = f'''i32.const 8 local.set $n
            loop $throws i32.const 42 block $out (result i32)
              try_table (catch $tag $out) i32.const 1 call $middle drop end unreachable
            end {CHECK} {CHECK}
            local.get $n i32.const 1 i32.sub local.tee $n br_if $throws end'''
        yield name.replace('t2-', 't2-uncaught-'), source.replace(last_throws, 'i32.const 1 call $middle drop'), 'uncaught', False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--phase', choices=['t1', 'osr', 't2'], default='t1')
    parser.add_argument('--mode', choices=['lazy', 'lazy+verification'], action='append')
    parser.add_argument('--trace', choices=['instruction', 'unwind'], action='append')
    parser.add_argument('--profile', choices=list(PROFILES), action='append')
    parser.add_argument('--case', action='append')
    parser.add_argument('--compile-threads', type=int)
    args = parser.parse_args()
    profiles = args.profile or (['no-t0-no-t2'] if args.phase == 't1' else ['no-t2'] if args.phase == 'osr' else ['all'])
    modes = args.mode or ['lazy']
    policies = args.trace or ['instruction']
    if args.compile_threads is None:
        args.compile_threads = 2 if args.phase == 't2' else 0
    if args.phase == 't2' and (profiles != ['all'] or args.compile_threads == 0):
        parser.error('T2 qualification requires profile all and compile workers')
    if args.phase == 'osr' and any('no-t0' in p for p in profiles):
        parser.error('OSR requires T0; use profiles all/no-t2')
    if args.phase == 't1' and any('no-t0' not in p for p in profiles):
        parser.error('Short T1 cases require no-t0/no-t0-no-t2 to prove native entry')
    selected = list(fixtures()) if args.phase == 't1' else list(osr_fixtures()) if args.phase == 'osr' else list(tier2_fixtures())
    wanted = args.case or (DEFAULT_CASES if args.phase == 't1' else [row[0] for row in selected])
    if not set(wanted) <= {row[0] for row in selected}:
        parser.error('unknown case for this phase')
    selected = [row for row in selected if row[0] in wanted]
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / 'runner.py').write_bytes(Path(__file__).read_bytes())
    (args.out / 'run_exception_cross_cli.py').write_bytes(Path(__file__).with_name('run_exception_cross_cli.py').read_bytes())
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')

    def command(label, parts):
        result = subprocess.run([str(x) for x in parts], capture_output=True, timeout=120)
        log = ansi.sub('', (result.stdout + result.stderr).decode(errors='replace'))
        (args.out / (label + '.log')).write_text(log)
        return result.returncode, log

    def encode(name, source):
        wat = args.out / (name + '.wat'); wasm = wat.with_suffix('.wasm')
        wat.write_text(source + '\n')
        for stage, parts in [('parse', [args.wasm_tools, 'parse', wat, '-o', wasm]),
                             ('validate', [args.wasm_tools, 'validate', wasm])]:
            status, log = command(name + '-' + stage, parts)
            assert status == 0, log
        return wasm

    provider = encode('provider', '(module (tag $t (export "t") (param i32)) '
                      '(func (export "raise") (param i32) local.get 0 throw $t))')
    for name, source, expectation, preload in selected:
        wasm = encode(name, source)
        if args.wasmtime:
            parts = [args.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', '-W', 'tail-call=y']
            if preload: parts += ['--preload', 'p=' + str(provider)]
            status, log = command(name + '-wasmtime', parts + [wasm])
            if expectation == 'uncaught':
                assert status != 0 and 'thrown Wasm exception' in log and 'wasm trap:' not in log, log
            elif expectation:
                assert status != 0 and expectation[0] in log and 'wasm trap:' in log, log
            else: assert status == 0, log
        for mode in modes:
            for policy in policies:
                for profile in profiles:
                    label = '-'.join([name, mode, policy, profile])
                    compiler_log = args.out / (label + '.compile.log')
                    parts = [args.uwvm, '-Rcc', 'tiered', '-Rcm', mode, *PROFILES[profile],
                             '-Rct', str(args.compile_threads), '-Rllvm-cache-path', 'disable',
                             '-Rllvm-call-stack', policy, '-Rclog', 'file', compiler_log,
                             '-WFE-exceptions', '-WFE-tail-call']
                    if preload: parts += ['-Wpre', provider, 'p']
                    parts += ['--run', wasm]
                    status, log = command(label, parts)
                    compilation = compiler_log.read_text() if compiler_log.exists() else ''
                    counters = {key: max(map(int, re.findall(r'\b' + key + r'=(\d+)', compilation)), default=0)
                                for key in ['compiled', 'tiered_switches', 'tiered_osr_ready',
                                            'tiered_full_requests', 'tiered_full_ready', 'tiered_full_failed']}
                    stack = [int(x) for x in re.findall(r'(?m)^uwvm: \[info\]\s+#\d+ .*?func_idx=(\d+)', log)]
                    if expectation == 'uncaught':
                        if name.startswith(('osr-uncaught-', 't2-uncaught-')):
                            expected = [0, 2] if '-tail-' in name else [0, 1, 2]
                        else:
                            expected = [0, 1, 2] if name == 'uncaught-direct' else [0, 1]
                        passed = status != 0 and 'uwvm: [fatal] Uncaught WebAssembly exception' in log
                        passed &= stack == expected and 'truncated' not in log.lower()
                    elif expectation:
                        passed = status != 0 and expectation[1] in log and 'uwvm: [fatal] Runtime crash (' in log
                        passed &= 'Uncaught WebAssembly exception' not in log
                    else: passed = status == 0
                    if args.phase == 't1':
                        evidence = ('no T0 configured; lazy native unit compiled and entry executed' if
                                    'compile-end ' in compilation and 'state=compiled' in compilation and
                                    '[uwvm-int-lazy] demand-request' not in compilation else '')
                    elif args.phase == 'osr':
                        if name.startswith(('osr-escape-', 'osr-uncaught-')):
                            entered = re.search(r'tiered-osr-enter .*?\bfn=1\b', compilation)
                            evidence = ('OSR entered its real native continuation and this activation exited exceptionally' if
                                        entered and counters['tiered_osr_ready'] == 0 else '')
                        else:
                            evidence = ('T0 loop OSR completed into native; exception resumed in enclosing caller' if
                                        'tiered-osr-request ' in compilation and counters['tiered_osr_ready'] > 0 else '')
                    else:
                        # Uncaught fatal exits before the end-of-run summary, so
                        # the ready event itself proves completion in that case.
                        ready_event = re.search(r'tiered-full-ready .*?\bfunctions=3\b', compilation)
                        entered = re.search(r'tiered-full-enter .*?\bfn=1\b', compilation)
                        evidence = ('T2 actual raw entry matched published full-module entry before throwing through the same target' if
                                    entered and ready_event and (expectation == 'uncaught' or counters['tiered_full_ready'] > 0) else '')
                    passed = bool(passed and evidence and counters['tiered_full_failed'] == 0)
                    rows.append(dict(case=name, mode=mode, trace=policy, profile=profile,
                                     command=[str(x) for x in parts], exit=status, stack=stack,
                                     evidence=evidence, counters=counters, passed=passed))
                    (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
                    if not passed:
                        raise RuntimeError(label + '\n' + log + '\n' + compilation[-8000:])
                    print('PASS', label, evidence, flush=True)
    summary = dict(passed=True, phase=args.phase, cases=len(selected), executions=len(rows), modes=modes,
                   profiles=profiles, policies=policies,
                   binary_sha256=hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
                   runner_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   fixture_source_sha256=hashlib.sha256(Path(__file__).with_name('run_exception_cross_cli.py').read_bytes()).hexdigest(),
                   scope='Numeric Core 3 exceptions; actual tiers proved per run. No throughput or complete Core 3 qualification.')
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS tiered numeric exceptions:', len(rows), 'executions')


if __name__ == '__main__':
    main()
