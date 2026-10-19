#!/usr/bin/env python3
"""Serial equal-input ordinary engine comparison with guest-reported timers.

Guest kernel clocks and process wall clocks have separate fields. WasmEdge
AOT compilation is a separate preparation record. Failures produce no ratio.
"""
import argparse
import fcntl
import json
import math
import os
from pathlib import Path
import random
import re
import resource
import statistics
import subprocess
import time

from build_products import admission, digest
from run_matrix import argv_for
from owned_process import remember_process, wait_process, terminate_group, owned_process_record


BENCHMARK_LOCK = '/tmp/uwvm-hll-benchmark.lock'


class OwnedRetirementError(RuntimeError):
    def __init__(self, result):
        super().__init__('benchmark stopped: owned process retirement was not proved')
        self.result = result


def stable_checks(case):
    checks = case.get('checks')
    if checks is None:
        checks = case.get('expected_stdout', '').splitlines()
    assert isinstance(checks, list) and checks and all(isinstance(v, str) and v for v in checks), 'nonempty stable output checks required'
    return '\n'.join(checks)


def select_cases(rows, requested=None, purpose=None):
    assert isinstance(rows, list) and rows, 'manifest must be a nonempty case list'
    names = [row['name'] for row in rows]
    assert len(names) == len(set(names)), 'duplicate manifest case names'
    assert all(re.fullmatch(r'[A-Za-z0-9_.-]+', name) for name in names), 'unsafe case name'
    if requested:
        assert len(requested) == len(set(requested)), 'duplicate requested cases'
        assert set(requested) <= set(names), 'missing requested cases: ' + repr(sorted(set(requested) - set(names)))
    selected = [row for row in rows if (row['name'] in requested if requested else purpose is None or row.get('purpose') == purpose)]
    assert selected, 'no matching cases'
    for row in selected:
        stable_checks(row)
        assert row.get('kernel_time') in ('ns', 'ms', 'ticks'), 'timed artifact required: ' + row['name']
        assert isinstance(row.get('args', []), list) and all(isinstance(v, str) for v in row.get('args', []))
        assert digest(row['wasm']) == row['wasm_sha256'], 'Wasm identity mismatch: ' + row['name']
    return selected


def kernel_seconds(stdout, case):
    stdout = stdout.replace('\r', '')
    if case['kernel_time'] == 'ticks':
        # Stopwatch.Frequency is reported by this exact C# guest. Never assume
        # that ticks are nanoseconds or accept a partial, unchecked timer line.
        lines = re.findall(r'^csharp kernel iterations=([0-9]+) ticks=([0-9]+) frequency=([0-9]+)\r?$', stdout, re.M)
        assert len(lines) == 1 and len(re.findall(r'^csharp kernel ', stdout, re.M)) == 1, ('one complete C# timer line required', stdout[-4000:])
        iterations, ticks, frequency = map(int, lines[0])
        expected = case.get('iterations', case.get('rounds'))
        if expected is None and case.get('args'):
            expected = int(case['args'][0])
        assert expected is not None and iterations == int(expected) and iterations > 0, 'C# timer iteration mismatch'
        assert frequency > 0, 'C# timer frequency must be positive'
        seconds = ticks / frequency
        assert math.isfinite(seconds), 'nonfinite C# timer'
        return seconds
    prefix = case.get('clock_line_prefix', 'Kernel ns: ')
    values = re.findall(r'^' + re.escape(prefix) + r'([0-9]+(?:\.[0-9]+)?)\r?$', stdout, re.M)
    assert len(values) == 1 and len(re.findall(r'^' + re.escape(prefix), stdout, re.M)) == 1, (prefix, stdout[-4000:])
    number = float(values[0])
    seconds = number / {'ns': 1e9, 'ms': 1e3}[case['kernel_time']]
    assert math.isfinite(seconds), 'nonfinite guest timer'
    return seconds


def validate_kernel_seconds(seconds, wall):
    assert math.isfinite(seconds) and math.isfinite(wall) and wall > 0, 'nonfinite or invalid elapsed time'
    if seconds == 0:
        raise ValueError('zero_timer: clock resolution insufficient; increase guest work')
    assert seconds > 0, 'negative guest timer'
    # The guest timer is inside the observed process/debug window. Allow clock
    # quantization and observation latency, retaining realtime clocks as such.
    tolerance = max(.05, .05 * wall)
    assert seconds <= wall + tolerance, ('guest timer exceeds enclosing wall window', seconds, wall, tolerance)


