#!/usr/bin/env python3
"""Independent host-namespace hardware count, in the existing 64GiB scope.

No seccomp/capability/sysctl changes. The keeper admits this controller before
launch; this script never writes cgroup.procs. Original container protocols
and evidence stay immutable. Temperature is observed; no engine ranking.
"""
import argparse, hashlib, json, os, pathlib, re, resource, select, signal
import time, types

P = pathlib.Path
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
AUX_SHA = '59fcb19664f87fa3447a6c9cb8a117885f42303e129de6d2d3756cf8a953b4f2'
RAW_EVENTS = ('cpu_core/event=0x3c/', 'cpu_core/event=0xc0/')
RAW_GROUP = '{' + ','.join(RAW_EVENTS) + '}'
ALIAS = dict(zip(RAW_EVENTS, ('cpu_core/cycles/', 'cpu_core/instructions/')))
INTEL_MAP = 'https://raw.githubusercontent.com/intel/perfmon/main/mapfile.csv'
INTEL_EVENTS = 'https://raw.githubusercontent.com/intel/perfmon/main/ADL/events/alderlake_goldencove_core.json'

def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)

def validate_raw_inventory(admission, libraries):
    """Type/shape validation only: actual keeper inventories have no schema."""
    def sha(value):
        return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None
    def absolute(value):
        return isinstance(value, str) and P(value).is_absolute() and '..' not in P(value).parts
    require(isinstance(admission, dict) and isinstance(libraries, dict), 'Raw inventories must be objects')
    require({'boot_id', 'host_init', 'cgroup_sysfs_absolute_path', 'host_mapping', 'host_perf', 'trusted_wrapper'} <= admission.keys(), 'Missing actual admission fields')
    require({'LD_LIBRARY_PATH', 'LD_PRELOAD', 'files'} <= libraries.keys(), 'Missing actual loader closure fields')
    require(isinstance(admission['boot_id'], str) and re.fullmatch(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}', admission['boot_id']), 'Invalid actual boot identity')
    init = admission['host_init']
    require(isinstance(init, dict) and {'pid', 'birth', 'ppid', 'uid', 'argv', 'exe', 'exe_sha256', 'cgroup', 'affinity'} <= init.keys(), 'Missing actual init fields')
    require(isinstance(init, dict) and type(init['pid']) is int and init['pid'] > 1 and type(init['ppid']) is int and init['ppid'] > 0, 'Invalid actual host init identity')
    require(isinstance(init['birth'], str) and init['birth'].isdigit() and int(init['birth']) > 0, 'Invalid host init birth')
    require(isinstance(init['uid'], list) and all(type(v) is int for v in init['uid']) and init['uid'] == [1000] * 4, 'Invalid actual init UID')
    require(init['argv'] == ['sleep', 'infinity'] and absolute(init['exe']) and sha(init['exe_sha256']), 'Invalid actual init command/ELF')
    require(isinstance(init['cgroup'], str) and init['cgroup'].startswith('0::/') and init['cgroup'].count('\n') == 1 and init['cgroup'].endswith('\n'), 'Invalid actual host cgroup text')
    require(absolute(admission['cgroup_sysfs_absolute_path']), 'Invalid actual cgroup root')
    require(isinstance(init['affinity'], list) and all(type(v) is int for v in init['affinity']) and set(init['affinity']) == {0, 2, 4, 6, *range(16, 32)} and len(init['affinity']) == 20, 'Invalid actual init affinity')
    require(isinstance(admission['host_mapping'], dict) and set(admission['host_mapping']) == {'/work', '/toolchain'} and all(absolute(v) for v in admission['host_mapping'].values()), 'Invalid actual namespace mapping')
    for key in ('host_perf', 'trusted_wrapper'):
        require(isinstance(admission[key], dict) and {'path', 'sha256'} <= admission[key].keys(), 'Missing actual ' + key + ' fields')
        require(isinstance(admission[key], dict) and absolute(admission[key]['path']) and sha(admission[key]['sha256']), 'Invalid actual ' + key + ' identity')
    require(isinstance(admission['host_perf'].get('version'), str), 'Missing actual perf version inventory')
    require(isinstance(libraries['LD_LIBRARY_PATH'], str) and libraries['LD_PRELOAD'] in (None, ''), 'Invalid actual loader environment inventory')
    require(isinstance(libraries['files'], list) and len(libraries['files']) >= 3, 'Missing actual perf/DSO closure')
    paths = []
    for file in libraries['files']:
        require(isinstance(file, dict) and {'path', 'real_path', 'bytes', 'sha256'} <= file.keys(), 'Missing actual closure-file fields')
        require(isinstance(file, dict) and absolute(file['path']) and absolute(file['real_path']) and type(file['bytes']) is int and file['bytes'] > 0 and sha(file['sha256']), 'Invalid actual perf/DSO closure file')
        paths.append(file['path'])
    require(len(paths) == len(set(paths)), 'Duplicate actual closure path')

def fixed_module(name, filename, sha):
    path = P(__file__).with_name(filename)
    blob = path.read_bytes()
    require(hashlib.sha256(blob).hexdigest() == sha, 'Immutable dependency changed: ' + filename)
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(blob, str(path), 'exec'), module.__dict__)
    require(path.read_bytes() == blob, 'Immutable dependency changed while loading')
    return module, path

