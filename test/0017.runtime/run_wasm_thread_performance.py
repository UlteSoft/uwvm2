#!/usr/bin/env python3
"""Build a real VM thread-entry probe against one frozen O3 runtime object.

The default is a semantic/creation-count smoke run, not a performance result.
Use --measure only during an otherwise idle benchmark window in the remote
64 GiB cgroup. No runtime or LLVM library is rebuilt by this runner.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import statistics
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--base-source-root', type=Path, required=True)
p.add_argument('--runtime-build', type=Path, required=True)
p.add_argument('--expected-source-id', required=True,
               help='full intended frozen source ID for the runtime object and test source')
p.add_argument('--wasm-tools', type=Path,
               help='exact validator for dumped shared-memory/atomic guest modules; required for timing')
p.add_argument('--out', type=Path, required=True)
p.add_argument('--measure', action='store_true')
p.add_argument('--run-only', action='store_true', help='Use already built, hash-verified binaries; suitable for a compile-free timing window')
p.add_argument('--variant', choices=['all', 'timed', 'qualify'], default='all')
p.add_argument('--cpu', type=int, default=0)
p.add_argument('--affinity', choices=['single', 'four'], default='single',
               help='pin guest workers to one P core or all four established P cores')
a = p.parse_args()
if a.measure and a.wasm_tools is None:
    p.error('formal thread timing requires --wasm-tools to qualify the generated guest syntax')
if a.cpu not in (0, 2, 4, 6):
    p.error('timed VM thread workload must use a P core (0, 2, 4, or 6)')
if a.affinity == 'four' and a.cpu != 0:
    p.error('four-P-core affinity uses canonical CPUs 0,2,4,6; pass --cpu 0')
guest_affinity = str(a.cpu) if a.affinity == 'single' else '0,2,4,6'
root = Path(__file__).resolve().parents[2]
subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
if a.run_only:
    assert a.out.is_dir(), 'run-only requires a completed build directory'
else:
    a.out.mkdir(parents=True, exist_ok=False)
out = a.out.resolve()
base = a.base_source_root.resolve()
runtime = a.runtime_build.resolve()

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

if root != base:
    raise RuntimeError('thread fixture and runtime must use the same frozen source checkout')
build_json = runtime/'build.json'
build = json.loads(build_json.read_text())
fingerprint = base/'tools/ci/wasm3_source_fingerprint.py'
source_id = subprocess.check_output(['python3', str(fingerprint), str(base),
                                     str(out/('source-'+('measurement' if a.measure else 'semantic')+'-before.json'))],
                                    text=True).strip()
if (source_id != a.expected_source_id or build['source_id'] != source_id or
        Path(build['source']).resolve() != base or
        build['runtime_object_sha256'] != sha(runtime/'runtime.o') or
        build['binary_sha256'] != sha(Path(build['cli_command'][-1]))):
    raise RuntimeError('thread runtime object/product build differs from intended frozen source')

source = root/'test/0017.runtime/wasm_thread_performance.cc'
paths = [source, Path(__file__).resolve(), runtime/'runtime.o', runtime/'runtime.command',
         runtime/'test.command', build_json, fingerprint,
         base/'test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h']
if a.wasm_tools is not None:
    paths.append(a.wasm_tools.resolve(strict=True))
before = {str(path): sha(path) for path in paths}
if a.run_only:
    assert json.loads((out/'input-sha256.json').read_text()) == before, 'build inputs changed before run-only'
else:
    (out/'input-sha256.json').write_text(json.dumps(before, indent=2)+'\n')
run_prefix = ('measurement-' if a.measure else 'semantic-') + a.affinity + '-'
binaries = json.loads((out/'build-binaries.json').read_text()) if a.run_only else {}
fields = ['memory.max', 'memory.swap.max', 'memory.current', 'memory.peak',
          'memory.events', 'cpuset.cpus.effective', 'cpu.max', 'cpu.stat', 'cpu.pressure']
def resources(label):
    data = {field: (Path('/sys/fs/cgroup')/field).read_text().strip() for field in fields}
    data['runner_affinity'] = sorted(os.sched_getaffinity(0))
    data['guest_affinity'] = guest_affinity
    frequency = Path(f'/sys/devices/system/cpu/cpu{a.cpu}/cpufreq')
    for name in ('scaling_cur_freq', 'scaling_governor'):
        try:
            data[name] = (frequency/name).read_text().strip()
        except (FileNotFoundError, PermissionError):
            data[name] = None
    data['p_core_frequency_by_cpu'] = {}
    for cpu in ((a.cpu,) if a.affinity == 'single' else (0, 2, 4, 6)):
        base = Path(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq')
        readings = {}
        for name in ('scaling_cur_freq', 'scaling_governor'):
            try:
                readings[name] = (base/name).read_text().strip()
            except (FileNotFoundError, PermissionError):
                readings[name] = None
        data['p_core_frequency_by_cpu'][str(cpu)] = readings
    data['thermal_millicelsius_by_zone_and_type'] = {}
    for zone in sorted(Path('/sys/class/thermal').glob('thermal_zone*')):
        try:
            data['thermal_millicelsius_by_zone_and_type'][f"{zone.name}:{(zone/'type').read_text().strip()}"] = int(
                (zone/'temp').read_text())
        except (FileNotFoundError, PermissionError, ValueError):
            continue
    (out/(label+'-resources.json')).write_text(json.dumps(data, indent=2)+'\n')
    return data
resources_before = resources(run_prefix+'before')
command = shlex.split((runtime/'test.command').read_text())
optimization_flags = {'-O0', '-O1', '-O2', '-O3', '-Os', '-Oz', '-Og', '-Ofast'}
for configured in [command, shlex.split((runtime/'runtime.command').read_text())]:
    selected = [flag for flag in configured if flag in optimization_flags]
    assert selected and selected[-1] == '-O3', 'requires actual O3 runtime and harness'
assert '-DUWVM2TEST_RUNNER_USE_LLVM_JIT' in command, 'this fixture qualifies LLVM full host entries'
assert command[command.index('-o')+1], 'missing output in runtime test template'
# Qualified build commands may spell their repository fixture as either a
# relative or an absolute path. Keep the replacement inside this source tree.
fixtures = [i for i, item in enumerate(command) if item.endswith('.cc') and
            (item.startswith('test/') or Path(item).is_relative_to(base/'test'))]
assert len(fixtures) == 1, fixtures
command[fixtures[0]] = str(source)
command += ['-I'+str(base/'test/0013.uwvm_int/strict')]
commands = []
guest_modules = {}
def run(label, args, timeout=300):
    (out/(label+'.command')).write_text(shlex.join(args)+'\n')
    with (out/(label+'.log')).open('w') as log:
        completed = subprocess.run(args, cwd=base, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    commands.append(dict(label=label, exit=completed.returncode, command=args))
    (out/(run_prefix+'commands.json')).write_text(json.dumps(commands, indent=2)+'\n')
    if completed.returncode:
        raise RuntimeError((label, completed.returncode))

for instrumented in ([False, True] if a.variant == 'all' else [a.variant == 'qualify']):
    name = 'qualify' if instrumented else 'timed'
    build = command.copy()
    build[build.index('-o')+1] = str(out/name)
    if instrumented:
        build += ['-DUWVM_THREAD_BENCH_QUALIFY_CREATION', '-Wl,--export-dynamic-symbol=pthread_create']
    if a.run_only:
        assert sha(out/name) == binaries[name], 'built binary changed before run-only'
    else:
        run(name+'-build', build)
        binaries[name] = sha(out/name)
        (out/'build-binaries.json').write_text(json.dumps(binaries, indent=2)+'\n')
    for policy in ['instruction', 'unwind']:
        samples, rounds = (9, 32) if a.measure and not instrumented else (1, 2)
        wasm = out/(name+'-'+policy+'-'+a.affinity+'.wasm')
        run(run_prefix+name+'-'+policy, ['taskset', '-c', guest_affinity, str(out/name), policy, str(samples), str(rounds), str(wasm)], timeout=180)
        if a.wasm_tools is not None:
            run(run_prefix+name+'-'+policy+'-validate-wasm',
                ['taskset', '-c', '16', str(a.wasm_tools), 'validate', str(wasm)], timeout=30)
        guest_modules[name+'/'+policy] = dict(path=str(wasm), sha256=sha(wasm),
            syntax_validated=a.wasm_tools is not None)
        log = (out/(run_prefix+name+'-'+policy+'.log')).read_text()
        assert 'PASS real VM thread entry/checksum/markers' in log
        rows = [json.loads(line) for line in log.splitlines() if line.startswith('{')]
        assert len(rows) == 12*samples
        groups = {}
        for row in rows:
            expected = row['workers']*rounds if row['threaded'] else 0
            assert not instrumented or row['native_creations'] == expected, row
            groups.setdefault((row['path'], row['workers'], row['threaded']), []).append(row)
        comparisons = []
        for path in ['std_native', 'vm_full_entry', 'vm_raw_entry']:
            for workers in [1, 4]:
                direct = groups[(path, workers, False)]
                threaded = groups[(path, workers, True)]
                assert [x['checksum'] for x in direct] == [x['checksum'] for x in threaded]
                wall_deltas = [new['wall_ns'] - old['wall_ns']
                               for old, new in zip(direct, threaded)]
                new_thread_wall = [item['wall_ns'] for item in threaded]
                new_thread_start = [item['start_latency_ns'] for item in threaded]
                wall_median = statistics.median(new_thread_wall)
                comparisons.append(dict(path=path, workers=workers,
                    same_thread_wall_median_ns=statistics.median(x['wall_ns'] for x in direct),
                    new_thread_wall_median_ns=wall_median,
                    paired_threaded_minus_direct_wall_ns=wall_deltas,
                    paired_threaded_minus_direct_wall_median_ns=statistics.median(wall_deltas),
                    paired_threaded_minus_direct_wall_p95_ns=(statistics.quantiles(
                        wall_deltas, n=20, method='inclusive')[18]
                        if len(wall_deltas) >= 2 else None),
                    new_thread_start_latency_p95_ns=(statistics.quantiles(
                        new_thread_start, n=20, method='inclusive')[18]
                        if len(new_thread_start) >= 2 else None),
                    tail_sample_count=len(new_thread_wall),
                    p95_is_estimate_from_nine_samples=len(new_thread_wall) == 9,
                    end_to_end_kernel_updates_per_second=workers * 1024 * 32 * 1e9 / wall_median,
                    four_worker_delta_includes_parallel_speedup=workers == 4,
                    paired_wall_ratio_median=statistics.median(b['wall_ns']/v['wall_ns'] for v, b in zip(direct, threaded)),
                    same_thread_guest_interval_median_ns=statistics.median(x['guest_interval_ns'] for x in direct),
                    new_thread_guest_interval_median_ns=statistics.median(x['guest_interval_ns'] for x in threaded)))
        (out/(run_prefix+name+'-'+policy+'.json')).write_text(json.dumps(dict(rows=rows, comparisons=comparisons), indent=2)+'\n')

if not a.run_only and (out/'timed').is_file():
    run('native-kernel-assembly', ['/toolchain/bin/llvm-objdump', '-d', '--disassemble-symbols=uwvm_vm_thread_native_kernel', str(out/'timed')])
assert before == {str(path): sha(path) for path in paths}, 'inputs changed during qualification'
after_source_id = subprocess.check_output(['python3', str(fingerprint), str(base),
    str(out/('source-'+('measurement' if a.measure else 'semantic')+'-after.json'))],
    text=True).strip()
if after_source_id != source_id:
    raise RuntimeError('frozen product source changed during thread qualification')
resources_after = resources(run_prefix+'after')
events_before = dict(line.split() for line in resources_before['memory.events'].splitlines())
events_after = dict(line.split() for line in resources_after['memory.events'].splitlines())
if any(events_before[name] != events_after[name] for name in ('oom', 'oom_kill')):
    raise RuntimeError('cgroup OOM occurred during VM thread qualification')
if a.measure:
    cpu_before = dict(line.split() for line in resources_before['cpu.stat'].splitlines())
    cpu_after = dict(line.split() for line in resources_after['cpu.stat'].splitlines())
    if any(cpu_before.get(name) != cpu_after.get(name)
           for name in ('nr_throttled', 'throttled_usec')):
        raise RuntimeError('cgroup CPU throttled during VM thread measurement')
subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
(out/(run_prefix+'summary.json')).write_text(json.dumps(dict(passed=True, measurement=a.measure,
    source_id=source_id, expected_source_id=a.expected_source_id,
    build_json_sha256=sha(build_json),
    guest_affinity=guest_affinity,
    source_base=str(base), runtime_object_sha256=sha(runtime/'runtime.o'),
    binary_sha256=binaries,
    guest_modules=guest_modules,
    wasm_tools_sha256=sha(a.wasm_tools) if a.wasm_tools is not None else None,
    scope='real VM full/raw host entries from fresh std::threads; shared memory plus atomic.fence; matched same-thread VM baseline; native thread reference; host stage clocks include fixed import transitions'), indent=2)+'\n')
print('PASS real VM thread semantic/creation qualification; final timing='+str(a.measure))
