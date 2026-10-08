#!/usr/bin/env python3
"""Measure thread-management substrates with paired native baselines inside remote cgroup limits."""
import argparse
import csv
import hashlib
import io
import json
import os
import platform
import re
import resource
import shlex
import statistics
import subprocess
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def environment():
    cgroup = Path('/sys/fs/cgroup')
    names = ['memory.max', 'memory.swap.max', 'cpuset.cpus.effective', 'cpu.stat',
             'cpu.pressure', 'memory.current', 'pids.current']
    return dict(platform=platform.platform(), affinity=sorted(os.sched_getaffinity(0)),
                cgroup={name: (cgroup / name).read_text().strip() for name in names if (cgroup / name).exists()})


def run(command, log):
    with log.open('w') as stream:
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=120)


def describe(values):
    return dict(min=min(values), median=statistics.median(values), max=max(values), samples=values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--base-source-root', type=Path, help='Complete source tree beneath a small frozen test overlay')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    base = args.base_source_root or root
    guard = ['bash', str(base / 'tools/ci/require_wasm3_test_cgroup.sh')]
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    subprocess.run(guard, check=True)
    args.out.mkdir(parents=True, exist_ok=False)
    out = args.out.resolve()
    source = root / 'test/0003.utils/thread/creation_performance.cc'
    inputs = [source, Path(__file__).resolve(), *sorted((root / 'src/uwvm2/utils/thread').glob('*'))]
    inputs = [path for path in inputs if path.is_file()]
    before = {str(path.relative_to(root)): sha(path) for path in inputs}
    compiler = os.environ.get('CXX', 'clang++')
    command = [compiler, '-std=c++26', '-stdlib=libc++', '-fexceptions', '-fno-rtti', '-O3', '-g0',
               '-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind', '-DUWVM=2',
               '-DUWVM_USE_UWVM_INT', '-DUWVM_DISABLE_JIT', '-DUWVM_USE_THREAD_LOCAL']
    for path in [root / 'src', base / 'src', base / 'third-parties/fast_io/include',
                 base / 'third-parties/bizwen/include', base / 'third-parties/boost_unordered/include']:
        command += ['-I', str(path)]
    commands = [command + [str(source), '-o', str(out / 'fixture'), '-pthread'],
                command + ['-DUWVM_THREAD_BENCH_QUALIFY_CREATION', str(source), '-o', str(out / 'qualification'),
                           '-pthread', '-Wl,--wrap=pthread_create']]
    (out / 'build.commands').write_text('\n'.join(shlex.join(item) for item in commands) + '\n')
    for index, item in enumerate(commands):
        run(item, out / f'build-{index}.log')
    run([str(out / 'qualification'), '--qualify'], out / 'qualification.log')
    run([compiler, '--version'], out / 'compiler.log')

    objdump = os.environ.get('LLVM_OBJDUMP', str(Path(compiler).with_name('llvm-objdump')))
    run([objdump, '--disassemble-symbols=uwvm_thread_bench_memory_kernel', str(out / 'fixture')], out / 'kernel.asm')
    assembly = (out / 'kernel.asm').read_text()
    assert '<uwvm_thread_bench_memory_kernel>:' in assembly
    # The current remote execution target is x86-64; this checks its exact shared
    # kernel, not code generation for the Wasm engine or for other architectures.
    assert platform.machine() == 'x86_64'
    assert not re.search(r'\b(?:callq?|lock|mfence|lfence|sfence)\b', assembly), assembly

    environment_before = environment()
    with (out / 'samples.csv').open('w') as output, (out / 'run.log').open('w') as errors:
        subprocess.run([str(out / 'fixture')], stdout=output, stderr=errors, check=True, timeout=120)
    environment_after = environment()
    assert 'PASS thread management paired workload checks' in (out / 'run.log').read_text()
    rows = list(csv.DictReader(io.StringIO((out / 'samples.csv').read_text())))
    assert len(rows) == 72
    groups = {}
    for row in rows:
        key = f"{row['profile']}/{row['workers']}"
        groups.setdefault(key, {}).setdefault(int(row['sample']), {})[row['variant']] = row
    profiles = {}
    for key, pairs in groups.items():
        assert sorted(pairs) == list(range(9))
        assert all(set(pair) == {'reference', 'managed'} for pair in pairs.values())
        assert all(pair['reference']['checksum'] == pair['managed']['checksum'] for pair in pairs.values())
        metrics = {}
        for field in ['wall_ns_per_round', 'body_ns_per_worker']:
            reference = [float(pairs[index]['reference'][field]) for index in range(9)]
            managed = [float(pairs[index]['managed'][field]) for index in range(9)]
            metrics[field] = dict(reference=describe(reference), managed=describe(managed),
                                  paired_ratio=describe([b / a for a, b in zip(reference, managed)]) if min(reference) > 0 else None)
        profiles[key] = metrics
    assert before == {str(path.relative_to(root)): sha(path) for path in inputs}
    subprocess.run(guard, check=True)
    result = dict(passed=True, scope='native thread-management substrate; not Wasm memory throughput or complete VM host entry',
                  workload=dict(samples=9, paired_order='AB/BA alternating', rounds_per_sample=32, warmup_pairs=2,
                                batch_tasks=64, domain_private_buffer_bytes=8192, domain_memory_passes=64),
                  caveat='Paired samples measure this execution only. Shared cgroup load, scheduling and clock changes remain visible in raw samples; no pass/fail performance threshold or no-regression claim.',
                  source=before, base=str(base), binary_sha256=sha(out / 'fixture'),
                  qualification='separate unmeasured binary verified successful direct native pthread_create calls; timed binary has no wrapper',
                  kernel_assembly='one common out-of-line memory body; no call, lock or explicit memory fence in x86-64 assembly',
                  environment_before=environment_before, environment_after=environment_after, profiles=profiles)
    (out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    for key, profile in profiles.items():
        wall = profile['wall_ns_per_round']
        body = profile['body_ns_per_worker']
        print(f"{key}: create/run/join median reference={wall['reference']['median']:.1f} ns managed={wall['managed']['median']:.1f} ns paired-ratio={wall['paired_ratio']['median']:.4f}")
        if body['paired_ratio']:
            print(f"  shared kernel per-worker median reference={body['reference']['median']:.1f} ns managed={body['managed']['median']:.1f} ns paired-ratio={body['paired_ratio']['median']:.4f}")
    print('PASS paired thread management fixture, workload equality, native creation qualification and kernel assembly')


if __name__ == '__main__':
    main()
