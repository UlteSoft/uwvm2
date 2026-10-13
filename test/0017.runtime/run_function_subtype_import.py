#!/usr/bin/env python3
"""Check Core 3 defined-function subtyping across actual module boundaries.

Run only in the remote 64 GiB Linux cgroup. Assemble/validate the same binaries
with wasm-tools, run Wasmtime independently, then exercise both products'
initializers and real interpreter/JIT entries. Equal machine carriers are not
a proof of defined-type matching. These are correctness checks, not timings.
https://webassembly.github.io/spec/core/valid/matching.html#match-deftype
https://webassembly.github.io/spec/core/exec/modules.html#instantiation
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
BASE = '(type $base (sub (func (result i32)))) '
CHILD = '(type $child (sub $base (func (result i32)))) '
OBJECTS = ('(type $object-base (sub (struct (field i32)))) '
           '(type $object-child (sub $object-base (struct (field i32) (field i64)))) ')
CHECK_42 = 'i32.const 42 i32.ne if unreachable end '
FEATURES = ('-WFE-reference-types', '-WFE-function-references', '-WFE-gc', '-WFE-tail-call')


def cases():
    provider = '(module ' + BASE + CHILD + '(func (export "target") (type $child) i32.const 42))'
    consumer = ('(module ' + BASE + '(import "P" "target" (func $target (type $base))) '
                '(func (export "_start") call $target ' + CHECK_42 + '))')
    yield 'function-child-to-base', provider, consumer, 'success'
    imported_table_header = ('(module ' + BASE + CHILD +
        '(import "P" "target" (func $target (type $base))) '
        '(table 1 funcref) (elem (i32.const 0) func $target) ')
    # Import admission accepts actual child <= declared parent, but ref.func
    # must retain the provider function's actual identity and defined type.
    yield 'parent-import-table-child', provider, (
        imported_table_header + '(func (export "_start") i32.const 0 '
        'call_indirect (type $child) ' + CHECK_42 + '))'), 'success'
    yield 'parent-import-table-child-tail', provider, (
        imported_table_header + '(func $pick (result i32) i32.const 0 '
        'return_call_indirect (type $child)) '
        '(func (export "_start") call $pick ' + CHECK_42 + '))'), 'success'
    cast_consumer = (imported_table_header + '(func (export "_start") i32.const 0 table.get '
                     'ref.cast (ref $child) call_ref $child ' + CHECK_42 + '))')
    yield 'parent-import-cast-child', provider, cast_consumer, 'success'
    yield 'parent-import-constexpr-function-field', provider, (
        '(module ' + BASE + CHILD + '(type $box (struct (field (ref null $base)))) '
        '(import "P" "target" (func $target (type $base))) (elem declare func $target) '
        '(global $keep (ref $box) (struct.new $box (ref.func $target))) '
        '(func (export "_start") global.get $keep struct.get $box 0 '
        'ref.cast (ref $child) call_ref $child ' + CHECK_42 + '))'), 'success'
    unrelated = ('(module (type $other (func (result i32))) '
                 '(func (export "target") (type $other) i32.const 42))')
    yield 'function-unrelated-final', unrelated, consumer, 'import-mismatch'
    parent = '(module ' + BASE + '(func (export "target") (type $base) i32.const 42))'
    require_child = ('(module ' + BASE + CHILD + '(import "P" "target" (func $target (type $child))) '
                     '(func (export "_start") call $target ' + CHECK_42 + '))')
    yield 'function-base-to-child', parent, require_child, 'import-mismatch'
    bridge = ('(module ' + BASE + '(import "P" "target" (func $target (type $base))) '
              '(export "target" (func $target)))')
    yield 'reexport-function-child', provider, require_child.replace('"P"', '"A"'), 'success', bridge
    yield 'reexport-base-to-child', parent, require_child.replace('"P"', '"A"'), 'import-mismatch', bridge
    yield 'reexport-parent-table-child', provider, (
        imported_table_header.replace('"P"', '"A"') + '(func (export "_start") i32.const 0 '
        'call_indirect (type $child) ' + CHECK_42 + '))'), 'success', bridge
    yield 'reexport-parent-cast-child', provider, cast_consumer.replace('"P"', '"A"'), 'success', bridge

    covariant = ('(type $base (sub (func (result (ref null $object-base))))) '
                 '(type $child (sub $base (func (result (ref $object-child))))) ')
    yield 'function-covariant-result', (
        '(module ' + OBJECTS + covariant + '(func (export "target") (type $child) '
        'struct.new_default $object-child))'), (
        '(module ' + OBJECTS + '(type $base (sub (func (result (ref null $object-base))))) '
        '(import "P" "target" (func $target (type $base))) '
        '(func (export "_start") call $target ref.is_null if unreachable end))'), 'success'
    contravariant = ('(type $base (sub (func (param (ref null $object-child)) (result i32)))) '
                     '(type $child (sub $base (func (param (ref null $object-base)) (result i32)))) ')
    yield 'function-contravariant-parameter', (
        '(module ' + OBJECTS + contravariant + '(func (export "target") (type $child) i32.const 42))'), (
        '(module ' + OBJECTS + '(type $base (sub (func (param (ref null $object-child)) (result i32)))) '
        '(import "P" "target" (func $target (type $base))) '
        '(func (export "_start") ref.null $object-child call $target ' + CHECK_42 + '))'), 'success'

    table_child = ('(module ' + BASE + CHILD + '(table (export "t") 1 funcref) '
                   '(func $target (type $child) i32.const 42) (elem (i32.const 0) func $target))')
    table_consumer = ('(module ' + BASE + '(import "P" "t" (table 1 funcref)) '
                      '(func (export "_start") i32.const 0 call_indirect (type $base) ' + CHECK_42 + '))')
    # Only the parent is present in the consumer. The child belongs to P and
    # must project to its nearest visible ancestor, not the same numeric index.
    yield 'foreign-table-child', table_child, table_consumer, 'success'
    table_unrelated = ('(module (type $other (func (result i32))) (table (export "t") 1 funcref) '
                       '(func $target (type $other) i32.const 42) (elem (i32.const 0) func $target))')
    yield 'foreign-table-unrelated-final', table_unrelated, table_consumer, 'indirect-mismatch'

    ref_provider = ('(module ' + BASE + CHILD + '(func $target (type $child) i32.const 42) '
                    '(elem declare func $target) '
                    '(func (export "get") (result (ref null $base)) ref.func $target))')
    ref_header = ('(module ' + BASE + '(import "P" "get" (func $get (result (ref null $base)))) ')
    ref_body = ('(func $pick (result i32) (local $slot (ref null $base)) '
                'call $get local.set $slot local.get $slot call_ref $base) ')
    # Repeated dynamic calls exercise the success-only projection cache while
    # each bridge still resolves the live function identity and entry address.
    ref_consumer = (ref_header + ref_body + '(func (export "_start") (local $n i32) '
                    'i32.const 32 local.set $n loop $again call $pick ' + CHECK_42 +
                    'local.get $n i32.const 1 i32.sub local.tee $n br_if $again end))')
    yield 'foreign-dynamic-call-ref', ref_provider, ref_consumer, 'success'
    tail_consumer = (ref_header + '(func $pick (result i32) (local $slot (ref null $base)) '
                     'call $get local.set $slot local.get $slot return_call_ref $base) '
                     '(func (export "_start") call $pick ' + CHECK_42 + '))')
    yield 'foreign-dynamic-return-call-ref', ref_provider, tail_consumer, 'success'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def configurations(ros):
    if ros:
        yield 'int-full', ('-Rint',)
        yield 'jit-full', ('-Raot', '-Rllvm-cache-path', 'disable')
    else:
        for engine in ('int', 'jit', 'tiered'):
            for mode in ('full', 'lazy', 'lazy+verification'):
                # Tiered's CLI contract rejects eager full compilation before
                # loading the module; it cannot be a semantic test entry.
                if engine == 'tiered' and mode == 'full':
                    continue
                flags = ('-Rcc', engine, '-Rcm', mode, '-Rct', '0')
                if engine in ('jit', 'tiered'):
                    flags += ('-Rllvm-cache-path', 'disable')
                yield engine + '-' + mode, flags


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--source-id', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--only-case', action='append')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    selected = list(cases())
    if args.only_case:
        unknown = set(args.only_case) - {case[0] for case in selected}
        if unknown:
            parser.error(f'unknown cases: {sorted(unknown)}')
        selected = [case for case in selected if case[0] in args.only_case]
    rows, artifacts = [], {}

    def check(label, command, outcome='success', oracle=False):
        argv = [str(part) for part in command]
        try:
            process = subprocess.run(argv, capture_output=True, timeout=120)
            code, raw = process.returncode, process.stdout + process.stderr
        except subprocess.TimeoutExpired as error:
            code, raw = None, (error.stdout or b'') + (error.stderr or b'')
        (args.out / (label + '.log')).write_bytes(raw)
        plain = ANSI.sub('', raw.decode(errors='replace')).lower()
        marker = {
            'import-mismatch': 'incompatible import type' if oracle else 'has a type mismatch.',
            'indirect-mismatch': 'indirect call type mismatch' if oracle else 'call_indirect: signature mismatch',
        }.get(outcome)
        if outcome == 'success':
            passed = code == 0
        else:
            passed = code is not None and code != 0 and marker in plain
            if any(token in plain for token in ('invalid parameter:', 'unknown parameter',
                    'parsing error in webassembly file', 'validation error in webassembly code')):
                passed = False
        row = {'label': label, 'argv': argv, 'exit': code, 'expected': outcome,
               'required_diagnostic': marker, 'passed': passed}
        rows.append(row)
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(json.dumps({'label': label, 'passed': passed}), flush=True)
        return passed

    for case in selected:
        name, provider, consumer, outcome = case[:4]
        roles = [('provider', provider), ('consumer', consumer)]
        if len(case) == 5:
            roles.append(('bridge', case[4]))
        binaries = {}
        valid = True
        for role, text in roles:
            wat, wasm = args.out / (name + '-' + role + '.wat'), args.out / (name + '-' + role + '.wasm')
            wat.write_text(text + '\n')
            assembled = check(name + '-' + role + '-parse', [args.wasm_tools, 'parse', wat, '-o', wasm])
            validated = assembled and check(name + '-' + role + '-validate',
                                             [args.wasm_tools, 'validate', '--features', 'all', wasm])
            valid = valid and validated
            if validated:
                artifacts[name + '-' + role] = {'wat_sha256': digest(wat), 'wasm_sha256': digest(wasm)}
            binaries[role] = wasm
        if not valid:
            continue
        provider_wasm, consumer_wasm = binaries['provider'], binaries['consumer']
        oracle_preload = ['--preload', 'P=' + str(provider_wasm)]
        preload = ['--wasm-set-main-module-name', 'C', '--wasm-preload-library', provider_wasm, 'P']
        if 'bridge' in binaries:
            oracle_preload += ['--preload', 'A=' + str(binaries['bridge'])]
            preload += ['--wasm-preload-library', binaries['bridge'], 'A']
        oracle_ok = check(name + '-wasmtime', [args.wasmtime, 'run', '-C', 'cache=n', '-W', 'gc=y',
                           *oracle_preload, consumer_wasm], outcome, oracle=True)
        if not oracle_ok:
            continue
        for mode, flags in configurations(args.ros):
            check(name + '-' + mode.replace('+', '-'),
                  [args.uwvm, *flags, *FEATURES, *preload, '--run', consumer_wasm], outcome)
    summary = {'passed': bool(rows) and all(row['passed'] for row in rows), 'cases': len(selected),
               'checks': len(rows), 'repository': 'ros' if args.ros else 'ordinary',
               'source_id': args.source_id, 'uwvm_sha256': digest(args.uwvm),
               'wasm_tools_sha256': digest(args.wasm_tools), 'wasmtime_sha256': digest(args.wasmtime),
               'runner_sha256': digest(Path(__file__)), 'artifacts': artifacts,
               'failures': [row for row in rows if not row['passed']],
               'cgroup': {key: Path('/sys/fs/cgroup', key).read_text().strip()
                          for key in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({key: summary[key] for key in ('passed', 'cases', 'checks')}), flush=True)
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
