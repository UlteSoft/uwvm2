#!/usr/bin/env python3
"""Validate Core 3 shorthand table/global declarations and independent gates."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


TYPES = {
    'anyref': (0x6e, 'gc'), 'eqref': (0x6d, 'gc'),
    'i31ref': (0x6c, 'gc'), 'structref': (0x6b, 'gc'),
    'arrayref': (0x6a, 'gc'), 'nullref': (0x71, 'gc'),
    'nullfuncref': (0x73, 'gc'),
    'nullexternref': (0x72, 'gc'),
    'exnref': (0x69, 'exceptions'), 'nullexnref': (0x74, 'exceptions'),
}
FEATURES = ('reference-types', 'gc', 'function-references',
            'exceptions', 'table-instructions')
EXPLICIT_FORMS = (
    ('nullable-func', '(type (func (param (ref null func))))',
     {'reference-types'}),
    ('nullable-extern', '(type (func (param (ref null extern))))',
     {'reference-types'}),
    ('nonnull-func', '(type (func (param (ref func))))',
     {'reference-types', 'function-references'}),
    ('nonnull-extern', '(type (func (param (ref extern))))',
     {'reference-types', 'function-references'}),
    ('nullable-nofunc', '(type (func (param (ref null nofunc))))',
     {'reference-types', 'gc'}),
    ('nullable-noextern', '(type (func (param (ref null noextern))))',
     {'reference-types', 'gc'}),
    ('nonnull-nofunc', '(type (func (param (ref nofunc))))',
     {'reference-types', 'gc'}),
    ('nonnull-noextern', '(type (func (param (ref noextern))))',
     {'reference-types', 'gc'}),
    ('ref-null-nofunc', '(func (export "_start") (drop (ref.null nofunc)))',
     {'reference-types', 'gc'}),
    ('ref-null-noextern', '(func (export "_start") (drop (ref.null noextern)))',
     {'reference-types', 'gc'}),
    ('local-nofunc', '(func (export "_start") (local nullfuncref) (drop (local.get 0)))',
     {'reference-types', 'gc'}),
    ('local-noextern', '(func (export "_start") (local nullexternref) (drop (local.get 0)))',
     {'reference-types', 'gc'}),
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--wasmtime', type=Path, required=True)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = a.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out.mkdir(parents=True, exist_ok=False)
    rows = []

    def check(name, phase, argv, expected, feature=None):
        result = subprocess.run([str(arg) for arg in argv], capture_output=True, timeout=60)
        output = result.stdout + result.stderr
        (a.out / f'{name}-{phase}.log').write_bytes(output)
        passed = (result.returncode == 0) == expected
        if not expected and feature:
            passed &= feature.encode() in output.lower()
        rows.append({'case': name, 'phase': phase, 'exit': result.returncode,
                     'expected_success': expected, 'passed': bool(passed),
                     'command': [str(arg) for arg in argv]})
        (a.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        return passed

    for site in ('table', 'global'):
        for spelling, (byte, defining_feature) in TYPES.items():
            name = f'{site}-{spelling}'
            declaration = (f'(table 1 {spelling})' if site == 'table' else
                           f'(import "x" "g" (global {spelling}))')
            wat = a.out / f'{name}.wat'
            wasm = wat.with_suffix('.wasm')
            wat.write_text(f'(module {declaration})\n')
            if not check(name, 'parse', [a.wasm_tools, 'parse', wat, '-o', wasm], True):
                continue
            data = wasm.read_bytes()
            # Each fixture has one short declaration and no custom section. This checks
            # the actual shorthand byte, rather than an equivalent rich `(ref null ht)`.
            encoded = data[11 if site == 'table' else 16] if len(data) >= 18 else data[11]
            rows.append({'case': name, 'phase': 'shorthand-byte', 'encoded': encoded,
                         'expected': byte, 'passed': encoded == byte})
            check(name, 'wasm-tools', [a.wasm_tools, 'validate', '--features', 'all', wasm], True)
            check(name, 'wasmtime', [a.wasmtime, 'compile', '-C', 'cache=n',
                                     '-W', 'all-proposals=y', wasm, '-o', '/dev/null'], True)
            enabled = [f'-WFE-{feature}' for feature in FEATURES]
            base = [a.uwvm, '-m', 'validation']
            check(name, 'enabled', [*base, *enabled, '--run', wasm], True)
            required = {'reference-types', defining_feature}
            if site == 'table':
                required.add('table-instructions')
            for feature in FEATURES:
                flags = [f'-WFD-{feature}' if item == feature else f'-WFE-{item}'
                         for item in FEATURES]
                check(name, f'{feature}-off', [*base, *flags, '--run', wasm],
                      feature not in required, feature if feature in required else None)

    # Core 3 bottom heaps belong to GC even when written as a typed reference.
    # Nullable func/extern are aliases for the Core 2 reference types; only their
    # non-nullable forms need typed function references. Verify each distinction
    # against the official wasm-tools feature validator and our independent CLI.
    for name, declaration, required in EXPLICIT_FORMS:
        wat = a.out / f'{name}.wat'
        wasm = wat.with_suffix('.wasm')
        wat.write_text(f'(module {declaration})\n')
        if not check(name, 'parse', [a.wasm_tools, 'parse', wat, '-o', wasm], True):
            continue
        for gc_on, function_refs_on in ((False, True), (True, False)):
            spec_features = 'wasm2,' + ('gc' if gc_on else '-gc') + ',' + (
                'function-references' if function_refs_on else '-function-references')
            expected = (gc_on or 'gc' not in required) and (
                function_refs_on or 'function-references' not in required)
            check(name, f'spec-gc{int(gc_on)}-func{int(function_refs_on)}',
                  [a.wasm_tools, 'validate', '--features', spec_features, wasm],
                  expected)
        enabled = [f'-WFE-{feature}' for feature in FEATURES]
        base = [a.uwvm, '-m', 'validation']
        check(name, 'enabled', [*base, *enabled, '--run', wasm], True)
        for feature in FEATURES:
            flags = [f'-WFD-{feature}' if item == feature else f'-WFE-{item}'
                     for item in FEATURES]
            check(name, f'{feature}-off', [*base, *flags, '--run', wasm],
                  feature not in required, feature if feature in required else None)

    # The old MVP table declaration is a control for the independent gates above.
    mvp = a.out / 'mvp-funcref.wat'
    mvp_wasm = mvp.with_suffix('.wasm')
    mvp.write_text('(module (table 1 funcref))\n')
    check('mvp-funcref', 'parse', [a.wasm_tools, 'parse', mvp, '-o', mvp_wasm], True)
    check('mvp-funcref', 'all-off', [a.uwvm, '-m', 'validation',
          *(f'-WFD-{feature}' for feature in FEATURES), '--run', mvp_wasm], True)
    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'source_root': str(root), 'uwvm_sha256': sha(a.uwvm),
               'wasm_tools_sha256': sha(a.wasm_tools), 'wasmtime_sha256': sha(a.wasmtime),
               'runner_sha256': sha(Path(__file__))}
    (a.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 abstract declaration gates: {sum(row["passed"] for row in rows)}/{len(rows)}')
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
