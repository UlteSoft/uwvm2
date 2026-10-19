#!/usr/bin/env python3
"""Qualify the O2 debug-minimal fixture; no claim for full O2 module DBG."""
import argparse
import json
import os
from pathlib import Path

from linux_control import digest, guard, invoke


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--oracle', type=Path, required=True)
    parser.add_argument('--original-cases', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    before = guard(args.out)
    build_path = args.build / 'build-receipt.json'
    build = json.loads(build_path.read_text())
    oracle = json.loads(args.oracle.read_text())
    assert build['passed'] and oracle['passed']
    assert build['original_receipt_sha256'] == oracle['build_receipt_sha256']
    artifact = build['artifact']
    assert digest(artifact['path']) == artifact['sha256']
    assert digest(args.wasmtime) == oracle['wasmtime_sha256']
    report = {'passed': False, 'before': before, 'cases': [],
              'build_receipt': str(build_path), 'build_receipt_sha256': digest(build_path),
              'native_oracle_report': str(args.oracle), 'native_oracle_report_sha256': digest(args.oracle),
              'wasmtime': str(args.wasmtime), 'wasmtime_sha256': digest(args.wasmtime),
              'original_cases': str(args.original_cases),
              'original_cases_sha256': digest(args.original_cases),
              'helper_sha256': digest(Path(__file__)),
              'configuration': 'O2 debug-minimal builtin profile; does not qualify the full O2 Wasm debugger'}
    report_path = args.out / 'report.json'
    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    save()
    try:
        env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON')
               and k not in {'UWVM_PY_TEST', 'UWVM_PY_AUX', 'UWVM_PY_EMPTY'}}
        rows = [row for row in oracle['cases']
                if row['engine'] == 'wasmtime' and row['entry'] == 'semantics']
        assert len(rows) == 11
        original_cases = json.loads(args.original_cases.read_text())
        assert len(original_cases) == 11
        for case in original_cases:
            name = case['name'].removeprefix('python-cpython-')
            row = next(value for value in rows if value['name'] == name)
            assert case['args'] == row['args']
            assert case['expected_stdout'] == row['stable_stdout']
            assert case['expected_exit'] == row.get('expected_exit', 0)
            assert case['expected_stderr'] == row.get('expected_stderr', '')
            load = case.get('load_args', [])
            environment = {}
            if load:
                assert load[0] == '--wasip1-noinherit-system-environment'
                assert (len(load) - 1) % 3 == 0
                for index in range(1, len(load), 3):
                    assert load[index] == '--wasip1-add-or-replace-environment'
                    environment[load[index + 1]] = load[index + 2]
            assert environment == row.get('environment', {})
        report['manifest_oracle_join_passed'] = True
        for row in rows:
            native = next(value for value in oracle['cases'] if value['engine'] == 'native'
                          and value['entry'] == 'semantics' and value['name'] == row['name'])
            assert native['passed'] and native['stable_stdout'] == row['stable_stdout']
            guest_env = row.get('environment', {})
            argv = [str(args.wasmtime), 'run', '-C', 'cache=n,parallel-compilation=n',
                    '-W', 'max-wasm-stack=16777216',
                    *[token for pair in guest_env.items() for token in ['--env', '='.join(pair)]],
                    artifact['path'], *row['args']]
            folder = args.out / row['name']
            process = invoke(argv, folder, env=env, expected_exit=row.get('expected_exit', 0))
            assert (folder / 'stdout.log').read_text() == row['stable_stdout']
            assert (folder / 'stderr.log').read_text() == row.get('expected_stderr', '')
            assert process['retired'] and process['ownership']['closed']
            report['cases'].append({'name': row['name'], 'passed': True,
                'args': row['args'], 'environment': guest_env,
                'expected_exit': row.get('expected_exit', 0), 'stable_stdout': row['stable_stdout'],
                'expected_stderr': row.get('expected_stderr', ''),
                'lifecycle': str(folder / 'receipt.json'), 'seconds': process['seconds']})
            save()
        manifests = []
        for row in original_cases:
            row = dict(row)
            row.update(name=row['name'].replace('python-cpython-', 'python-cpython-debug-minimal-O2-'),
                       wasm=artifact['path'], wasm_sha256=artifact['sha256'],
                       optimization=build['optimization'], purpose='debug-fixture-producer-profile',
                       producer_receipt=str(build_path), producer_receipt_sha256=digest(build_path),
                       oracle=str(report_path), source_language_debugger_claim=False,
                       debug_scope='Wasm/native CPython minimal fixture; no full-stdlib or Python locals bridge',
                       disabled_builtins=build['disabled_builtins'], full_stdlib_claim=False,
                       full_O2_module_debug_claim=False)
            # O2 module inspection is not the new O0 module's inspection.
            row.pop('module_inspection', None)
            row.pop('module_inspection_sha256', None)
            row.pop('oracle_sha256', None)
            manifests.append(row)
        assert len(manifests) == 11
        report['passed'] = True
        report['after'] = guard(args.out)
        save()
        for row in manifests:
            row['oracle_sha256'] = digest(report_path)
        (args.out / 'cases.json').write_text(json.dumps(manifests, indent=2) + '\n')
    except BaseException as error:
        report['error'] = repr(error)
        report['after'] = guard(args.out)
        save()
        raise


if __name__ == '__main__':
    main()
