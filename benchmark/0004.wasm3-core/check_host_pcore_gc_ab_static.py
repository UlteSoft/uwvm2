#!/usr/bin/env python3
"""Pure synthetic four-build contracts only; no guest/Linux guard execution."""
import ast, copy, hashlib, json, pathlib, runpy, shlex

P = pathlib.Path
path = P(__file__).with_name('run_current_host_pcore_gc_ab.py')
ast.parse(path.read_bytes())
m = runpy.run_path(str(path), run_name='synthetic_ab_contract')

def synthetic_plan():
    # These deliberately nonexistent bindings are never written as a runnable
    # actual plan. This test calls pure validators, never main/HostGuard.
    plan = {'schema': 'uwvm-host-pcore-gc-ab-plan-v1', 'execute_ready': True,
            'execution_order': list(m['ORDER']), 'products': {}, 'commands': [],
            'fixtures': {m['FIXTURE']: {'path': '/work/synthetic/ring.wasm', 'sha256': m['FIXTURE_SHA']}},
            'compiler': {'path': '/toolchain/bin/clang++', 'sha256': '5' * 64},
            'linker': {'path': '/toolchain/bin/ld.lld', 'sha256': '6' * 64},
            'wasmtime': {'path': '/work/synthetic/oracle', 'sha256': '7' * 64}}
    plan['build_dependency_files'] = [dict(plan['compiler']), dict(plan['linker']),
                                     {'path': '/work/synthetic/link.rsp', 'sha256': m['RSP_SHA']}]
    for index, key in enumerate(m['KEYS'], 1):
        root = '/work/synthetic/' + key
        pin = {'source': root + '/src-tree', 'source_id': 'sha256:' + str(index) * 64,
               'runtime': root + '/runtime.o', 'runtime_sha256': '9' * 64,
               'binary': root + '/uwvm', 'binary_sha256': 'a' * 64,
               'build_json': root + '/build.json', 'build_json_sha256': 'b' * 64,
               'cold': {'path': root + '/cold.json', 'sha256': 'c' * 64, 'row_index': 0}, 'commands': {}}
        if key.endswith('_A'):
            pin['source_id'] = m['A_SOURCE_IDS'][key.rsplit('_', 1)[0]]
        flags = [plan['compiler']['path'], *['-DUWVM_EXPERIMENTAL_' + macro + '=1' for macro in m['EXPERIMENTS']],
                 *['-DUWVM_ENABLE_UWVM_INT_' + macro for macro in m['COMBINES']], '-O3', '-DUWVM_USE_LLVM_JIT',
                 '-DUWVM2_BUILD_SOURCE_ID=u8"' + pin['source_id'] + '"', '-I', pin['source'] + '/src']
        pin['commands']['runtime'] = {'path': root + '/runtime.command.json', 'sha256': 'd' * 64,
                                      'argv': [*flags, '-c', 'src/runtime.cpp', '-o', pin['runtime']]}
        pin['commands']['cli'] = {'path': root + '/cli.command.json', 'sha256': 'e' * 64,
                                  'argv': [*flags, '-fuse-ld=lld', 'src/main.cpp', pin['runtime'],
                                           '@/work/synthetic/link.rsp', '-o', pin['binary']]}
        plan['products'][key] = pin
        mode = ['-Rcc', 'jit', '-Rcm', 'full'] if key.startswith('ordinary_') else ['-Raot']
        plan['commands'].append({'profile': key + '/unwind/auto', 'fixture': m['FIXTURE'],
                                 'argv': ['taskset', '-c', '0', pin['binary'], *mode, *m['POLICY_ARGV'], plan['fixtures'][m['FIXTURE']]['path']]})
    return plan

good = synthetic_plan()
m['validate_plan'](good)
negative_names = []
def reject(name, function):
    try:
        function()
    except (RuntimeError, TypeError, KeyError, IndexError):
        negative_names.append(name)
    else:
        raise AssertionError('Synthetic counterexample accepted: ' + name)