def host_path(value, mapping):
    """Map actual container paths, without editing historical compiler argv."""
    require(isinstance(value, str) and P(value).is_absolute(), 'Expected absolute source-bound path')
    require('..' not in P(value).parts, 'Path traversal in namespace mapping')
    matches = [(src, dst) for src, dst in mapping.items() if value == src or value.startswith(src + '/')]
    require(len(matches) == 1, 'Ambiguous/unmapped execution path: ' + value)
    src, dst = matches[0]
    require(P(src).is_absolute() and P(dst).is_absolute() and '..' not in P(dst).parts, 'Invalid host path mapping')
    return dst + value[len(src):]

def mapped_plan(plan, mapping):
    host = json.loads(json.dumps(plan))
    for pin in host['products'].values():
        for key in ('source', 'binary', 'runtime', 'build_json'):
            pin[key] = host_path(pin[key], mapping)
        for command in pin['commands'].values():
            command['path'] = host_path(command['path'], mapping)
    host['wasmtime']['path'] = host_path(host['wasmtime']['path'], mapping)
    for fixture in host['fixtures'].values():
        fixture['path'] = host_path(fixture['path'], mapping)
    for command in host['commands']:
        for key in ('argv', 'diagnostic_argv'):
            if key in command:
                command[key] = [host_path(arg, mapping) if P(arg).is_absolute() else arg for arg in command[key]]
    return host

def validate_init(expected, actual, actual_exe):
    require(actual['pid'] == expected['pid'] and actual['birth'] == int(expected['birth']), 'Host init PID/birth changed')
    require(actual['ppid'] == expected['ppid'], 'Host init parent changed')
    require(actual['uid'] == expected['uid'] == [1000] * 4, 'Host init UID changed')
    require(actual['argv'] == [os.fsencode(arg) for arg in expected['argv']] == [b'sleep', b'infinity'], 'Host init command changed')
    require(actual['cgroup'] == expected['cgroup'], 'Host init cgroup changed')
    require(str(actual_exe) == expected['exe'], 'Host init executable changed')

def loaded_executable_identity(proc_exe, digest):
    # /proc/PID/exe link text belongs to the process's mount namespace. Do not
    # resolve that text through the controller's possibly different root.
    # Opening proc_exe itself reads the ELF actually loaded by the original PID.
    return {'link_text': os.readlink(proc_exe), 'sha256': digest(proc_exe),
            'byte_source': str(proc_exe)}

def validate_scope(snapshot, init_pid, self_pid, live_owned):
    require(snapshot['memory.max'] == str(64 << 30), '64GiB guard changed')
    require(snapshot['memory.swap.max'] == '0', 'Swap-zero guard changed')
    require(snapshot['cpuset.cpus.effective'] == '0,2,4,6,16-31', 'CPU admission changed')
    allowed = {init_pid, self_pid} | set(live_owned)
    roster = set(map(int, snapshot['cgroup.procs'].split()))
    require({init_pid, self_pid} <= roster <= allowed, 'Missing baseline/unowned concurrent host cgroup process')

class HostBase:
    """Explicit telemetry adapter; never calls or mocks the container Guard."""
    def __init__(self, base, cg):
        self.base, self.cg = base, cg
    def __getattr__(self, name):
        return getattr(self.base, name)
    def telemetry(self):
        temps = {}
        for zone in P('/sys/class/thermal').glob('thermal_zone*'):
            try:
                temps[zone.name + ':' + (zone / 'type').read_text().strip()] = int((zone / 'temp').read_text())
            except (FileNotFoundError, PermissionError, ValueError):
                continue
        sensor = [v for k, v in temps.items() if k.split(':', 1)[1] == 'x86_pkg_temp' or k.split(':', 1)[1].startswith('TCPU')]
        require(sensor, 'No actual package/CPU temperature observation')
        return {'time_ns': time.time_ns(), 'temperatures': temps, 'cpu_temperature': max(sensor),
                'scaling_cur_freq': int(P('/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq').read_text()),
                'cpu_stat': self.base.counts(self.cg / 'cpu.stat'),
                'memory_events': self.base.counts(self.cg / 'memory.events'), 'actual_cgroup_root': str(self.cg)}

