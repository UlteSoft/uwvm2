#!/usr/bin/env python3
"""Exercise Core 3 tail transfers, their new traps, and retired stack frames.

Interpreter coverage includes differing prototypes, dynamic tables and table
mutation. The current native JIT subset is scalar/vector direct full/lazy JIT on tailcc-capable targets.
Each guest is run by Wasmtime too; no legacy-call fixture substitutes for a
return_call/return_call_indirect instruction.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


def fixtures(backend):
    mutual = '''(module
      (func $a (param $n i32) (param $s i64) (result i64) (local $z i64)
        local.get $z i64.eqz if else unreachable end
        i64.const 17 local.set $z
        local.get $n i32.eqz if local.get $s return end
        local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add return_call $b)
      (func $b (param $n i32) (param $s i64) (result i64) (local $z i32)
        local.get $z i32.eqz if else unreachable end
        i32.const 31 local.set $z
        local.get $n i32.eqz if local.get $s return end
        local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add return_call $a)
      (func (export "_start")
        i32.const 1000001 i64.const 13 call $a i64.const 7000020 i64.ne if unreachable end))'''
    yield ('mutual', mutual, None)
    if backend == 'jit':
        # Each body exceeds the lazy compiler's 8 KiB grouping budget. With
        # -Rct 0, the first tail edge to the other function must demand-compile
        # its target instead of using an eagerly grouped/prefetched entry.
        cold = mutual.replace('(local $z i64)', '(local $z i64) ' + 'nop ' * 9000)
        cold = cold.replace('(local $z i32)', '(local $z i32) ' + 'nop ' * 9000)
        yield ('cold-large-functions', cold, None)
    yield ('retired-frames', '''(module
      (func $a (param $n i32)
        local.get $n i32.eqz if unreachable end
        local.get $n i32.const 1 i32.sub return_call $b)
      (func $b (param $n i32)
        local.get $n i32.eqz if unreachable end
        local.get $n i32.const 1 i32.sub return_call $a)
      (func (export "_start") i32.const 1000001 call $a))''', ('unreachable', 'unreachable', [1, 2]))
    yield ('different-parameters', '''(module
      (func $a (param $n i32) (param $s i64) (result i64)
        local.get $n i32.eqz if local.get $s return end
        i64.const 37
        local.get $n i32.const 1 i32.sub local.get $s i64.const 7 i64.add
        f64.const 9 i32.const 11 return_call $b)
      (func $b (param $n i32) (param $s i64) (param $v f64) (param $x i32) (result i64)
        local.get $v f64.const 9 f64.ne if unreachable end
        local.get $x i32.const 11 i32.ne if unreachable end
        local.get $n local.get $s return_call $a)
      (func (export "_start")
        i32.const 100000 i64.const 13 call $a i64.const 700013 i64.ne if unreachable end))''', None)
    # Force register exhaustion in both directions. Every argument is checked;
    # a constant result would miss corrupted outgoing stack-argument slots.
    params = ' '.join(f'(param $v{i} i64)' for i in range(24))
    pushes = ' '.join(f'i64.const {101+i}' for i in range(24))
    checks = ' '.join(f'local.get $v{i} i64.const {101+i} i64.ne if unreachable end' for i in range(24))
    yield ('stack-parameters', f'''(module
      (func $a (param $n i32) (result i64)
        local.get $n i32.eqz if i64.const 177 return end
        local.get $n i32.const 1 i32.sub {pushes} return_call $b)
      (func $b (param $n i32) {params} (result i64)
        {checks} local.get $n return_call $a)
      (func (export "_start") i32.const 100000 call $a i64.const 177 i64.ne if unreachable end))''', None)
    yield ('vector-parameters', '''(module
      (func $a (param $n i32) (param $v v128) (result v128)
        local.get $n i32.eqz if local.get $v return end
        local.get $n i32.const 1 i32.sub local.get $v i64.const 23 return_call $b)
      (func $b (param $n i32) (param $v v128) (param $x i64) (result v128)
        local.get $x i64.const 23 i64.ne if unreachable end
        local.get $n local.get $v return_call $a)
      (func (export "_start")
        i32.const 100000 v128.const i32x4 1 2 3 4 call $a
        v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true if else unreachable end))''', None)
    # Exceed every native register-return budget and mix unaligned scalar,
    # vector and reference fields. Every result is checked in both entry ABIs.
    tuple_types = ['i32', 'i64', 'f32', 'f64', 'v128', 'funcref', 'externref'] + ['i64'] * 24
    tuple_values = ['i32.const 31', 'i64.const 37', 'f32.const 2.5', 'f64.const 9.25',
                    'v128.const i32x4 1 2 3 4', 'ref.null func', 'ref.null extern'] + [f'i64.const {201+i}' for i in range(24)]
    tuple_checks = ['i32.const 31 i32.ne', 'i64.const 37 i64.ne', 'f32.const 2.5 f32.ne', 'f64.const 9.25 f64.ne',
                    'v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true i32.eqz', 'ref.is_null i32.eqz', 'ref.is_null i32.eqz'] + [f'i64.const {201+i} i64.ne' for i in range(24)]
    for indirect_tuple in [False, True]:
        results = '(result ' + ' '.join(tuple_types) + ')'
        tail_a = 'i32.const 0 return_call_indirect (type $ta)' if indirect_tuple else 'return_call $a'
        tail_b = 'i32.const 1 return_call_indirect (type $tb)' if indirect_tuple else 'return_call $b'
        yield ('tuple-' + ('indirect' if indirect_tuple else 'direct'), f'''(module
          (type $ta (func (param i32) {results}))
          (type $tb (func (param i32 f64) {results}))
          (table 2 funcref) (elem (i32.const 0) $a $b)
          (func $a (type $ta) (param $n i32) {results}
            local.get $n i32.eqz if {' '.join(tuple_values)} return end
            local.get $n i32.const 1 i32.sub f64.const 9 {tail_b})
          (func $b (type $tb) (param $n i32) (param $x f64) {results}
            local.get $x f64.const 9 f64.ne if unreachable end local.get $n {tail_a})
          (func (export "_start")
            i32.const 100001 call $a {' '.join(c+' if unreachable end' for c in reversed(tuple_checks))}
            i32.const 100001 i32.const 0 call_indirect (type $ta)
            {' '.join(c+' if unreachable end' for c in reversed(tuple_checks))}))''', None)
    yield ('indirect-retired-frames', '''(module
      (type $t (func (param i32))) (table 2 funcref) (elem (i32.const 0) $a $b)
      (func $a (type $t) (param $n i32)
        local.get $n i32.eqz if unreachable end
        local.get $n i32.const 1 i32.sub i32.const 1 return_call_indirect (type $t))
      (func $b (type $t) (param $n i32)
        local.get $n i32.eqz if unreachable end
        local.get $n i32.const 1 i32.sub i32.const 0 return_call_indirect (type $t))
      (func (export "_start") i32.const 1000001 call $a))''', ('unreachable', 'unreachable', [1, 2]))
    yield ('table-mutation', '''(module
      (type $t (func (param i32) (result i32)))
      (table $unused 1 funcref) (table $selected 1 2 funcref)
      (elem (table $selected) (i32.const 0) $one) (elem declare func $two)
      (func $one (type $t) local.get 0 i32.const 1 i32.add)
      (func $two (type $t) local.get 0 i32.const 2 i32.add)
      (func $dispatch (param i32) (result i32)
        i32.const 41 local.get 0 return_call_indirect $selected (type $t))
      (func (export "_start")
        i32.const 0 call $dispatch i32.const 42 i32.ne if unreachable end
        i32.const 0 ref.func $two table.set $selected
        i32.const 0 call $dispatch i32.const 43 i32.ne if unreachable end
        ref.func $one i32.const 1 table.grow $selected i32.const 1 i32.ne if unreachable end
        i32.const 1 call $dispatch i32.const 42 i32.ne if unreachable end))''', None)
    for name, selector, reference, diagnostic in [
        ('out-of-bounds', 3, 'out of bounds table access', 'table index out of bounds'),
        ('negative-index', -1, 'out of bounds table access', 'table index out of bounds'),
        ('null', 2, 'uninitialized element', 'uninitialized element'),
        ('type', 1, 'indirect call type mismatch', 'signature mismatch'),
    ]:
        yield ('indirect-' + name, '''(module
          (type $t (func (param i32) (result i32)))
          (table 3 funcref) (elem (i32.const 0) $ok $wrong)
          (func $ok (type $t) local.get 0)
          (func $wrong (param f64) (result i32) i32.const 0)
          (func $dispatch (param i32) (result i32)
            i32.const 42 local.get 0 return_call_indirect (type $t))
          (func (export "_start") i32.const %d call $dispatch drop))''' % selector,
               (reference, diagnostic, [2, 3]))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('uwvm', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--backend', choices=['int', 'jit', 'tiered'], required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--mode', action='append', help='Run only named compilation modes from this backend')
    p.add_argument('--case', action='append', help='Run only named new-syntax fixtures')
    p.add_argument('--wat2wasm', required=True)
    p.add_argument('--wasmtime', required=True)
    a = p.parse_args()
    if a.ros and a.backend == 'tiered': p.error('ROS has no tiered backend')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.output = a.output.resolve(); a.output.mkdir(parents=True, exist_ok=False)
    a.uwvm = a.uwvm.resolve()
    binary_hash = hashlib.sha256(a.uwvm.read_bytes()).hexdigest()
    modes = ['full'] if a.ros else ['full', 'lazy', 'lazy+verification']
    policies = ['instruction', 'unwind', 'none'] if a.backend != 'int' else ['instruction']
    tiers = {'all': [], 'no-t0': ['-Rtiered-disable-t0'], 'no-t2': ['-Rtiered-disable-t2'], 'no-t0-no-t2': ['-Rtiered-disable-t0', '-Rtiered-disable-t2']}
    if a.backend == 'tiered': modes = list(tiers)
    if a.mode:
        if not set(a.mode) <= set(modes): p.error('invalid compilation mode for backend/product')
        modes = a.mode
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    rows = []
    def run(label, command, diagnostic=None, stack=None):
        result = subprocess.run(command, capture_output=True, timeout=90)
        log = ansi.sub('', (result.stdout + result.stderr).decode(errors='replace'))
        (a.output / (label + '.log')).write_text(log)
        actual_stack = [int(x) for x in re.findall(r'func_idx=(\d+)', log)]
        passed = (result.returncode == 0 if diagnostic is None else result.returncode != 0 and diagnostic in log)
        if stack is not None:
            passed = passed and (actual_stack in stack if stack and isinstance(stack[0], list) else actual_stack == stack)
        rows.append(dict(case=label, command=command, exit=result.returncode, stack=actual_stack, passed=passed))
        (a.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{label}: {result.returncode}, expected {diagnostic!r}, stack {stack}\n{log}')
    selected = list(fixtures('jit' if a.backend == 'tiered' else a.backend))
    if a.case:
        if not set(a.case) <= {item[0] for item in selected}: p.error('unknown fixture')
        selected = [item for item in selected if item[0] in a.case]
    for name, wat, trap in selected:
        src = a.output / (name + '.wat'); src.write_text(wat + '\n'); wasm = src.with_suffix('.wasm')
        run(name + '-assemble', [a.wat2wasm, '--enable-tail-call', str(src), '-o', str(wasm)])
        run(name + '-wasmtime', [a.wasmtime, '-C', 'cache=n', '-W', 'tail-call=y', str(wasm)], trap[0] if trap else None)
        for mode in modes:
            for policy in policies:
                base = ['-Raot' if a.backend == 'jit' else '-Rint'] if a.ros else ['-Rcc', a.backend, '-Rcm', mode]
                if a.backend == 'tiered': base = ['-Rtiered', *tiers[mode]]
                if a.backend != 'int':
                    base += ['-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy]
                base += ['-Rclog', 'file', str(a.output / f'{name}-{mode}-{policy}.compile.log')]
                # This switch controls JIT recording only. A T0 activation still
                # has the interpreter's logical stack when native recording is off.
                expected_stack = None
                if trap:
                    expected_stack = [] if policy == 'none' else trap[2]
                    if a.backend == 'tiered' and mode in ('all', 'no-t2') and policy == 'none':
                        expected_stack = [[], trap[2]]
                run(f'{name}-{mode}-{policy}', [str(a.uwvm), *base, '-WFE-tail-call', '--run', str(wasm)],
                    trap[1] if trap else None, expected_stack)
        run(name + '-disabled', [str(a.uwvm), '-m', 'validation', '-WFD-tail-call', '--run', str(wasm)], '--wasm-feature-enable-tail-call')
    assert binary_hash == hashlib.sha256(a.uwvm.read_bytes()).hexdigest()
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (a.output / 'summary.json').write_text(json.dumps(dict(passed=True, checks=len(rows), backend=a.backend,
        ros=a.ros, binary_sha256=binary_hash, modes=modes, policies=policies), indent=2) + '\n')
    print(f'PASS {a.backend} tail transfers: {len(rows)} CLI/Wasmtime/gate checks, including retired-frame diagnostics')


if __name__ == '__main__':
    main()
