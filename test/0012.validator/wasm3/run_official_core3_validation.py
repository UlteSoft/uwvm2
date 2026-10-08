#!/usr/bin/env python3
"""Check binary modules extracted from the pinned official Core 3 WAST suite.

The suite is an external input, so the report records its Git commit and every
module SHA. Execution assertions in WAST are deliberately outside this runner;
this checks binary decoding and validation with the official expected verdict.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys


CORE3_CASES = (
    'array_new_elem', 'array_init_elem', 'br_on_cast', 'call_ref',
    'memory-multi', 'memory64', 'ref_cast', 'return_call_ref',
    'table64', 'throw_ref', 'try_table', 'type-subtyping',
)
FEATURES = (
    '-WFE-bulk-memory', '-WFE-exceptions', '-WFE-extended-const',
    '-WFE-function-references', '-WFE-gc', '-WFE-memory64',
    '-WFE-multi-memory', '-WFE-multi-value', '-WFE-multiple-tables',
    '-WFE-reference-types', '-WFE-relaxed-simd', '-WFE-simd',
    '-WFE-table-initializer', '-WFE-table-instructions', '-WFE-table64',
    '-WFE-tail-call', '-WFE-threads',
)
# Match the Core 3 + threads feature set under test. Wasmtime's broader
# all-proposals switch can admit unrelated future proposals and distort the
# expected-validity comparison for a standard-only implementation.
WASMTIME_CORE3_FEATURES = (
    'bulk-memory', 'multi-memory', 'multi-value', 'reference-types', 'simd',
    'relaxed-simd', 'tail-call', 'threads', 'shared-memory', 'memory64',
    'function-references', 'gc', 'extended-const', 'exceptions',
)
# These assertions belong to the older threads proposal suite, where a second
# memory/table was forbidden. Core 3 enables multi-memory/multiple-tables, so
# those exact binary modules are valid with the feature set above. Keep the
# original WAST expectation in each result, and scope this override to the
# audited official suite commit rather than silently rewriting newer suites.
CORE3_SUPERSEDED_THREADS_ASSERTIONS = {
    'proposals/threads/memory': {14, 15},
    'proposals/threads/imports': {310, 314, 318, 405, 409, 413},
}
AUDITED_SUITE_COMMIT = 'b464a4cd100d98175ae6e3890db89a2e6c8302f7'
MULTIPLE_SUPERTYPE_WAST = '''(assert_invalid
  (module
    (type $parent1 (sub (struct)))
    (type $parent2 (sub (struct)))
    (type $child (sub $parent1 $parent2 (struct))))
  "multiple supertypes"
)
'''
# Exact binary equivalent of the assertion above: 3 types, with the third
# struct subtype declaring both indices 0 and 1 as direct supertypes.
MULTIPLE_SUPERTYPE_BINARY = bytes.fromhex(
    '0061736d01000000010f0350005f0050005f00500200015f00')
VALID = {'module', 'assert_unlinkable', 'assert_uninstantiable'}
INVALID = {'assert_invalid', 'assert_malformed'}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def invoke(command, timeout=60):
    return subprocess.run([str(part) for part in command], capture_output=True,
                          timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, action='append', required=True,
                        help='One or more actual product binaries')
    parser.add_argument('--expected-source-id', required=True,
                        help='Exact product source ID embedded in every --uwvm binary')
    parser.add_argument('--case', action='append', help='WAST stem; defaults to a Core 3 subset')
    parser.add_argument('--case-plan', type=Path,
                        help='Pinned JSON plan containing all_stems and official_suite_commit')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(source/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    suite = args.suite.resolve(strict=True)
    args.out.mkdir(parents=True, exist_ok=False)
    fingerprint = source/'tools/ci/wasm3_source_fingerprint.py'
    before_manifest = args.out/'source-before.json'
    source_id = subprocess.check_output(
        [sys.executable, str(fingerprint), str(source), str(before_manifest)],
        text=True).strip()
    if source_id != args.expected_source_id:
        raise RuntimeError(f'official WAST source ID differs: {source_id}')
    snapshot = source/'snapshot-source.json'
    if snapshot.is_file() and json.loads(snapshot.read_text())['source_id'] != source_id:
        raise RuntimeError('frozen source snapshot ID differs')
    products = [path.resolve(strict=True) for path in args.uwvm]
    embedded = {}
    for product in products:
        source_ids = {item.decode() for item in re.findall(
            rb'sha256:[0-9a-f]{64}', product.read_bytes())}
        if source_ids != {source_id}:
            raise RuntimeError(f'{product}: embedded source IDs differ: {source_ids}')
        embedded[str(product)] = sorted(source_ids)
    suite_commit = invoke(['git', '-C', suite, 'rev-parse', 'HEAD'])
    assert suite_commit.returncode == 0, suite_commit.stderr
    plan_sha = None
    if args.case_plan:
        if args.case:
            parser.error('--case and --case-plan are mutually exclusive')
        plan_path = args.case_plan.resolve(strict=True)
        plan = json.loads(plan_path.read_text())
        if plan.get('official_suite_commit') != suite_commit.stdout.decode().strip():
            raise RuntimeError('official WAST suite differs from pinned case plan')
        cases = tuple(plan['all_stems'])
        if len(cases) != plan['unique_stems'] or len(set(cases)) != len(cases):
            raise RuntimeError('official WAST case plan has duplicate or missing stems')
        plan_sha = digest(plan_path)
    else:
        cases = tuple(dict.fromkeys(args.case or CORE3_CASES))
    meta = {'suite_commit': suite_commit.stdout.decode().strip(),
            'suite_url': 'https://github.com/WebAssembly/testsuite',
            'source_root': str(source), 'source_id': source_id,
            'source_before_manifest_sha256': digest(before_manifest),
            'cases': list(cases), 'case_plan_sha256': plan_sha,
            'wasm_tools_sha256': digest(args.wasm_tools),
            'wasmtime_sha256': digest(args.wasmtime),
            'uwvm_sha256': {str(path): digest(path) for path in products},
            'uwvm_embedded_source_ids': embedded,
            'runner_sha256': digest(Path(__file__))}
    (args.out/'inputs.json').write_text(json.dumps(meta, indent=2)+'\n')
    rows = []
    conversion_failures = []
    conversion_workarounds = []
    runs_jsonl = args.out/'runs.jsonl'
    runs_jsonl.touch()
    for case_index, case in enumerate(cases):
        wast = suite/(case+'.wast')
        if not wast.is_file():
            raise FileNotFoundError(wast)
        case_dir = args.out/case
        case_dir.mkdir(parents=True)
        manifest = case_dir/'commands.json'
        converted = invoke([args.wasm_tools, 'json-from-wast', wast,
                            '--wasm-dir', case_dir, '-o', manifest], timeout=120)
        (case_dir/'convert.log').write_bytes(converted.stdout+converted.stderr)
        used_type_subtyping_workaround = False
        if converted.returncode and case == 'type-subtyping' and meta['suite_commit'] == AUDITED_SUITE_COMMIT:
            original = wast.read_text()
            if original.count(MULTIPLE_SUPERTYPE_WAST) != 1:
                raise RuntimeError('audited multiple-supertype assertion changed')
            # wasm-tools 1.259 cannot lower this one malformed text assertion.
            # Preserve line numbers, convert every other assertion, and test an
            # equivalent hand-encoded invalid binary in the same module loop.
            filtered = case_dir/'type-subtyping-tool-compatible.wast'
            filtered.write_text(original.replace(MULTIPLE_SUPERTYPE_WAST,
                '\n'*MULTIPLE_SUPERTYPE_WAST.count('\n'), 1))
            converted = invoke([args.wasm_tools, 'json-from-wast', filtered,
                                '--wasm-dir', case_dir, '-o', manifest], timeout=120)
            (case_dir/'convert-filtered.log').write_bytes(converted.stdout+converted.stderr)
            used_type_subtyping_workaround = converted.returncode == 0
            if used_type_subtyping_workaround:
                conversion_workarounds.append({'case': case, 'line': 954,
                    'original_wast_sha256': digest(wast),
                    'filtered_wast_sha256': digest(filtered),
                    'binary_hex': MULTIPLE_SUPERTYPE_BINARY.hex(),
                    'reason': 'wasm-tools 1.259 cannot lower the multiple-supertype malformed text assertion'})
        if converted.returncode:
            conversion_failures.append({'case': case, 'wast_sha256': digest(wast),
                                        'exit': converted.returncode,
                                        'log': str(Path(case)/'convert.log')})
            continue
        commands = json.loads(manifest.read_text())['commands']
        if used_type_subtyping_workaround:
            binary = case_dir/'manual-multiple-supertypes.wasm'
            binary.write_bytes(MULTIPLE_SUPERTYPE_BINARY)
            commands.append({'type': 'assert_invalid', 'line': 954,
                             'filename': binary.name})
        for index, command in enumerate(commands):
            kind = command['type']
            if kind not in VALID | INVALID or 'filename' not in command:
                continue
            wasm = case_dir/command['filename']
            if wasm.suffix != '.wasm':
                continue  # Text-format malformed modules are not binary-validator inputs.
            wast_expected = kind in VALID
            superseded = (meta['suite_commit'] == AUDITED_SUITE_COMMIT and
                          kind == 'assert_invalid' and
                          command['line'] in CORE3_SUPERSEDED_THREADS_ASSERTIONS.get(case, ()))
            expected = wast_expected or superseded
            checks = [('wasmtime', [args.wasmtime, 'compile', '-C', 'cache=n',
                                   '-W', 'all-proposals=n',
                                   *(option for feature in WASMTIME_CORE3_FEATURES
                                     for option in ('-W', feature+'=y')),
                                   wasm, '-o', '/dev/null'])]
            checks.extend((f'uwvm-{number}', [product, '-m', 'validation',
                            *FEATURES, '--run', wasm])
                          for number, product in enumerate(products))
            for label, argv in checks:
                result = invoke(argv)
                passed = (result.returncode == 0) == expected
                log_name = f'{index:04d}-{label}.log'
                (case_dir/log_name).write_bytes(result.stdout+result.stderr)
                rows.append({'case': case, 'line': command['line'], 'type': kind,
                             'binary': wasm.name, 'sha256': digest(wasm),
                             'validator': label, 'expected_valid': expected,
                             'wast_expected_valid': wast_expected,
                             'superseded_by_core3': superseded,
                             'exit': result.returncode, 'passed': passed,
                             'log': str(Path(case)/log_name)})
                with runs_jsonl.open('a') as stream:
                    stream.write(json.dumps(rows[-1], sort_keys=True)+'\n')
        (args.out/'progress.json').write_text(json.dumps({
            'completed_cases': cases[:case_index+1],
            'checks': len(rows), 'failures': sum(not row['passed'] for row in rows),
            'conversion_failures': conversion_failures}, indent=2)+'\n')
    (args.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
    after_manifest = args.out/'source-after.json'
    after_id = subprocess.check_output(
        [sys.executable, str(fingerprint), str(source), str(after_manifest)],
        text=True).strip()
    if after_id != source_id or any(digest(path) != meta['uwvm_sha256'][str(path)]
                                     for path in products):
        raise RuntimeError('official WAST source or product changed during validation')
    bad = [row for row in rows if not row['passed']]
    summary = {'passed': not bad and not conversion_failures and bool(rows),
               'complete': not conversion_failures,
               'conversion_failures': conversion_failures, 'checks': len(rows),
               'conversion_workarounds': conversion_workarounds,
               'binary_modules': len({(row['case'], row['binary']) for row in rows}),
               'superseded_proposal_assertions': len({(row['case'], row['line'])
                   for row in rows if row['superseded_by_core3']}),
               'source_after_manifest_sha256': digest(after_manifest),
               'runs_jsonl_sha256': digest(runs_jsonl),
               'runs_json_sha256': digest(args.out/'runs.json'),
               'failures': bad, **meta}
    (args.out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(f'official Core 3 WAST binary validation {len(rows)-len(bad)}/{len(rows)} '
          f'({summary["binary_modules"]} modules, '
          f'{len(conversion_failures)} WAST conversion failures)', flush=True)
    if bad or conversion_failures:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
