#!/usr/bin/env python3
"""Serial real-producer AS/MoonGC source break/step matrix on candidate products.

Prepare-only resolves original producer maps without executing a VM. Execution
requires all selected product identities and preserves each helper transcript.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def producer_source(wasm, suffix):
    # The runner resolves the module's filesystem location before loading it.
    # Resolve relative map roots against that location, retaining the producer's
    # absolute spelling when it declares one (MoonBit uses /tmp paths).
    sidecar = Path(str(wasm) + '.map')
    mapping = json.loads(sidecar.read_text())
    users = [name for name in mapping['sources'] if name.endswith(suffix)]
    assert len(users) == 1, (sidecar, users)
    declared = Path(mapping.get('sourceRoot', '')) / users[0]
    if not declared.is_absolute():
        declared = wasm.resolve().parent / declared
    expected = os.path.normpath(str(declared))
    source = Path(expected)
    assert source.is_file(), ('producer source is missing', source)
    assert len(source.read_text().splitlines()) >= 18
    return sidecar, source, expected


def stop_driver(child):
    if child.poll() is not None:
        return
    # SIGINT lets the helper's finally clause close its separate PTY session.
    os.killpg(child.pid, signal.SIGINT)
    try:
        child.wait(timeout=15)
    except subprocess.TimeoutExpired:
        # Retain pidfds before killing the direct driver, so PID reuse cannot
        # turn recovery into a signal to an unrelated process.
        owned = []
        pending = [child.pid]
        while pending:
            pid = pending.pop()
            try:
                owned.append(os.pidfd_open(pid))
                for task in Path(f'/proc/{pid}/task').iterdir():
                    pending.extend(int(value) for value in (task / 'children').read_text().split())
            except (FileNotFoundError, ProcessLookupError):
                pass
        for fd in reversed(owned):
            try:
                signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(fd)
        child.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--helpers', type=Path)
    parser.add_argument('--runner', type=Path, help='Optional immutable copy of java/run_source_debug.py')
    parser.add_argument('--repository', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    parser.add_argument('--language', action='append', choices=['assemblyscript', 'moonbit-gc'])
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    helpers = args.helpers or args.arena / 'uwvm2-ros/test/0050.high_level_languages'
    sys.path.insert(0, str(helpers))
    from build_products import admission
    before = admission()
    runner = args.runner or helpers / 'java/run_source_debug.py'
    assert runner.is_file()
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'prepare_only': args.prepare_only, 'admission_before': before,
              'scope': 'actual original producer source break, raw function name, source step, guest checksum and normal shutdown',
              'typed_language_values_claim': False, 'helper_sha256': digest(runner), 'plans': [], 'runs': []}
    roots = args.arena / 'other/artifacts'
    definitions = {
        'assemblyscript': (roots / 'assemblyscript/assemblyscript-debug.wasm', 'workload.ts', 'workload/workload'),
        'moonbit-gc': (roots / 'moonbit/moonbit-wasm-gc-debug.wasm', 'workload.mbt', '_M0FP24uwvm11high__level8workload')}
    for language in args.language or list(definitions):
        wasm, suffix, name = definitions[language]
        sidecar, source, expected = producer_source(wasm, suffix)
        expected_stdout = args.arena / 'other/test/0050.high_level_languages' / ('moonbit' if language == 'moonbit-gc' else language) / 'expected.stdout'
        for repository in args.repository or ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            binary = args.arena / 'products' / repository / 'uwvm'
            destination = args.out / (language + '-' + repository)
            argv = [sys.executable, str(runner), '--binary', str(binary), '--wasm', str(wasm),
                    '--source', str(source), '--expected-source', expected, '--line', '18',
                    '--expression', 'acc', '--expected-name', name, '--expected-after-step-line', '18',
                    '--expected-stdout', str(expected_stdout), '--helpers', str(helpers), '--out', str(destination)]
            if repository != 'uwvm2':
                argv.append('--ros')
            report['plans'].append({'language': language, 'repository': repository, 'argv': argv,
                'wasm_sha256': digest(wasm), 'map': str(sidecar), 'map_sha256': digest(sidecar),
                'source': str(source), 'source_sha256': digest(source), 'expected_source': expected,
                'expected_raw_name': name, 'report': str(destination / 'report.json')})
    path = args.out / 'matrix.json'
    path.write_text(json.dumps(report, indent=2))
    if args.prepare_only:
        print(json.dumps({'prepared': len(report['plans']), 'manifest': str(path)}))
        return
    lock = os.open('/tmp/uwvm-hll-benchmark.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    try:
        # All product identities are admitted before the first VM starts.
        for plan in report['plans']:
            binary = args.arena / 'products' / plan['repository'] / 'uwvm'
            identity = json.loads((binary.parent / 'identity.json').read_text())
            assert identity['binary_sha256'] == digest(binary)
            plan['binary_sha256'] = identity['binary_sha256']
        for plan in report['plans']:
            child = None
            row = {'language': plan['language'], 'repository': plan['repository'], 'passed': False}
            try:
                with (args.out / (plan['language'] + '-' + plan['repository'] + '.driver.log')).open('wb') as log:
                    child = subprocess.Popen(plan['argv'], stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                    row['exit'] = child.wait(timeout=1800)
                detail = json.loads(Path(plan['report']).read_text())
                for key in ['passed', 'source_breakpoint_pass', 'source_step_pass', 'function_name_display_pass',
                            'source_variable_value_verified', 'typed_language_values_claim', 'positions', 'locals', 'print', 'error']:
                    if key in detail:
                        row[key] = detail[key]
                assert row['exit'] == 0 and all(detail[key] for key in ['passed', 'source_breakpoint_pass', 'source_step_pass', 'function_name_display_pass', 'expected_guest_output_verified'])
                assert digest(plan['map']) == plan['map_sha256'] and digest(plan['source']) == plan['source_sha256']
                assert digest(runner) == report['helper_sha256']
            except Exception as error:
                row.update(passed=False, driver_error=repr(error))
            finally:
                if child is not None:
                    stop_driver(child)
                report['runs'].append(row)
                path.write_text(json.dumps(report, indent=2))
        report['admission_after'] = admission()
        assert before['membership'] == report['admission_after']['membership']
        assert before['memory_events'] == report['admission_after']['memory_events']
        report['passed'] = len(report['runs']) == len(report['plans']) and all(row['passed'] for row in report['runs'])
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        path.write_text(json.dumps(report, indent=2))
        os.close(lock)
    print(json.dumps({'passed': report['passed'], 'runs': len(report['runs']), 'report': str(path)}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
