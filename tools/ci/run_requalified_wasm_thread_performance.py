#!/usr/bin/env python3
"""Qualify and measure freshly linked real-VM thread fixtures in the Linux cgroup."""

import argparse
from collections import defaultdict
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import statistics
import subprocess
import time


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def memory_events():
    return {name: int(value) for name, value in
            (line.split() for line in Path('/sys/fs/cgroup/memory.events').read_text().splitlines())}


def summary(values):
    return {'median': statistics.median(values), 'minimum': min(values),
            'maximum': max(values),
            'mad': statistics.median(abs(value - statistics.median(values)) for value in values)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--measure', action='store_true')
    parser.add_argument('--samples', type=int, default=21)
    parser.add_argument('--rounds', type=int, default=1024)
    args = parser.parse_args()
    if not (1 <= args.samples <= 99 and 1 <= args.rounds <= 1024):
        parser.error('samples must be 1..99 and rounds 1..1024')
    source = args.source.resolve(strict=True)
    build = args.build.resolve(strict=True)
    subprocess.run(['bash', str(source/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    events_before = memory_events()
    args.out.mkdir(parents=True, exist_ok=False)
    out = args.out.resolve()
    record = json.loads((build/'build.json').read_text())
    expected = record['source_id']
    current = subprocess.check_output(['python3', str(source/'tools/ci/wasm3_source_fingerprint.py'),
                                       str(source), str(out/'source-before.json')], text=True).strip()
    if expected != current or record['source'] != str(source):
        raise RuntimeError('build source and current source differ')
    binaries = {name: build/name for name in ('qualify', 'timed')}
    for name, binary in binaries.items():
        if sha(binary) != record[name+'_sha256']:
            raise RuntimeError(name+' binary differs from recorded build')
    policies = ('instruction', 'unwind')
    executions = []
    for name in (('qualify', 'timed') if not args.measure else ('timed',)):
        for policy in policies:
            samples, rounds = (args.samples, args.rounds) if args.measure else (1, 2)
            prefix = out/(name+'-'+policy)
            command = ['taskset', '-c', '0', str(binaries[name]), policy, str(samples),
                       str(rounds), str(prefix)+'.wasm']
            start = time.monotonic()
            with Path(str(prefix)+'.log').open('w') as log:
                completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=300)
            elapsed = time.monotonic() - start
            log = Path(str(prefix)+'.log').read_text()
            if completed.returncode or 'PASS real VM thread entry/checksum/markers' not in log:
                raise RuntimeError(f'{name}/{policy}: fixture failed with exit {completed.returncode}')
            rows = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
            if len(rows) != 12*samples:
                raise RuntimeError(f'{name}/{policy}: expected {12*samples} rows, got {len(rows)}')
            groups = defaultdict(list)
            for row in rows:
                if row['rounds'] != rounds or row['checksum'] == 0:
                    raise RuntimeError('invalid guest thread sample')
                if any(not math.isfinite(v) or v < 0 for k, v in row.items() if k.endswith('_ns')):
                    raise RuntimeError('invalid timing sample')
                if name == 'qualify' and row['native_creations'] != (row['workers']*rounds if row['threaded'] else 0):
                    raise RuntimeError('qualifier native-thread count differs')
                groups[(row['path'], row['workers'], row['threaded'])].append(row)
            comparisons = []
            for path in ('std_native', 'vm_full_entry', 'vm_raw_entry'):
                for workers in (1, 4):
                    direct = sorted(groups[(path, workers, False)], key=lambda row: row['sample'])
                    threaded = sorted(groups[(path, workers, True)], key=lambda row: row['sample'])
                    if len(direct) != samples or len(threaded) != samples or [v['checksum'] for v in direct] != [v['checksum'] for v in threaded]:
                        raise RuntimeError('unpaired or divergent thread samples')
                    comparisons.append({'path': path, 'workers': workers,
                                        'threaded_over_direct_wall_ratio': summary([t['wall_ns']/d['wall_ns'] for d, t in zip(direct, threaded)]),
                                        'threaded_minus_direct_wall_ns': summary([t['wall_ns']-d['wall_ns'] for d, t in zip(direct, threaded)])})
            save(str(prefix)+'.json', {'command': command, 'elapsed_seconds': elapsed,
                                      'rows': rows, 'comparisons': comparisons})
            executions.append({'binary': name, 'policy': policy, 'elapsed_seconds': elapsed,
                               'binary_sha256': record[name+'_sha256']})
            print('PASS', name, policy, 'samples', samples, flush=True)
    after = subprocess.check_output(['python3', str(source/'tools/ci/wasm3_source_fingerprint.py'),
                                     str(source), str(out/'source-after.json')], text=True).strip()
    if after != expected or any(sha(path) != record[name+'_sha256'] for name, path in binaries.items()):
        raise RuntimeError('source or fixture changed during execution')
    subprocess.run(['bash', str(source/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    events_after = memory_events()
    if any(events_after[name] != events_before[name] for name in ('oom', 'oom_kill')):
        raise RuntimeError('cgroup OOM status changed during thread qualification')
    save(out/'summary.json', {'passed': True, 'source_id': expected, 'build': str(build),
                             'measure': args.measure, 'samples': args.samples if args.measure else 1,
                             'rounds': args.rounds if args.measure else 2,
                             'cpu_affinity': sorted(os.sched_getaffinity(0)),
                             'cgroup': {key: Path('/sys/fs/cgroup', key).read_text().strip()
                                        for key in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')},
                             'memory_events_before': events_before,
                             'memory_events_after': events_after,
                             'executions': executions})


if __name__ == '__main__':
    main()
