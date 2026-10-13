#!/usr/bin/env python3
"""Independent per-guest hardware counting; no sampling, SW events or ranking.

The frozen long runner is read-only. This runner owns two direct children:
a stopped P0 guest and an E16 perf-stat attachment. An actual control ACK
enables the fixed hardware group before the guest is released. All counts
cover the whole guest process, including bootstrap/initialization/JIT work;
they are never labeled Wasm-only ROI counts.
"""
import argparse, decimal, hashlib, json, os, pathlib, re, resource, select
import signal, statistics, subprocess, threading, time, types

P = pathlib.Path
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
AUX_SHA = '59fcb19664f87fa3447a6c9cb8a117885f42303e129de6d2d3756cf8a953b4f2'
EVENTS = ('cpu_core/cycles/', 'cpu_core/instructions/')
EVENT_GROUP = '{' + ','.join(EVENTS) + '}'
D = decimal.Decimal

def fixed_module(name, filename, sha):
    path = P(__file__).with_name(filename)
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != sha:
        raise RuntimeError('Changed immutable dependency: ' + filename)
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(data, str(path), 'exec'), module.__dict__)
    if path.read_bytes() != data:
        raise RuntimeError('Dependency changed while loading: ' + filename)
    return module, path

def parse_stat(text, debug_text=''):
    """Cross-check actual JSON with -vv exact raw count/enabled/running lines.

    JSON pcnt-running is rounded to two decimals. Never reverse it into an
    exact time_enabled value. An unsupported raw format remains unqualified.
    This parser does not interpret synthesized tests as Linux receipts.
    """
    objects = []
    raw = {name: [] for name in EVENTS}
    errors = []
    for source, content in (('stat_output', text), ('perf_stderr', debug_text)):
      for line in content.splitlines():
        stripped = line.strip()
        if source == 'stat_output' and stripped.startswith('{'):
            try:
                value = json.loads(stripped, parse_float=D)
                if not isinstance(value, dict):
                    raise ValueError('not an object')
                objects.append(value)
            except (ValueError, decimal.InvalidOperation) as failure:
                errors.append('Invalid actual perf JSON: ' + str(failure))
        match = re.fullmatch(r'(cpu_core/(?:cycles|instructions)/):\s*(-?\d+):\s*(\d+)\s+(\d+)\s+(\d+)', stripped)
        if match:
            name, cpu, count, enabled, running = match.groups()
            value = {'cpu': int(cpu), 'count': int(count),
                     'time_enabled_ns': int(enabled), 'time_running_ns': int(running)}
            previous = next((row for row in raw[name] if row['cpu'] == value['cpu']), None)
            if previous is None:
                raw[name].append(dict(value, actual_log_sources=[source]))
            elif any(previous[k] != value[k] for k in value):
                errors.append('Conflicting repeated actual raw event/CPU: ' + name)
            elif source in previous['actual_log_sources']:
                errors.append('Duplicate actual raw event/CPU in one log: ' + name)
            else:
                # Same record mirrored across output streams is not two counts.
                previous['actual_log_sources'].append(source)
    result = {'event_group': EVENT_GROUP, 'scaled': False, 'sampling': False,
              'scope': 'whole owned guest single TID, user+kernel, no inheritance',
              'events': {}, 'failures': errors, 'counter_quality_passed': False}
    for name in EVENTS:
        rows = [row for row in objects if row.get('event') == name]
        if len(rows) != 1:
            errors.append('Expected exactly one JSON hardware event: ' + name)
            continue
        row = rows[0]
        try:
            count = D(str(row['counter-value']))
            runtime = D(str(row['event-runtime']))
            percentage = D(str(row['pcnt-running']))
            if (not all(x.is_finite() for x in (count, runtime, percentage)) or
                    count <= 0 or count != count.to_integral_value() or
                    runtime <= 0 or runtime != runtime.to_integral_value() or
                    not D(0) < percentage <= D(100)):
                raise ValueError('nonpositive/nonintegral/unavailable counter')
        except (KeyError, ValueError, decimal.InvalidOperation) as failure:
            errors.append(name + ': ' + str(failure))
            continue
        actual = {'raw_counter_value': int(count), 'json_time_running_ns': int(runtime),
                  'json_pcnt_running': str(percentage), 'raw_per_cpu': raw[name],
                  'time_enabled_ns': None, 'time_running_ns': None}
        result['events'][name] = actual
        if not raw[name]:
            errors.append('No exact -vv enabled/running evidence: ' + name)
            continue
        if any(r['count'] > 0 and r['cpu'] not in (-1, 0) for r in raw[name]):
            errors.append('Non-P0 positive raw hardware count: ' + name)
        if any(r['time_enabled_ns'] <= 0 or not 0 <= r['time_running_ns'] <= r['time_enabled_ns'] for r in raw[name]):
            errors.append('Invalid exact enabled/running evidence: ' + name)
            continue
        total_count = sum(r['count'] for r in raw[name])
        enabled = sum(r['time_enabled_ns'] for r in raw[name])
        running = sum(r['time_running_ns'] for r in raw[name])
        actual.update(time_enabled_ns=enabled, time_running_ns=running)
        exact = D(running) * 100 / D(enabled)
        actual['exact_pcnt_running'] = str(exact)
        if total_count != int(count) or running != int(runtime):
            errors.append('JSON/raw count or time_running mismatch: ' + name)
        if abs(exact - percentage) > D('0.011'):
            errors.append('JSON rounded running percentage mismatch: ' + name)
        if exact < D(99):
            errors.append('Hardware running/enabled below99%: ' + name)
    unexpected = [row['event'] for row in objects if 'event' in row and row['event'] not in EVENTS]
    if unexpected:
        errors.append('Unexpected event (no software/other PMU fallback): ' + repr(unexpected))
    if len(result['events']) == 2:
        a, b = (result['events'][name] for name in EVENTS)
        if a['time_enabled_ns'] is not None and b['time_enabled_ns'] is not None:
            if (a['time_enabled_ns'], a['time_running_ns']) != (b['time_enabled_ns'], b['time_running_ns']):
                errors.append('Grouped events do not share the same exact enabled/running interval')
            result['ipc_whole_guest'] = str(D(b['raw_counter_value']) / D(a['raw_counter_value']))
    result['counter_quality_passed'] = not errors
    return result