def validate_stable_stdout(stdout, case):
    # kernel_seconds has already required one complete, valid timer line.
    # Removing it must leave exactly the declared stable output, in order.
    timer = (r'csharp kernel iterations=[0-9]+ ticks=[0-9]+ frequency=[0-9]+' if case['kernel_time'] == 'ticks'
             else re.escape(case.get('clock_line_prefix', 'Kernel ns: ')) + r'[0-9]+(?:\.[0-9]+)?')
    stable = [line for line in stdout.replace('\r', '').splitlines() if not re.fullmatch(timer, line)]
    expected = case.get('checks', case.get('expected_stdout', '').splitlines())
    assert stable == expected, ('complete stable stdout mismatch', stable, expected)


def process_identity(pid):
    root = Path('/proc') / str(pid)
    tasks = {}
    for task in (root / 'task').iterdir():
        try:
            tasks[task.name] = {'affinity': sorted(os.sched_getaffinity(int(task.name))),
                                'cgroup': (task / 'cgroup').read_text().strip()}
        except (FileNotFoundError, ProcessLookupError):
            continue
    return {'pid': pid, 'cgroup': (root / 'cgroup').read_text().strip(),
            'status': (root / 'status').read_text(), 'tasks': tasks}


def check_admission(before, after):
    assert before['membership'] == after['membership'], 'cgroup membership changed'
    def events(value):
        return dict((name, int(count)) for name, count in (line.split() for line in value.splitlines()))
    old, new = events(before['memory_events']), events(after['memory_events'])
    assert all(new.get(key, 0) == old.get(key, 0) for key in ('high', 'max', 'oom', 'oom_kill', 'oom_group_kill')), 'cgroup memory pressure event changed'


