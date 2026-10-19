#!/usr/bin/env python3
"""Check C/C++ libc argv/environ termination against three VMs and Wasmtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def admission(expected):
    assert os.uname().sysname == 'Linux'
    membership = Path('/proc/self/cgroup').read_text()
    assert membership == '0::' + expected + '\n'
    group = Path('/sys/fs/cgroup') / expected.lstrip('/')
    assert (group / 'memory.max').read_text().strip() == str(64 << 30)
    assert (group / 'memory.swap.max').read_text().strip() == '0'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return {'membership': membership.strip(), 'memory_events': (group / 'memory.events').read_text()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--runner', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros', 'wasmtime'])
    args = parser.parse_args()
    before = admission(args.expected_cgroup)
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': before, 'runs': [], 'artifacts': {}, 'runners': {}}
    runners = [(repo, args.arena / 'products' / repo / 'uwvm') for repo in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']]
    runners.append(('wasmtime', args.wasmtime))
    if args.runner:
        runners = [(runner, binary) for runner, binary in runners if runner in args.runner]
    for runner, binary in runners:
        report['runners'][runner] = {'path': str(binary), 'sha256': digest(binary)}
    for language, filename in [('c', 'c-abi.wasm'), ('c++', 'cxx-abi.wasm')]:
        target = args.artifacts / filename
        report['artifacts'][language] = {'path': str(target), 'sha256': digest(target)}
        for tail in ([], ['世界'], ['世界', '', 'tail']):
            for count in (0, 1, 3):
                env = [('ABI_A', 'one'), ('ABI_EMPTY', ''), ('ABI_UTF8', '世界')][:count]
                expected = f'c-abi compat: PASS language={language} argc={1 + len(tail)} env={count}\n'
                for runner, binary in runners:
                    if runner == 'wasmtime':
                        options = [token for key, value in env for token in ['--env', key + '=' + value]]
                        command = [str(binary), 'run', *options, str(target), *tail]
                    else:
                        mode = ['-Rcm', 'full', '-Rcc', 'jit'] if runner == 'uwvm2' else ['-Raot']
                        options = ['--wasip1-global-noinherit-system-environment']
                        options += [token for key, value in env for token in ['--wasip1-global-add-or-replace-environment', key, value]]
                        command = [str(binary), *mode, '-Rct', '0', '-Rllvm-cache-path', 'disable', *options, '--run', str(target), *tail]
                    start = time.monotonic()
                    row = {'runner': runner, 'language': language, 'user_argument_count': len(tail),
                           'environment_count': count, 'argv': command, 'passed': False}
                    try:
                        p = subprocess.run(command, capture_output=True, text=True, timeout=30)
                        row.update({'exit': p.returncode, 'stdout': p.stdout, 'stderr': p.stderr,
                                    'seconds': time.monotonic() - start,
                                    'passed': p.returncode == 0 and p.stdout == expected})
                    except Exception as error:
                        row['error'] = repr(error)
                    report['runs'].append(row)
                    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    report['admission_after'] = admission(args.expected_cgroup)
    assert report['admission_before']['memory_events'] == report['admission_after']['memory_events']
    report['passed'] = bool(report['runs']) and all(row['passed'] for row in report['runs'])
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({'passed': report['passed'], 'cases': len(report['runs']),
                      'failures': [row for row in report['runs'] if not row['passed']]}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
