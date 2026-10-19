#!/usr/bin/env python3
"""Measure actual native VM startup for all profiles and large RSA boundaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--vm', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=7)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux' or not 3 <= args.samples <= 31:
        raise RuntimeError('authorized native Linux and bounded sample count required')
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    os.umask(0o077)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    cases = [case for case in fixtures['cases'] if case['expected'] == 'ALLOW' and
             ('authenticated fd_write guest entry witness' in case['name'] or 'genuine ' in case['name'])]
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'source_sha256': sha(__file__),
              'vm_sha256': sha(args.vm), 'fixtures_sha256': sha(args.fixtures / 'fixtures.json'),
              'affinity': sorted(os.sched_getaffinity(0)), 'ambient_load': os.getloadavg(),
              'filesystem': 'private tmpfs; measures process and verification CPU, not durable disk latency',
              'warmup_runs': 2, 'samples_per_mode': args.samples, 'rows': []}
    try:
        for index, case in enumerate(cases):
            directory = args.output / str(index)
            directory.mkdir(mode=0o700)
            state = directory / 'state'
            state.mkdir(mode=0o700)
            (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            policy = directory / 'policy'
            policy.write_text(case['policy'].replace('@STATE_DIR@', str(state)))
            samples = {'ordinary': [], 'secure': []}
            for iteration in range(args.samples + 2):
                for secure in (False, True):
                    command = [str(args.vm)]
                    if secure:
                        command += ['--secure', 'module', case['module'], str(policy)]
                    command += ['--run', case['module']]
                    start = time.perf_counter_ns()
                    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
                    elapsed = (time.perf_counter_ns() - start) / 1e6
                    if result.returncode != 0 or case['guest_marker'] not in result.stdout:
                        raise RuntimeError('actual guest startup failed: ' + case['name'] + ': ' + result.stderr)
                    if iteration >= 2:
                        samples['secure' if secure else 'ordinary'].append(elapsed)
            medians = {key: statistics.median(values) for key, values in samples.items()}
            report['rows'].append({'case': case['name'], 'algorithm': case['algorithm'],
                'samples_ms': samples, 'p50_ms': medians,
                'p50_added_ms': medians['secure'] - medians['ordinary']})
            print(json.dumps({key: value for key, value in report['rows'][-1].items() if key != 'samples_ms'}), flush=True)
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