def invoke(argv, env, folder, timeout=300):
    folder.mkdir(parents=True, exist_ok=False)
    start = time.monotonic()
    usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    membership = Path('/proc/self/cgroup').read_text().strip()
    cgroup = Path('/sys/fs/cgroup') / membership[3:].lstrip('/')
    initial_events = (cgroup / 'memory.events').read_text()
    memory_observations = []
    identity, error, placement = None, None, []
    cleanup_error, ownership, retired = None, None, False
    pending, process_end, usage_after = None, None, None
    expected_affinity = sorted(os.sched_getaffinity(0))
    expected_cgroup = membership
    with (folder / 'stdout.log').open('wb') as out, (folder / 'stderr.log').open('wb') as err:
        child = subprocess.Popen(argv, env=env, stdout=out, stderr=err, start_new_session=True)
        def observe(snapshot):
            elapsed = time.monotonic() - start
            rss = re.search(r'^VmRSS:\s+([0-9]+)\s+kB$', snapshot['status'], re.M)
            anon = int(dict(line.split() for line in (cgroup / 'memory.stat').read_text().splitlines())['anon'])
            memory_observations.append({'seconds': elapsed, 'rss_bytes': int(rss[1]) * 1024 if rss else None,
                                        'cgroup_anon_bytes': anon})
            assert rss and int(rss[1]) * 1024 < 56 << 30 and anon < 60 << 30, 'benchmark memory budget exceeded'
            assert (cgroup / 'memory.events').read_text() == initial_events, 'cgroup memory pressure during benchmark'
            if placement and placement[-1]['tasks'] == snapshot['tasks']:
                placement[-1]['last_seconds'] = elapsed
                placement[-1]['observations'] += 1
            else:
                placement.append({'first_seconds': elapsed, 'last_seconds': elapsed,
                                  'observations': 1, 'tasks': snapshot['tasks']})
            assert snapshot['cgroup'] == expected_cgroup, 'target escaped cgroup'
            assert snapshot['tasks'] and all(row['affinity'] == expected_affinity and row['cgroup'] == expected_cgroup
                                             for row in snapshot['tasks'].values()), 'target TID placement mismatch'
        try:
            # Authenticate before any poll/wait can reap a short-lived leader.
            # Retain sampled helper pidfds throughout normal and error paths.
            remember_process(child, force=True)
            try:
                identity = process_identity(child.pid)
                observe(identity)
            except (FileNotFoundError, ProcessLookupError):
                # A process that exited before /proc inspection remains an
                # explicit failure, not an untraced performance sample.
                raise RuntimeError('target exited before /proc identity capture')
            last_observation = time.monotonic()
            def watch():
                nonlocal last_observation
                now = time.monotonic()
                if now - last_observation >= .5:
                    last_observation = now
                    try:
                        observe(process_identity(child.pid))
                    except (FileNotFoundError, ProcessLookupError):
                        # An owned child may finish between the timed wait and
                        # /proc read. Its successful exit is still checked below.
                        if child.poll() is None:
                            raise
            remaining = timeout - (time.monotonic() - start)
            if remaining <= 0:
                raise subprocess.TimeoutExpired(argv, timeout)
            code = wait_process(child, remaining, observe=watch)
            # Successful process timing ends before pidfd helper retirement.
            # Cleanup cost is separately retained, never folded into throughput.
            process_end = time.monotonic()
            usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
        except BaseException as exc:
            pending, error = exc, repr(exc)
        finally:
            cleanup_start = time.monotonic()
            try:
                terminate_group(child)
                ownership = owned_process_record(child)
                retired = bool(ownership and ownership['closed'] and child.returncode is not None)
                assert retired, 'owned process retirement was not proved'
            except BaseException as exc:
                cleanup_error = repr(exc)
                ownership = owned_process_record(child)
                # A failed initial pidfd authentication can race a very short
                # direct child. Reap that child if it already ended; never fall
                # back to signalling an unauthenticated numeric PID or PGID.
                try:
                    child.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass
                error = (error + '; ' if error else '') + 'owned cleanup failed: ' + cleanup_error
                if not isinstance(exc, Exception) and (pending is None or isinstance(pending, Exception)):
                    pending = exc
            cleanup_seconds = time.monotonic() - cleanup_start
            code = child.returncode
            if process_end is None:
                process_end = time.monotonic()
                usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
    result = {'argv': argv, 'exit': code, 'error': error, 'target': identity,
            'placement_watch_interval_seconds': .5, 'placement_observations': placement,
            'memory_observations': memory_observations,
            'process_wall_seconds': process_end - start,
            'process_user_seconds': usage_after.ru_utime - usage_before.ru_utime,
            'process_system_seconds': usage_after.ru_stime - usage_before.ru_stime,
            'cleanup_seconds': cleanup_seconds, 'lifecycle_wall_seconds': time.monotonic() - start,
            'process_ownership': ownership, 'cleanup_error': cleanup_error, 'retired': retired,
            'stdout': (folder / 'stdout.log').read_text(errors='replace'),
            'stderr': (folder / 'stderr.log').read_text(errors='replace')}
    # Retain cleanup proof even when an interrupt is re-raised to the caller.
    with (folder / 'lifecycle.json').open('w') as stream:
        stream.write(json.dumps({key: value for key, value in result.items() if key not in ('stdout', 'stderr')},
                                indent=2, allow_nan=False))
        stream.flush()
        os.fsync(stream.fileno())
    if pending is not None and not isinstance(pending, Exception):
        raise pending.with_traceback(pending.__traceback__)
    return result


