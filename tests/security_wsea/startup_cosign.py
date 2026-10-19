#!/usr/bin/env python3
"""Measure full native cosign process startup plus offline WSEA verification."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures', 'go-cli', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('matrix', Path(__file__).with_name('algorithm_matrix.py'))
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    identity = matrix.elf_identity(args.go_cli)
    if os.uname().machine != 'x86_64' or identity['machine'] != 62:
        raise RuntimeError('native x86_64 full cosign required; no QEMU prefix accepted')
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    for name, digest in fixtures['files_sha256'].items():
        if matrix.sha(args.fixtures / name) != digest:
            raise RuntimeError('frozen fixture changed: ' + name)
    cases = [case for case in fixtures['cases'] if case['expected'] == 'ALLOW' and
             ('authenticated fd_write guest entry witness' in case['name'] or 'genuine ' in case['name'])]
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'driver_sha256': matrix.sha(__file__),
              'binary_sha256': matrix.sha(args.go_cli), 'binary_elf': identity,
              'fixtures_sha256': matrix.sha(args.fixtures / 'fixtures.json'),
              'affinity': sorted(os.sched_getaffinity(0)), 'ambient_load': os.getloadavg(),
              'filesystem': 'private tmpfs; includes full CLI startup, no network or durable state writes',
              'warmup_runs': 2, 'samples_per_case': 7, 'rows': []}
    try:
        for index, case in enumerate(cases):
            directory = args.output / str(index)
            directory.mkdir(mode=0o700)
            state = directory / 'state'
            state.mkdir(mode=0o700)
            (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            policy = directory / 'policy'
            policy.write_text(case['policy'].replace('@STATE_DIR@', str(state)))
            command = [str(args.go_cli), 'wasm', 'secure', 'verify', case['module'], '--policy', str(policy)]
            samples = []
            for iteration in range(9):
                start = time.perf_counter_ns()
                result = subprocess.run(command, capture_output=True, text=True, timeout=30)
                elapsed = (time.perf_counter_ns() - start) / 1e6
                if result.returncode != 0 or json.loads(result.stdout).get('status') != 'VERIFIED':
                    raise RuntimeError('full CLI verification failed: ' + case['name'] + ': ' + result.stderr)
                if iteration >= 2:
                    samples.append(elapsed)
            row = {'name': case['name'], 'algorithm': case['algorithm'], 'argv': command,
                   'samples_ms': samples, 'p50_ms': statistics.median(samples)}
            report['rows'].append(row)
            print(case['name'], row['p50_ms'], flush=True)
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
