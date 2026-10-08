#!/usr/bin/env python3
"""Pair instruction/unwind diagnostics on identical native-EH workloads.

Consume completed O3 full-JIT measurements from bench_exception_cross_cli.py.
The matched program, loop count, process affinity and generated pb-o3 policy are
identical within each AB/BA pair. All enabled diagnostic work remains included.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import statistics
import subprocess
import time
from bench_exception_cross_cli import fixture


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--wasmtime', type=Path, required=True)
    p.add_argument('--instruction-baseline', type=Path, required=True)
    p.add_argument('--unwind-baseline', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--cpu', type=int, default=0)
    p.add_argument('--workload', action='append', choices=['ordinary-call', 'cross-depth-1', 'cross-depth-8'])
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    allowed = os.sched_getaffinity(0)
    assert a.cpu in allowed
    os.sched_setaffinity(0, {a.cpu})
    a.out.mkdir(parents=True, exist_ok=False)
    (a.out/'runner.py').write_bytes(Path(__file__).read_bytes())
    (a.out/'fixture-generator.py').write_bytes(Path(__file__).with_name('bench_exception_cross_cli.py').read_bytes())
    with a.uwvm.open('rb') as stream:
        binary_sha = hashlib.file_digest(stream, 'sha256').hexdigest()
    baselines = []
    for policy, directory in [('instruction', a.instruction_baseline), ('unwind', a.unwind_baseline)]:
        data = json.loads((directory/'summary.json').read_text())
        assert data['passed_semantics'] and data['sha256']['uwvm'] == binary_sha
        assert data['backend'] == 'llvm-jit' and data['mode'] == 'full'
        assert data['build_optimization'] == 'O3' and data['generated_optimization'] == 'pb-o3'
        assert data['call_stack_policy'] == policy and data['persistent_cache'] is False
        (a.out/(policy+'-baseline.json')).write_text(json.dumps(data, indent=2)+'\n')
        (a.out/(policy+'-build-manifest.input')).write_bytes((directory/'build-manifest.input').read_bytes())
        baselines.append(data)
    minimum = max(data['target_sample_ns'] for data in baselines)
    rows = []

    def resources(label):
        processes = []
        for proc in Path('/proc').iterdir():
            if not proc.name.isdecimal():
                continue
            try:
                state = (proc/'stat').read_text()
                fields = state[state.rfind(')')+2:].split()
                comm = (proc/'comm').read_text().strip()
                processes.append(dict(pid=int(proc.name), comm=comm, state=fields[0],
                                      user_ticks=int(fields[11]), system_ticks=int(fields[12])))
                assert fields[0] in ('Z', 'X') or not comm.startswith(('clang', 'cc1', 'cc1plus', 'rustc', 'ld.lld')), comm
            except (OSError, ValueError, IndexError):
                pass
        data = dict(processes=processes, allowed_affinity=sorted(allowed), affinity=sorted(os.sched_getaffinity(0)),
                    cgroup={n: (Path('/sys/fs/cgroup')/n).read_text().strip() for n in
                            ('memory.max', 'memory.swap.max', 'memory.current', 'memory.events', 'cpuset.cpus.effective', 'cpu.stat')})
        (a.out/(label+'-resources.json')).write_text(json.dumps(data, indent=2)+'\n')

    def run(label, command, phase, count):
        start = time.perf_counter_ns()
        result = subprocess.run([str(x) for x in command], capture_output=True, timeout=240)
        elapsed = time.perf_counter_ns()-start
        (a.out/(label+'.log')).write_bytes(result.stdout+result.stderr)
        rows.append(dict(label=label, command=[str(x) for x in command], phase=phase, iterations=count,
                         elapsed_ns=elapsed, exit=result.returncode))
        (a.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
        assert result.returncode == 0, (label, result.returncode, result.stdout, result.stderr)
        return elapsed

    summaries = []
    resources('before')
    for workload, baseline_name in [('ordinary-call', 'unused-feature'),
                                    ('cross-depth-1', 'native-unwind-depth'),
                                    ('cross-depth-8', 'native-unwind-depth')]:
        if a.workload and workload not in a.workload:
            continue
        count = max(next(row['iterations'] for row in data['comparisons'] if row['comparison'] == baseline_name)
                    for data in baselines)
        for attempt in range(5):
            wat = a.out/f'{workload}-{count}.wat'
            wat.write_text(fixture(workload, count)+'\n')
            wasm = wat.with_suffix('.wasm')
            run(wat.stem+'-assemble', [a.wasm_tools, 'parse', wat, '-o', wasm], 'assemble', count)
            run(wat.stem+'-validate', [a.wasm_tools, 'validate', wasm], 'validate', count)
            run(wat.stem+'-reference', [a.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', wasm], 'reference', count)
            commands = []
            for policy in ['instruction', 'unwind']:
                mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
                commands.append([a.uwvm]+mode+['-Rct', '0', '-Rllvm-full-policy', 'pb-o3',
                                '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable',
                                '-WFE-exceptions', '--run', wasm])
            times = [run(f'{workload}-calibration-{attempt}-{index}', command, 'calibration', count)
                     for index, command in enumerate(commands)]
            if min(times) >= minimum*1.2:
                break
            count = math.ceil(count*max(2, minimum*1.3/min(times)))
            assert count <= 0x7fffffff, 'cannot amortize startup within the guest signed i32 loop'
        else:
            raise RuntimeError('calibration did not amortize startup')
        samples = []
        for pair in range(9):
            row = {}
            for index in ([0, 1] if pair % 2 == 0 else [1, 0]):
                row[index] = run(f'{workload}-pair-{pair}-{index}', commands[index], 'sample', count)
            assert min(row.values()) >= minimum, 'sample too short; retain this failed iteration and recalibrate'
            samples.append(row)
        result = dict(workload=workload, iterations=count, pairs=9,
                      wasm_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest(),
                      median_instruction_ns_per_iteration=statistics.median(row[0] for row in samples)/count,
                      median_unwind_ns_per_iteration=statistics.median(row[1] for row in samples)/count,
                      paired_unwind_over_instruction=[row[1]/row[0] for row in samples],
                      median_paired_unwind_over_instruction=statistics.median(row[1]/row[0] for row in samples))
        summaries.append(result)
        print('MEASURED', workload, 'unwind/instruction', result['median_paired_unwind_over_instruction'], flush=True)
    resources('after')
    (a.out/'summary.json').write_text(json.dumps(dict(passed=True, binary_sha256=binary_sha,
        runtime_optimization='O3', generated_optimization='pb-o3', persistent_cache=False,
        minimum_sample_ns=minimum, comparisons=summaries, compilation_and_startup_included=True,
        scope='Same complete guest work, actual enabled diagnostic capture and cleanup; not isolated unwinder timing'), indent=2)+'\n')
    print('PASS paired native diagnostic policy measurements', flush=True)


if __name__ == '__main__':
    main()
