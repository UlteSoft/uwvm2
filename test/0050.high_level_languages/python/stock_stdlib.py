#!/usr/bin/env python3
"""Separate stock-CLI/stdlib qualification, never a frozen-fixture substitute.

All copying, hashing and execution require the original SSH Linux cgroup.
The guest source is passed unchanged with -c. Failures remain per module.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

from linux_control import digest, guard, invoke

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from inspect_wasm import inspect
from run_matrix import argv_for


def inventory(directory):
    result = []
    for path in sorted(directory.rglob('*')):
        if path.is_file():
            result.append({'path': str(path.relative_to(directory)),
                           'bytes': path.stat().st_size, 'sha256': digest(path)})
    return result


def stage_bundle(source, wasi_build, destination):
    lib = source / 'Lib'
    relative = Path((wasi_build / 'pybuilddir.txt').read_text().strip())
    assert not relative.is_absolute() and '..' not in relative.parts
    sysconfig = (wasi_build / relative).resolve()
    assert sysconfig.is_relative_to(wasi_build.resolve())
    assert lib.is_dir() and sysconfig.is_dir()
    inputs = [lib, sysconfig]
    expected_bytes = 0
    for directory in inputs:
        for path in directory.rglob('*'):
            if '__pycache__' in path.parts or path.suffix in {'.pyc', '.pyo'}:
                continue
            if path.is_symlink():
                raise RuntimeError('symlink in proposed stdlib bundle: ' + str(path))
            if path.is_file():
                expected_bytes += path.stat().st_size
    assert expected_bytes < 512 << 20, 'bundle exceeds explicit 512MiB static-input budget'
    assert shutil.disk_usage(destination.parent).free > expected_bytes + (2 << 30)
    destination.mkdir()
    ignore = shutil.ignore_patterns('__pycache__', '*.pyc', '*.pyo')
    shutil.copytree(lib, destination / 'lib/python3.14', ignore=ignore)
    shutil.copytree(sysconfig, destination / 'sysconfig', ignore=ignore)
    assets = inventory(destination)
    assert sum(row['bytes'] for row in assets) == expected_bytes
    return {'source_lib': str(lib), 'wasi_sysconfig': str(sysconfig),
            'files': assets, 'bytes': expected_bytes,
            'excluded': ['__pycache__', '*.pyc', '*.pyo'],
            'scope': 'complete copied source Lib and generated WASI sysconfig, not all APIs supported'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--products', type=Path, default=Path('/tmp/high-level-20261017/products'))
    parser.add_argument('--load-args-policy', type=Path,
                        help='Explicit repository -> list[str] extra UWVM load arguments; default {}')
    parser.add_argument('--engines', default='native,wasmtime,uwvm2,uwvm2-stable,uwvm2-ros')
    parser.add_argument('--timeout', type=int, default=1200)
    args = parser.parse_args()
    policy_path = args.load_args_policy.resolve(strict=True) if args.load_args_policy is not None else None
    policy_bytes = policy_path.read_bytes() if policy_path is not None else None
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate load-args policy key: ' + key)
            result[key] = value
        return result
    policy = (json.loads(policy_bytes.decode('utf-8'), object_pairs_hook=unique_object)
              if policy_bytes is not None else {})
    # Assemble arguments only: the shared validator checks the entire mapping,
    # including entries for repositories not selected in this invocation.
    argv_for(args.products / 'uwvm2/uwvm', 'uwvm2',
             {'wasm': str(args.build / 'artifacts/cpython-3.14.8-standard.wasm'),
              'load_args_by_repository': policy}, False)
    policy_sha256 = hashlib.sha256(policy_bytes).hexdigest() if policy_bytes is not None else None
    args.out.mkdir(parents=True, exist_ok=False)
    before = guard(args.out)
    policy_snapshot = args.out / 'load-args-policy.json' if policy_bytes is not None else None
    if policy_snapshot is not None:
        policy_snapshot.write_bytes(policy_bytes)
        assert digest(policy_snapshot) == policy_sha256 == digest(policy_path)
    os.sched_setaffinity(0, {4})
    own = Path(__file__).resolve().parent
    source_file = own / 'stock_stdlib_probe.py'
    source_bytes = source_file.read_bytes()
    code = source_bytes.decode('utf-8')
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()
    saved_source = args.out / 'stock_stdlib_probe.py'
    saved_source.write_bytes(source_bytes)
    assert digest(saved_source) == source_sha256 == digest(source_file)
    build_path = args.build / 'build-receipt.json'
    build = json.loads(build_path.read_text())
    assert build['passed']
    source = Path(build['source'])
    wasi = Path(build['wasi_build'])
    native = Path(build['native_python']['path'])
    assert digest(native) == build['native_python']['sha256']
    native_relative = Path((native.parent / 'pybuilddir.txt').read_text().strip())
    assert not native_relative.is_absolute() and '..' not in native_relative.parts
    native_generated = (native.parent / native_relative).resolve()
    assert native_generated.is_relative_to(native.parent.resolve())
    assert native_generated.is_dir()
    wasm = args.build / 'artifacts/cpython-3.14.8-standard.wasm'
    assert digest(wasm) == digest(wasi / 'python.wasm')
    stock_module = inspect(wasm)
    exports = {value['name']: value['kind'] for value in stock_module['exports']}
    assert exports.get('_start') == 0 and exports.get('memory') == 2
    assert all(value['module'] == 'wasi_snapshot_preview1' for value in stock_module['imports'])
    bundle = args.out / 'bundle'
    bundle_record = stage_bundle(source, wasi, bundle)
    bundle_path = args.out / 'stdlib-assets.json'
    bundle_path.write_text(json.dumps(bundle_record, indent=2) + '\n')
    native_assets_path = args.out / 'native-generated-assets.json'
    native_assets_path.write_text(json.dumps({'directory': str(native_generated),
        'files': inventory(native_generated),
        'scope': 'native CPython extension/sysconfig dependencies; never mounted into Wasm'}, indent=2) + '\n')
    guest_arguments = ['-S', '-B', '-X', 'utf8', '-c', code, 'π', '你好', '', 'space value']
    payload_env = {'UWVM_PY_AUX': 'two-你好', 'UWVM_PY_EMPTY': '', 'UWVM_PY_TEST': 'python-π'}
    guest_env = {'PYTHONHOME': '/python', 'PYTHONPATH': '/python/sysconfig', **payload_env}
    controller_env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON')
                      and k not in payload_env}
    report = {'passed': False, 'before': before, 'cases': [],
              'kind': 'real stock CPython CLI with explicit stdlib preopen',
              'build_receipt': str(build_path), 'build_receipt_sha256': digest(build_path),
              'helper_sha256': digest(Path(__file__)), 'guest_source': str(source_file),
              'load_args_policy': {'provided_path': str(args.load_args_policy) if args.load_args_policy is not None else None,
                  'resolved_path': str(policy_path) if policy_path is not None else None,
                  'sha256_before': policy_sha256, 'map': policy,
                  'snapshot': str(policy_snapshot) if policy_snapshot is not None else None,
                  'scope': 'Explicit selected repository policy, not proof of formal minimality; native/Wasmtime unchanged'},
              'helper_dependencies': {str(path): digest(path) for path in [
                  own / 'linux_control.py', own.parent / 'owned_process.py',
                  own.parent / 'run_matrix.py', own.parent / 'inspect_wasm.py']},
              'guest_source_sha256': source_sha256,
              'executed_c_string_utf8_sha256': hashlib.sha256(code.encode('utf-8')).hexdigest(),
              'stdlib_assets': str(bundle_path), 'stdlib_assets_sha256': digest(bundle_path),
              'stdlib_bytes': bundle_record['bytes'], 'stdlib_file_count': len(bundle_record['files']),
              'native_generated_directory': str(native_generated),
              'native_generated_assets': str(native_assets_path),
              'native_generated_assets_sha256': digest(native_assets_path),
              'stock_wasm': str(wasm), 'stock_wasm_sha256': digest(wasm),
              'stock_module': stock_module, 'stock_import_count': len(stock_module['imports']),
              'stock_identity_scope': 'first independent stock-CLI identity; not one of six frozen artifacts',
              'guest_environment': guest_env,
              'guest_argv': ['-c', 'π', '你好', '', 'space value'],
              'preopens': [{'host': str(bundle), 'guest': '/python'}],
              'filesystem_rights_claim': 'explicit bundle only; mount syntax alone is not read-only enforcement',
              'python_language_trace_is_Rdbg_bridge': False,
              'native_concurrency_or_full_asyncio_claim': False}
    report_path = args.out / 'report.json'
    bindings = {str(Path(__file__).resolve()): report['helper_sha256'],
                str(source_file): source_sha256, str(saved_source): source_sha256,
                str(build_path): report['build_receipt_sha256'],
                str(bundle_path): report['stdlib_assets_sha256'],
                str(native_assets_path): report['native_generated_assets_sha256'],
                **report['helper_dependencies']}
    if policy_path is not None:
        bindings[str(args.load_args_policy.absolute())] = policy_sha256
        bindings[str(policy_path)] = policy_sha256
        bindings[str(policy_snapshot)] = policy_sha256
    expected_probes = {'runtime', 'args_env', 'dataclasses', 'annotationlib', 'inspect',
                       'typing', 'weakref_gc', 'collections', 'queue', 'traceback',
                       'json', 'enum', 're', 'math', 'struct', 'python_trace'}
    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    save()
    native_stdout = None
    native_passed = False
    try:
        engines = args.engines.split(',')
        assert engines[0] == 'native' and len(engines) == len(set(engines))
        for engine in engines:
            folder = args.out / 'runs' / engine
            if engine == 'native':
                native_env = {'PYTHONPATH': str(bundle / 'lib/python3.14') + ':' + str(native_generated),
                              **payload_env}
                # Native build path discovery is retained. Its own shared
                # extensions/sysconfig are distinct from the WASI build data.
                # Environment is applied after the authentication bootstrap.
                argv = ['/usr/bin/env', '-i', *[key + '=' + value for key, value in native_env.items()],
                        str(native), *guest_arguments]
                binary = native
            elif engine == 'wasmtime':
                binary = args.wasmtime
                argv = [str(binary), 'run', '-C', 'cache=n,parallel-compilation=n',
                        '-W', 'max-wasm-stack=16777216', '--dir', str(bundle) + '::/python',
                        *[token for pair in guest_env.items() for token in ['--env', '='.join(pair)]],
                        str(wasm), *guest_arguments]
            else:
                assert engine in {'uwvm2', 'uwvm2-stable', 'uwvm2-ros'}
                binary = args.products / engine / 'uwvm'
                load = ['--wasip1-noinherit-system-environment', '--wasip1-mount-dir', '/python', str(bundle),
                        *[token for pair in guest_env.items()
                          for token in ['--wasip1-add-or-replace-environment', *pair]]]
                argv = argv_for(binary, engine, {'wasm': str(wasm), 'args': guest_arguments,
                                                'load_args': load,
                                                'load_args_by_repository': policy}, False)
            row = {'engine': engine, 'passed': False, 'binary': str(binary),
                   'binary_sha256_before': digest(binary), 'argv': argv,
                   'load_args_policy_sha256': policy_sha256,
                   'repository_load_args': policy.get(engine, []) if engine.startswith('uwvm2') else [],
                   'lifecycle': str(folder / 'receipt.json')}
            try:
                invoke(argv, folder, env=controller_env, timeout=args.timeout)
            except Exception as error:
                row['execution_error'] = repr(error)
            process = {}
            if (folder / 'receipt.json').exists():
                process = json.loads((folder / 'receipt.json').read_text())
                row['exit'] = process.get('exit')
                row['retired'] = process.get('retired', False)
                row['ownership'] = process.get('ownership')
            stdout_bytes = (folder / 'stdout.log').read_bytes() if (folder / 'stdout.log').exists() else b''
            stderr_bytes = (folder / 'stderr.log').read_bytes() if (folder / 'stderr.log').exists() else b''
            utf8_valid = True
            try:
                stdout, stderr = stdout_bytes.decode('utf-8'), stderr_bytes.decode('utf-8')
            except UnicodeDecodeError as error:
                utf8_valid = False
                row['utf8_error'] = repr(error)
                stdout, stderr = stdout_bytes.decode('utf-8', 'replace'), stderr_bytes.decode('utf-8', 'replace')
            allowed = ('[llvm-jit-full] ', '[llvm-jit-cache] ', '[gc-managed] ') if engine.startswith('uwvm2') else ()
            diagnostics, guest_stderr = [], []
            prefix_bytes = tuple(prefix.encode('ascii') for prefix in allowed)
            for line in stderr_bytes.splitlines(keepends=True):
                if prefix_bytes and b'\r' not in line and line.endswith(b'\n') and line.startswith(prefix_bytes):
                    diagnostics.append(line)
                else:
                    guest_stderr.append(line)
            guest_stderr_bytes = b''.join(guest_stderr)
            assert not any(prefix in code for prefix in allowed), 'fixed guest emits no engine-prefix literals'
            row.update(stdout=stdout, stderr=stderr,
                       stdout_sha256=hashlib.sha256(stdout_bytes).hexdigest(),
                       stderr_sha256=hashlib.sha256(stderr_bytes).hexdigest(),
                       utf8_valid=utf8_valid,
                       engine_diagnostics=b''.join(diagnostics).decode('utf-8', 'replace'),
                       guest_stderr=guest_stderr_bytes.decode('utf-8', 'replace'),
                       guest_stderr_sha256=hashlib.sha256(guest_stderr_bytes).hexdigest(),
                       stderr_filter={'whole_line_prefixes': list(allowed),
                                      'complete_newline_required': True,
                                      'carriage_returns_rejected': True,
                                      'all_other_stderr_must_be_empty': True,
                                      'guest_source_contains_prefix_literals': any(prefix in code for prefix in allowed)},
                       binary_sha256_after=digest(binary), wasm_sha256_after=digest(wasm))
            try:
                result = json.loads(stdout)
                row['module_results'] = result
                assert isinstance(result, dict) and isinstance(result.get('passed'), bool)
                assert result['schema'] == 1 and result['kind'] == 'stock-cpython-stdlib-operations'
                probes = result['probes']
                assert isinstance(probes, dict) and set(probes) == expected_probes
                assert all(isinstance(value, dict) for value in probes.values())
                assert all(value['status'] in {'PASS', 'FAIL'} for value in probes.values())
                for value in probes.values():
                    if value['status'] == 'PASS':
                        assert isinstance(value['details'], dict)
                    else:
                        assert isinstance(value['error_type'], str) and isinstance(value['error'], str)
                row['failed_modules'] = [name for name, value in probes.items() if value['status'] == 'FAIL']
                assert result['passed'] == (not row['failed_modules'])
                assert result['summary'] == {'passed': len(probes) - len(row['failed_modules']),
                                             'failed': len(row['failed_modules']), 'total': len(probes)}
                canonical = json.dumps(result, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n'
                assert stdout_bytes == canonical.encode('utf-8'), 'noncanonical or non-LF JSON stdout'
                modules_ok = result['passed'] and utf8_valid
            except (ValueError, AssertionError, KeyError, TypeError) as error:
                row['output_error'] = repr(error)
                modules_ok = False
            ownership = row.get('ownership') or {}
            lifecycle_ok = (not row.get('execution_error') and not process.get('error')
                and row.get('exit') == 0 and row.get('retired') and ownership.get('closed')
                and bool(ownership.get('members'))
                and all(not member['pidfd_retained'] for member in ownership.get('members', []))
                and row['binary_sha256_before'] == row['binary_sha256_after']
                and row['wasm_sha256_after'] == report['stock_wasm_sha256'])
            if engine == 'native':
                native_stdout = stdout_bytes
                native_passed = modules_ok and lifecycle_ok and guest_stderr_bytes == b''
                (args.out / 'expected-stdout.txt').write_bytes(stdout_bytes)
            row['complete_stdout_matches_native'] = stdout_bytes == native_stdout
            row['passed'] = (native_passed and modules_ok and lifecycle_ok
                             and guest_stderr_bytes == b'' and row['complete_stdout_matches_native'])
            report['cases'].append(row)
            save()
        report['stdlib_assets_unchanged'] = inventory(bundle) == bundle_record['files']
        native_record = json.loads(native_assets_path.read_text())
        report['native_generated_assets_unchanged'] = inventory(native_generated) == native_record['files']
        assert report['stdlib_assets_unchanged'] and report['native_generated_assets_unchanged']
        report['passed'] = bool(report['cases']) and all(row['passed'] for row in report['cases'])
    except BaseException as error:
        report['passed'] = False
        report['error'] = repr(error)
        raise
    finally:
        policy_after = (digest(policy_path) if policy_path is not None and policy_path.is_file() else None)
        provided_policy_after = (digest(args.load_args_policy)
                                 if args.load_args_policy is not None and args.load_args_policy.is_file() else None)
        report['load_args_policy'].update(sha256_after=policy_after,
                                         provided_path_sha256_after=provided_policy_after,
                                         unchanged=provided_policy_after == policy_after == policy_sha256)
        report['input_mutations'] = [path for path, expected in bindings.items()
                                     if not Path(path).is_file() or digest(path) != expected]
        if report['input_mutations']:
            report['passed'] = False
        try:
            report['after'] = guard(args.out)
        except Exception as error:
            report['after_guard_error'] = repr(error)
            report['passed'] = False
        save()
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
