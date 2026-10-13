#!/usr/bin/env python3
"""Pair one unchanged Core 3 exception workload across two actual O3 CLIs.

This measures whole-process execution including startup and JIT compilation.
AB/BA order reduces drift; it does not isolate unwinder internals. Both sides
execute the same validated binary and check the guest's final state.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import resource
import statistics
import subprocess
import sys
import time


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--policy', choices=('instruction', 'unwind'), required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--iterations', type=int, default=50000)
    parser.add_argument('--pairs', type=int, default=9)
    parser.add_argument('--min-sample-seconds', type=float, default=0.1)
    parser.add_argument('--startup-multiple', type=float, default=20.0)
    args = parser.parse_args()
    if args.pairs < 9 or args.iterations < 1 or args.min_sample_seconds <= 0 or args.startup_multiple < 20:
        parser.error('require at least nine pairs, positive iterations/duration and 20x startup')
    subprocess.run(['bash', str(args.source_root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    sys.path.insert(0, str(args.source_root/'test/0014.llvm_jit'))
    from bench_exception_cross_cli import fixture

    args.out.mkdir(parents=True, exist_ok=False)
    (args.out/'runner.py').write_bytes(Path(__file__).read_bytes())
    hashes = {key: sha(value) for key, value in (
        ('before', args.before), ('after', args.after), ('wasm_tools', args.wasm_tools),
        ('wasmtime', args.wasmtime))}
    cgroup = {name: Path('/sys/fs/cgroup', name).read_text().strip()
              for name in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}
    if cgroup != {'memory.max': '68719476736', 'memory.swap.max': '0',
                  'cpuset.cpus.effective': '0,2,4,6,16-31'}:
        raise RuntimeError(f'unexpected test cgroup: {cgroup}')
    rows = []

    def execute(label: str, command: list[str], phase: str, count: int = 0) -> int:
        command = list(map(str, command))
        start = time.perf_counter_ns()
        result = subprocess.run(command, capture_output=True, timeout=240)
        elapsed = time.perf_counter_ns() - start
        (args.out/(label+'.log')).write_bytes(result.stdout+result.stderr)
        rows.append({'label': label, 'phase': phase, 'command': command,
                     'iterations': count, 'elapsed_ns': elapsed, 'exit': result.returncode})
        (args.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
        if result.returncode:
            raise RuntimeError(f'{label}: exit {result.returncode}: {(args.out/(label+".log")).read_text(errors="replace")[-3000:]}')
        return elapsed

    mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    def uwvm_command(binary: Path, wasm: Path) -> list[str]:
        return [str(binary), *mode, '-Rct', '0', '-Rllvm-full-policy', 'pb-o3',
                '-Rllvm-call-stack', args.policy, '-Rllvm-cache-path', 'disable',
                '-WFE-exceptions', '--run', str(wasm)]

    empty_wat = args.out/'empty.wat'
    empty_wat.write_text('(module (func (export "_start")))\n')
    empty_wasm = args.out/'empty.wasm'
    execute('empty-assemble', [args.wasm_tools, 'parse', empty_wat, '-o', empty_wasm], 'assemble')
    startup = [execute(f'empty-{which}-{index}', uwvm_command(binary, empty_wasm), 'startup')
               for index in range(3) for which, binary in (('before', args.before), ('after', args.after))]
    startup_ns = max(statistics.median(startup[0::2]), statistics.median(startup[1::2]))
    target_ns = max(args.min_sample_seconds * 1e9, args.startup_multiple * startup_ns)

    # The reference executes the same Core 3 syntax at a bounded count, then
    # each measured guest checks its exact count-dependent LCG endpoint itself.
    reference_wat = args.out/'reference.wat'
    reference_wat.write_text(fixture('cross-depth-8', min(args.iterations, 1000))+'\n')
    reference_wasm = args.out/'reference.wasm'
    execute('reference-assemble', [args.wasm_tools, 'parse', reference_wat, '-o', reference_wasm], 'assemble')
    execute('reference-validate', [args.wasm_tools, 'validate', reference_wasm], 'validate')
    execute('reference-wasmtime', [args.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', reference_wasm], 'reference')

    count = args.iterations
    for attempt in range(5):
        wat = args.out/f'cross-depth-8-{count}.wat'
        wat.write_text(fixture('cross-depth-8', count)+'\n')
        wasm = wat.with_suffix('.wasm')
        execute(f'assemble-{attempt}', [args.wasm_tools, 'parse', wat, '-o', wasm], 'assemble')
        execute(f'validate-{attempt}', [args.wasm_tools, 'validate', wasm], 'validate')
        commands = [uwvm_command(args.before, wasm), uwvm_command(args.after, wasm)]
        times = [execute(f'calibration-{attempt}-{side}', command, 'calibration', count)
                 for side, command in zip(('before', 'after'), commands)]
        if min(times) >= target_ns * 1.2:
            break
        count = math.ceil(count * max(2, target_ns * 1.3 / min(times)))
        if count > 0x7fffffffffffffff:
            raise RuntimeError('calibration exceeded signed i64 limit')
    else:
        raise RuntimeError('failed to amortize startup within five calibrations')

    ratios, samples = [], []
    for pair in range(args.pairs):
        measured = {}
        for index in ((0, 1) if pair % 2 == 0 else (1, 0)):
            side = ('before', 'after')[index]
            measured[index] = execute(f'pair-{pair:02d}-{side}', commands[index], 'sample', count)
        samples.extend(measured.values())
        ratios.append(measured[1] / measured[0])
    if min(samples) < target_ns:
        raise RuntimeError('a sample was shorter than 100ms/20x startup target')
    result = {'passed': True, 'comparison': 'same cross-depth-8 throw/catch guest before vs after',
              'product': 'uwvm2-ros' if args.ros else 'uwvm2', 'policy': args.policy,
              'build_optimization': 'O3', 'generated_optimization': 'pb-o3',
              'cpu': 0, 'cgroup': cgroup, 'hashes': hashes, 'wasm_sha256': sha(wasm),
              'iterations': count, 'pairs': args.pairs, 'startup_ns': startup_ns,
              'minimum_sample_ns': min(samples), 'target_ns': target_ns,
              'paired_after_over_before': ratios,
              'median_paired_after_over_before': statistics.median(ratios),
              'limitation': 'Whole-process wall time includes launch and uncached JIT, not isolated native unwind cost.'}
    (args.out/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
    print('PASS', result['product'], args.policy,
          'pairs', args.pairs, 'median after/before', result['median_paired_after_over_before'])


if __name__ == '__main__':
    main()