def plan_negative(name, change):
    value = copy.deepcopy(good)
    change(value)
    reject(name, lambda: m['validate_plan'](value))

plan_negative('not_actual_ready', lambda p: p.update(execute_ready=False))
plan_negative('placeholder_sid', lambda p: p['products']['ordinary_B'].update(source_id='<actual-after-build>'))
plan_negative('placeholder_elf_hash', lambda p: p['products']['ordinary_B'].update(binary_sha256=None))
plan_negative('zero_hash', lambda p: p['products']['ordinary_B'].update(binary_sha256='0' * 64))
plan_negative('different_workload', lambda p: p['fixtures'][m['FIXTURE']].update(sha256='f' * 64))
plan_negative('order_AABB', lambda p: p['execution_order'].__setitem__(slice(0, 4), ['ordinary_A', 'ordinary_A', 'ordinary_B', 'ordinary_B']))
plan_negative('missing_fourth_build', lambda p: p['products'].pop('ros_B'))
plan_negative('A_runtime_reused', lambda p: p['products']['ordinary_B'].update(runtime=p['products']['ordinary_A']['runtime']))
plan_negative('A_source_reused', lambda p: p['products']['ordinary_B'].update(source=p['products']['ordinary_A']['source']))
plan_negative('A_source_id_reused', lambda p: p['products']['ordinary_B'].update(source_id=p['products']['ordinary_A']['source_id']))
plan_negative('A_not_preserved_r10', lambda p: p['products']['ordinary_A'].update(source_id='sha256:' + 'f' * 64))
plan_negative('cold_row_boolean', lambda p: p['products']['ordinary_B']['cold'].update(row_index=True))
plan_negative('source_define_mismatch', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-DUWVM2_BUILD_SOURCE_ID=u8"wrong"'))
plan_negative('macro_disabled', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].__setitem__(1, '-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0'))
plan_negative('macro_duplicate', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=1'))
plan_negative('macro_undef_override', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-UUWVM_EXPERIMENTAL_COMPACT_NUMERIC'))
plan_negative('macro_split_D', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].__setitem__(slice(1, 1), ['-D', 'UWVM_EXPERIMENTAL_COMPACT_NUMERIC=0']))
plan_negative('macro_Xpreprocessor', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].__setitem__(slice(1, 1), ['-Xpreprocessor', '-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0']))
plan_negative('macro_Xclang', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].__setitem__(slice(1, 1), ['-Xclang', '-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0']))
plan_negative('macro_Wp', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-Wp,-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0'))
plan_negative('extra_experiment', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-DUWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST=1'))
plan_negative('different_optimization', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-O0'))
plan_negative('different_ABI', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-fno-exceptions'))
plan_negative('different_external_header', lambda p: p['products']['ordinary_B']['commands']['runtime']['argv'].insert(1, '-I/other/compiler/header'))
plan_negative('foreign_runtime_link', lambda p: p['products']['ordinary_B']['commands']['cli']['argv'].__setitem__(-4, '/work/foreign/runtime.o'))
plan_negative('unbound_rsp', lambda p: p['products']['ordinary_B']['commands']['cli']['argv'].__setitem__(-3, '@/work/foreign/link.rsp'))
plan_negative('unreviewed_rsp_hash', lambda p: p['build_dependency_files'][-1].update(sha256='f' * 64))
plan_negative('different_linker', lambda p: p['linker'].update(path='/toolchain/bin/other-lld'))
plan_negative('compiler_not_in_closure', lambda p: p['build_dependency_files'].pop(0))
plan_negative('duplicate_dependency', lambda p: p['build_dependency_files'].append(p['build_dependency_files'][0]))
plan_negative('hidden_guest_argv', lambda p: p['commands'][0].update(diagnostic_argv=['other']))
plan_negative('wrong_guest_cpu', lambda p: p['commands'][0]['argv'].__setitem__(2, '2'))
plan_negative('wrong_mode_ROS', lambda p: p['commands'][2]['argv'].__setitem__(4, '-Rcc'))
plan_negative('wrong_EH_policy', lambda p: p['commands'][0]['argv'].__setitem__(p['commands'][0]['argv'].index('auto'), 'native-unwind'))

pin = good['products']['ordinary_A']
receipt = {'source': pin['source'], 'source_id': pin['source_id'], 'source_id_after': pin['source_id'],
           'compiler_sha256': good['compiler']['sha256'], 'binary_sha256': pin['binary_sha256'],
           'fresh_runtime': True, 'old_object_reused': False, 'passed': True,
           'memory_max': str(64 << 30), 'swap_max': '0', 'cpuset': '0,2,4,6,16-31',
           'events_before': 'oom 0\noom_kill 0\n', 'events_after': 'oom 0\noom_kill 0\n',
           'steps': [{'step': step, 'returncode': 0} for step in ('runtime', 'cli')]}
sidecars = {step: pin['commands'][step]['argv'] for step in ('runtime', 'cli')}
m['validate_build_receipt'](pin, receipt, sidecars, good['compiler']['sha256'])
for name, field, value in [('unbuilt_runtime', 'fresh_runtime', False), ('reused_runtime', 'old_object_reused', True),
                           ('failed_build', 'passed', False), ('built_wrong_source', 'source_id_after', 'sha256:' + 'f' * 64),
                           ('built_wrong_compiler', 'compiler_sha256', 'f' * 64), ('built_wrong_ELF', 'binary_sha256', 'f' * 64),
                           ('compile_swap', 'swap_max', 'max'), ('compile_memory', 'memory_max', str(128 << 30))]:
    value_receipt = dict(receipt, **{field: value})
    reject(name, lambda value_receipt=value_receipt: m['validate_build_receipt'](pin, value_receipt, sidecars, good['compiler']['sha256']))
bad_receipt = copy.deepcopy(receipt)
bad_receipt['steps'][0]['returncode'] = False
reject('boolean_compile_return', lambda: m['validate_build_receipt'](pin, bad_receipt, sidecars, good['compiler']['sha256']))
bad_sidecars = dict(sidecars, runtime=sidecars['runtime'] + ['changed'])
reject('sidecar_bytes_disagree', lambda: m['validate_build_receipt'](pin, receipt, bad_sidecars, good['compiler']['sha256']))
expected_cold_argv = ['taskset', '-c', '16', *good['commands'][0]['argv'][3:]]
cold = {'passed': True, 'source_ids': {'ordinary': pin['source_id']},
        'rows': [{'exit': 0, 'passed': True, 'timed': False, 'fixture': m['FIXTURE'],
                  'profile': 'ordinary/unwind/auto', 'argv': expected_cold_argv,
                  'log': '/work/synthetic/cold.log', 'log_sha256': 'f' * 64}]}
m['validate_cold_row'](pin, cold, 'ordinary_A', expected_cold_argv)
for name, change in [('cold_wrong_source', lambda c: c['source_ids'].update(ordinary='sha256:' + 'f' * 64)),
                      ('cold_no_rows', lambda c: c.update(rows=[])),
                      ('cold_failed_exit', lambda c: c['rows'][0].update(exit=1)),
                      ('cold_bool_exit', lambda c: c['rows'][0].update(exit=False)),
                      ('cold_timed_row', lambda c: c['rows'][0].update(timed=True)),
                      ('cold_wrong_fixture', lambda c: c['rows'][0].update(fixture='different')),
                      ('cold_wrong_policy', lambda c: c['rows'][0].update(profile='ordinary/instruction/auto')),
                      ('cold_wrong_elf', lambda c: c['rows'][0]['argv'].__setitem__(3, '/work/other/uwvm')),
                      ('cold_missing_log_sha', lambda c: c['rows'][0].pop('log_sha256'))]:
    bad = copy.deepcopy(cold); change(bad)
    reject(name, lambda bad=bad: m['validate_cold_row'](pin, bad, 'ordinary_A', expected_cold_argv))
bad = dict(receipt, events_after='oom 1\noom_kill 0\n')
reject('compile_oom_increase', lambda: m['validate_build_receipt'](pin, bad, sidecars, good['compiler']['sha256']))

# Actual read-only r10 recipe bytes, with no compiler execution. Its existing
# NDEBUG and timestamp flags are explicitly recorded, never described as absent.
actual_rsp = path.parents[2] / 'build/wasm3-evidence/performance-20261002-r10-compiler-response/linux-r10-consumer-link.rsp'
if actual_rsp.exists():
    blob = actual_rsp.read_bytes()
    tokens = shlex.split(blob.decode())
    libraries = [{'path': token, 'sha256': '1' * 64} for token in tokens if token.startswith('/')]
    proof = m['response_receipt'](blob, libraries)
    assert proof['reviewed_non_GC_macro'] == '-DNDEBUG' and not proof['GC_macro_override']
    for name, suffix in [('rsp_macro_override', b'\n"-DUWVM_EXPERIMENTAL_COMPACT_NUMERIC=0"\n'),
                         ('rsp_split_macro', b'\n"-D" "UWVM_EXPERIMENTAL_COMPACT_NUMERIC=0"\n'),
                         ('rsp_nested', b'\n"@/other.rsp"\n'), ('rsp_compiler_forward', b'\n"-Xclang" "-O0"\n')]:
        reject(name, lambda suffix=suffix: m['response_receipt'](blob + suffix, libraries))
    reject('rsp_missing_archive_closure', lambda: m['response_receipt'](blob, libraries[1:]))

def fingerprint(files):
    files = [{'path': key, 'sha256': value} for key, value in sorted(files.items())]
    return {'files': files, 'source_id': 'sha256:' + hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()}
source_A = {path: pair[0] for path, pair in m['GC_DIFF'].items()}
source_B = {path: pair[1] for path, pair in m['GC_DIFF'].items()}
source_A['third-parties/example/header.h'] = source_B['third-parties/example/header.h'] = 'a' * 64
delta = m['reviewed_source_difference'](fingerprint(source_A), fingerprint(source_B))
assert len(delta['actual_changed_files']) == 2 and delta['all_other_source_and_vendor_files_identical']
for name, change in [('GC_mixed_IO_change', lambda p: p.update({'src/IO.h': 'b' * 64})),
                      ('GC_mixed_vendor_change', lambda p: p.update({'third-parties/example/header.h': 'b' * 64})),
                      ('GC_wrong_after_image', lambda p: p.update({next(iter(m['GC_DIFF'])): 'b' * 64})),
                      ('GC_missing_after_file', lambda p: p.pop(next(iter(m['GC_DIFF']))))]:
    bad = dict(source_B); change(bad)
    reject(name, lambda bad=bad: m['reviewed_source_difference'](fingerprint(source_A), fingerprint(bad)))
bad = fingerprint(source_B);bad['source_id'] = 'sha256:' + 'f' * 64
reject('GC_fingerprint_digest_changed', lambda: m['reviewed_source_difference'](fingerprint(source_A), bad))
bad = fingerprint(source_B);bad['files'].append(dict(bad['files'][0]))
reject('GC_duplicate_fingerprint_path', lambda: m['reviewed_source_difference'](fingerprint(source_A), bad))

actual = {'passed': True, 'iterations': 512000000, 'expected_checksum': 1687964949,
          'gc-managed': {'allocations': 512000000, 'collections': 125000, 'reclaimed': 511998975,
                         **dict.fromkeys(('peer_skips', 'policy_skips', 'heap_rejections', 'population_rejections', 'disabled', 'reason'), 0)},
          'gc-sealed-experimental': {'committed_slots': 512000000, 'remaining': 0}}
assert m['exact_gc'](actual)
for name, section, field, value in [('wrong_collection_schedule', 'gc-managed', 'collections', 124999),
                                   ('wrong_reclaim', 'gc-managed', 'reclaimed', 512000000),
                                   ('wrong_pending', 'gc-sealed-experimental', 'remaining', 1)]:
    bad = copy.deepcopy(actual);bad[section][field] = value
    assert not m['exact_gc'](bad), name

row = {'during': [{'scaling_cur_freq': n} for n in (5000000, 5100000, 5200000, 5300000, 5400000)]}
freq = m['frequency_receipt'](row)
assert freq['sufficient'] and freq['p05_khz'] == 5020000 and freq['median_khz'] == 5200000 and freq['p95_khz'] == 5380000
rows = [dict(ab_build=key, frequency=copy.deepcopy(freq), quality_failures=[], hard_failures=[], exact_gc_passed=True,
             hardware_counting={'counter_quality_passed': True, 'events': {
                 'some_derived_cycles_key': {'actual_raw_event_name': 'cpu_core/event=0x3c/', 'raw_counter_value': 100},
                 'some_derived_instructions_key': {'actual_raw_event_name': 'cpu_core/event=0xc0/', 'raw_counter_value': 200}}},
             guest_execution_ns=1000000000, guest_wall_ns=1010000000) for key in m['ORDER']]
pairs = m['pair_receipts'](rows, True)
assert len(pairs) == 4 and all(p['frequency_matched_counter_pair'] for p in pairs)
assert not any(p['frequency_matched_counter_pair'] for p in m['pair_receipts'](rows, False))
bad = copy.deepcopy(rows);bad[1]['frequency']['median_khz'] = 6000000
assert not m['pair_receipts'](bad, True)[0]['frequency_matched_counter_pair']
bad = copy.deepcopy(rows);bad[1]['quality_failures'] = ['CPU throttling changed']
assert not m['pair_receipts'](bad, True)[0]['frequency_matched_counter_pair']
bad = copy.deepcopy(rows);bad[1]['frequency']['sufficient'] = False
assert not m['pair_receipts'](bad, True)[0]['frequency_matched_counter_pair']
assert m['pair_receipts'](rows[:1], False) == []
bad = copy.deepcopy(rows);bad[1]['hardware_counting']['events']['some_derived_cycles_key']['actual_raw_event_name'] = 'wrong'
assert not m['pair_receipts'](bad, True)[0]['frequency_matched_counter_pair']
ratios = copy.deepcopy(rows)
for index, value in ((1, 200), (2, 50)):
    ratios[index]['hardware_counting']['events']['some_derived_cycles_key']['raw_counter_value'] = value
pairs = m['pair_receipts'](ratios, True)
assert pairs[0]['cycles_B_over_A'] == '2' and pairs[1]['cycles_B_over_A'] == '0.5'
aggregate = m['descriptive_pair_summary'](pairs)['ordinary']
assert aggregate['metrics']['cycles_B_over_A']['geometric_mean'] == '1.0'
assert aggregate['metrics']['cycles_B_over_A']['range'] == ['0.5', '2'] and not aggregate['pair_effect_estimate_qualified']
assert not m['descriptive_pair_summary'](pairs[:1])['ordinary']['metrics']
metrics = m['whole_workload_metrics']({'hardware_counting': {'events': {'derived_cycles_key': {'actual_raw_event_name': 'cpu_core/event=0x3c/', 'raw_counter_value': 1024000000},
                       'derived_instructions_key': {'actual_raw_event_name': 'cpu_core/event=0xc0/', 'raw_counter_value': 2048000000}}},
                       'guest_execution_ns': 1536000000, 'guest_wall_ns': 2048000000})
assert metrics['cycles_per_step_including_startup_and_JIT'] == '2' and metrics['CPI_whole_guest'] == '0.5'
assert metrics['internal_guest_ns_per_workload_step'] == '3' and metrics['not_a_collector_latency']
print(json.dumps({'schema': 'uwvm-pure-synthetic-ab-checks-v1', 'runner_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                  'negative_count': len(negative_names), 'negative_cases': negative_names,
                  'exact_gc_negative_cases': 3, 'frequency_pair_negative_cases': 4,
                  'executed_native': False, 'executed_linux_guard': False, 'actual_build_qualification': False}, indent=2))
