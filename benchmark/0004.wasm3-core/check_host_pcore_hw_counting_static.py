#!/usr/bin/env python3
"""Pure synthetic validation only; no guest, perf, SSH or Linux guard runs."""
import ast, copy, hashlib, json, os, pathlib, runpy, types

P = pathlib.Path
path = P(__file__).with_name('run_current_host_pcore_hw_counting.py')
ast.parse(path.read_bytes())
m = runpy.run_path(str(path), run_name='static_host_counter_checks')
expected = {'pid': 900, 'birth': '123', 'ppid': 800, 'uid': [1000] * 4,
            'argv': ['sleep', 'infinity'], 'exe': '/usr/bin/sleep', 'cgroup': '0::/example.scope\n'}
actual = dict(expected, birth=123, argv=[b'sleep', b'infinity'])
m['validate_init'](expected, actual, P(expected['exe']))
# Pure mount-namespace regression: reported link text need not identify the
# controller's same-name path. The digest must read original /proc/PID/exe.
loaded_function = m['loaded_executable_identity']
original_os = loaded_function.__globals__['os']
proc_exe = P('/proc/900/exe')
opened = []
try:
    loaded_function.__globals__['os'] = types.SimpleNamespace(readlink=lambda path: '/usr/bin/sleep')
    def fake_digest(path):
        opened.append(path)
        return '4' * 64
    loaded = loaded_function(proc_exe, fake_digest)
finally:
    loaded_function.__globals__['os'] = original_os
assert loaded == {'link_text': '/usr/bin/sleep', 'sha256': '4' * 64, 'byte_source': '/proc/900/exe'}
assert opened == [proc_exe]
m['validate_init'](expected, actual, loaded['link_text'])
snapshot = {'memory.max': str(64 << 30), 'memory.swap.max': '0',
            'cpuset.cpus.effective': '0,2,4,6,16-31', 'cgroup.procs': '900\n901\n'}
m['validate_scope'](snapshot, 900, 901, [])
rejected = []
def rejection(name, function):
    try:
        function()
    except RuntimeError:
        rejected.append(name)
    else:
        raise AssertionError('Counterexample was accepted: ' + name)
for name, key, bad in (('init_birth', 'birth', 124), ('init_cgroup', 'cgroup', '0::/foreign\n')):
    altered = dict(actual, **{key: bad})
    rejection(name, lambda altered=altered: m['validate_init'](expected, altered, P(expected['exe'])))
rejection('init_executable', lambda: m['validate_init'](expected, actual, P('/usr/bin/other')))
for name, key, bad in (('foreign_roster', 'cgroup.procs', '900\n901\n999\n'),
                       ('memory64', 'memory.max', str(128 << 30)), ('swap_zero', 'memory.swap.max', 'max'),
                       ('cpuset', 'cpuset.cpus.effective', '0-31')):
    altered = dict(snapshot, **{key: bad})
    rejection(name, lambda altered=altered: m['validate_scope'](altered, 900, 901, []))
rejection('mapping_traversal', lambda: m['host_path']('/work/../foreign', {'/work': '/actual/work'}))
assert len(rejected) == 8
shape_admission = {'boot_id': '00000000-0000-0000-0000-000000000000',
                   'host_init': dict(expected, affinity=[0, 2, 4, 6, *range(16, 32)], exe_sha256='0' * 64),
                   'cgroup_sysfs_absolute_path': '/sys/fs/cgroup/example.scope',
                   'host_mapping': {'/work': '/actual/work', '/toolchain': '/actual/toolchain'},
                   'host_perf': {'path': '/usr/bin/perf', 'sha256': '1' * 64, 'version': 'synthetic'},
                   'trusted_wrapper': {'path': '/actual/wrapper.py', 'sha256': '2' * 64}}
shape_libraries = {'LD_LIBRARY_PATH': '/actual/libs', 'LD_PRELOAD': None,
                   'files': [{'path': path, 'real_path': path, 'bytes': 1, 'sha256': '3' * 64}
                             for path in ('/usr/bin/perf', '/actual/ld-linux.so', '/actual/libc.so')]}
