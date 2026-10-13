#!/usr/bin/env python3
"""Paired interpreter or LLVM EH measurements; never complete EH qualification.

Each workload checks its final integer state in the guest. Ordinary feature
on/off uses the exact same .wasm and executable. Other pairs deliberately
measure different lowering paths with equal guest work: protected normal calls,
the local throw-to-branch optimization, and throwing through 1/8 callees.
Compilation/startup are included, measured, and amortized by calibrated loops.
O1 measurements are development observations, not release-performance evidence.
A LLVM run explicitly fixes one diagnostic call-stack policy for both variants;
policy-to-policy timing requires separate runs, never mixed A/B work.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import resource
import statistics
import subprocess
import time


SEED = 123456789
MASK = (1 << 32) - 1
STEP = 'i32.const 1664525 i32.mul i32.const 1013904223 i32.add'


def expected_state(iterations):
    # Affine exponentiation computes the LCG endpoint in O(log n). The guest
    # executes every step; Wasmtime separately checks these generated fixtures.
    multiplier, increment = 1, 0
    power_multiplier, power_increment = 1664525, 1013904223
    remaining = iterations
    while remaining:
        if remaining & 1:
            multiplier = multiplier * power_multiplier & MASK
            increment = (increment * power_multiplier + power_increment) & MASK
        power_increment = (power_multiplier + 1) * power_increment & MASK
        power_multiplier = power_multiplier * power_multiplier & MASK
        remaining >>= 1
    return (multiplier * SEED + increment) & MASK


def fixture(kind, iterations):
    # Keep the LCG state at the intended i32 width, but use a separate i64 loop
    # count: O3 can make the local catch path so fast that a 100 ms sample needs
    # more than INT32_MAX iterations. Both sides retain the identical loop.
    declarations = ''
    operation = ''
    local_comparison = kind in ('local-throw', 'local-branch')
    if kind in ('ordinary-call', 'protected-normal-call'):
        declarations = f'(func $leaf (param i32) (result i32) local.get 0 {STEP})'
        operation = 'local.get $state call $leaf'
        if kind == 'protected-normal-call':
            declarations = '(tag $t (param i32)) ' + declarations
            operation = f'block $out (result i32) try_table (result i32) (catch $t $out) {operation} end end'
    elif kind in ('local-throw', 'local-branch'):
        value = f'local.get $state {STEP}'
        if kind == 'local-throw':
            declarations = '(tag $t (param i32))'
            operation = f'block $out (result i32) try_table (catch $t $out) {value} throw $t end unreachable end'
        else:
            # Keep the same nested block shape as the statically selected catch.
            operation = f'block $out (result i32) block {value} br $out end unreachable end'
    elif kind in ('cross-depth-1', 'cross-depth-8'):
        depth = 1 if kind == 'cross-depth-1' else 8
        declarations = '(tag $t (param i32)) '
        for level in range(depth - 1):
            declarations += f'(func $f{level} (param i32) local.get 0 call $f{level+1}) '
        declarations += f'(func $f{depth-1} (param i32) local.get 0 {STEP} throw $t)'
        operation = 'block $out (result i32) try_table (catch $t $out) local.get $state call $f0 end unreachable end'
    else:
        raise ValueError(kind)
    # A sequentially consistent store after either local path is an observable
    # per-iteration effect. Without it, O3 can eliminate or summarize the
    # complete local loop, leaving only JIT startup in the timed interval.
    memory = '(memory 1 1 shared)' if local_comparison else ''
    barrier = 'i32.const 0 local.get $state i32.atomic.store' if local_comparison else ''
    readback = f'i32.const 0 i32.atomic.load i32.const {expected_state(iterations)} i32.ne if unreachable end' if local_comparison else ''
    return f'''(module {memory} {declarations}
      (func (export "_start") (local $state i32) (local $remaining i64)
        i32.const {SEED} local.set $state i64.const {iterations} local.set $remaining
        loop $again {operation} local.set $state {barrier}
          local.get $remaining i64.const 1 i64.sub local.tee $remaining i64.const 0 i64.ne br_if $again
        end
        local.get $state i32.const {expected_state(iterations)} i32.ne if unreachable end
        {readback}))'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, help='Omit to qualify only fixture semantics with Wasmtime')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--backend', choices=['int', 'llvm'], default='int')
    parser.add_argument('--trace', choices=['instruction', 'unwind', 'none'],
                        help='LLVM diagnostic call-stack policy (default: instruction)')
    parser.add_argument('--mode', choices=['full', 'lazy', 'lazy+verification'], default='full')
    parser.add_argument('--build-optimization', choices=['O0', 'O1', 'O2', 'O3', 'Os', 'Oz'])
    parser.add_argument('--build-manifest', type=Path, help='Optional exact compiler/source manifest to preserve')
    parser.add_argument('--iterations', type=int, default=2000000, help='Initial nonthrowing/local iteration count')
    parser.add_argument('--throw-iterations', type=int, default=50000)
    parser.add_argument('--comparison', action='append', choices=['unused-feature', 'normal-protected-call', 'local-static-lowering', 'native-unwind-depth'],
                        help='Select a measured path; local optimized lowering also has a separate exact-object comparison')
    parser.add_argument('--reference-max-iterations', type=int, default=200000,
                        help='Bound semantic reference work independently of optimized UWVM calibration')
    parser.add_argument('--pairs', type=int, default=9)
    parser.add_argument('--min-sample-seconds', type=float, default=0.1)
    parser.add_argument('--startup-multiple', type=float, default=20.0)
    args = parser.parse_args()
    if args.uwvm and not args.build_optimization:
        parser.error('--uwvm requires an explicit --build-optimization; do not infer release flags')
    if args.ros and args.mode != 'full':
        parser.error('ROS only supports full mode')
    if args.backend == 'int' and args.trace:
        parser.error('--trace selects LLVM call-stack policy and requires --backend llvm')
    trace_policy = (args.trace or 'instruction') if args.backend == 'llvm' else None
    if not 0 < args.iterations <= 0x7fffffffffffffff or not 0 < args.throw_iterations <= 0x7fffffffffffffff:
        parser.error('iteration counts must be positive signed i64 values')
    if args.reference_max_iterations < 1:
        parser.error('reference iteration cap must be positive')
    if args.pairs < 9 or args.min_sample_seconds <= 0 or args.startup_multiple < 20:
        parser.error('require at least 9 pairs, positive duration, and at least 20x startup amortization')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / 'runner.py').write_bytes(Path(__file__).read_bytes())
    rows, modules, references = [], {}, {}
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in (
        ('wasm_tools', args.wasm_tools), ('wasmtime', args.wasmtime), ('uwvm', args.uwvm)) if path}
    if args.build_manifest:
        contents = args.build_manifest.read_bytes()
        (args.out / 'build-manifest.input').write_bytes(contents)
        hashes['build_manifest'] = hashlib.sha256(contents).hexdigest()
    environment = {name: Path('/sys/fs/cgroup', name).read_text().strip()
                   for name in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}
    environment['cpuinfo'] = Path('/proc/cpuinfo').read_text()
    (args.out / 'environment.json').write_text(json.dumps(environment, indent=2) + '\n')

    def run(label, command, phase, iterations=0):
        start = time.perf_counter_ns()
        result = subprocess.run([str(part) for part in command], capture_output=True, timeout=240)
        elapsed = time.perf_counter_ns() - start
        (args.out / (label + '.log')).write_bytes(result.stdout + result.stderr)
        row = dict(label=label, phase=phase, command=[str(part) for part in command],
                   elapsed_ns=elapsed, iterations=iterations, exit=result.returncode)
        rows.append(row)
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError(f'{label}: exit {result.returncode}\n{result.stdout!r}\n{result.stderr!r}')
        return elapsed

    def encode(kind, count):
        key = (kind, count)
        if key in modules:
            return modules[key]
        label = kind + '-' + str(count)
        path = args.out / (label + '.wat')
        source = '(module (func (export "_start")))' if kind == 'empty' else fixture(kind, count)
        path.write_text(source + '\n')
        wasm = path.with_suffix('.wasm')
        run(label + '-assemble', [args.wasm_tools, 'parse', path, '-o', wasm], 'assemble')
        run(label + '-validate', [args.wasm_tools, 'validate', wasm], 'validate')
        reference_count = min(count, args.reference_max_iterations)
        reference_key = (kind, reference_count)
        if reference_key not in references:
            if reference_count == count:
                reference_wasm = wasm
            else:
                reference_source = args.out / (kind + '-' + str(reference_count) + '-reference.wat')
                reference_source.write_text(fixture(kind, reference_count) + '\n')
                reference_wasm = reference_source.with_suffix('.wasm')
                run(reference_source.stem + '-assemble', [args.wasm_tools, 'parse', reference_source, '-o', reference_wasm], 'assemble')
                run(reference_source.stem + '-validate', [args.wasm_tools, 'validate', reference_wasm], 'validate')
            reference_flags = ['-W', 'exceptions=y'] + (['-W', 'threads=y', '-W', 'shared-memory=y']
                                                    if kind in ('local-throw', 'local-branch') else [])
            run(reference_wasm.stem + '-wasmtime', [args.wasmtime, '-C', 'cache=n', *reference_flags, reference_wasm], 'reference', reference_count)
            references[reference_key] = reference_wasm
        modules[key] = wasm
        return wasm

    def command(kind, count, enabled):
        if args.backend == 'llvm':
            base = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', args.mode]
            base += ['-Rllvm-call-stack', trace_policy, '-Rllvm-cache-path', 'disable']
            if args.mode == 'full':
                base += ['-Rllvm-full-policy', 'pb-o3']
        else:
            base = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', args.mode]
        feature_flags = ['-WFE-exceptions' if enabled else '-WFD-exceptions']
        if kind in ('local-throw', 'local-branch'):
            feature_flags.append('-WFE-threads')
        return [args.uwvm] + base + ['-Rct', '0', *feature_flags,
                                    '--run', encode(kind, count)]

    comparisons = [
        ('unused-feature', ('ordinary-call', False), ('ordinary-call', True), args.iterations),
        ('normal-protected-call', ('ordinary-call', True), ('protected-normal-call', True), args.iterations),
        ('local-static-lowering', ('local-branch', True), ('local-throw', True), args.iterations),
        ('native-unwind-depth', ('cross-depth-1', True), ('cross-depth-8', True), args.throw_iterations),
    ]
    if args.comparison:
        comparisons = [item for item in comparisons if item[0] in args.comparison]
    summaries = []
    startup_ns = None
    if args.uwvm:
        startup = [run('startup-' + str(i), command('empty', 0, True), 'startup') for i in range(3)]
        startup_ns = statistics.median(startup)
    target_ns = max(args.min_sample_seconds * 1e9, (startup_ns or 0) * args.startup_multiple)
    for name, variant_a, variant_b, count in comparisons:
        if not args.uwvm:
            encode(variant_a[0], count)
            encode(variant_b[0], count)
            print('PASS Wasmtime semantics', name, 'iterations', count, flush=True)
            continue
        for attempt in range(5):
            commands = [command(kind, count, enabled) for kind, enabled in (variant_a, variant_b)]
            calibration = [run(f'{name}-calibration-{attempt}-{side}', commands[i], 'calibration', count)
                           for i, side in enumerate(('A', 'B'))]
            if min(calibration) >= target_ns * 1.2:
                break
            count = math.ceil(count * max(2, target_ns * 1.3 / min(calibration)))
            if count > 0x7fffffffffffffff:
                raise RuntimeError('calibration would exceed signed i64 iteration limit')
        else:
            raise RuntimeError(name + ': failed to amortize startup after 5 calibrations')
        samples_a, samples_b, ratios = [], [], []
        for pair in range(args.pairs):
            order = (0, 1) if pair % 2 == 0 else (1, 0)
            pair_samples = {}
            for index in order:
                side = 'AB'[index]
                pair_samples[index] = run(f'{name}-pair-{pair:02d}-{side}', commands[index], 'sample', count)
            samples_a.append(pair_samples[0])
            samples_b.append(pair_samples[1])
            ratios.append(pair_samples[1] / pair_samples[0])
        if min(samples_a + samples_b) < target_ns:
            raise RuntimeError(name + ': a measured sample is too short; increase iteration counts')
        summaries.append(dict(comparison=name, variant_a=variant_a, variant_b=variant_b, iterations=count,
            pairs=args.pairs, median_a_ns=statistics.median(samples_a), median_b_ns=statistics.median(samples_b),
            median_a_ns_per_iteration=statistics.median(samples_a)/count,
            median_b_ns_per_iteration=statistics.median(samples_b)/count,
            paired_b_over_a=ratios, median_paired_b_over_a=statistics.median(ratios),
            minimum_sample_ns=min(samples_a + samples_b)))
        print('MEASURED', name, 'iterations', count, 'median B/A', summaries[-1]['median_paired_b_over_a'], flush=True)
    module_hashes = {str(path.name): hashlib.sha256(path.read_bytes()).hexdigest() for path in modules.values()}
    summary = dict(passed_semantics=True, comparisons=summaries, sha256=hashes, module_sha256=module_hashes,
        backend='uwvm-int' if args.backend == 'int' else 'llvm-jit', mode=args.mode,
        call_stack_policy=trace_policy, build_optimization=args.build_optimization,
        generated_optimization=('pb-o3' if args.mode == 'full' else 'runtime lazy policy') if args.backend == 'llvm' else None,
        persistent_cache=False if args.backend == 'llvm' else None,
        qualification=('development observation only' if args.build_optimization != 'O3'
                       else 'release timing measurements; code-generation review is separately required'),
        compilation_and_startup_included=True, median_empty_startup_ns=startup_ns, target_sample_ns=target_ns,
        reference_iteration_cap=args.reference_max_iterations,
        reference_scope="Same workload semantics at bounded loop counts; each larger UWVM workload independently checks its exact affine endpoint",
        selected_comparisons=[item[0] for item in comparisons],
        trace_policy=('Production interpreter trace retained equally; no exception diagnostic suppression'
                      if args.backend == 'int' else
                      'LLVM diagnostic policy ' + trace_policy + ' retained equally for both variants; '
                      'none disables diagnostic frames, not native guest exception propagation'),
        native_unwind_depth='Actual guest exception handling at depths 1/8 with the same policy; includes value allocation, '
                            'enabled trace capture and cleanup, not isolated unwinder timing or a diagnostic-policy comparison')
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS benchmark fixture semantics;', len(summaries), 'paired comparisons measured')


if __name__ == '__main__':
    main()