class HostGuard:
    def __init__(self, base, admission):
        self.b, self.admission = base, admission
        self.cg = admission['host_init']['cgroup']
        require(self.cg.startswith('0::/') and self.cg.count('\n') == 1 and self.cg.endswith('\n'), 'Expected exact host unified cgroup text')
        self.root = P(admission['cgroup_sysfs_absolute_path']).resolve(strict=True)
        require(self.root == P('/sys/fs/cgroup') / self.cg.strip()[4:], 'Cgroup sysfs root disagrees with namespace receipt')
        require(os.getuid() == 1000 and os.sched_getaffinity(0) == {16}, 'Host controller must be UID1000/E16')
        require(P('/proc/sys/kernel/random/boot_id').read_text().strip() == admission['boot_id'], 'Host admission boot changed')
        self.self = base.ident(os.getpid())
        require(self.self['cgroup'] == self.cg, 'Controller was not admitted to the exact target cgroup')
        self.self_exe = os.readlink('/proc/self/exe')
        self.init = base.ident(admission['host_init']['pid'])
        self.init_proc_exe = P('/proc') / str(self.init['pid']) / 'exe'
        self.init_executable = loaded_executable_identity(self.init_proc_exe, base.digest)
        validate_init(admission['host_init'], self.init, self.init_executable['link_text'])
        require(self.init_executable['sha256'] == admission['host_init']['exe_sha256'], 'Host init loaded ELF changed')
        require(os.sched_getaffinity(self.init['pid']) == set(admission['host_init']['affinity']), 'Host init affinity changed')
        self.events = base.counts(self.root / 'memory.events')
        self.work = P(admission['host_mapping']['/work']).resolve(strict=True)
        self.owned = []
        self.check()

    def check(self):
        b = self.b
        require(P('/proc/sys/kernel/random/boot_id').read_text().strip() == self.admission['boot_id'], 'Boot changed')
        snapshot = {key: (self.root / key).read_text().strip() for key in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective', 'cgroup.procs')}
        validate_scope(snapshot, self.init['pid'], os.getpid(), [r['process'].pid for r in self.owned if not r['reaped']])
        for original in (self.self, self.init):
            current = b.ident(original['pid'])
            require(all(current[k] == original[k] for k in ('birth', 'ppid', 'pgid', 'uid', 'cgroup', 'argv', 'cpus')), 'Host baseline identity changed')
            expected_exe = self.self_exe if original is self.self else self.admission['host_init']['exe']
            require(os.readlink(P('/proc') / str(original['pid']) / 'exe') == expected_exe, 'Host baseline executable changed')
        now = b.counts(self.root / 'memory.events')
        require(all(now[k] == self.events[k] for k in ('oom', 'oom_kill')), 'Target cgroup OOM changed')
        require(int((self.root / 'memory.current').read_text()) < 56 << 30, 'Target cgroup headroom')
        fs = os.statvfs(self.work)
        require(fs.f_bavail * fs.f_frsize >= 1 << 30, 'Diagnostic disk floor1GiB')
        available = next(line.split()[1] for line in P('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:'))
        require(int(available) * 1024 >= 8 << 30, 'Physical memory headroom8GiB')
        for entry in self.owned:
            if entry['reaped']:
                continue
            pid = entry['process'].pid
            try:
                row = b.ident(pid)
            except FileNotFoundError:
                require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing owned process is not proved retired')
                continue
            require(row['birth'] == entry['birth'] and row['ppid'] == os.getpid() and row['pgid'] == pid, 'Owned birth/ancestry/PGID changed')
            require(pid in set(map(int, snapshot['cgroup.procs'].split())) or row['state'] == 'Z', 'Live owned child escaped target roster')
            require(row['uid'] == [1000] * 4 and row['cgroup'] == self.cg, 'Owned UID/cgroup changed')
            require(row['rss'] <= 6 << 30, 'Owned child RSS above6GiB')
            if row['state'] == 'Z':
                continue
            allowed = [entry['exec_argv']] if entry['actual_exec'] is not None else entry['allowed_argv']
            require(row['argv'] in allowed, 'Owned command changed')
            if row['argv'] == entry['exec_argv']:
                try:
                    actual_exe = (P('/proc') / str(pid) / 'exe').resolve(strict=True)
                except FileNotFoundError:
                    require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing executable is not proved retired')
                    continue
                require(actual_exe == entry['expected_exe'], 'Owned executable changed')
                if entry['actual_exec'] is None:
                    entry['actual_exec'] = dict(b.json_ident(row), executable=str(actual_exe), time_ns=time.time_ns())
            try:
                tasks = list((P('/proc') / str(pid) / 'task').iterdir())
            except FileNotFoundError:
                require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Missing tasks are not proved retired')
                continue
            require(entry['role'] != 'guest' or len(tasks) == 1, 'Counting guest unexpectedly has multiple TIDs')
            for task in tasks:
                try:
                    fields = dict(line.split(':', 1) for line in (task / 'status').read_text().splitlines() if ':' in line)
                except FileNotFoundError:
                    require(bool(select.select([entry['pidfd']], [], [], 0)[0]), 'Live owned task disappeared')
                    continue
                require(fields['Cpus_allowed_list'].strip() == entry['cpu'] and list(map(int, fields['Uid'].split())) == [1000] * 4, 'Owned TID escaped CPU/UID')

def host_source_id(source, output):
    # Same repository-relative canonical byte algorithm as the fixed source
    # fingerprint tool; run in this controller, without an extra child.
    entries = []
    for directory in ('src', 'third-parties'):
        root = (P(source) / directory).resolve(strict=True)
        require(root.is_dir(), 'Missing source/dependency tree')
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._'):
                entries.append({'path': (P(directory) / path.relative_to(root)).as_posix(), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    require(entries, 'Empty source fingerprint')
    source_id = 'sha256:' + hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    output.write_text(json.dumps({'source_id': source_id, 'files': entries}, indent=2) + '\n')
    return source_id

def host_closure(base, plan, libraries, out, stage):
    for product, pin in plan['products'].items():
        for key, sha in (('binary', 'binary_sha256'), ('runtime', 'runtime_sha256'), ('build_json', 'build_json_sha256')):
            require(base.digest(pin[key]) == pin[sha], product + ' ' + key + ' changed')
        for command in pin['commands'].values():
            require(base.digest(command['path']) == command['sha256'], 'Compiler sidecar changed')
        require(host_source_id(pin['source'], out / (product + '-source-' + stage + '.json')) == pin['source_id'], 'Current source differs from built bytes')
    require(base.digest(plan['wasmtime']['path']) == plan['wasmtime']['sha256'], 'Reference ELF changed')
    for fixture in plan['fixtures'].values():
        require(base.digest(fixture['path']) == fixture['sha256'], 'Pinned fixture changed')
    require(os.environ.get('LD_LIBRARY_PATH', '') == libraries['LD_LIBRARY_PATH'] and not os.environ.get('LD_PRELOAD') and not os.environ.get('LD_AUDIT'), 'Actual loader environment disagrees with closure')
    for file in libraries['files']:
        path = P(file['path'])
        require(str(path.resolve(strict=True)) == file['real_path'] and path.stat().st_size == file['bytes'] and base.digest(path) == file['sha256'], 'Actual perf/DSO closure changed: ' + str(path))

def pmu_receipt():
    root = P('/sys/bus/event_source/devices/cpu_core')
    files = {name: (root / name).read_bytes() for name in ('type', 'cpus', 'caps/pmu_name', 'format/event', 'format/umask', 'events/cpu-cycles', 'events/instructions')}
    values = {key: blob.decode().strip() for key, blob in files.items()}
    require(values['type'] == '4' and values['cpus'] == '0-15', 'Actual P0 core PMU changed')
    require(values['caps/pmu_name'] == 'alderlake_hybrid', 'Actual Intel hybrid PMU changed')
    require(values['format/event'] == 'config:0-7' and values['format/umask'] == 'config:8-15', 'Actual raw PMU format changed')
    for key, expected in (('events/cpu-cycles', 0x3c), ('events/instructions', 0xc0)):
        fields = dict(part.split('=', 1) for part in values[key].split(','))
        require(int(fields['event'], 0) == expected and int(fields.get('umask', '0'), 0) == 0 and set(fields) <= {'event', 'umask'}, 'Actual sysfs architectural event changed')
    cpu0 = dict(line.split(':', 1) for line in P('/proc/cpuinfo').read_text().split('\n\n')[0].splitlines() if ':' in line)
    cpu0 = {k.strip(): v.strip() for k, v in cpu0.items()}
    require(cpu0['processor'] == '0' and cpu0['vendor_id'] == 'GenuineIntel' and cpu0['cpu family'] == '6' and cpu0['model'] == '183', 'Intel B7 event contract requires actual matching CPU')
    return {'sysfs_root': str(root), 'actual_values': values, 'file_sha256': {key: hashlib.sha256(blob).hexdigest() for key, blob in files.items()},
            'cpu0_identity': {key: cpu0[key] for key in ('vendor_id', 'cpu family', 'model', 'stepping', 'model name')},
            'official_map': INTEL_MAP, 'official_events': INTEL_EVENTS, 'events': ['CPU_CLK_UNHALTED.THREAD_P', 'INST_RETIRED.ANY_P'],
            'raw_group': RAW_GROUP, 'umask': 0, 'counter_kind': 'programmable architectural, not fixed-counter event'}

def normalized_math(hw, stat_text, debug_text):
    # Derived aliases are used only to reuse the frozen mathematical checks.
    # Original bytes, actual argv and actual event names remain untouched.
    def derived(text):
        for raw, alias in ALIAS.items():
            text = text.replace(raw, alias)
        return text
    result = hw.parse_stat(derived(stat_text), derived(debug_text))
    actual_json_events = []
    for line in stat_text.splitlines():
        if line.strip().startswith('{'):
            try:
                actual_json_events.append(json.loads(line)['event'])
            except (ValueError, KeyError, TypeError):
                continue  # The frozen parser preserves malformed JSON failure.
    if sorted(actual_json_events) != sorted(RAW_EVENTS):
        result['failures'].append('Actual JSON names do not match the explicit raw PMU group')
        result['counter_quality_passed'] = False
    result['observed_actual_json_event_names'] = actual_json_events
    result['derived_math_event_group'] = result.pop('event_group')
    result['event_group'] = RAW_GROUP
    for raw, alias in ALIAS.items():
        if alias in result['events']:
            result['events'][alias]['actual_raw_event_name'] = raw
            result['events'][alias]['dictionary_key_is_derived_alias'] = True
    result['actual_raw_event_group'] = RAW_GROUP
    result['derived_math_alias_mapping'] = ALIAS
    result['math_quality_passed'] = result['counter_quality_passed']
    result['counter_quality_passed'] = False
    result['requires_actual_final_guest_attribute_receipt'] = True
    result['failures'].append('Awaiting actual final-guest PMU/group/no-sampling attribution')
    return result

def final_attributes(debug_text, guest_pid):
    instances = []
    for block in debug_text.split('perf_event_attr:')[1:]:
        pattern = r'^sys_perf_event_open:\s*pid\s+(\d+)\s+cpu\s+(-?\d+)\s+group_fd\s+(-?\d+)\s+flags\s+(0x[0-9a-fA-F]+)(?:[^\S\n]*=[^\S\n]*(-?\d+))?[^\S\n]*$'
        for match in re.finditer(pattern, block, re.M):
            if int(match[1]) != guest_pid:
                continue
            attribute_text = block[:match.start()]
            fields = dict(re.findall(r'^\s+([a-z_]+)\s+(0x[0-9a-fA-F]+|\d+)(?:\s|$)', attribute_text, re.M))
            instances.append({'pid': int(match[1]), 'cpu': int(match[2]), 'group_fd': int(match[3]), 'flags': match[4],
                              'actual_return_fd': int(match[5]) if match[5] is not None else None,
                              'actual_open_line': match[0], 'printed_fields': fields, 'actual_attribute_text': attribute_text})
    errors = []
    if len(instances) != 2:
        errors.append('Expected exactly two final-guest event instances; no type0/probe/fallback substitution')
    else:
        for index, row in enumerate(instances):
            fields = row['printed_fields']
            if int(fields.get('type', '-1'), 0) != 4 or int(fields.get('config', '-1'), 0) != (0x3c, 0xc0)[index]:
                errors.append('Final guest PMU type/config mismatch')
            if row['cpu'] != -1 or (row['group_fd'] != -1 if index == 0 else row['group_fd'] < 0):
                errors.append('Final guest attachment/group mismatch')
            if row['actual_return_fd'] is None or row['actual_return_fd'] < 0:
                errors.append('No actual successful final-guest open return FD')
            read_format = re.search(r'^\s+read_format\s+([^\n]+)', row['actual_attribute_text'], re.M)
            if not read_format or not all(field in read_format[1].split('|') for field in ('TOTAL_TIME_ENABLED', 'TOTAL_TIME_RUNNING', 'GROUP')):
                errors.append('Final guest grouped enabled/running read_format is not proved')
            if any(int(fields.get(key, '0'), 0) != 0 for key in ('sample_period', 'sample_freq', 'freq', 'inherit', 'inherit_thread')):
                errors.append('Unexpected sampling/inheritance attr')
            if any(int(fields.get(key, '0'), 0) != 0 for key in ('exclude_kernel', 'exclude_user', 'exclude_guest')):
                errors.append('Unexpected counting privilege/domain fallback')
        if (instances[0]['actual_return_fd'] is None or instances[1]['group_fd'] != instances[0]['actual_return_fd'] or
                instances[1]['actual_return_fd'] == instances[0]['actual_return_fd']):
            errors.append('Actual second group_fd does not prove membership in the returned leader FD')
    return {'guest_pid': guest_pid, 'instances': instances, 'failures': errors, 'passed': not errors,
            'unprinted_zero_fields': 'Absent fields stay absent in the receipt. perf stat CLI uses counting/no-inherit; nonzero sampling/domain flags reject.'}

def validate_perf_sigint_admission(entry, current, pidfd_ready, guest, guest_pidfd_ready, cg, parent):
    require(entry['role'] == 'perf' and not entry['reaped'] and entry['pidfd'] is not None and not pidfd_ready, 'Perf retired before owned SIGINT')
    require(current['pid'] == entry['process'].pid and current['birth'] == entry['birth'] and current['ppid'] == parent and current['pgid'] == current['pid'], 'Original perf identity changed before SIGINT')
    require(current['uid'] == [1000] * 4 and current['cgroup'] == cg and current['cpus'] == '16' and current['argv'] == entry['exec_argv'] and entry['actual_exec'] is not None, 'Original perf command/UID/cgroup/E16 changed before SIGINT')
    require(guest['role'] == 'guest' and guest['reaped'] and guest['process'].returncode == 0 and guest['pidfd'] is not None and guest_pidfd_ready, 'Guest retirement is not proved before SIGINT')

def submit_owned_signal(hw, base, guard, entry, sig, receipts):
    if entry['role'] != 'perf' or sig != signal.SIGINT:
        return hw.signal_owned(entry, sig)
    guard.check()
    guests = [child for child in guard.owned if child['role'] == 'guest']
    require(guests, 'No original guest before perf SIGINT')
    guest = guests[-1]
    require(entry['pidfd'] is not None and guest['pidfd'] is not None, 'Missing original PIDFD before perf SIGINT')
    current = base.ident(entry['process'].pid)
    validate_perf_sigint_admission(entry, current, bool(select.select([entry['pidfd']], [], [], 0)[0]),
                                  guest, bool(select.select([guest['pidfd']], [], [], 0)[0]), guard.cg, os.getpid())
    require(os.readlink(P('/proc') / str(current['pid']) / 'exe') == str(entry['expected_exe']), 'Perf loaded executable changed before SIGINT')
    receipt = {'signal': int(sig), 'original_perf_identity': base.json_ident(current),
               'original_perf_pidfd': entry['pidfd'], 'original_guest_pid': guest['process'].pid,
               'original_guest_birth': guest['birth'], 'guest_reaped_before_delivery': guest['reaped'],
               'guest_returncode_before_delivery': guest['process'].returncode,
               'guest_pidfd_readable_before_delivery': True, 'perf_pidfd_live_before_delivery': True,
               'pidfd_send_signal_succeeded': False, 'delivery_started_ns': time.perf_counter_ns()}
    receipts.append(receipt)
    # This is the successful syscall receipt, not merely a callback invocation.
    signal.pidfd_send_signal(entry['pidfd'], sig)
    receipt['pidfd_send_signal_succeeded'] = True
    receipt['delivery_completed_ns'] = time.perf_counter_ns()

def perf_completion(row):
    """perf stat can print final counts then re-raise our SIGINT at exit."""
    failures = []
    if row['perf_exit_after_owned_SIGINT'] not in (0, -int(signal.SIGINT)):
        failures.append('Unexpected actual perf termination status')
    if (row['guest_exit'] != 0 or not row['semantic_receipt']['passed'] or row['hard_failures'] or
            not row['hardware_counting'].get('math_quality_passed') or
            not row['hardware_counting']['final_guest_attributes']['passed']):
        failures.append('Guest/ownership/raw math/final attributes did not close')
    ack, release, retired = (row.get(key) for key in ('perf_enable_ack_ns', 'guest_release_ns', 'guest_retired_ns'))
    if row['perf_actual_control_ack'] != 'ack\n' or not all(type(v) is int for v in (ack, release, retired)) or not ack < release < retired:
        failures.append('Actual enable ACK/guest interval did not close')
    receipts = row.get('owned_perf_sigint_receipts', [])
    if len(receipts) != 1:
        failures.append('Expected exactly one successful owned perf SIGINT receipt')
    else:
        proof = receipts[0]
        perf = row['perf_actual_exec']
        guest = row['guest_admission']
        identity = proof['original_perf_identity']
        if (proof.get('signal') != int(signal.SIGINT) or not proof.get('pidfd_send_signal_succeeded') or
                not proof.get('perf_pidfd_live_before_delivery') or not proof.get('guest_pidfd_readable_before_delivery') or
                not proof.get('guest_reaped_before_delivery') or proof.get('guest_returncode_before_delivery') != 0 or
                type(proof.get('delivery_started_ns')) is not int or type(proof.get('delivery_completed_ns')) is not int or
                type(retired) is not int or not retired < proof['delivery_started_ns'] <= proof['delivery_completed_ns']):
            failures.append('Actual PIDFD SIGINT submission/guest retirement is not proved')
        if (not perf or not guest or any(identity.get(key) != perf.get(key) for key in ('pid', 'birth', 'ppid', 'pgid', 'uid', 'cgroup', 'argv', 'cpus')) or
                proof.get('original_guest_pid') != guest['pid'] or proof.get('original_guest_birth') != guest['birth']):
            failures.append('Owned SIGINT receipt does not match the original guest/perf identity')
    return {'passed': not failures, 'failures': failures,
            'status_policy': 'Only exit0 or re-raised owned SIGINT, with live original PIDFD successful submission after proved guest retirement and final counts.'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('plan', 'admission', 'perf_libraries', 'out'):
        parser.add_argument('--' + key.replace('_', '-'), type=P, required=True)
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    hw, hw_path = fixed_module('uwvm_frozen_counter_protocol', 'run_current_pcore_hw_counting.py', HW_SHA)
    base, base_path = fixed_module('uwvm_frozen_long_parser', 'run_current_pcore_diagnostic.py', BASE_SHA)
    aux, aux_path = fixed_module('uwvm_frozen_aux_parser', 'run_current_pcore_aux_diagnostic.py', AUX_SHA)
    input_shas = {str(path): base.digest(path) for path in (args.plan, args.admission, args.perf_libraries, P(__file__), hw_path, base_path, aux_path)}
    origin = json.loads(args.plan.read_text())
    admission = json.loads(args.admission.read_text())
    libraries = json.loads(args.perf_libraries.read_text())
    validate_raw_inventory(admission, libraries)
    wrapper = P(admission['trusted_wrapper']['path'])
    require(base.digest(wrapper) == admission['trusted_wrapper']['sha256'], 'Actual trusted admission wrapper changed')
    input_shas[str(wrapper)] = admission['trusted_wrapper']['sha256']
    require(origin['schema'] == 'uwvm-current-pcore-gc-eh-plan-v1', 'Expected immutable source-bound plan')
    plan = mapped_plan(origin, admission['host_mapping'])
    desired = {(product + '/unwind/auto', 'gc-allocation-ring-512000000') for product in ('ordinary', 'ros')} | {(product + '/unwind/native-unwind', 'eh_throws') for product in ('ordinary', 'ros')}
    selected = [item for item in plan['commands'] if (item['profile'], item['fixture']) in desired]
    require(len(selected) == 4 and {(r['profile'], r['fixture']) for r in selected} == desired, 'Expected four exact hardware commands')
    for name in ('gc-allocation-ring-512000000', 'eh_throws'):
        expected = base.LONG_FIXTURES[name][2] if name in base.LONG_FIXTURES else aux.FIXTURES[name]['sha256']
        require(plan['fixtures'][name]['sha256'] == expected, 'Pinned fixture binding changed')
    perf = P(admission['host_perf']['path']).resolve(strict=True)
    require(perf.read_bytes()[:4] == b'\x7fELF' and base.digest(perf) == admission['host_perf']['sha256'], 'Actual host perf ELF changed')
    require(any(f['real_path'] == str(perf) and f['sha256'] == admission['host_perf']['sha256'] for f in libraries['files']), 'Host perf absent from actual library closure')
    require(any('ld-linux' in f['real_path'] for f in libraries['files']) and any('/libc.so' in f['path'] for f in libraries['files']), 'Missing actual loader/libc closure')
    require(not args.out.exists(), 'New evidence directory required')
    args.out.mkdir(parents=True)
    base.save(args.out / 'original-plan.json', origin)
    base.save(args.out / 'host-plan.json', plan)
    base.save(args.out / 'actual-host-admission.json', admission)
    base.save(args.out / 'perf-libraries.json', libraries)
    for name, path in (('runner.py', P(__file__)), ('protocol.py', hw_path), ('guard-parser.py', base_path), ('aux.py', aux_path)):
        (args.out / name).write_bytes(path.read_bytes())
    require(args.execute, 'Explicit --execute required after keeper admission/inventory and root review')
    def cancel(signum, frame):
        raise KeyboardInterrupt('Controlled cancellation ' + str(signum))
    signal.signal(signal.SIGTERM, cancel)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    rows, error = [], None
    before_ok = after_ok = complete = False
    pmu_before = pmu_after = None
    try:
        guard = HostGuard(base, admission)
        adapted = HostBase(base, guard.root)
        base.save(args.out / 'actual-host-initial-identity.json', {'controller': base.json_ident(guard.self),
                  'controller_executable': str(guard.self_exe), 'init': base.json_ident(guard.init),
                  'init_executable': admission['host_init']['exe'], 'actual_cgroup_root': str(guard.root),
                  'init_loaded_executable': guard.init_executable,
                  'memory_events': guard.events, 'roster': (guard.root / 'cgroup.procs').read_text()})
        host_closure(base, plan, libraries, args.out, 'before')
        guard.check()
        pmu_before = pmu_receipt()
        base.save(args.out / 'pmu-before.json', pmu_before)
        before_ok = True
        # The measure function's code and two-child protocol remain exactly
        # frozen. Only its explicit event group and derived math adapter differ
        # in this new scope; the imported module itself is never mutated.
        signal_receipts = []
        execution_globals = dict(hw.measure.__globals__, EVENT_GROUP=RAW_GROUP,
                                 parse_stat=lambda stat, debug='': normalized_math(hw, stat, debug),
                                 signal_owned=lambda entry, sig: submit_owned_signal(hw, base, guard, entry, sig, signal_receipts))
        measure = types.FunctionType(hw.measure.__code__, execution_globals, 'host_measure')
        for item in selected:
            signal_receipts.clear()
            row = measure(adapted, aux, guard, item, perf, args.out)
            row['owned_perf_sigint_receipts'] = list(signal_receipts)
            debug_path = args.out / (row['label'] + '.perf.log')
            attrs = final_attributes(debug_path.read_text(errors='replace'), row['guest_admission']['pid']) if row['guest_admission'] else {'passed': False, 'failures': ['No admitted guest']}
            counts = row['hardware_counting']
            previous_counter_failures = counts['failures'][:]
            counts['final_guest_attributes'] = attrs
            counts['counter_quality_passed'] = counts.get('math_quality_passed', False) and attrs['passed']
            counts['failures'] = [f for f in counts['failures'] if not f.startswith('Awaiting actual final-guest')]
            counts['failures'].extend(attrs['failures'])
            completion = perf_completion(row)
            row['actual_perf_completion'] = completion
            counts['counter_quality_passed'] = counts['counter_quality_passed'] and completion['passed']
            counts['failures'].extend(completion['failures'])
            row['quality_failures'] = [reason for reason in row['quality_failures'] if reason not in previous_counter_failures] + counts['failures']
            row['measurement_namespace'] = 'host, admitted to exact existing target cgroup'
            row['input_sha256'] = input_shas
            rows.append(row)
            with (args.out / 'qualified.jsonl').open('a') as stream:
                stream.write(json.dumps(row, allow_nan=False) + '\n')
            require(not row['hard_failures'], 'Host ownership/resource/deadline failure')
            require(row['semantic_receipt']['passed'], 'Actual guest correctness failed')
            require(completion['passed'], 'Actual host perf submission/termination/final counts failed')
        host_closure(base, plan, libraries, args.out, 'after')
        guard.check()
        pmu_after = pmu_receipt()
        base.save(args.out / 'pmu-after.json', pmu_after)
        require(pmu_before == pmu_after, 'Actual PMU/CPU changed')
        require(all(base.digest(path) == sha for path, sha in input_shas.items()), 'Frozen input/dependency changed')
        require(loaded_executable_identity(guard.init_proc_exe, base.digest) == guard.init_executable, 'Host init loaded ELF changed')
        after_ok = complete = True
    except BaseException as failure:
        error = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        base.save(args.out / 'summary.json', {'schema': 'uwvm-host-per-guest-hardware-counting-v1', 'input_sha256': input_shas,
                  'rows': len(rows), 'complete': complete, 'closure_before_ok': before_ok, 'closure_after_ok': after_ok,
                  'execution_error': error, 'raw_event_group': RAW_GROUP, 'derived_math_alias_mapping': ALIAS,
                  'semantic_pass_count': sum(r['semantic_receipt']['passed'] for r in rows),
                  'hardware_quality_pass_count': sum(r['hardware_counting']['counter_quality_passed'] for r in rows),
                  'formal_acceptance': False, 'temperature_policy': 'observation_only', 'boot_id': admission['boot_id'],
                  'actual_cgroup_root': admission['cgroup_sysfs_absolute_path'],
                  'limitations': ['Whole guest execution after bootstrap GO includes ELF startup/JIT compilation, not Wasm-only ROI.',
                                  'Original raw.jsonl remains frozen-protocol output; qualified.jsonl adds explicit final-PMU attribution.',
                                  'Exact verbose enabled/running required; rounded JSON percentage is never inverted.',
                                  'Host P0/SMT activity requires independent observation; no software/events/admin/global changes.']})

if __name__ == '__main__':
    main()