m['validate_raw_inventory'](shape_admission, shape_libraries)
shape_rejected = []
for name in ('missing_init', 'bool_pid', 'noninteger_uid', 'bad_sha', 'relative_mapping', 'bool_bytes', 'duplicate_library', 'preload'):
    a, b = copy.deepcopy(shape_admission), copy.deepcopy(shape_libraries)
    if name == 'missing_init': del a['host_init']
    elif name == 'bool_pid': a['host_init']['pid'] = True
    elif name == 'noninteger_uid': a['host_init']['uid'] = [1000.0] * 4
    elif name == 'bad_sha': a['host_perf']['sha256'] = 'fake'
    elif name == 'relative_mapping': a['host_mapping']['/work'] = 'relative'
    elif name == 'bool_bytes': b['files'][0]['bytes'] = True
    elif name == 'duplicate_library': b['files'].append(b['files'][0])
    else: b['LD_PRELOAD'] = 'inject.so'
    try: m['validate_raw_inventory'](a, b)
    except RuntimeError: shape_rejected.append(name)
    else: raise AssertionError(name)
good = '\n'.join('perf_event_attr:\n  type 4\n  config ' + config +
                 '\n  read_format TOTAL_TIME_ENABLED|TOTAL_TIME_RUNNING|ID|GROUP\n' +
                 'sys_perf_event_open: pid 902 cpu -1 group_fd ' + group + ' flags 0x8 = ' + fd
                 for config, group, fd in [('0x3c', '-1', '12'), ('0xc0', '12', '13')])
assert m['final_attributes'](good, 902)['passed']
attribute_rejected = []
for name, text, pid in [('wrong_pmu', good.replace('type 4', 'type 10'), 902),
                        ('wrong_guest', good, 903), ('broken_group', good.replace('group_fd 12', 'group_fd -1'), 902),
                        ('sampling', good.replace('  type 4', '  sample_period 100\n  type 4'), 902),
                        ('domain_fallback', good.replace('  type 4', '  exclude_kernel 1\n  type 4'), 902),
                        ('missing_format', good.replace('  read_format TOTAL_TIME_ENABLED|TOTAL_TIME_RUNNING|ID|GROUP\n', ''), 902),
                        ('different_leader_fd', good.replace('group_fd 12', 'group_fd 11'), 902),
                        ('no_actual_return_fd', good.replace(' = 12', '').replace(' = 13', ''), 902),
                        ('failed_open_return', good.replace(' = 13', ' = -1'), 902),
                        ('reused_event_fd', good.replace(' = 13', ' = 12'), 902)]:
    assert not m['final_attributes'](text, pid)['passed'], name
    attribute_rejected.append(name)
hw, _ = m['fixed_module']('static_frozen_hw', 'run_current_pcore_hw_counting.py', m['HW_SHA'])
stats = '\n'.join(json.dumps({'counter-value': '1000', 'event': event, 'event-runtime': 100000000, 'pcnt-running': 100.0}) for event in m['RAW_EVENTS'])
debug = '\n'.join(event + ': -1: 1000 100000000 100000000' for event in m['RAW_EVENTS'])
result = m['normalized_math'](hw, stats, debug)
assert result['math_quality_passed'] and not result['counter_quality_passed']
assert result['actual_raw_event_group'] == m['RAW_GROUP'] and result['derived_math_alias_mapping'] == m['ALIAS']
assert not m['normalized_math'](hw, stats, debug + '\n' + debug)['math_quality_passed']
assert not m['normalized_math'](hw, stats, '')['math_quality_passed']
perf_identity = {'pid': 902, 'birth': 456, 'ppid': 901, 'pgid': 902, 'uid': [1000] * 4,
                 'argv': [b'/actual/perf', b'stat'], 'cgroup': '0::/example.scope\n', 'cpus': '16'}
perf_entry = {'role': 'perf', 'reaped': False, 'pidfd': 11, 'process': types.SimpleNamespace(pid=902),
              'birth': 456, 'exec_argv': perf_identity['argv'], 'actual_exec': dict(perf_identity)}
guest_entry = {'role': 'guest', 'reaped': True, 'pidfd': 10,
               'process': types.SimpleNamespace(pid=900, returncode=0)}
