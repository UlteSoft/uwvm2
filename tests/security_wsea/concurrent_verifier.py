#!/usr/bin/env python3
"""Run eight concurrent production-verifier callers over all signing profiles.

This is verifier concurrency coverage; it does not claim multithreaded guest
execution or replace the per-product guest-executing QEMU qualification.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures', 'runner', 'matrix-driver', 'runtime-receipt', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--target', choices=['x86_64', 'aarch64'], required=True)
    parser.add_argument('--exec-prefix-json', required=True)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('authorized SSH Linux only')
    spec = importlib.util.spec_from_file_location('concurrent_wsea_matrix', args.matrix_driver)
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    before = matrix.cgroup_snapshot(True)
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    manifest = json.loads((args.fixtures / 'fixtures.json').read_text())
    cases = [case for case in manifest['cases'] if case['expected'] == 'ALLOW' and
             ('authenticated fd_write guest entry witness' in case['name'] or 'genuine ' in case['name'])]
    if len(cases) != 17 or not all('RSA8192' in case['name'] for case in cases[-2:]):
        raise RuntimeError('all thirteen profiles and four genuine RSA boundaries required')
    requests = []
    for index, case in enumerate(cases):
        directory = args.output / str(index)
        directory.mkdir(mode=0o700)
        state = directory / 'state'
        state.mkdir(mode=0o700)
        (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
        policy = directory / 'policy'
        policy.write_text(case['policy'].replace('@STATE_DIR@', str(state)))
        core, cose, certificates, crls = matrix.embedded_parts(Path(case['module']).read_bytes())
        forged = directory / 'forged.wasm'
        forged.write_bytes(matrix.pack_embedded(core, cose[:-1] + bytes([cose[-1] ^ 1]), certificates, crls))
        requests.append(str(policy) + '\t' + case['module'] + '\t' + str(forged) + '\n')
    request_path = args.output / 'requests.tsv'
    request_path.write_text(''.join(requests))
    identity = matrix.elf_identity(args.runner)
    matrix.target_elf(identity, args.target)
    runtime = json.loads(args.runtime_receipt.read_text())
    if runtime['target'] != args.target or any(matrix.sha(item['path']) != item['sha256'] for item in runtime['runtime_files']):
        raise RuntimeError('changed qualified runtime')
    prefix = json.loads(args.exec_prefix_json)
    if not isinstance(prefix, list) or not prefix:
        raise RuntimeError('explicit actual QEMU/loader prefix required')
    argv = [*prefix, str(args.runner), str(request_path)]
    result = subprocess.run(argv, capture_output=True, text=True, timeout=300)
    (args.output / 'runner.stdout').write_text(result.stdout)
    (args.output / 'runner.stderr').write_text(result.stderr)
    report = {'status': 'FAIL', 'scope': __doc__, 'target': args.target, 'execution': 'qemu', 'argv': argv,
              'source_sha256': matrix.sha(__file__), 'runner_sha256': matrix.sha(args.runner),
              'fixtures_sha256': matrix.sha(args.fixtures / 'fixtures.json'), 'elf': identity,
              'runtime_receipt': str(args.runtime_receipt), 'runtime_receipt_sha256': matrix.sha(args.runtime_receipt),
              'returncode': result.returncode, 'stderr': result.stderr, 'records': [], 'cgroup_before': before}
    try:
        data = json.loads(result.stdout)
        report.update(threads=data['threads'], rounds=data['rounds'], cases=data['cases'], records=data['records'], controls=data['controls'])
        if (result.returncode == 0 and data['status'] == 'PASS' and data['threads'] == 8 and
                data['rounds'] == 4 and data['cases'] == 17 and len(data['records']) == 552 and
                all(item['passed'] for item in data['records']) and len(data['controls']) == 4 and
                all(item['passed'] for item in data['controls'])):
            report['status'] = 'PASS'
    finally:
        report['cgroup_after'] = matrix.cgroup_snapshot(True)
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'records': len(report['records'])}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
