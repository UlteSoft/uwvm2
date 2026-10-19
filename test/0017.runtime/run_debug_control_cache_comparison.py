#!/usr/bin/env python3
"""Real Linux cgroup PTYs: control/cache kernels and independent scalar oracles.

Reuse the immutable non-tiny make_debug_workload_matrix inputs. No compilation
or profiling competes with timings. Different engines keep ordinary baselines.
"""
import argparse
from array import array
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys

NAMES = ['branches', 'dispatch', 'scan32k', 'scan256k', 'scan8m', 'chase8m']


def oracle(name):
    mask = (1 << 32) - 1
    if name in ['branches', 'dispatch']:
        x, total = 7, 0
        for _ in range(300000):
            x = (x * 1664525 + 1013904223) & mask
            if name == 'branches':
                total = total + (((x << 3) | (x >> 29)) & mask) if x & 4096 else total ^ (x >> 7)
                total = (total + (71 if x & 65536 else -19)) & mask
            else:
                case = (x >> 17) & 3
                total = [total + x, total ^ x, total - x, total + (x >> 3)][case] & mask
        return total
    words = {'scan32k': 8192, 'scan256k': 65536, 'scan8m': 2097152, 'chase8m': 2097152}[name]
    x, total = 0, 0
    if name == 'chase8m':
        for _ in range(2000000):
            x = (x * 1664525 + 1013904223) & (words - 1)
            total = (total + x) & mask
        return total
    memory = array('I', ((i * 1664525 + 1013904223) & (words - 1) for i in range(words)))
    assert memory.itemsize == 4
    for _ in range(2000000):
        value = memory[x]
        total = (total + value) & mask
        memory[x] = value ^ total
        x = (x + 1) & (words - 1)
    return total


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['uwvm', 'baseline', 'wasmtime', 'lldb', 'fixtures', 'out']:
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--cpu', type=int, required=True)
    p.add_argument('--ros', action='store_true')
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    assert os.sched_getaffinity(0) == {a.cpu}
    a.out.mkdir(parents=True, exist_ok=False)
    digest = lambda f: hashlib.sha256(f.read_bytes()).hexdigest()
    paths = [a.uwvm, a.baseline, a.wasmtime, a.lldb]
    manifest = json.loads((a.fixtures / 'manifest.json').read_text())
    inputs, expected = [], {}
    for name in NAMES:
        row = next(v for v in manifest if v['name'] == name)
        assert not row['tiny']
        wasm = a.fixtures / (name + '.wasm')
        assert digest(wasm) == row['wasm_sha256']
        expected[name] = oracle(name)
        files = [a.fixtures / (name + suffix) for suffix in ['.wat', '.wasm', '.c', '-native']]
        inputs.append({**row, 'expected_checksum': expected[name], 'file_sha256': {str(f): digest(f) for f in files}})
        paths += files
    before = {str(f): digest(f) for f in paths}
    d = {'passed': False, 'harness_sha256': digest(Path(__file__)), 'inputs': inputs, 'oracle_basis': 'Unsigned 32-bit scalar arithmetic; scan contents updated on every read/write; dependent chase uses exact LCG successor. Computed independently before engine execution.',
         'samples': [], 'immutable_before': before, 'sampling_used': False,
         'cgroup': Path('/proc/self/cgroup').read_text().strip(), 'affinity': [a.cpu]}
    def save():
        (a.out / 'result.json').write_text(json.dumps(d, indent=2) + '\n')
    save()
    tests = Path(__file__).resolve().parent
    argv = [sys.executable, str(tests / 'run_debug_workload_matrix.py'), '--uwvm', str(a.uwvm),
            '--baseline', str(a.baseline), '--baseline-codegen', 'less', '--baseline-pipeline', 'debug-local-ssa',
            '--fixtures', str(a.fixtures), '--out', str(a.out / 'uwvm'), '--repeats', '3',
            '--case', 'none', '--case', 'cold', '--case', 'disabled', *(['--ros'] if a.ros else ['--native'])]
    for name in NAMES:
        argv += ['--only', name]
    subprocess.run(argv, check=True, timeout=2400)
    matrix = json.loads((a.out / 'uwvm/result.json').read_text())
    assert matrix['passed']
    for v in matrix['samples']:
        assert int(v['checksum']) == expected[v['name']], v
        d['samples'].append({'engine': 'native' if v['version'] == 'native' else ('uwvm2-ros' if a.ros else 'uwvm2'), **v})
    assert len(d['samples']) == (144 if a.ros else 180)
    save()
    if not a.ros:
        argv = [sys.executable, str(tests / 'run_debug_wasmtime_comparison.py'), '--wasmtime', str(a.wasmtime),
                '--lldb', str(a.lldb), '--fixtures', str(a.fixtures), '--out', str(a.out / 'wasmtime'),
                '--cpu', str(a.cpu), '--repeats', '3', '--mode', 'ordinary', '--mode', 'gdb-o2', '--mode', 'guest-lldb']
        for name in NAMES:
            argv += ['--only', name]
        subprocess.run(argv, check=True, timeout=2400)
        matrix = json.loads((a.out / 'wasmtime/result.json').read_text())
        assert matrix['passed']
        for v in matrix['samples']:
            assert int(v['checksum']) == expected[v['name']], v
            d['samples'].append({'engine': 'wasmtime', 'version': 'current', **v})
        assert len(d['samples']) == 234
    groups = {}
    for v in d['samples']:
        key = '/'.join([v['name'], v['engine'], v.get('version', 'current'), v['mode']])
        groups.setdefault(key, []).append(v['kernel_ns'])
    d['ranges_ns'] = {k: {'count': len(v), 'median': statistics.median(v), 'min': min(v), 'max': max(v), 'all_ns': v}
                      for k, v in groups.items()}
    d['immutable_after'] = {str(f): digest(f) for f in paths}
    assert d['immutable_before'] == d['immutable_after']
    d['passed'] = True
    save()
    print('PASS independent control/cache checksums and real debugger timings:', len(d['samples']), flush=True)


if __name__ == '__main__':
    main()
