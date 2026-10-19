#!/usr/bin/env python3
"""Replay all real positive/adversarial vectors through one verifier process.

This exercises public-key mathematical memoization across changing signatures,
certificate encodings, policies and revocation state. Authorization is fresh.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--runner', type=Path, required=True)
    parser.add_argument('--exec-prefix-json', default='[]')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('authorized Linux only')
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    manifest = json.loads((args.fixtures / 'fixtures.json').read_text())
    for name, digest in manifest['files_sha256'].items():
        if sha(args.fixtures / name) != digest:
            raise RuntimeError('frozen fixture changed: ' + name)
    groups = {}
    requests = []
    for index, case in enumerate(manifest['cases']):
        if case['group'] not in groups:
            directory = args.output / ('state-' + str(len(groups)))
            directory.mkdir(mode=0o700)
            (directory / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            groups[case['group']] = directory
        policy = args.output / ('case-' + str(index) + '.policy')
        policy.write_text(case['policy'].replace('@STATE_DIR@', str(groups[case['group']])))
        # Different protected policy files can share a state directory. Their
        # fingerprint covers exact policy/root bytes, not the configuration path.
        requests.append(str(policy) + '\t' + case['module'] + '\tmatrix.app\n')
    prefix = json.loads(args.exec_prefix_json)
    argv = [*prefix, str(args.runner), '--batch']
    process = subprocess.run(argv, input=''.join(requests), capture_output=True, text=True, timeout=600)
    responses = process.stdout.splitlines()
    report = {'status': 'FAIL', 'scope': __doc__, 'driver_sha256': sha(__file__),
        'runner_sha256': sha(args.runner), 'fixtures_sha256': sha(args.fixtures / 'fixtures.json'),
        'argv': argv, 'process_returncode': process.returncode, 'stderr': process.stderr, 'records': []}
    for index, case in enumerate(manifest['cases']):
        actual = responses[index] if index < len(responses) else ''
        passed = actual == '0\tALLOW' if case['expected'] == 'ALLOW' else (
            actual.startswith('2\tDENY ') and (not case['codes'] or any(code in actual for code in case['codes'])))
        report['records'].append({'name': case['name'], 'expected': case['expected'], 'response': actual,
            'logical_case_exit_code': 0 if case['expected'] == 'ALLOW' else 2, 'passed': passed})
    if process.returncode == 0 and len(responses) == len(requests) and all(x['passed'] for x in report['records']):
        report['status'] = 'PASS'
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'cases': len(report['records'])}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
