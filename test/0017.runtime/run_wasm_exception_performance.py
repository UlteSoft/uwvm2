#!/usr/bin/env python3
"""Measure real Wasm normal calls and caught exceptions in a frozen VM build.

Run the timing phase only in an otherwise idle bounded Linux test cgroup. The
same LCG checksum is checked by all three modules; one in sixteen calls of the
throwing variant crosses a native Wasm exception boundary.
"""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import statistics
import subprocess
import time


ORDINARY = {'int-full': ['-Rcc', 'int', '-Rcm', 'full'],
            'jit-full': ['-Rcc', 'jit', '-Rcm', 'full'],
            'tiered-lazy': ['-Rcc', 'tiered', '-Rcm', 'lazy']}
ROS = {'int-full': ['-Rint'], 'jit-full': ['-Raot']}
MULTIPLIER = 1664525
INCREMENT = 1013904223
MASK = 0xffff_ffff


def cpu_telemetry(cpu):
    frequency = Path(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq')
    data = {'timestamp_ns': time.time_ns(),
            'cgroup_cpu_stat': Path('/sys/fs/cgroup/cpu.stat').read_text().strip()}
    for name in ('scaling_cur_freq', 'scaling_governor'):
        try:
            data[name] = (frequency/name).read_text().strip()
        except (FileNotFoundError, PermissionError):
            data[name] = None
    sensors = {}
    for zone in sorted(Path('/sys/class/thermal').glob('thermal_zone*')):
        try:
            sensors[f"{zone.name}:{(zone/'type').read_text().strip()}"] = int(
                (zone/'temp').read_text())
        except (FileNotFoundError, PermissionError, ValueError):
            continue
    data['thermal_millicelsius_by_zone_and_type'] = sensors
    return data


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def memory_events():
    return {name: int(value) for name, value in
            (line.split() for line in Path('/sys/fs/cgroup/memory.events').read_text().splitlines())}


def lcg_result(steps):
    result_mul, result_add = 1, 0
    power_mul, power_add = MULTIPLIER, INCREMENT
    while steps:
        if steps & 1:
            result_mul = result_mul * power_mul & MASK
            result_add = (result_add * power_mul + power_add) & MASK
        power_add = (power_add * (power_mul + 1)) & MASK
        power_mul = power_mul * power_mul & MASK
        steps >>= 1
    return result_add


def wat(name, iterations):
    eh = name != 'plain_normal'
    throws = name == 'eh_throws'
    tag = '(tag $event (param i32))' if eh else ''
    counter = '(global $catches (mut i32) (i32.const 0))' if eh else ''
    maybe_throw = '''local.get $x
        i32.const 15
        i32.and
        i32.eqz
        if
          local.get $next
          throw $event
        end''' if throws else ''
    call = '''block $after (result i32)
          block $caught (result i32)
            try_table (result i32) (catch $event $caught)
              local.get $value
              call $step
            end
            br $after
          end
          global.get $catches
          i32.const 1
          i32.add
          global.set $catches
        end''' if eh else 'local.get $value\n        call $step'
    check_catches = f'''global.get $catches
        i32.const {iterations // 16 if throws else 0}
        i32.ne
        if unreachable end''' if eh else ''
    return f'''(module
      {tag}
      {counter}
      (func $step (param $x i32) (result i32)
        (local $next i32)
        local.get $x
        i32.const {MULTIPLIER}
        i32.mul
        i32.const {INCREMENT}
        i32.add
        local.set $next
        {maybe_throw}
        local.get $next)
      (func (export "_start")
        (local $index i32) (local $value i32)
        block $done
          loop $again
            local.get $index
            i32.const {iterations}
            i32.ge_u
            br_if $done
            {call}
            local.set $value
            local.get $index
            i32.const 1
            i32.add
            local.set $index
            br $again
          end
        end
        local.get $value
        i32.const 0x{lcg_result(iterations):08x}
        i32.ne
        if unreachable end
        {check_catches}))
'''


def wasmtime_timed_command(binary, wasm, cpu):
    # The same validated Wasm file is used by UWVM and Wasmtime. Keep this
    # comparison in its own native mode, separate from UWVM unwind/trace.
    return ['taskset', '-c', str(cpu), str(binary), 'run', '-C', 'cache=n',
            '-W', 'exceptions=y', str(wasm)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--expected-source-id', required=True,
                        help='full intended frozen source ID for this product')
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--compare-wasmtime', action='store_true',
                        help='time the same validated real cross-function EH Wasm in Wasmtime')
    parser.add_argument('--mode', action='append', choices=['int-full', 'jit-full', 'tiered-lazy'])
    parser.add_argument('--samples', type=int, default=1)
    parser.add_argument('--normal-count', type=int, default=10000)
    parser.add_argument('--throw-count', type=int, default=1600)
    parser.add_argument('--cpu', type=int, default=0)
    args = parser.parse_args()
    if not (1 <= args.samples <= 99 and 16 <= args.normal_count < 2**31 and
            16 <= args.throw_count < 2**31 and args.throw_count % 16 == 0):
        parser.error('invalid sample or iteration count')
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    if args.cpu not in (0, 2, 4, 6):
        parser.error('timed guest processes must use a P core (0, 2, 4, or 6)')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    cgroup = {name: Path('/sys/fs/cgroup', name).read_text().strip()
              for name in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}
    if cgroup != {'memory.max': '68719476736', 'memory.swap.max': '0',
                  'cpuset.cpus.effective': '0,2,4,6,16-31'}:
        raise RuntimeError(f'unexpected performance cgroup: {cgroup}')
    events_before = memory_events()
    fingerprint = root/'tools/ci/wasm3_source_fingerprint.py'
    source_id = subprocess.check_output(['python3', str(fingerprint), str(root),
                                         str(args.out/'source-before.json')], text=True).strip()
    if source_id != args.expected_source_id:
        raise RuntimeError('benchmark source differs from the intended frozen source ID')
    build_json = args.uwvm.parent/'build.json'
    build = json.loads(build_json.read_text())
    if (build['source_id'] != source_id or build['source'] != str(root)
            or build['binary_sha256'] != digest(args.uwvm)):
        raise RuntimeError('benchmark VM differs from its exact-source O3 build')
    modes = ROS if args.ros else ORDINARY
    selected = args.mode or list(modes)
    binaries = {}
    inputs = {'source_root': str(root), 'uwvm': str(args.uwvm),
              'uwvm_sha256': digest(args.uwvm), 'runner_sha256': digest(Path(__file__)),
              'wasm_tools_sha256': digest(args.wasm_tools),
              'wasmtime_sha256': digest(args.wasmtime),
              'cgroup_memory_max': Path('/sys/fs/cgroup/memory.max').read_text().strip(),
              'cgroup_swap_max': Path('/sys/fs/cgroup/memory.swap.max').read_text().strip(),
              'cpuset': Path('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip(),
              'source_id': source_id, 'build_json_sha256': digest(build_json),
              'expected_source_id': args.expected_source_id,
              'memory_events_before': events_before,
              'compare_wasmtime': args.compare_wasmtime,
              'cpu_telemetry_before': cpu_telemetry(args.cpu),
              'samples': args.samples, 'normal_count': args.normal_count,
              'throw_count': args.throw_count, 'cpu': args.cpu, 'modes': selected}
    (args.out/'inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')

    def run(label, command):
        start = time.perf_counter_ns()
        result = subprocess.run([str(value) for value in command], capture_output=True, timeout=180)
        elapsed = time.perf_counter_ns() - start
        (args.out/(label+'.log')).write_bytes(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError(f'{label}: exit {result.returncode}; see {label}.log')
        return elapsed

    for name in ('plain_normal', 'eh_normal', 'eh_throws'):
        count = args.throw_count if name == 'eh_throws' else args.normal_count
        source = args.out/(name+'.wat')
        binary = args.out/(name+'.wasm')
        source.write_text(wat(name, count))
        run(name+'-parse', [args.wasm_tools, 'parse', source, '-o', binary])
        run(name+'-validate', [args.wasm_tools, 'validate', binary])
        run(name+'-wasmtime', [args.wasmtime, 'run', '-C', 'cache=n',
                              '-W', 'exceptions=y', binary])
        binaries[name] = binary
    inputs['binaries'] = {name: digest(path) for name, path in binaries.items()}
    (args.out/'inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')

    rows = []
    cases = ('plain_normal', 'eh_normal', 'eh_throws')
    profiles = []
    for mode in selected:
        if mode not in modes:
            parser.error('mode does not belong to selected product')
        for policy in (('none',) if mode == 'int-full' else ('unwind', 'instruction')):
            prefix = ['taskset', '-c', str(args.cpu), args.uwvm, *modes[mode]]
            if mode != 'int-full':
                prefix += ['-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable']
            profiles.append((mode, policy, prefix))
    if args.compare_wasmtime:
        profiles.append(('wasmtime', 'native', None))

    def timed_command(mode, policy, prefix, name):
        if mode == 'wasmtime':
            return wasmtime_timed_command(args.wasmtime, binaries[name], args.cpu)
        flags = ['-WFD-exceptions'] if name == 'plain_normal' else ['-WFE-exceptions']
        return [*prefix, *flags, '--run', binaries[name]]

    # Warm every profile before any timed sample. Alternate and rotate entire
    # VM/policy profiles across samples so clock and thermal drift cannot give
    # Wasmtime or one UWVM trace policy every late sample.
    for mode, policy, prefix in profiles:
        for name in cases:
            run(f'{mode}-{policy}-{name}-warmup',
                timed_command(mode, policy, prefix, name))
    for sample in range(args.samples):
        start = (sample // 2) % len(profiles)
        ordered = profiles[start:] + profiles[:start]
        if sample % 2:
            ordered.reverse()
        profile_order = [f'{mode}/{policy}' for mode, policy, _ in ordered]
        for mode, policy, prefix in ordered:
            for name in (cases if sample % 2 == 0 else tuple(reversed(cases))):
                command = timed_command(mode, policy, prefix, name)
                telemetry_before = cpu_telemetry(args.cpu)
                elapsed = run(f'{mode}-{policy}-{name}-{sample}', command)
                rows.append({'mode': mode, 'policy': policy, 'name': name,
                             'sample': sample, 'profile_order': profile_order,
                             'wall_ns': elapsed,
                             'cpu_telemetry_before': telemetry_before,
                             'cpu_telemetry_after': cpu_telemetry(args.cpu),
                             'iterations': args.throw_count if name == 'eh_throws' else args.normal_count,
                             'caught_exceptions': args.throw_count // 16 if name == 'eh_throws' else 0,
                             'command': [str(part) for part in command]})
                (args.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
    groups = {}
    summary_modes = [(mode, policy) for mode in selected
                     for policy in (('none',) if mode == 'int-full' else ('unwind', 'instruction'))]
    if args.compare_wasmtime:
        summary_modes.append(('wasmtime', 'native'))
    for mode, policy in summary_modes:
        for name in ('plain_normal', 'eh_normal', 'eh_throws'):
            timings = [row['wall_ns'] for row in rows
                       if (row['mode'], row['policy'], row['name']) == (mode, policy, name)]
            median = statistics.median(timings)
            groups[f'{mode}/{policy}/{name}'] = {'median_wall_ns': median,
                'mad_wall_ns': statistics.median(abs(value - median) for value in timings),
                'minimum_wall_ns': min(timings), 'maximum_wall_ns': max(timings),
                'sample_count': len(timings),
                'p95_wall_ns_estimate': (statistics.quantiles(
                    timings, n=20, method='inclusive')[18] if len(timings) >= 2 else None),
                'p99_wall_ns_estimate': (statistics.quantiles(
                    timings, n=100, method='inclusive')[98] if len(timings) >= 2 else None),
                'p99_sample_qualified': len(timings) >= 100}
    after_id = subprocess.check_output(['python3', str(fingerprint), str(root),
                                        str(args.out/'source-after.json')], text=True).strip()
    events_after = memory_events()
    cpu_after = cpu_telemetry(args.cpu)
    before_cpu = dict(line.split() for line in inputs['cpu_telemetry_before']['cgroup_cpu_stat'].splitlines())
    after_cpu = dict(line.split() for line in cpu_after['cgroup_cpu_stat'].splitlines())
    if (after_id != source_id or digest(args.uwvm) != inputs['uwvm_sha256']
            or any(events_after[name] != events_before[name] for name in ('oom', 'oom_kill'))
            or any(after_cpu.get(name) != before_cpu.get(name)
                   for name in ('nr_throttled', 'throttled_usec'))):
        raise RuntimeError('source, binary, cgroup OOM, or CPU throttling changed during benchmark')
    result = {'passed': True, 'source_id': source_id, 'groups': groups, 'inputs': inputs,
              'memory_events_after': events_after, 'cpu_telemetry_after': cpu_after}
    (args.out/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'Wasm exception performance: {len(rows)} successful timed executions')


if __name__ == '__main__':
    main()
