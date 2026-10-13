#!/usr/bin/env python3
"""Interleaved HEAD/current memory benchmarks on one permitted physical P-core.

This checks sustained execution, not startup/validation latency. The independent
binary verifies every timed result. Confidence intervals are paired bootstrap
intervals; a noisy or regressed comparison fails instead of being called a pass.
Cross-platform correctness and assembly checks are separate from native timing.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import random
import resource
import statistics
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('fixtures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--cpu', type=int, default=0)
    # An odd count with count % 4 == 3 gives a nonzero independent checksum;
    # accidentally skipping the loop cannot pass by returning an initial zero.
    parser.add_argument('--iterations', type=int, default=2000003)
    parser.add_argument('--rounds', type=int, default=11)
    parser.add_argument('--limit', type=float, default=1.05)
    parser.add_argument('--jit-call-stack', choices=('instruction', 'unwind'), default='unwind')
    parser.add_argument('--metric', choices=('wall', 'thread-cpu'), default='wall',
                        help='wall remains the default; thread-cpu diagnoses descheduling separately')
    parser.add_argument('--require-independent-loads', action='store_true')
    parser.add_argument('--kernels', nargs='+', choices=('scalar-aligned', 'scalar-unaligned', 'simd-aligned',
                        'load-scalar-aligned', 'load-scalar-unaligned', 'load-simd-aligned'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError('requested CPU is outside the sandbox allocation')
    os.sched_setaffinity(0, {args.cpu})
    os.environ['UWVM_TEST_JIT_CALL_STACK'] = args.jit_call_stack
    args.output.mkdir(parents=True, exist_ok=False)
    rng = random.Random(0x5741534d)
    kernels = ['scalar-aligned', 'scalar-unaligned', 'simd-aligned']
    if args.require_independent_loads:
        kernels += ['load-' + kernel for kernel in kernels.copy()]
    if args.kernels:
        kernels = list(dict.fromkeys(args.kernels))
        if args.require_independent_loads and not all('load-' + name in kernels for name in
                ('scalar-aligned', 'scalar-unaligned', 'simd-aligned')):
            parser.error('--require-independent-loads requires all three independent-load kernels')
    records = []
    comparisons = []
    for configuration in sorted(args.build.iterdir()):
        if not configuration.is_dir() or not any((configuration / name).exists() for name in ('baseline', 'current')):
            continue
        if not all((configuration / name).is_file() for name in ('baseline', 'current')):
            raise RuntimeError('incomplete baseline/current build: ' + str(configuration))
        for kernel in kernels:
            variants = [('baseline', 'legacy'), ('current', 'legacy'), ('current', 'indexed')]
            timings = {variant: [] for variant in variants}
            for round_index in range(args.rounds):
                order = variants.copy()
                rng.shuffle(order)
                for version, syntax in order:
                    binary = configuration / version
                    fixture = args.fixtures / (kernel + '-' + syntax + '.wasm')
                    command = [str(binary.resolve()), str(fixture.resolve()), str(args.iterations), '1']
                    # These diagnostics cover the complete child (including JIT
                    # startup), never substitute for the guest-only timed sample.
                    usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
                    run = subprocess.run(command, capture_output=True, text=True, timeout=120)
                    usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
                    if run.returncode:
                        raise RuntimeError(f'{command}: {run.returncode}\n{run.stdout}\n{run.stderr}')
                    measured = json.loads(run.stdout)
                    elapsed = measured['nanoseconds' if args.metric == 'wall' else 'thread_cpu_nanoseconds'][0]
                    timings[version, syntax].append(elapsed)
                    records.append(dict(configuration=configuration.name, kernel=kernel, version=version,
                                        syntax=syntax, round=round_index,
                                        process_diagnostics={field: getattr(usage_after, field) - getattr(usage_before, field)
                                                             for field in ('ru_utime', 'ru_stime', 'ru_minflt', 'ru_majflt',
                                                                           'ru_nvcsw', 'ru_nivcsw')}, **measured))
            for variant, reference in [(('current', 'legacy'), ('baseline', 'legacy')),
                                       (('current', 'indexed'), ('current', 'legacy'))]:
                paired = [a / b for a, b in zip(timings[variant], timings[reference])]
                log_ratios = list(map(math.log, paired))
                estimates = sorted(math.exp(statistics.mean(rng.choices(log_ratios, k=len(log_ratios))))
                                   for _ in range(5000))
                upper = estimates[4874]
                result = dict(metric=args.metric, configuration=configuration.name, kernel=kernel, candidate=variant, reference=reference,
                              ratio=math.exp(statistics.mean(log_ratios)), interval95=[estimates[124], upper],
                              permitted_ratio=args.limit, within_limit=upper <= args.limit,
                              median_ns={':'.join(k): statistics.median(v) for k, v in timings.items()})
                comparisons.append(result)
                print(json.dumps(result), flush=True)
            (args.output / 'samples.json').write_text(json.dumps(records, indent=2) + '\n')
    metadata = dict(metric=args.metric, cpu=args.cpu, allowed_cpus=Path('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip(),
                    memory_max=Path('/sys/fs/cgroup/memory.max').read_text().strip(),
                    iterations=args.iterations, rounds=args.rounds, kernels=kernels, independent_loads=args.require_independent_loads, jit_call_stack=args.jit_call_stack,
                    binaries={str(p): digest(p) for p in args.build.glob('*/*')
                              if p.name in ('baseline', 'current')},
                    fixtures={p.name: digest(p) for p in args.fixtures.glob('*.wasm')})
    (args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (args.output / 'comparisons.json').write_text(json.dumps(comparisons, indent=2) + '\n')
    if not comparisons or not all(item['within_limit'] for item in comparisons):
        raise SystemExit('FAIL/INCONCLUSIVE: investigate ratios and assembly; do not claim no regression')
    print('PASS paired native memory timing within the recorded confidence bound')


if __name__ == '__main__':
    main()
