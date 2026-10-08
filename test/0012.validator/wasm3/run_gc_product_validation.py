#!/usr/bin/env python3
"""Compare exact Core 3 aggregate/cast syntax with Wasmtime and the product validator."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

from run_gc_validation import CASES as AGGREGATE_CASES
from run_reference_cast import CASES as CAST_CASES


RICH_MATCH_CASES = {}
for kind, definition, incompatible in (
        ('struct', '(struct (field i32))', '(struct (field f64))'),
        ('array', '(array i32)', '(array f64)')):
    for operation in ('local', 'table'):
        for relation in ('structurally-equivalent', 'different-layout', 'explicit-subtype'):
            name = f'{kind}-{operation}-{relation}'
            parent = f'(type $a (sub {definition}))' if relation == 'explicit-subtype' else f'(type $a {definition})'
            child = (f'(type $b (sub $a {definition}))' if relation == 'explicit-subtype'
                     else f'(type $b {incompatible if relation == "different-layout" else definition})')
            declaration = '(table 1 (ref null $a))' if operation == 'table' else ''
            body = ('i32.const 0 local.get 0 table.set 0' if operation == 'table'
                    else '(local (ref null $a)) local.get 0 local.set 1')
            RICH_MATCH_CASES[name] = (relation != 'different-layout',
                f'{parent} {child} {declaration} (func (param (ref $b)) {body})')

RICH_INLINE_CASES = {
    'select-struct': (True,
        '(type $s (struct (field i32))) '
        '(func (param (ref $s) (ref $s) i32) (result (ref $s)) '
        'local.get 0 local.get 1 local.get 2 select (result (ref $s)))'),
    'select-array': (True,
        '(type $a (array i32)) '
        '(func (param (ref $a) (ref $a) i32) (result (ref $a)) '
        'local.get 0 local.get 1 local.get 2 select (result (ref $a)))'),
    'block-struct': (True,
        '(type $s (struct)) '
        '(func (param (ref $s)) (result (ref $s)) '
        '(block (result (ref $s)) local.get 0))'),
    'block-array': (True,
        '(type $a (array i32)) '
        '(func (param (ref $a)) (result (ref $a)) '
        '(block (result (ref $a)) local.get 0))'),
    'block-anyref-shorthand': (True,
        '(func (result anyref) (block (result anyref) ref.null any))'),
    'block-i31-nonnull': (True,
        '(func (result (ref i31)) '
        '(block (result (ref i31)) i32.const 17 ref.i31))'),
    'select-struct-wrong-heap': (False,
        '(type $s (struct (field i32))) (type $t (struct (field f64))) '
        '(func (param (ref $s) (ref $t) i32) (result (ref $s)) '
        'local.get 0 local.get 1 local.get 2 select (result (ref $s)))'),
    'block-struct-wrong-heap': (False,
        '(type $s (struct (field i32))) (type $t (struct (field f64))) '
        '(func (param (ref $t)) (result (ref $s)) '
        '(block (result (ref $s)) local.get 0))'),
}

REF_EQ_CASES = {
    'eqref-pair': (True,
        '(func (param eqref eqref) (result i32) local.get 0 local.get 1 ref.eq)'),
    'i31-struct-pair': (True,
        '(func (param i31ref structref) (result i32) local.get 0 local.get 1 ref.eq)'),
    'polymorphic': (True, '(func (result i32) unreachable ref.eq)'),
    'funcref-operand': (False,
        '(func (param funcref eqref) (result i32) local.get 0 local.get 1 ref.eq)'),
    'externref-operand': (False,
        '(func (param externref eqref) (result i32) local.get 0 local.get 1 ref.eq)'),
    'numeric-operand': (False,
        '(func (param i32 eqref) (result i32) local.get 0 local.get 1 ref.eq)'),
}

REF_NULL_CASES = {
    'struct-type': (True, '(type $s (struct)) (func (result (ref null $s)) ref.null $s)'),
    'array-type': (True, '(type $a (array i32)) (func (result (ref null $a)) ref.null $a)'),
    'abstract-any': (True, '(func (result anyref) ref.null any)'),
    'abstract-eq': (True, '(func (result eqref) ref.null eq)'),
    'abstract-i31': (True, '(func (result (ref null i31)) ref.null i31)'),
    'abstract-struct': (True, '(func (result structref) ref.null struct)'),
    'abstract-array': (True, '(func (result arrayref) ref.null array)'),
    'abstract-none': (True, '(func (result anyref) ref.null none)'),
    'abstract-exn': (True, '(func (result exnref) ref.null exn)'),
    'abstract-noexn': (True, '(func (result exnref) ref.null noexn)'),
    'unknown-type': (False, '(func (result anyref) ref.null 37)'),
    'cast-exn': (True, '(func (param exnref) (result exnref) local.get 0 ref.cast (ref null exn))'),
    'br-on-non-null-exn': (True,
        '(func (param exnref) (result (ref exn)) '
        '(block (result (ref exn)) local.get 0 br_on_non_null 0 unreachable))'),
    'br-on-null-exn': (True, '(func (param exnref) (block local.get 0 br_on_null 0 drop))'),
    'as-non-null-exn': (True, '(func (param exnref) (result (ref exn)) local.get 0 ref.as_non_null)'),
    'is-null-exn': (True, '(func (param exnref) (result i32) local.get 0 ref.is_null)'),
}
EXN_FEATURE_CASES = {
    'ref-null-abstract-exn', 'ref-null-abstract-noexn', 'ref-null-cast-exn',
    'ref-null-br-on-non-null-exn', 'ref-null-br-on-null-exn', 'ref-null-as-non-null-exn',
    'ref-null-is-null-exn',
}
EXN_FUNCTION_REFERENCE_CASES = {
    'ref-null-br-on-non-null-exn', 'ref-null-br-on-null-exn', 'ref-null-as-non-null-exn',
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
    cases = [('aggregate-' + name, valid, text) for name, (valid, text) in AGGREGATE_CASES.items()]
    cases.extend(('cast-' + name, valid, text) for name, (valid, text) in CAST_CASES.items())
    cases.extend(('rich-match-' + name, valid, text) for name, (valid, text) in RICH_MATCH_CASES.items())
    cases.extend(('rich-inline-' + name, valid, text) for name, (valid, text) in RICH_INLINE_CASES.items())
    cases.extend(('ref-eq-' + name, valid, text) for name, (valid, text) in REF_EQ_CASES.items())
    cases.extend(('ref-null-' + name, valid, text) for name, (valid, text) in REF_NULL_CASES.items())
    rows = []

    def run(name, phase, command, should_pass, strict=True):
        result = subprocess.run([str(item) for item in command], capture_output=True, timeout=120)
        output = result.stdout + result.stderr
        (args.out / f'{name}-{phase}.log').write_bytes(output)
        passed = (result.returncode == 0) == should_pass
        rows.append({'case': name, 'phase': phase, 'command': [str(item) for item in command],
                     'exit': result.returncode, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if strict and not passed:
            raise RuntimeError(f'{name} {phase}: exit={result.returncode}\n'
                               f'{output.decode(errors="replace")[-700:]}')

    for name, valid, text in cases:
        wat = args.out / f'{name}.wat'
        wasm = args.out / f'{name}.wasm'
        wat.write_text('(module ' + text + ')\n')
        run(name, 'parse', [args.wasm_tools, 'parse', wat, '-o', wasm], True)
        # Wasm-tools is used for byte generation. Keep its validator verdict as separate
        # evidence: it currently accepts some invalid br_on_cast downcasts rejected by
        # both Core 3 validity rules and Wasmtime's official validating compiler.
        run(name, 'wasm-tools-validation', [args.wasm_tools, 'validate', '--features', 'all', wasm], valid, False)
        run(name, 'wasmtime-validation',
            [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,function-references=y,tail-call=y',
             wasm, '-o', '/dev/null'], valid)
        if name.startswith('rich-inline-') or name.startswith('ref-eq-'):
            run(name, 'wasmtime-function-references-off-gc-on',
                [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,function-references=n',
                 wasm, '-o', '/dev/null'], valid)
            run(name, 'wasmtime-gc-off',
                [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=n,function-references=y',
                 wasm, '-o', '/dev/null'], False)
        if name in EXN_FEATURE_CASES:
            # Exception references are independent of GC. ref.cast itself belongs to GC.
            gc_off_expected = name != 'ref-null-cast-exn'
            run(name, 'wasmtime-gc-off-exceptions-on',
                [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=n,exceptions=y,function-references=y',
                 wasm, '-o', '/dev/null'], gc_off_expected)
            run(name, 'wasmtime-gc-on-exceptions-off',
                [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,exceptions=n',
                 wasm, '-o', '/dev/null'], False)
            run(name, 'wasmtime-function-references-off',
                [args.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,exceptions=y,function-references=n',
                 wasm, '-o', '/dev/null'], name not in EXN_FUNCTION_REFERENCE_CASES)
        if not args.oracle_only:
            run(name, 'uwvm-validation',
                [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-function-references',
                 '-WFE-gc', '-WFE-exceptions', '-WFE-tail-call', '--run', wasm], valid)
            if name.startswith('rich-inline-') or name.startswith('ref-eq-'):
                run(name, 'uwvm-function-references-off-gc-on',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc',
                     '--run', wasm], valid)
                run(name, 'uwvm-gc-off',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-function-references',
                     '--run', wasm], False)
            if name in EXN_FEATURE_CASES:
                run(name, 'uwvm-gc-off-exceptions-on',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-function-references', '-WFE-exceptions',
                     '--run', wasm], gc_off_expected)
                run(name, 'uwvm-gc-on-exceptions-off',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc',
                     '--run', wasm], False)
                run(name, 'uwvm-function-references-off',
                    [args.uwvm, '-m', 'validation', '-WFE-reference-types', '-WFE-gc', '-WFE-exceptions',
                     '--run', wasm], name not in EXN_FUNCTION_REFERENCE_CASES)
    summary = {'passed': all(row['passed'] for row in rows if row['phase'] != 'wasm-tools-validation'),
               'wasm_tools_disagreements': [row['case'] for row in rows
                                            if row['phase'] == 'wasm-tools-validation' and not row['passed']],
               'cases': len(cases),
               'checks': len(rows), 'runner_sha256': sha(Path(__file__)),
               'wasm_tools_sha256': sha(args.wasm_tools), 'wasmtime_sha256': sha(args.wasmtime)}
    if args.uwvm is not None:
        summary['uwvm_sha256'] = sha(args.uwvm)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 product GC validation: {sum(row["passed"] for row in rows)}/{len(rows)}', flush=True)


if __name__ == '__main__':
    main()
