#!/usr/bin/env python3
"""Check that independent Core 3 features gate only their own syntax.

In particular, GC constant constructors are valid with extended-const off:
Core 3 lists ref.i31 and struct.new as constant instructions added by GC.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CASES = {
    'gc-i31-constant': {
        'wat': '(module (global $g i31ref (ref.i31 (i32.const 7))) '
               '(func (export "_start") global.get $g i31.get_s i32.const 7 i32.ne '
               'if unreachable end))',
        'enabled': ['-WFE-gc', '-WFD-extended-const', '-WFD-function-references',
                    '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'gc',
        'wasmtime': ['-W', 'gc=y', '-W', 'extended-const=n'],
    },
    'gc-struct-constant': {
        'wat': '(module (type $s (struct (field i32))) '
               '(global $g (ref $s) (struct.new $s (i32.const 7))) '
               '(func (export "_start") global.get $g struct.get $s 0 '
               'i32.const 7 i32.ne if unreachable end))',
        'enabled': ['-WFE-gc', '-WFD-extended-const', '-WFD-function-references',
                    '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'gc',
        'wasmtime': ['-W', 'gc=y', '-W', 'extended-const=n'],
    },
    'gc-nofunc-heap': {
        'wat': '(module (func (export "_start") ref.null nofunc ref.is_null '
               'i32.eqz if unreachable end))',
        'enabled': ['-WFE-gc', '-WFD-function-references', '-WFD-exceptions',
                    '-WFD-threads'],
        'disabled': 'gc',
        'wasmtime': ['-W', 'gc=y', '-W', 'function-references=n'],
    },
    'function-references-call': {
        'wat': '(module (type $t (func (result i32))) '
               '(func $f (type $t) i32.const 9) (elem declare func $f) '
               '(func (export "_start") ref.func $f call_ref $t '
               'i32.const 9 i32.ne if unreachable end))',
        'enabled': ['-WFE-function-references', '-WFD-gc', '-WFD-exceptions',
                    '-WFD-threads'],
        'disabled': 'function-references',
        'wasmtime': ['-W', 'function-references=y', '-W', 'gc=n'],
    },
    'exceptions-exnref-heap': {
        'wat': '(module (global $g exnref (ref.null exn)) '
               '(func (export "_start") global.get $g ref.is_null '
               'i32.eqz if unreachable end))',
        'enabled': ['-WFE-exceptions', '-WFD-gc', '-WFD-function-references',
                    '-WFD-threads'],
        'disabled': 'exceptions',
        'wasmtime': ['-W', 'exceptions=y', '-W', 'gc=n'],
    },
    'threads-shared-fence': {
        'wat': '(module (memory 1 1 shared) (func (export "_start") atomic.fence))',
        'enabled': ['-WFE-threads', '-WFD-gc', '-WFD-function-references',
                    '-WFD-exceptions'],
        'disabled': 'threads',
        'wasmtime': ['-W', 'threads=y', '-W', 'shared-memory=y', '-W', 'gc=n'],
    },
    'extended-const-numeric': {
        'wat': '(module (global $g i32 (i32.add (i32.const 1) (i32.const 2))) '
               '(func (export "_start") global.get $g i32.const 3 i32.ne '
               'if unreachable end))',
        'enabled': ['-WFE-extended-const', '-WFD-gc', '-WFD-function-references',
                    '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'extended-const',
        'wasmtime': ['-W', 'extended-const=y', '-W', 'gc=n'],
    },
    'memory64-scalar': {
        'wat': '(module (memory i64 1) '
               '(func (export "_start") i64.const 0 i32.const 7 i32.store '
               'i64.const 0 i32.load i32.const 7 i32.ne if unreachable end))',
        'enabled': ['-WFE-memory64', '-WFD-gc', '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'memory64',
        'wasmtime': ['-W', 'memory64=y'],
    },
    'table64-size': {
        'wat': '(module (table i64 1 funcref) '
               '(func (export "_start") table.size 0 i64.const 1 i64.ne '
               'if unreachable end))',
        'enabled': ['-WFE-table64', '-WFD-gc', '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'table64',
        # Wasmtime 48 accepts table64 by default but exposes no -W table64 switch.
        'wasmtime': [],
    },
    'multi-memory-size': {
        'wat': '(module (memory 1) (memory 2) '
               '(func (export "_start") memory.size 1 i32.const 2 i32.ne '
               'if unreachable end))',
        'enabled': ['-WFE-multi-memory', '-WFD-gc', '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'multi-memory',
        'wasmtime': ['-W', 'multi-memory=y'],
    },
    'tail-call-direct': {
        'wat': '(module (func $f (result i32) i32.const 7) '
               '(func $tail (result i32) return_call $f) '
               '(func (export "_start") call $tail i32.const 7 i32.ne '
               'if unreachable end))',
        'enabled': ['-WFE-tail-call', '-WFD-gc', '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'tail-call',
        'wasmtime': ['-W', 'tail-call=y'],
    },
    'relaxed-simd-exact-lanes': {
        # Integral lanes are representable exactly, so the relaxed conversion
        # has an unambiguous expected result despite its general nondeterminism.
        'wat': '(module (func (export "_start") '
               'v128.const f32x4 1 2 3 4 i32x4.relaxed_trunc_f32x4_s '
               'i32x4.extract_lane 0 i32.const 1 i32.ne if unreachable end))',
        'enabled': ['-WFE-simd', '-WFE-relaxed-simd', '-WFD-gc',
                    '-WFD-exceptions', '-WFD-threads'],
        'disabled': 'relaxed-simd',
        'wasmtime': ['-W', 'simd=y', '-W', 'relaxed-simd=y'],
    },
    'explicit-table-initializer': {
        'wat': '(module (table 1 funcref (ref.null func)) '
               '(func (export "_start") i32.const 0 table.get 0 '
               'ref.is_null i32.eqz if unreachable end))',
        # UWVM exposes the table initializer independently. Wasmtime groups
        # the syntax with Function References and has no separate CLI gate.
        'enabled': ['-WFE-table-initializer', '-WFD-function-references',
                    '-WFD-gc', '-WFD-extended-const', '-WFD-exceptions',
                    '-WFD-threads'],
        'disabled': 'table-initializer',
        'wasmtime': ['-W', 'function-references=y'],
    },
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-id', help='Expected source fingerprint of the product binary')
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')],
                   check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    modes = {'int-full': ['-Rint'], 'jit-full': ['-Raot']} if args.ros else {
        'int-full': ['-Rcc', 'int', '-Rcm', 'full'],
        'jit-full': ['-Rcc', 'jit', '-Rcm', 'full'],
    }
    rows = []
    supplemental = []

    def check(name, command, expected, diagnostic=None):
        result = subprocess.run([str(part) for part in command], capture_output=True,
                                timeout=60)
        output = result.stdout + result.stderr
        (args.out / (name + '.log')).write_bytes(output)
        plain = re.sub(rb'\x1b\[[0-9;]*m', b'', output).decode(errors='replace').lower()
        passed = ((result.returncode == 0) == expected and
                  (diagnostic is None or diagnostic in plain))
        rows.append({'case': name, 'command': [str(part) for part in command],
                     'exit': result.returncode, 'passed': bool(passed),
                     'expected_success': expected, 'required_diagnostic': diagnostic})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            print(f'FAIL {name}: {plain[-500:]}', flush=True)
        return passed

    binaries = {}
    for name, case in CASES.items():
        wat = args.out / (name + '.wat')
        wasm = args.out / (name + '.wasm')
        wat.write_text(case['wat'] + '\n')
        binaries[name] = wasm
        check(name + '-parse', [args.wasm_tools, 'parse', wat, '-o', wasm], True)
        if not wasm.exists():
            continue
        check(name + '-validate', [args.wasm_tools, 'validate', wasm], True)
        check(name + '-wasmtime', [args.wasmtime, 'run', '-C', 'cache=n',
                                   *case['wasmtime'], wasm], True)
        disabled = [('-WFD-' + case['disabled']) if flag == ('-WFE-' + case['disabled'])
                    else flag for flag in case['enabled']]
        for mode, runtime_flags in modes.items():
            check(name + '-' + mode, [args.uwvm, *runtime_flags,
                                      *case['enabled'], '--run', wasm], True)
        # The unrelated multi-memory feature is explicitly on for other cases;
        # the multi-memory case itself must keep only its independent off gate.
        unrelated = [] if case['disabled'] == 'multi-memory' else ['-WFE-multi-memory']
        check(name + '-feature-off', [args.uwvm, '-m', 'validation',
                                      *disabled, *unrelated, '--run', wasm],
              False, '--wasm-feature-enable-' + case['disabled'])
        if name == 'relaxed-simd-exact-lanes':
            # The base SIMD gate must independently reject the vector value
            # even when the relaxed extension remains enabled.
            command = [args.uwvm, '-m', 'validation', '-WFE-relaxed-simd',
                       '-WFD-simd', '--run', wasm]
            result = subprocess.run([str(part) for part in command], capture_output=True, timeout=60)
            output = result.stdout + result.stderr
            (args.out / 'relaxed-simd-base-simd-off.log').write_bytes(output)
            plain = re.sub(rb'\x1b\[[0-9;]*m', b'', output).decode(errors='replace').lower()
            passed = result.returncode != 0 and ('illegal value type' in plain or
                                                   '--wasm-feature-enable-simd' in plain or
                                                   'requires simd' in plain)
            supplemental.append({'case': 'relaxed-simd-base-simd-off',
                                 'command': [str(part) for part in command],
                                 'exit': result.returncode, 'passed': passed})

    summary = {'passed': all(row['passed'] for row in rows) and all(row['passed'] for row in supplemental),
               'checks': len(rows), 'supplemental': supplemental, 'source_id': args.source_id,
               'ros': args.ros, 'product_sha256': sha256(args.uwvm),
               'wasmtime_sha256': sha256(args.wasmtime),
               'wasm_tools_sha256': sha256(args.wasm_tools),
               'runner_sha256': sha256(Path(__file__))}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 feature independence: {sum(row["passed"] for row in rows)}/{len(rows)}')
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