def engine_command(engine, recipes, arena, case, prepared, row, out, products_root=None):
    wasm, args = case['wasm'], case.get('args', [])
    env = dict(os.environ)
    if engine in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        binary = (Path(products_root) if products_root is not None else arena / 'products') / engine / 'uwvm'
        return argv_for(binary, engine, case), env, str(binary)
    if engine == 'wasmtime':
        item = recipes[engine]
        return [item['binary'], *[wasm if t == 'INPUT.wasm' else t for t in item['run']], *args], env, item['binary']
    if engine == 'wavm':
        item = recipes[engine]
        env.update(item.get('environment', {}))
        return [item['binary'], *[wasm if t == 'INPUT.wasm' else t for t in item['run']], *args], env, item['binary']
    item = recipes['wasmedge']
    mode = engine.removeprefix('wasmedge-')
    if mode == 'aot':
        key = case['name']
        derivative = out / 'aot' / (key + '.so')
        derivative.parent.mkdir(parents=True, exist_ok=True)
        if key not in prepared:
            compiler_hash = digest(item['binary'])
            argv = [item['binary'], *[wasm if t == 'INPUT.wasm' else str(derivative) if t == 'OUTPUT.so' else t for t in item['compile']]]
            compilation = invoke(argv, env, out / 'aot-build' / key)
            row['aot_compilation'] = compilation
            if not compilation['retired']:
                raise OwnedRetirementError(compilation)
            assert compilation['exit'] == 0 and not compilation['error'], compilation['stderr'][-4000:]
            assert digest(item['binary']) == compiler_hash, 'AOT compiler changed during preparation'
            prepared[key] = {**compilation, 'original_wasm_sha256': digest(wasm),
                             'native_sha256': digest(derivative), 'native_path': str(derivative),
                             'compiler_sha256': compiler_hash,
                             'scope': 'offline AOT build; excluded from run/process/kernel samples'}
        row['aot'] = prepared[key]
        assert digest(wasm) == prepared[key]['original_wasm_sha256'] == case['wasm_sha256']
        assert digest(derivative) == prepared[key]['native_sha256'], 'AOT derivative changed before run'
        assert digest(item['binary']) == prepared[key]['compiler_sha256'], 'AOT compiler/runtime identity changed'
        argv = [item['binary'], *[str(derivative) if t == 'OUTPUT.so' else t for t in item['aot_run']], *args]
    else:
        argv = [item['binary'], *[wasm if t == 'INPUT.wasm' else t for t in item[mode + '_run']], *args]
    return argv, env, item['binary']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['arena', 'cases', 'recipes', 'out']:
        p.add_argument('--' + key, required=True, type=Path)
    p.add_argument('--products-root', type=Path,
                   help='explicit immutable UWVM product directory; defaults to arena/products')
    p.add_argument('--engine', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros', 'wasmtime', 'wavm',
                                                       'wasmedge-jit', 'wasmedge-interpreter', 'wasmedge-aot'])
    p.add_argument('--case', action='append')
    p.add_argument('--purpose', default='ordinary-long')
    p.add_argument('--samples', type=int, default=5)
    p.add_argument('--cpu', type=int, default=21)
    a = p.parse_args()
    before = admission()
    assert a.cpu in os.sched_getaffinity(0) and 1 <= a.samples <= 15
    os.sched_setaffinity(0, {a.cpu})
    os.environ.update(TERM='dumb', NO_COLOR='1')
    lock = os.open(BENCHMARK_LOCK, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    a.out.mkdir(parents=True, exist_ok=False)
    recipes = json.loads(a.recipes.read_text())
    cases = select_cases(json.loads(a.cases.read_text()), a.case, a.purpose)
    assert all('gate_ready' not in case and 'gate_done' not in case for case in cases), 'ordinary comparison requires independent ungated benchmark artifacts'
    engines = a.engine or ['uwvm2', 'uwvm2-stable', 'uwvm2-ros', 'wasmtime', 'wavm', 'wasmedge-jit', 'wasmedge-aot']
    assert len(engines) == len(set(engines)), 'duplicate requested engines'
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    helpers = [Path(__file__), Path(__file__).with_name('run_matrix.py'),
               Path(__file__).with_name('owned_process.py'), Path(__file__).with_name('build_products.py')]
    helper_hashes = {str(path): digest(path) for path in helpers}
    report = {'passed': False, 'admission_before': before, 'boot': boot, 'cpu': a.cpu,
              'products_root': str(a.products_root if a.products_root is not None else a.arena / 'products'),
              'cases_sha256': digest(a.cases),
              'recipes_sha256': digest(a.recipes), 'script_sha256': digest(__file__), 'samples_per_pair': a.samples,
              'helpers_sha256': helper_hashes, 'warmup_rounds': 1, 'shuffle_seed_base': 1729, 'execution_order': [],
              'kernel_scope': 'guest timer around workload only; clock source and units retained per case',
              'process_scope': 'cold process launch, compilation/load, guest initialization, workload and leader exit; authenticated helper retirement recorded separately', 'pairs': []}
    prepared, pairs, binary_hashes = {}, {}, {}
    def check_inputs():
        assert digest(a.cases) == report['cases_sha256'] and digest(a.recipes) == report['recipes_sha256'], 'benchmark manifest/recipes changed'
        assert all(digest(path) == checksum for path, checksum in helper_hashes.items()), 'benchmark helper changed'
    def save():
        path = a.out / 'report.json'
        temporary = path.with_suffix('.json.tmp')
        with temporary.open('w') as stream:
            stream.write(json.dumps(report, indent=2, allow_nan=False))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(a.out, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    for case in cases:
        assert digest(case['wasm']) == case['wasm_sha256']
        for engine in engines:
            row = {'case': case['name'], 'engine': engine, 'wasm_sha256': digest(case['wasm']),
                   'kernel_clock': case.get('kernel_clock', 'guest-reported'), 'kernel_time': case['kernel_time'],
                   'samples': [], 'failures': [], 'qualified': False}
            report['pairs'].append(row)
            pairs[case['name'], engine] = row
    schedule = [(case, engine) for case in cases for engine in engines]
    save()
    # Independent warmup first, then shuffled paired rounds; retain every sample.
    for iteration in range(a.samples + 1):
        random.Random(1729 + iteration).shuffle(schedule)
        for case, engine in schedule:
            row = pairs[case['name'], engine]
            if iteration and not row['qualified']:
                continue
            report['execution_order'].append({'iteration': iteration, 'case': case['name'], 'engine': engine,
                                               'scope': 'warmup' if iteration == 0 else 'measured'})
            print(json.dumps({'iteration': iteration, 'case': case['name'], 'engine': engine, 'state': 'started'}), flush=True)
            result, retirement_failure = None, None
            try:
                check_inputs()
                assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == boot, 'boot changed'
                argv, env, binary = engine_command(engine, recipes, a.arena, case, prepared, row, a.out, a.products_root)
                binary_hash = digest(binary)
                if engine not in binary_hashes:
                    binary_hashes[engine] = binary_hash
                    if engine.startswith('uwvm2'):
                        assert json.loads((Path(binary).parent / 'identity.json').read_text())['binary_sha256'] == binary_hash
                assert binary_hash == binary_hashes[engine], 'engine binary changed between samples'
                result = invoke(argv, env, a.out / 'runs' / case['name'] / engine / str(iteration))
                if not result['retired']:
                    raise OwnedRetirementError(result)
                result['binary_sha256'] = binary_hash
                check_inputs()
                assert digest(binary) == binary_hash and digest(case['wasm']) == case['wasm_sha256'], 'input changed during run'
                if engine == 'wasmedge-aot':
                    native_hash = digest(row['aot']['native_path'])
                    result['aot_native_sha256_after'] = native_hash
                    result['aot_native_sha256_before'] = row['aot']['native_sha256']
                    assert native_hash == row['aot']['native_sha256'], 'AOT derivative changed during run'
                assert result['exit'] == 0 and not result['error'], (result['error'], result['stderr'][-4000:])
                result['kernel_seconds'] = kernel_seconds(result['stdout'], case)
                validate_kernel_seconds(result['kernel_seconds'], result['process_wall_seconds'])
                validate_stable_stdout(result['stdout'], case)
                if iteration == 0:
                    row.update(qualified=True, warmup=result)
                else:
                    row['samples'].append(result)
            except Exception as error:
                if isinstance(error, OwnedRetirementError):
                    result, retirement_failure = error.result, error
                row['failure'] = repr(error)
                row['failure_class'] = 'zero_timer' if 'zero_timer:' in str(error) else 'invalid_or_failed_execution'
                row['failures'].append({'iteration': iteration, 'error': repr(error), 'result': result})
                row['qualified'] = False
            save()
            if retirement_failure is not None:
                # Do not contaminate later pairs with an unretired process.
                report['error'] = repr(retirement_failure)
                save()
                raise retirement_failure
    for row in report['pairs']:
        if row['qualified'] and len(row['samples']) == a.samples:
            row['metrics'] = {key: {'median': statistics.median(sample[key] for sample in row['samples']),
                                    'minimum': min(sample[key] for sample in row['samples']),
                                    'maximum': max(sample[key] for sample in row['samples'])}
                              for key in ['kernel_seconds', 'process_wall_seconds', 'process_user_seconds', 'process_system_seconds']}
    try:
        report['admission_after'] = admission()
        check_admission(before, report['admission_after'])
        assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == boot
        report['cases_sha256_after'] = digest(a.cases)
        report['recipes_sha256_after'] = digest(a.recipes)
        report['helpers_sha256_after'] = {str(path): digest(path) for path in helpers}
        check_inputs()
        report['passed'] = all(row['qualified'] and 'metrics' in row for row in report['pairs'])
        # Passed is a durable receipt: flush raw logs/derivatives before it.
        for path in a.out.rglob('*'):
            if path.is_file() and not path.is_symlink():
                with path.open('rb') as stream:
                    os.fsync(stream.fileno())
        for path in sorted((path for path in a.out.rglob('*') if path.is_dir()), key=lambda value: len(value.parts), reverse=True):
            fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
            try:
                os.fsync(fd)
            finally:
                os.close(fd)
        report['artifacts_synced_before_receipt'] = True
    except BaseException as error:
        report['passed'] = False
        report['error'] = repr(error)
        raise
    finally:
        save()
        os.close(lock)


if __name__ == '__main__':
    main()