m['validate_perf_sigint_admission'](perf_entry, perf_identity, False, guest_entry, True, '0::/example.scope\n', 901)
signal_admission_rejected = []
for name in ('perf_wrong_role', 'perf_retired', 'perf_pidfd_closed', 'perf_pidfd_readable', 'perf_birth_changed',
             'perf_argv_changed', 'perf_cgroup_changed', 'perf_not_E16', 'guest_not_reaped', 'guest_pidfd_not_readable'):
    e, ident, guest = copy.deepcopy(perf_entry), copy.deepcopy(perf_identity), copy.deepcopy(guest_entry)
    ready, guest_ready = False, True
    if name == 'perf_wrong_role': e['role'] = 'guest'
    elif name == 'perf_retired': e['reaped'] = True
    elif name == 'perf_pidfd_closed': e['pidfd'] = None
    elif name == 'perf_pidfd_readable': ready = True
    elif name == 'perf_birth_changed': ident['birth'] += 1
    elif name == 'perf_argv_changed': ident['argv'] = [b'/foreign/perf']
    elif name == 'perf_cgroup_changed': ident['cgroup'] = '0::/foreign.scope\n'
    elif name == 'perf_not_E16': ident['cpus'] = '0'
    elif name == 'guest_not_reaped': guest['reaped'] = False
    else: guest_ready = False
    try: m['validate_perf_sigint_admission'](e, ident, ready, guest, guest_ready, '0::/example.scope\n', 901)
    except RuntimeError: signal_admission_rejected.append(name)
    else: raise AssertionError(name)
identity = dict(perf_identity, argv=['/actual/perf', 'stat'])
proof = {'signal': 2, 'original_perf_identity': copy.deepcopy(identity), 'original_guest_pid': 900, 'original_guest_birth': 123,
         'guest_reaped_before_delivery': True, 'guest_returncode_before_delivery': 0,
         'guest_pidfd_readable_before_delivery': True, 'perf_pidfd_live_before_delivery': True,
         'pidfd_send_signal_succeeded': True, 'delivery_started_ns': 31, 'delivery_completed_ns': 32}
completed = {'perf_exit_after_owned_SIGINT': -2, 'guest_exit': 0, 'semantic_receipt': {'passed': True}, 'hard_failures': [],
             'hardware_counting': {'math_quality_passed': True, 'final_guest_attributes': {'passed': True}},
             'perf_actual_control_ack': 'ack\n', 'perf_enable_ack_ns': 10, 'guest_release_ns': 20, 'guest_retired_ns': 30,
             'owned_perf_sigint_receipts': [proof], 'perf_actual_exec': identity, 'guest_admission': {'pid': 900, 'birth': 123}}
assert m['perf_completion'](completed)['passed']
assert m['perf_completion'](dict(completed, perf_exit_after_owned_SIGINT=0))['passed']
completion_rejected = []
for name in ('other_exit', 'missing_ack', 'missing_retired', 'no_math', 'no_attributes', 'perf_early_exit',
             'external_sigint', 'failed_send', 'signal_before_guest_retired', 'guest_identity_changed', 'perf_identity_changed'):
    row = copy.deepcopy(completed)
    if name == 'other_exit': row['perf_exit_after_owned_SIGINT'] = -15
    elif name == 'missing_ack': row['perf_actual_control_ack'] = None
    elif name == 'missing_retired': row['guest_retired_ns'] = None
    elif name == 'no_math': row['hardware_counting']['math_quality_passed'] = False
    elif name == 'no_attributes': row['hardware_counting']['final_guest_attributes']['passed'] = False
    elif name == 'perf_early_exit': row['owned_perf_sigint_receipts'][0]['perf_pidfd_live_before_delivery'] = False
    elif name == 'external_sigint': row['owned_perf_sigint_receipts'] = []
    elif name == 'failed_send': row['owned_perf_sigint_receipts'][0]['pidfd_send_signal_succeeded'] = False
    elif name == 'signal_before_guest_retired': row['owned_perf_sigint_receipts'][0]['delivery_started_ns'] = 29
    elif name == 'guest_identity_changed': row['owned_perf_sigint_receipts'][0]['original_guest_birth'] += 1
    else: row['owned_perf_sigint_receipts'][0]['original_perf_identity']['argv'] = ['/foreign/perf']
    assert not m['perf_completion'](row)['passed'], name
    completion_rejected.append(name)
print(json.dumps({'runner_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'AST_passed': True,
                  'synthetic_host_counterexamples_rejected': rejected,
                  'synthetic_final_attribute_counterexamples_rejected': attribute_rejected,
                  'synthetic_raw_inventory_shape_counterexamples_rejected': shape_rejected,
                  'derived_math_checks_passed': True, 'actual_linux_guard_or_counter_execution': False,
                  'synthetic_loaded_executable_namespace_checks_passed': True,
                  'synthetic_sigint_admission_counterexamples_rejected': signal_admission_rejected,
                  'synthetic_perf_completion_counterexamples_rejected': completion_rejected,
                  'guest_perf_SSH_or_build_execution': False}, indent=2))
