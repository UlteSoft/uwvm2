#!/usr/bin/env python3
"""Remeasure frozen, already qualified VM thread binaries without compiling.

Run only in an exclusive remote benchmark window. Inputs are SHA-checked against
an existing run directory, and the new output directory must not exist. This
measures the recorded runtime revision, not later exception implementations.
"""
import argparse
import collections
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import shlex
import statistics
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--previous-root', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--guard', type=Path, required=True)
parser.add_argument('--product', action='append', choices=['uwvm2', 'uwvm2-ros'],
                    help='Measure only the named product; default is both')
parser.add_argument('--samples', type=int, default=21)
parser.add_argument('--rounds', type=int, default=1024)
a = parser.parse_args()
assert 1 <= a.samples <= 99 and 1 <= a.rounds <= 1024
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
subprocess.run(['bash', str(a.guard)], check=True)
a.out.mkdir(parents=True, exist_ok=False)
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()

def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')

def snapshot():
    cgroup = Path('/sys/fs/cgroup')
    keys = ['memory.max', 'memory.swap.max', 'memory.current', 'memory.peak',
            'memory.events', 'cpu.stat', 'cpuset.cpus.effective']
    result = {'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'affinity': sorted(os.sched_getaffinity(0)),
              'cgroup': {key: (cgroup/key).read_text().strip() for key in keys},
              'proc_stat': Path('/proc/stat').read_text(),
              'loadavg': Path('/proc/loadavg').read_text().strip(),
              'processes': []}
    for entry in sorted(Path('/proc').iterdir()):
        if not entry.name.isdigit():
            continue
        try:
            status = (entry/'status').read_text()
            result['processes'].append({'pid': int(entry.name),
                'comm': (entry/'comm').read_text().strip(),
                'status': '\n'.join(line for line in status.splitlines()
                    if line.startswith(('State:', 'Threads:', 'Cpus_allowed_list:'))),
                'cmdline': (entry/'cmdline').read_bytes().replace(b'\0', b' ').decode(errors='replace')})
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            pass
    for name in ['scaling_governor', 'scaling_cur_freq', 'cpuinfo_max_freq']:
        p = Path('/sys/devices/system/cpu/cpu0/cpufreq')/name
        if p.exists():
            result[name] = p.read_text().strip()
    return result

def spread(values):
    center = statistics.median(values)
    quartiles = statistics.quantiles(values, n=4, method='inclusive') if len(values) > 1 else [center]*3
    return {'median': center, 'minimum': min(values), 'maximum': max(values),
            'mad': statistics.median(abs(v-center) for v in values),
            'iqr': quartiles[2]-quartiles[0],
            'mean': statistics.mean(values),
            'cv': statistics.stdev(values)/statistics.mean(values) if len(values) > 1 and statistics.mean(values) else 0}

verified = {}
products = a.product or ['uwvm2', 'uwvm2-ros']
assert len(products) == len(set(products)), 'duplicate product'
for repo in products:
    previous = a.previous_root/repo
    if len(products) == 1 and not previous.is_dir():
        previous = a.previous_root
    inputs = json.loads((previous/'input-sha256.json').read_text())
    for path, expected in inputs.items():
        assert sha(path) == expected, ('input changed', path)
    binary = previous/'timed'
    expected = json.loads((previous/'build-binaries.json').read_text())['timed']
    assert sha(binary) == expected, ('binary changed', binary)
    verified[repo] = {'binary': str(binary), 'binary_sha256': expected, 'inputs': inputs}
save(a.out/'verified-inputs.json', verified)
save(a.out/'wrapper.json', {'path': str(Path(__file__).resolve()), 'sha256': sha(__file__),
    'samples': a.samples, 'rounds': a.rounds, 'cpu': 0,
    'products': products,
    'scope': 'real VM thread lifecycle and memory work; matched same-thread VM baseline; exact runtime revision recorded in verified inputs'})
save(a.out/'before-resources.json', snapshot())
commands = []
for repo, proof in verified.items():
    target = a.out/repo
    target.mkdir()
    for policy in ['instruction', 'unwind']:
        prefix = target/policy
        args = ['taskset', '-c', '0', proof['binary'], policy,
                str(a.samples), str(a.rounds), str(prefix)+'.wasm']
        Path(str(prefix)+'.command').write_text(shlex.join(args)+'\n')
        save(Path(str(prefix)+'-before.json'), snapshot())
        begin = time.monotonic()
        with Path(str(prefix)+'.log').open('w') as output:
            result = subprocess.run(args, stdout=output, stderr=subprocess.STDOUT, timeout=300)
        elapsed = time.monotonic()-begin
        save(Path(str(prefix)+'-after.json'), snapshot())
        commands.append({'repo': repo, 'policy': policy, 'args': args,
                         'exit': result.returncode, 'elapsed_seconds': elapsed})
        save(a.out/'commands.json', commands)
        assert result.returncode == 0, commands[-1]
        text = Path(str(prefix)+'.log').read_text()
        assert 'PASS real VM thread entry/checksum/markers' in text
        rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
        assert len(rows) == 12*a.samples
        groups = collections.defaultdict(list)
        for row in rows:
            assert row['rounds'] == a.rounds and row['checksum'] != 0
            assert all(math.isfinite(row[key]) and row[key] >= 0 for key in row if key.endswith('_ns'))
            groups[(row['path'], row['workers'], row['threaded'])].append(row)
        comparisons = []
        variation = []
        for key, values in sorted(groups.items()):
            assert sorted(v['sample'] for v in values) == list(range(a.samples))
            variation.append({'path': key[0], 'workers': key[1], 'threaded': key[2],
                'wall_ns': spread([v['wall_ns'] for v in values]),
                'guest_interval_ns': spread([v['guest_interval_ns'] for v in values]),
                'sample_wall_median_ms': statistics.median(v['wall_ns'] for v in values)*a.rounds/1e6})
        for path in ['std_native', 'vm_full_entry', 'vm_raw_entry']:
            for workers in [1, 4]:
                direct = sorted(groups[(path, workers, False)], key=lambda x: x['sample'])
                threaded = sorted(groups[(path, workers, True)], key=lambda x: x['sample'])
                assert [v['checksum'] for v in direct] == [v['checksum'] for v in threaded]
                comparisons.append({'path': path, 'workers': workers,
                    'paired_wall_ratio': spread([t['wall_ns']/d['wall_ns'] for d,t in zip(direct,threaded)]),
                    'paired_wall_delta_ns': spread([t['wall_ns']-d['wall_ns'] for d,t in zip(direct,threaded)])})
        save(Path(str(prefix)+'.json'), {'rows': rows, 'variation': variation, 'comparisons': comparisons})
        print('PASS', repo, policy, 'seconds', round(elapsed, 3), flush=True)
for repo, proof in verified.items():
    assert sha(proof['binary']) == proof['binary_sha256']
    for path, expected in proof['inputs'].items():
        assert sha(path) == expected, ('input changed during measurement', path)
save(a.out/'after-resources.json', snapshot())
subprocess.run(['bash', str(a.guard)], check=True)
save(a.out/'summary.json', {'passed': True, 'samples': a.samples, 'rounds': a.rounds,
    'products': products,
    'commands': commands, 'scope': 'real VM thread lifecycle on fixed CPU0; runtime revision in verified inputs is authoritative; no historical regression or new EH claim'})