class CountingGuard:
    """Independent roster for two direct owned children; old Guard is unchanged."""
    def __init__(self, base):
        self.b = base
        original = base.Guard()
        self.cg, self.self, self.init, self.events = original.cg, original.self, original.init, original.events
        self.owned = []

    def check(self):
        b = self.b
        b.require(P('/sys/fs/cgroup/memory.max').read_text().strip() == str(64 << 30), '64GiB guard changed')
        b.require(P('/sys/fs/cgroup/memory.swap.max').read_text().strip() == '0', 'swap guard changed')
        b.require(P('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip() == '0,2,4,6,16-31', 'CPU admission changed')
        roster = set(map(int, P('/sys/fs/cgroup/cgroup.procs').read_text().split()))
        allowed = {1, os.getpid()} | {entry['process'].pid for entry in self.owned if not entry['reaped']}
        b.require(roster <= allowed, 'Unowned concurrent cgroup process: ' + str(roster - allowed))
        for original in (self.self, self.init):
            current = b.ident(original['pid'])
            b.require(all(current[k] == original[k] for k in ('birth', 'ppid', 'uid', 'cgroup', 'argv')), 'Baseline identity changed')
        now = b.counts('/sys/fs/cgroup/memory.events')
        b.require(all(now[k] == self.events[k] for k in ('oom', 'oom_kill')), 'Cgroup OOM changed')
        b.require(int(P('/sys/fs/cgroup/memory.current').read_text()) < 56 << 30, 'Cgroup headroom')
        fs = os.statvfs('/work')
        b.require(fs.f_bavail * fs.f_frsize >= 1 << 30, 'Diagnostic disk floor1GiB')
        available = next(line.split()[1] for line in P('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:'))
        b.require(int(available) * 1024 >= 8 << 30, 'Physical memory headroom8GiB')
        for entry in self.owned:
            if entry['reaped']:
                continue
            pid = entry['process'].pid
            try:
                row = b.ident(pid)
            except FileNotFoundError:
                # wait4 and the monitor run concurrently. The still-open
                # original PIDFD proves retirement without trusting PID reuse.
                b.require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing owned process is not proved retired')
                continue
            b.require(row['birth'] == entry['birth'] and row['ppid'] == os.getpid() and row['pgid'] == pid, 'Owned birth/ancestry/PGID changed')
            b.require(pid in roster or row['state'] == 'Z', 'Live owned process escaped roster')
            b.require(row['uid'] == [1000] * 4 and row['cgroup'] == self.cg, 'Owned UID/cgroup changed')
            b.require(row['rss'] <= 6 << 30, 'Owned child RSS above6GiB')
            if row['state'] == 'Z':
                continue
            allowed_argv = [entry['exec_argv']] if entry['actual_exec'] is not None else entry['allowed_argv']
            b.require(row['argv'] in allowed_argv, 'Owned child command changed')
            if row['argv'] == entry['exec_argv']:
                try:
                    actual_exe = (P('/proc') / str(pid) / 'exe').resolve(strict=True)
                except FileNotFoundError:
                    b.require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing owned executable is not proved retired')
                    continue
                b.require(actual_exe == entry['expected_exe'], 'Owned child executable changed')
                if entry['actual_exec'] is None:
                    entry['actual_exec'] = dict(b.json_ident(row), executable=str(actual_exe), time_ns=time.time_ns())
            try:
                tasks = list((P('/proc') / str(pid) / 'task').iterdir())
            except FileNotFoundError:
                b.require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing owned tasks are not proved retired')
                continue
            if entry['role'] == 'guest':
                b.require(len(tasks) == 1, 'Hardware count fixture unexpectedly has multiple guest TIDs')
            for task in tasks:
                try:
                    values = dict(line.split(':', 1) for line in (task / 'status').read_text().splitlines() if ':' in line)
                except FileNotFoundError:
                    b.require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Live owned task disappeared')
                    continue
                b.require(values['Cpus_allowed_list'].strip() == entry['cpu'], 'Owned TID escaped fixed CPU')
                b.require(list(map(int, values['Uid'].split())) == [1000] * 4, 'Owned TID UID changed')

def signal_owned(entry, sig):
    if not entry['reaped'] and entry['pidfd'] is not None:
        try:
            signal.pidfd_send_signal(entry['pidfd'], sig)
        except ProcessLookupError:
            pass

def spawn_stopped(base, guard, role, argv, stream, inherited=()):
    readfd, writefd = os.pipe()
    entry = {'role': role, 'cpu': '0' if role == 'guest' else '16', 'process': None,
             'pidfd': None, 'birth': None, 'reaped': False, 'admitted': None, 'actual_exec': None}
    try:
        boot = base.BOOT if role == 'guest' else base.BOOT.replace('{0}', '{16}')
        bootstrap_argv = ['python3', '-c', boot, str(readfd), *argv]
        exec_argv = argv[3:] if role == 'guest' else argv
        if role == 'guest':
            base.require(argv[:3] == ['taskset', '-c', '0'], 'Expected exact P0 guest prefix')
        entry['exec_argv'] = [os.fsencode(arg) for arg in exec_argv]
        entry['expected_exe'] = P(exec_argv[0]).resolve(strict=True)
        entry['allowed_argv'] = [[os.fsencode(arg) for arg in args] for args in (bootstrap_argv, argv, exec_argv)]
        process = subprocess.Popen(bootstrap_argv,
                                   stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=True, pass_fds=(readfd, *inherited))
        entry['process'] = process
        os.close(readfd)
        readfd = None
        # EOF is the only fallback if acquiring the exact PIDFD fails.
        entry['pidfd'] = os.pidfd_open(process.pid)
        first = base.ident(process.pid)
        entry['birth'] = first['birth']
        os.write(writefd, b'x')
        os.close(writefd)
        writefd = None
        end = time.monotonic() + 5
        while first['state'] != 'T':
            base.require(time.monotonic() < end, 'Owned bootstrap failed to stop')
            time.sleep(.002)
            first = base.ident(process.pid)
        guard.owned.append(entry)
        guard.check()
        entry['admitted'] = base.json_ident(first)
        return entry
    except BaseException:
        # A second interrupt cannot bypass the only EOF/PIDFD/reap cleanup.
        oldmask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
        try:
            for fd in (readfd, writefd):
                if fd is not None:
                    os.close(fd)
            signal_owned(entry, signal.SIGKILL)
            if entry['process'] is not None:
                _, status, _ = os.wait4(entry['process'].pid, 0)
                entry['process'].returncode = os.waitstatus_to_exitcode(status)
                entry['reaped'] = True
            if entry['pidfd'] is not None:
                os.close(entry['pidfd'])
                entry['pidfd'] = None
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, oldmask)
        raise

def measure(base, aux, guard, item, perf, out):
    guard.check()
    before = base.telemetry()
    label = item['fixture'] + '-' + item['profile'].replace('/', '-')
    guest_log, perf_log, stats_log = (out / (label + suffix) for suffix in ('.guest.log', '.perf.log', '.stat.txt'))
    guest = profiler = None
    stop = threading.Event()
    watcher = None
    watcher_started = False
    points, errors = [], []
    ctlread, ctlwrite = os.pipe()
    ackread, ackwrite = os.pipe()
    argv = base.effective_argv(item)
    perf_argv = None
    acknowledged = released = retired = elapsed = None
    actual_ack = None
    guest_usage = None
    cancel = None
    try:
        with guest_log.open('wb') as gs, perf_log.open('wb') as ps:
            guest = spawn_stopped(base, guard, 'guest', argv, gs)
            perf_argv = [str(perf), 'stat', '-j', '--no-scale', '--no-big-num', '-vv',
                         '--no-inherit', '-p', str(guest['process'].pid), '--delay=-1',
                         '--control=fd:' + str(ctlread) + ',' + str(ackwrite),
                         '-e', EVENT_GROUP, '-o', str(stats_log)]
            profiler = spawn_stopped(base, guard, 'perf', perf_argv, ps, (ctlread, ackwrite))
            os.close(ctlread)
            ctlread = None
            os.close(ackwrite)
            ackwrite = None
            window_start = time.perf_counter_ns()

            def monitor():
                try:
                    while not stop.wait(.02):
                        guard.check()
                        points.append(base.telemetry())
                        for log in (guest_log, perf_log, stats_log):
                            base.require(not log.exists() or log.stat().st_size <= 4 << 20, 'Output exceeds4MiB')
                        base.require(time.perf_counter_ns() - window_start <= 90_000_000_000, '90s hardware window timeout')
                except BaseException as failure:
                    errors.append(type(failure).__name__ + ': ' + str(failure))
                    signal_owned(guest, signal.SIGKILL)
                    signal_owned(profiler, signal.SIGKILL)

            watcher = threading.Thread(target=monitor)
            oldmask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
            try:
                watcher.start()
                watcher_started = True
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, oldmask)
            signal_owned(profiler, signal.SIGCONT)
            os.write(ctlwrite, b'enable\n')
            ack = b''
            deadline = time.monotonic() + 5
            while len(ack) < 4:
                remaining = deadline - time.monotonic()
                base.require(remaining > 0, 'No actual perf enable ACK; guest remains stopped')
                ready, _, _ = select.select([ackread], [], [], remaining)
                base.require(ready, 'No actual perf enable ACK; guest remains stopped')
                chunk = os.read(ackread, 4 - len(ack))
                base.require(chunk, 'Perf retired without enable ACK; guest remains stopped')
                ack += chunk
            base.require(ack == b'ack\n', 'Unexpected actual perf enable ACK; guest remains stopped')
            actual_ack = os.fsdecode(ack)
            acknowledged = time.perf_counter_ns()
            guard.check()
            base.require(profiler['actual_exec'] is not None, 'Missing actual perf executable/argv receipt')
            released = time.perf_counter_ns()
            signal_owned(guest, signal.SIGCONT)
            _, status, guest_usage = os.wait4(guest['process'].pid, 0)
            retired = time.perf_counter_ns()
            elapsed = retired - released
            guest['process'].returncode = os.waitstatus_to_exitcode(status)
            guest['reaped'] = True
            # Stop only this authenticated direct perf child, never its PID number.
            signal_owned(profiler, signal.SIGINT)
            _, status, _ = os.wait4(profiler['process'].pid, 0)
            profiler['process'].returncode = os.waitstatus_to_exitcode(status)
            profiler['reaped'] = True
    except BaseException as failure:
        errors.append(type(failure).__name__ + ': ' + str(failure))
        if isinstance(failure, (KeyboardInterrupt, SystemExit)):
            cancel = failure
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            signal.signal(signal.SIGINT, signal.SIG_IGN)
    finally:
        for fd in (ctlread, ctlwrite, ackread, ackwrite):
            if fd is not None:
                os.close(fd)
        stop.set()
        try:
            if watcher_started:
                watcher.join()
        except BaseException as failure:
            errors.append('Watcher retirement: ' + str(failure))
        finally:
            for entry in (guest, profiler):
                if entry is None:
                    continue
                if not entry['reaped']:
                    signal_owned(entry, signal.SIGKILL)
                    _, status, _ = os.wait4(entry['process'].pid, 0)
                    entry['process'].returncode = os.waitstatus_to_exitcode(status)
                    entry['reaped'] = True
                if entry['pidfd'] is not None:
                    os.close(entry['pidfd'])
                    entry['pidfd'] = None
    try:
        after = base.telemetry()
        guard.check()
    except BaseException as failure:
        errors.append('Post-retirement guard/telemetry: ' + str(failure))
        after = dict(before, post_telemetry_failed=str(failure))
    exit_code = guest['process'].returncode if guest else None
    semantic = (base.semantic_receipt if item['fixture'] in base.LONG_FIXTURES else aux.semantic_receipt)(item, guest_log, exit_code)
    if guest is not None and guest['actual_exec'] is None:
        errors.append('Missing actual guest executable/argv receipt')
    counters = parse_stat(stats_log.read_text(errors='replace'), perf_log.read_text(errors='replace')) if stats_log.exists() else {'counter_quality_passed': False, 'failures': ['No actual perf statistics']}
    if perf_log.exists() and re.search(r'(?:disabl\w* grouping|not matched|grouping (?:is )?not supported|broken group)', perf_log.read_text(errors='replace'), re.I):
        counters['failures'].append('Actual perf reported an event-group fallback')
        counters['counter_quality_passed'] = False
    quality = counters['failures'][:]
    if elapsed is None or elapsed < 100_000_000:
        quality.append('Sub100ms or incomplete whole-process sample')
    if not points:
        quality.append('Missing in-window frequency telemetry')
    if any(before['cpu_stat'].get(k) != after['cpu_stat'].get(k) for k in ('nr_throttled', 'throttled_usec')):
        quality.append('CPU throttling changed')
    row = {'label': label, 'fixture': item['fixture'], 'profile': item['profile'],
           'guest_argv': argv, 'base_argv': item['argv'], 'perf_argv': perf_argv,
           'guest_admission': guest['admitted'] if guest else None,
           'perf_admission': profiler['admitted'] if profiler else None,
           'guest_actual_exec': guest['actual_exec'] if guest else None,
           'perf_actual_exec': profiler['actual_exec'] if profiler else None,
           'perf_control_command': 'enable\n', 'perf_actual_control_ack': actual_ack,
           'perf_enable_ack_ns': acknowledged, 'guest_release_ns': released,
           'guest_retired_ns': retired, 'enable_ack_to_guest_retired_wall_ns': retired - acknowledged if retired and acknowledged else None,
           'guest_wall_ns': elapsed, 'guest_exit': exit_code,
           'perf_exit_after_owned_SIGINT': profiler['process'].returncode if profiler else None,
           'user_seconds': guest_usage.ru_utime if guest_usage else None,
           'system_seconds': guest_usage.ru_stime if guest_usage else None,
           'maxrss_kib': guest_usage.ru_maxrss if guest_usage else None,
           'before': before, 'during': points, 'after': after,
           'temperature_policy': 'observation_only',
           'median_run_frequency_khz': statistics.median(p['scaling_cur_freq'] for p in points) if points else None,
           'semantic_receipt': semantic, 'guest_execution_ns': semantic.get('guest_execution_ns'),
           'hardware_counting': counters, 'hard_failures': errors,
           'quality_failures': quality, 'formal_acceptance': False,
           'log_sha256': {log.name: base.digest(log) for log in (guest_log, perf_log, stats_log) if log.exists()}}
    with (out / 'raw.jsonl').open('a') as stream:
        stream.write(json.dumps(row, allow_nan=False) + '\n')
    if cancel is not None:
        raise cancel
    return row

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=P, required=True)
    parser.add_argument('--perf', type=P, required=True, help='Actual direct installed perf ELF; inventory first')
    parser.add_argument('--out', type=P, required=True)
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    base, base_path = fixed_module('uwvm_counting_fixed_guard', 'run_current_pcore_diagnostic.py', BASE_SHA)
    aux, aux_path = fixed_module('uwvm_counting_fixed_aux', 'run_current_pcore_aux_diagnostic.py', AUX_SHA)
    plan_sha, runner_sha = base.digest(args.plan), base.digest(__file__)
    plan = json.loads(args.plan.read_text())
    base.require(base.digest(args.plan) == plan_sha, 'Plan changed while loading')
    base.require(plan['schema'] == 'uwvm-current-pcore-gc-eh-plan-v1', 'Wrong immutable-plan schema')
    desired = {(product + '/unwind/auto', 'gc-allocation-ring-512000000') for product in ('ordinary', 'ros')} | {(product + '/unwind/native-unwind', 'eh_throws') for product in ('ordinary', 'ros')}
    selected = [item for item in plan['commands'] if (item['profile'], item['fixture']) in desired]
    base.require(len(selected) == 4 and {(r['profile'], r['fixture']) for r in selected} == desired, 'Expected exact four source-bound hardware commands')
    for name in ('gc-allocation-ring-512000000', 'eh_throws'):
        expected = base.LONG_FIXTURES[name][2] if name in base.LONG_FIXTURES else aux.FIXTURES[name]['sha256']
        base.require(plan['fixtures'][name]['sha256'] == expected, 'Pinned self-checking fixture mismatch')
    perf = args.perf.resolve(strict=True)
    base.require(perf.is_file() and perf.open('rb').read(4) == b'\x7fELF', 'Direct actual perf ELF required')
    perf_sha = base.digest(perf)
    base.require(not args.out.exists(), 'New evidence directory required')
    args.out.mkdir(parents=True)
    base.save(args.out / 'plan.json', plan)
    (args.out / 'runner.py').write_bytes(P(__file__).read_bytes())
    (args.out / 'guard.py').write_bytes(base_path.read_bytes())
    (args.out / 'aux.py').write_bytes(aux_path.read_bytes())
    base.save(args.out / 'counting-plan.json', {'schema': 'uwvm-per-guest-hardware-counting-plan-v1', 'plan_sha256': plan_sha,
              'perf': str(perf), 'perf_sha256': perf_sha, 'event_group': EVENT_GROUP, 'commands': selected,
              'no_inherit': True, 'sampling': False, 'scaled': False, 'formal_acceptance': False,
              'scope': 'Whole guest single TID; initialization/JIT compilation included, not Wasm-only ROI.'})
    base.require(args.execute, 'Explicit --execute required after keeper installed-version/control/event inventory')
    def cancel(signum, frame):
        raise KeyboardInterrupt('Controlled cancellation ' + str(signum))
    signal.signal(signal.SIGTERM, cancel)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    rows, error = [], None
    before_ok = after_ok = complete = False
    boot_id = P('/proc/sys/kernel/random/boot_id').read_text().strip()
    try:
        base.closure(plan, args.out, 'before')
        before_ok = True
        guard = CountingGuard(base)
        guard.check()
        for item in selected:
            row = measure(base, aux, guard, item, perf, args.out)
            rows.append(row)
            base.require(not row['hard_failures'], 'Hardware ownership/resource/deadline failure')
            base.require(row['semantic_receipt']['passed'], 'Guest correctness/retirement failure')
            base.require(row['perf_exit_after_owned_SIGINT'] == 0, 'Actual perf attach/collection failed; preserve permission error')
        base.closure(plan, args.out, 'after')
        base.require(base.digest(args.plan) == plan_sha and base.digest(__file__) == runner_sha, 'Plan/runner changed')
        base.require(base.digest(base_path) == BASE_SHA and base.digest(aux_path) == AUX_SHA and base.digest(perf) == perf_sha, 'Guard/parser/perf changed')
        base.require(P('/proc/sys/kernel/random/boot_id').read_text().strip() == boot_id, 'Boot changed')
        after_ok = complete = True
    except BaseException as failure:
        error = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        base.save(args.out / 'summary.json', {'schema': 'uwvm-per-guest-hardware-counting-v1', 'runner_sha256': runner_sha,
                  'guard_sha256': BASE_SHA, 'aux_sha256': AUX_SHA, 'plan_sha256': plan_sha, 'perf_sha256': perf_sha,
                  'boot_id': boot_id, 'rows': len(rows), 'complete': complete, 'closure_before_ok': before_ok,
                  'closure_after_ok': after_ok, 'execution_error': error, 'event_group': EVENT_GROUP,
                  'semantic_pass_count': sum(r['semantic_receipt']['passed'] for r in rows),
                  'hardware_quality_pass_count': sum(r['hardware_counting']['counter_quality_passed'] for r in rows),
                  'formal_acceptance': False, 'temperature_policy': 'observation_only',
                  'limitations': ['Whole guest counters include initialization and JIT compilation.',
                                  'JSON percentage cannot supply exact time_enabled without actual verbose raw evidence.',
                                  'No software events, sampling, system-wide counting or engine ranking.',
                                  'Host P0/SMT activity needs independent keeper observation; never signal foreign processes.']})

if __name__ == '__main__':
    main()
