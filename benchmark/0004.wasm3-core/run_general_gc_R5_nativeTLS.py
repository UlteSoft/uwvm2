#!/usr/bin/env python3
"""Narrow R5-nativeTLS GeneralGC binding to frozen P0 plain/HW samplers.

No SSH/build/admin/cgroup mutation. Old S6e runner and raw results stay immutable.
The actual current 32-product long-cold receipt must exist and pass first.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import select
import signal
import statistics
import subprocess
import tempfile
import time
import types
import threading

HERE = Path(__file__).resolve().parent
SAMPLER_SHA = '91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8'
HOST_SHA = '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5'
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
LONG_RECIPE_SHA = '2afc5478b9b0d7d8bd94d29d92ea07e33d4f8ccf7de36104606f90d5453d23aa'
LONG_SUMMARY_SHA = '9da5e9d032c0aa551888883af3e6403b6aa16196cb5f853d74f13f368f9d0580'
LONG_RECEIPTS_SHA = '7d4690397319afcacd9a3523f0e9418e3b42504f0a5def8b4cf4fa6b1e780a5d'
LONG_COMMANDS_SHA = 'b28bec8d942f46acaff190928fd0c2f34dfe628ba63744338d6f2dff2ef58562'
PROFILE = 'ordinary/unwind/R5-nativeTLS-default'
WT_PROFILE = 'wasmtime49/copying'
SCHEMA = 'uwvm-r5-nativetls-general-gc-plan-v1'
CAPTURE_KEY = 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path):
    path = Path(path).resolve(strict=True)
    return {'path': str(path), 'bytes': path.stat().st_size, 'sha256': digest(path)}


def atomic(path, data):
    path = Path(path)
    data = data.encode() if isinstance(data, str) else data
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def save(path, value):
    atomic(path, json.dumps(value, indent=2, allow_nan=False) + '\n')


def load(name, path, sha):
    path = Path(path)
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == sha, 'Frozen dependency changed: ' + str(path))
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(data, str(path), 'exec'), module.__dict__)
    return module


def dependencies():
    sampler = load('_unchanged_general_gc_sampler', HERE / 'run_current_general_gc.py', SAMPLER_SHA)
    cold = load('_current_r5_long_binding', HERE / 'prepare_general_gc_R5_nativeTLS_long_cold.py',
                LONG_RECIPE_SHA)
    return sampler, cold


def original_compile_bindings(cold, sampler):
    # The original actual build schema is preserved. RT1 and host3 include their
    # real original_binding, original argv/log/MD and source-dependent inputs.
    build = cold.B / 'builds/debug-joint-r5-nativeTLS-current-cli-cold-20261002-r2'
    before = json.loads((build / 'closure-before.json').read_text())
    executable = json.loads((build / 'main.executable-bind.json').read_text())
    prefix = before['shared_compile_prefix']
    require('-DUWVM_USE_THREAD_LOCAL' in prefix and
            '-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1' in prefix and
            not any(v.startswith('-DUWVM_EXPERIMENTAL_') for v in prefix),
            'Not the actual nativeTLS/default R5 profile')
    files = {}
    sampler.absolute_pin_maps(before, files)
    sampler.absolute_pin_maps(executable, files)
    for name, component in executable['components'].items():
        bound = component.get('original_binding', component)
        proof = bound['actual_compile_proof']
        argv = proof['actual_argv']
        require(proof['returncode'] == 0 and argv[:argv.index('-MD')] == prefix,
                'Current compile TU prefix mismatch')
        require(proof['argv_sha256'] == hashlib.sha256(json.dumps(argv).encode()).hexdigest(),
                'Current actual compile argv SHA mismatch')
        require(argv.count('-MF') == 1 and argv.count('-o') == 1, 'Ambiguous component MD/output')
        md = pin(argv[argv.index('-MF') + 1])
        require(md['sha256'] == bound['dependency_file_sha256'], 'Actual component MD changed')
        files[md['path']] = md
        sampler.add_pin(files, argv[argv.index('-o') + 1], bound['output'])
        require(not any(v in ('-D', '-U', '-Xclang', '-Xpreprocessor') or v.startswith(('-U', '-B'))
                        for v in argv), 'Unexpected compiler override')
        for v in argv:
            if v.startswith('@'):
                response = pin(v[1:])
                sampler.response_contract(Path(response['path']).read_bytes())
                files[response['path']] = response
    require(executable['actual_link_proof']['returncode'] == 0, 'Actual CLI link failed')
    link_argv = executable['actual_link_proof']['actual_argv']
    require(executable['actual_link_proof']['argv_sha256'] ==
            hashlib.sha256(json.dumps(link_argv).encode()).hexdigest(), 'Actual link argv changed')
    for v in link_argv:
        if v.startswith('@'):
            response = pin(v[1:])
            sampler.response_contract(Path(response['path']).read_bytes())
            files[response['path']] = response
    require(len(files) >= 2800, 'Incomplete actual transitive dependency maps')
    return files, before, executable


def actual_long_cold(cold):
    require(digest(cold.D / 'closure.py') == LONG_RECIPE_SHA, 'Actual long helper changed')
    require(digest(cold.D / 'summary.json') == LONG_SUMMARY_SHA and
            digest(cold.E / 'receipts.json') == LONG_RECEIPTS_SHA and
            digest(cold.D / 'commands.json') == LONG_COMMANDS_SHA,
            'Actual reviewed R5 long summary/receipt/command bytes changed')
    summary = json.loads((cold.D / 'summary.json').read_text())
    before = json.loads((cold.D / 'closure-before.json').read_text())
    require(summary['passed_semantics'] is True and
            summary['all_allocation_collector_qualified'] is True and
            summary['all_mutation_field_lookup_qualified'] is True and
            summary['source_product_transitive_dependencies_tools_fixtures_before_after_equal'] is True and
            summary['source_id_external'] == cold.SID and summary['product_sha256'] == cold.PRODUCT_SHA and
            summary['old_S6e_product_used_for_R5_qualification'] is False and
            summary['before_sha256'] == digest(cold.D / 'closure-before.json'),
            'Actual current R5 long cold did not pass/close')
    require(len(summary['rows']) == 32, 'Actual long product row count')
    require(cold.bindings() == before['bindings'], 'Current long inputs differ from actual before')
    require(json.loads((cold.D / 'source-before.json').read_text()) ==
            json.loads((cold.D / 'source-after.json').read_text()), 'Long source closure changed')
    rows = json.loads((cold.E / 'receipts.json').read_text())
    by_label = {row['label']: row for row in rows}
    require(len(rows) == len(by_label) == 34 and all(row['passed'] is True for row in rows),
            'Complete 34-stage actual long receipt required')
    expected_commands = dict(cold.commands())
    for family in cold.FAMILIES:
        for phase in cold.PHASES:
            for n in cold.ITERATIONS:
                definition = before['bindings']['fixtures'][f'{family}-{phase}-{n}']['expected']
                for policy in cold.POLICIES:
                    label = f'{family}-{phase}-{n}-ordinary-{policy}'
                    log = cold.E / (label + '.log')
                    cold.retired_receipt(by_label[label], label, expected_commands[label],
                                         cold.D / 'commands.json', log, current=True)
                    cold.parse_log(log.read_text(), definition, phase)
    return summary, before


def make_plan(output):
    sampler, cold = dependencies()
    summary, before = actual_long_cold(cold)
    files, build_before, executable = original_compile_bindings(cold, sampler)
    fixtures = {}
    commands = []
    tool_pins = list(before['bindings']['tools'].values())
    wasmtime = next(v for v in tool_pins if Path(v['path']).name == 'wasmtime')
    for family in cold.FAMILIES:
        for phase in cold.PHASES:
            for n in cold.ITERATIONS:
                name = f'{family}-{phase}-{n}'
                original = before['bindings']['fixtures'][name]
                definition = json.loads(Path(original['manifest']['path']).read_text())
                sampler.fixture_contract(definition)
                fixtures[name] = dict(original['wasm'], manifest=original['manifest'], definition=definition)
                original_product = cold.argv_for(family, phase, n, 'unwind')
                require(original_product[:5] == ['env', '-u', CAPTURE_KEY,
                        'LD_LIBRARY_PATH=' + cold.LD_PATH, 'RAYON_NUM_THREADS=1'],
                        'Actual cold environment changed')
                commands.append({'fixture': name, 'profile': PROFILE,
                                 'argv': ['taskset', '-c', '0', *original_product[5:]]})
                wt = original['old_official_and_copying_only']['wasmtime-start']['actual_argv']
                require(wt[:3] == ['env', 'LD_LIBRARY_PATH=' + cold.LD_PATH, 'RAYON_NUM_THREADS=1'] and
                        wt[3] == wasmtime['path'] and wt[-1] == original['wasm']['path'],
                        'Original Wasmtime start argv/environment changed')
                commands.append({'fixture': name, 'profile': WT_PROFILE,
                                 'argv': ['taskset', '-c', '0', *wt[3:]]})
    product_build = cold.B / 'builds/debug-joint-r5-nativeTLS-current-cli-cold-20261002-r2'
    provenance = [pin(cold.D / n) for n in ('summary.json', 'closure-before.json', 'commands.json',
                                          'source-before.json', 'source-after.json', 'closure.py', 'supervisor.py')]
    provenance += [pin(cold.E / 'receipts.json')]
    provenance += [pin(product_build / n) for n in ('closure-before.json', 'closure-after.json',
                                                   'main.executable-bind.json', 'commands.json')]
    provenance += [before['bindings']['product']['origin_build_receipts'],
                   before['bindings']['reviewed_R5_closure_helper'],
                   before['bindings']['old_oracle_commands'], before['bindings']['old_oracle_receipts']]
    plan = {'schema': SCHEMA, 'execute_ready': True, 'source': str(cold.SOURCE),
            'source_id': cold.SID, 'product': before['bindings']['product']['product'],
            'profile': PROFILE, 'LD_LIBRARY_PATH': cold.LD_PATH, 'wasmtime': wasmtime,
            'tools': tool_pins, 'files': list(files.values()), 'provenance': provenance,
            'fixtures': fixtures, 'commands': commands,
            'actual_long_cold_summary': pin(cold.D / 'summary.json'),
            'actual_long_bindings': before['bindings'],
            'native_TLS_all_TUs': True, 'experiments': 'all omitted',
            'all_stages_fresh': False, 'same_task_actual_RT1_host3_reused': True,
            'actual_loaded_DSO_maps_qualified': False,
            'CAPTURE_compiled_support': 1, 'CAPTURE_runtime': 'stripped; actual stopped environment required',
            'formal_acceptance': False, 'temperature_policy': 'observation_only',
            'measurement_boundaries': {'internal_Wasm_timer': 'Excludes startup/JIT; includes setup, all GC/field access/checksum; not pure collector ROI.',
                                       'parent_wait4': 'Whole process including startup/JIT and all guest TIDs, cold new process.',
                                       'pure_HW': 'Single product TID whole guest including startup/JIT; no sampling or SW counters.'}}
    validate_plan(plan, sampler, cold)
    require(not output.exists(), 'Fresh plan output required')
    output.parent.mkdir(parents=True, exist_ok=True)
    save(output, plan)


def validate_plan(plan, sampler, cold):
    require(plan.get('schema') == SCHEMA and plan.get('execute_ready') is True and
            plan.get('source_id') == cold.SID and plan['product']['sha256'] == cold.PRODUCT_SHA and
            plan['product']['bytes'] == cold.PRODUCT_BYTES and plan.get('native_TLS_all_TUs') is True and
            plan.get('experiments') == 'all omitted' and plan.get('formal_acceptance') is False and
            plan.get('temperature_policy') == 'observation_only', 'Wrong or placeholder R5 plan')
    require(plan['source'] == str(cold.SOURCE) and plan['LD_LIBRARY_PATH'] == cold.LD_PATH and
            len(plan['fixtures']) == 16 and len(plan['commands']) == 32 and len(plan['files']) >= 2800,
            'Incomplete current R5 plan')
    expected = set()
    for name, value in plan['fixtures'].items():
        definition = value['definition']
        sampler.fixture_contract(definition)
        require(name == f"{definition['family']}-{definition['phase']}-{definition['iterations']}" and
                value['sha256'] == definition['wasm_sha256'], 'Exact fixture identity changed')
        expected.update((name, profile) for profile in (PROFILE, WT_PROFILE))
    require(len({(row['fixture'], row['profile']) for row in plan['commands']}) == 32 and
            {(row['fixture'], row['profile']) for row in plan['commands']} == expected,
            'Duplicate/missing exact paired commands')
    for item in plan['commands']:
        definition = plan['fixtures'][item['fixture']]['definition']
        if item['profile'] == PROFILE:
            expected_argv = ['taskset', '-c', '0',
                             *cold.argv_for(definition['family'], definition['phase'],
                                            definition['iterations'], 'unwind')[5:]]
        else:
            expected_argv = ['taskset', '-c', '0', plan['wasmtime']['path'], '-C',
                             'cache=n,collector=copying', '-W',
                             'all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y',
                             plan['fixtures'][item['fixture']]['path']]
        require(item['argv'] == expected_argv, 'Original command/feature/policy/fixture bytes changed')


def semantic(cold, plan, item, log, code):
    definition = plan['fixtures'][item['fixture']]['definition']
    result = {'passed': False, 'exit_zero': code == 0, 'family': definition['family'],
              'phase': definition['phase'], 'iterations': definition['iterations'],
              'collector_qualified': False, 'mutation_lookup_qualified': False,
              'failures': [], 'formal_acceptance': False,
              'checksum_proof': 'Exact-byte independent copying run checksum in pinned cold receipt; self-checking _start must exit0.'}
    if item['profile'] == WT_PROFILE:
        result['passed'] = code == 0
        return result
    try:
        require(code == 0, 'Actual product did not exit0')
        value = cold.parse_log(log.read_text(errors='replace'), definition['expected'], definition['phase'])
        result.update(value)
        result['guest_execution_ns'] = value['internal_timers']['wasm_execution_ns']
        result['whole_process_reported_ns'] = value['internal_timers']['reported_process_ns']
        result['mutation_lookup_qualified'] = value['field_lookup_qualified']
        result['passed'] = True
    except (RuntimeError, KeyError, OSError) as error:
        result['failures'].append(type(error).__name__ + ': ' + str(error))
    result['collector_qualified'] = result['collector_qualified'] and result['passed']
    result['mutation_lookup_qualified'] = result['mutation_lookup_qualified'] and result['passed']
    return result


def closure(sampler, cold, host, plan, libraries, out, stage):
    validate_plan(plan, sampler, cold)
    for value in [plan['product'], plan['wasmtime'], *plan['tools'], *plan['files'], *plan['provenance']]:
        sampler.verify_pin(value)
    for value in plan['fixtures'].values():
        sampler.verify_pin(value)
        require(sampler.read_pin(value['manifest']) == value['definition'], 'Fixture manifest changed')
    require(cold.bindings() == plan['actual_long_bindings'], 'Current source/product/long oracle binding changed')
    require(host.host_source_id(plan['source'], out / ('source-' + stage + '.json')) == cold.SID,
            'Actual R5 source changed')
    require(os.environ.get('LD_LIBRARY_PATH', '') == plan['LD_LIBRARY_PATH'] == libraries['LD_LIBRARY_PATH'] and
            os.environ.get('RAYON_NUM_THREADS') == '1' and not os.environ.get('LD_PRELOAD') and
            not os.environ.get('LD_AUDIT'), 'Actual loader/thread environment changed')
    for value in libraries['files']:
        sampler.verify_pin(value)
        require(str(Path(value['path']).resolve(strict=True)) == value['real_path'], 'Actual DSO path changed')


def stopped_environment_view(hw, receipts):
    # Rebind only Popen for the immutable spawn function. No global environment
    # mutation and no weaker guard. EOF/PIDFD/bootstrap protocol stays exact.
    globals_ = dict(hw.spawn_stopped.__globals__)
    def popen(*args, **kwargs):
        require('env' not in kwargs, 'Frozen spawn unexpectedly supplies its own environment')
        environment = dict(os.environ)
        removed = CAPTURE_KEY in environment
        environment.pop(CAPTURE_KEY, None)
        kwargs['env'] = environment
        # Allocate/audit before Popen: failure to append cannot orphan a child
        # that the immutable spawn function has not yet received.
        record = {'pid': None, 'removed_inherited_capture_key': removed,
                  'explicit_child_environment_sha256': hashlib.sha256(
                      b'\0'.join(os.fsencode(k + '=' + v) for k, v in sorted(environment.items()))).hexdigest()}
        receipts.append(record)
        process = subprocess.Popen(*args, **kwargs)
        record['pid'] = process.pid # Existing key; original spawn receives this exact child.
        return process
    globals_['subprocess'] = types.SimpleNamespace(**dict(subprocess.__dict__, Popen=popen))
    spawned = types.FunctionType(hw.spawn_stopped.__code__, globals_, '_unchanged_owned_stopped_spawn')
    def spawn(base, guard, role, argv, stream, inherited=()):
        entry = spawned(base, guard, role, argv, stream, inherited)
        try:
            require(entry['pidfd'] is not None and
                    not select.select([entry['pidfd']], [], [], 0)[0], 'Stopped original child retired')
            row = base.ident(entry['process'].pid)
            require(row['birth'] == entry['birth'] and row['state'] == 'T' and
                    row['cgroup'] == guard.cg and row['uid'] == [1000] * 4,
                    'Actual stopped child identity changed')
            raw = (Path('/proc') / str(entry['process'].pid) / 'environ').read_bytes()
            entries = [v for v in raw.split(b'\0') if v]
            require(all(b'=' in v for v in entries), 'Malformed actual stopped environment')
            pairs = [v.split(b'=', 1) for v in entries]
            environment = {os.fsdecode(k): os.fsdecode(v) for k, v in pairs}
            require(len(environment) == len(pairs) and CAPTURE_KEY not in environment,
                    'Actual stopped child capture variable present/duplicate env key')
            require(environment.get('LD_LIBRARY_PATH') == os.environ['LD_LIBRARY_PATH'] and
                    environment.get('RAYON_NUM_THREADS') == '1', 'Actual stopped loader/thread environment differs')
            witness = {'role': role, 'pid': entry['process'].pid, 'original_birth': entry['birth'],
                       'state': row['state'], 'cgroup': row['cgroup'], 'uid': row['uid'],
                       'capture_present': False, 'actual_environ_sha256': hashlib.sha256(raw).hexdigest(),
                       'actual_loader_matches_plan': True, 'actual_RAYON_NUM_THREADS': '1',
                       'stopped_before_guest_GO': True, 'original_PIDFD_live': True}
            next(v for v in reversed(receipts) if v['pid'] == entry['process'].pid).update(witness)
            return entry
        except BaseException:
            # The caller has not received this entry. This exact unreaped owned
            # Popen/PIDFD child must be retired here; no numeric PID signal.
            oldmask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT, signal.SIGTERM})
            try:
                hw.signal_owned(entry, signal.SIGKILL)
                _, status, _ = os.wait4(entry['process'].pid, 0)
                entry['process'].returncode = os.waitstatus_to_exitcode(status)
                entry['reaped'] = True
                if entry['pidfd'] is not None:
                    os.close(entry['pidfd'])
                    entry['pidfd'] = None
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, oldmask)
            raise
    return types.SimpleNamespace(**dict(hw.__dict__, spawn_stopped=spawn))


def r5_terminal_guard(sampler, host, base, admission, out, plan):
    # Keep both immutable checks intact. Only these two observed exit races can
    # trigger a fresh, bounded original-PIDFD observation and a full recheck.
    captured = {}
    observed = types.SimpleNamespace(**base.__dict__)
    def capture_ident(pid):
        row = base.ident(pid)
        snapshot = {'row': row}
        if row['argv'] == [] and row['rss'] == 0:
            try:
                snapshot['actual_stat'] = sampler.actual_mm_stat((Path('/proc') / str(pid) / 'stat').read_text())
            except FileNotFoundError:
                snapshot['actual_stat_absent'] = True
        captured[pid] = snapshot
        return row # Original fields; this observer never changes proc data.
    observed.ident = capture_ident
    guard = sampler.observed_guard(host, observed, admission, out)
    original_check = guard.check
    serialized = threading.RLock()
    recoveries = [0]
    def check():
        with serialized:
            try:
                return original_check()
            except RuntimeError as initial:
                text = str(initial)
                require(text in ('Owned command changed', 'Missing executable is not proved retired'), text)
                eligible = []
                for entry in guard.owned:
                    if not isinstance(entry.get('pidfd'), int) or entry['pidfd'] < 0 or entry['cpu'] != '0':
                        continue
                    if text == 'Owned command changed':
                        if entry['role'] != 'reference' or entry['actual_exec'] is not None or \
                           str(entry['expected_exe']) != plan['wasmtime']['path']:
                            continue
                    else:
                        if entry['role'] != 'guest' or not isinstance(entry['actual_exec'], dict) or \
                           str(entry['expected_exe']) != plan['product']['path']:
                            continue
                    eligible.append(entry)
                require(len(eligible) == 1, text + ': no unique eligible original child')
                entry = eligible[0]
                pid, original_fd = entry['process'].pid, entry['pidfd']
                initial_snapshot = captured[pid]
                initial_row = initial_snapshot['row']
                require(initial_row['pid'] == pid and initial_row['birth'] == entry['birth'] and
                        initial_row['ppid'] == os.getpid() and initial_row['pgid'] == pid and
                        initial_row['uid'] == [1000] * 4 and initial_row['cgroup'] == guard.cg and
                        initial_row['cpus'] == '0', 'Initial failed observation identity changed')
                if text == 'Owned command changed':
                    initial_stat = initial_snapshot.get('actual_stat')
                    require(sampler.cleared_owned_row(entry, initial_row, guard.cg, os.getpid()) and
                            isinstance(initial_stat, dict) and
                            sampler.same_owned_stat(entry, initial_stat, os.getpid()) and
                            len(initial_stat['mm']) == 12 and all(v == 0 for v in initial_stat['mm'].values()),
                            'Observed live argv/MM violation remains hard')
                else:
                    require(initial_row['argv'] == entry['exec_argv'],
                            'Observed product command violation remains hard')
                admitted = entry['admitted']
                require(admitted['pid'] == pid and admitted['birth'] == entry['birth'] and
                        admitted['ppid'] == os.getpid() and admitted['pgid'] == pid and
                        admitted['uid'] == [1000] * 4 and admitted['cgroup'] == guard.cg and
                        admitted['cpus'] == '0', 'Original stopped admission changed')
                if entry['role'] == 'guest':
                    actual = entry['actual_exec']
                    require(actual['pid'] == pid and actual['birth'] == entry['birth'] and
                            actual['ppid'] == os.getpid() and actual['pgid'] == pid and
                            actual['uid'] == [1000] * 4 and actual['cgroup'] == guard.cg and
                            actual['cpus'] == '0' and
                            actual['argv'] == [os.fsdecode(v) for v in entry['exec_argv']] and
                            actual['executable'] == str(entry['expected_exe']),
                            'Product actual loaded identity changed')
                started = time.perf_counter_ns()
                deadline = started + 2_000_000_000
                audit = {'initial_guard_error': text, 'pid': pid, 'original_birth': entry['birth'],
                         'original_pidfd': original_fd, 'role': entry['role'], 'started_ns': started,
                         'deadline_ns': deadline, 'actual_exec': entry['actual_exec'],
                         'actual_exec_synthesized': False, 'initial_failed_row': base.json_ident(initial_row),
                         'initial_actual_stat': initial_snapshot.get('actual_stat'),
                         'observations': [], 'terminal_confirmed': False,
                         'complete_original_check_repeated': False}
                try:
                    while True:
                        require(entry['pidfd'] == original_fd and time.perf_counter_ns() <= deadline,
                                'Original FD changed or terminal observation exceeded2s')
                        ready = bool(select.select([original_fd], [], [], 0)[0])
                        if entry['reaped']:
                            # This flag is assigned by the unchanged sampler only
                            # after wait4 returned for this exact Popen child.
                            require(ready and type(entry['process'].returncode) is int,
                                    'Reaped original child lacks PIDFD/actual wait4 status')
                            audit['terminal_kind'] = 'actual_original_wait4_reaped'
                            audit['actual_wait4_returncode'] = entry['process'].returncode
                            break
                        try:
                            row = base.ident(pid)
                            require(sampler.cleared_owned_row(entry, row, guard.cg, os.getpid()),
                                    'Fresh terminal candidate has live/wrong ownership or argv')
                            stat = sampler.actual_mm_stat((Path('/proc') / str(pid) / 'stat').read_text())
                        except FileNotFoundError:
                            # Missing reads only become terminal proof with the
                            # original FD ready plus a second real absent stat,
                            # or same-birth zero-MM Z. Never substitute a row.
                            ready = bool(select.select([original_fd], [], [], 0)[0])
                            require(ready, 'Missing terminal candidate not original-PIDFD retired')
                            try:
                                stat = sampler.actual_mm_stat((Path('/proc') / str(pid) / 'stat').read_text())
                            except FileNotFoundError:
                                audit['terminal_kind'] = 'actual_stat_absent_original_PIDFD_ready'
                                break
                            require(sampler.same_owned_stat(entry, stat, os.getpid()) and
                                    stat['state'] == 'Z' and len(stat['mm']) == 12 and
                                    all(v == 0 for v in stat['mm'].values()), 'Partial terminal stat changed')
                            audit['observations'].append({'actual_stat': stat, 'original_PIDFD_ready': ready})
                            audit['terminal_kind'] = 'actual_Z_after_partial_missing_read'
                            break
                        require(sampler.same_owned_stat(entry, stat, os.getpid()) and len(stat['mm']) == 12 and
                                all(v == 0 for v in stat['mm'].values()), 'Fresh terminal MM/birth/ancestry changed')
                        ready = bool(select.select([original_fd], [], [], 0)[0])
                        if len(audit['observations']) < 32:
                            audit['observations'].append({'row': base.json_ident(row), 'actual_stat': stat,
                                                          'original_PIDFD_ready': ready})
                        if row['state'] == stat['state'] == 'Z' and ready:
                            audit['terminal_kind'] = 'actual_Z_original_PIDFD_ready'
                            break
                        time.sleep(min(.005, max(0, (deadline-time.perf_counter_ns()) / 1_000_000_000)))
                    require(entry['pidfd'] == original_fd and time.perf_counter_ns() <= deadline,
                            'Actual terminal proof exceeded2s or original FD changed')
                    audit['terminal_confirmed'] = True
                    # No cached executable, synthetic Z or overwritten argv is
                    # returned to HostGuard. Re-run all original resource,
                    # roster, PIDFD, TID and identity checks on fresh real data.
                    result = original_check()
                    audit['complete_original_check_repeated'] = True
                    return result
                finally:
                    audit['finished_ns'] = time.perf_counter_ns()
                    recoveries[0] += 1
                    if recoveries[0] <= 16:
                        save(out / ('R5-terminal-observation-' + str(recoveries[0]) + '-' + str(started) + '.json'), audit)
    guard.check = check
    return guard


def run(args):
    sampler, cold = dependencies()
    host = load('_unchanged_host_counter', HERE / 'run_current_host_pcore_hw_counting.py', HOST_SHA)
    hw, hw_path = host.fixed_module('_unchanged_hw_protocol', 'run_current_pcore_hw_counting.py', HW_SHA)
    original, base_path = host.fixed_module('_unchanged_base_parser', 'run_current_pcore_diagnostic.py', BASE_SHA)
    plan = json.loads(args.plan.read_text())
    validate_plan(plan, sampler, cold)
    admission = json.loads(args.admission.read_text())
    libraries = json.loads(args.perf_libraries.read_text())
    host.validate_raw_inventory(admission, libraries)
    require(digest(admission['trusted_wrapper']['path']) == admission['trusted_wrapper']['sha256'],
            'Actual scope wrapper changed')
    input_files = [Path(__file__), HERE / 'run_current_general_gc.py',
                   HERE / 'prepare_general_gc_R5_nativeTLS_long_cold.py',
                   HERE / 'run_current_host_pcore_hw_counting.py', hw_path, base_path,
                   args.plan, args.admission, args.perf_libraries, Path(admission['trusted_wrapper']['path'])]
    input_shas = {str(path): digest(path) for path in input_files}
    require(not args.out.exists(), 'Fresh output required')
    args.out.mkdir(parents=True)
    for name, value in (('plan.json', plan), ('admission.json', admission), ('perf-libraries.json', libraries)):
        save(args.out / name, value)
    for name, path in (('runner.py', Path(__file__)), ('sampler.py', HERE / 'run_current_general_gc.py'),
                       ('host.py', HERE / 'run_current_host_pcore_hw_counting.py'),
                       ('protocol.py', hw_path), ('base.py', base_path)):
        atomic(args.out / name, path.read_bytes())
    require(args.execute, 'Explicit reviewed --execute required')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    def cancel(signum, frame):
        raise KeyboardInterrupt('Controlled cancellation ' + str(signum))
    signal.signal(signal.SIGTERM, cancel)
    guard = r5_terminal_guard(sampler, host, original, admission, args.out, plan)
    adapted = host.HostBase(original, guard.root)
    base = types.SimpleNamespace(**original.__dict__)
    base.telemetry = adapted.telemetry
    base.effective_argv = lambda item: list(item['argv'])
    receipt = lambda item, log, code: semantic(cold, plan, item, log, code)
    base.semantic_receipt = receipt
    aux = types.SimpleNamespace(semantic_receipt=receipt)
    rows = []
    before_ok = after_ok = complete = False
    error = None
    pmu_before = pmu_after = None
    try:
        closure(sampler, cold, host, plan, libraries, args.out, 'before')
        guard.check()
        before_ok = True
        selected = plan['commands'] if args.measurement == 'unprofiled' else [
            row for row in plan['commands'] if row['profile'] == PROFILE and
            plan['fixtures'][row['fixture']]['definition']['iterations'] == 2000000]
        if args.measurement == 'hardware':
            pmu_before = host.pmu_receipt()
            save(args.out / 'pmu-before.json', pmu_before)
            perf = Path(admission['host_perf']['path']).resolve(strict=True)
            require(perf.read_bytes()[:4] == b'\x7fELF' and digest(perf) == admission['host_perf']['sha256'],
                    'Actual direct perf ELF changed')
            require(any(v['real_path'] == str(perf) and v['sha256'] == digest(perf)
                        for v in libraries['files']), 'Actual perf DSO closure absent')
        for pair in range(args.pairs):
            for index, item in enumerate(selected if pair % 2 == 0 else selected[::-1]):
                out = args.out / (f'{pair}-{index}-' + item['fixture'] + '-' + item['profile'].replace('/', '-'))
                out.mkdir()
                environment_receipts = []
                current_hw = stopped_environment_view(hw, environment_receipts)
                if args.measurement == 'unprofiled':
                    row = sampler.plain_sample(base, current_hw, guard, item, out, receipt)
                else:
                    signals = []
                    globals_ = dict(hw.measure.__globals__, EVENT_GROUP=host.RAW_GROUP,
                                    spawn_stopped=current_hw.spawn_stopped,
                                    parse_stat=lambda stat, debug='': host.normalized_math(hw, stat, debug),
                                    signal_owned=lambda entry, sig: host.submit_owned_signal(hw, base, guard, entry, sig, signals))
                    measure = types.FunctionType(hw.measure.__code__, globals_, '_unchanged_hw_measure')
                    row = measure(base, aux, guard, item, perf, out)
                    row['owned_perf_sigint_receipts'] = signals[:]
                    perf_log = out / (row['label'] + '.perf.log')
                    attrs = host.final_attributes(perf_log.read_text(errors='replace'),
                                                  row['guest_admission']['pid']) if row['guest_admission'] else {
                                                      'passed': False, 'failures': ['No actual guest']}
                    counts = row['hardware_counting']
                    prior = counts['failures'][:]
                    counts['final_guest_attributes'] = attrs
                    counts['failures'] = [v for v in prior if not v.startswith('Awaiting actual final-guest')] + attrs['failures']
                    completion = host.perf_completion(row)
                    row['actual_perf_completion'] = completion
                    counts['counter_quality_passed'] = counts.get('math_quality_passed', False) and attrs['passed'] and completion['passed']
                    counts['failures'] += completion['failures']
                    row['quality_failures'] = [v for v in row['quality_failures'] if v not in prior] + counts['failures']
                row.update(pair=pair, measurement_family=args.measurement, source_id_external=cold.SID,
                           product_sha256=cold.PRODUCT_SHA, native_TLS_all_TUs=True,
                           child_environment_receipts=environment_receipts,
                           host_smt_noise='unknown without independent actual observer',
                           input_sha256=input_shas)
                save(out / 'qualified.json', row)
                rows.append(row)
                with (args.out / 'rows.jsonl').open('a') as stream:
                    stream.write(json.dumps(row, allow_nan=False) + '\n')
                require(not row['hard_failures'], 'Actual hard ownership/resource/deadline failure')
                require(row['semantic_receipt']['passed'], 'Actual guest/managed counters qualification failed')
                require(environment_receipts and all(v.get('capture_present') is False and
                        v.get('stopped_before_guest_GO') is True for v in environment_receipts),
                        'Actual stopped capture-clear witness missing')
                if args.measurement == 'hardware':
                    require(row['actual_perf_completion']['passed'] and row['hardware_counting']['counter_quality_passed'],
                            'Actual owned perf protocol/final event attrs/exact enabled-running failed')
        closure(sampler, cold, host, plan, libraries, args.out, 'after')
        guard.check()
        if args.measurement == 'hardware':
            pmu_after = host.pmu_receipt()
            save(args.out / 'pmu-after.json', pmu_after)
            require(pmu_before == pmu_after, 'Actual PMU changed')
        require(all(digest(path) == sha for path, sha in input_shas.items()), 'Immutable inputs changed')
        after_ok = complete = True
    except BaseException as failure:
        error = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        pairs = []
        for name in plan['fixtures']:
            for pair in range(args.pairs):
                chosen = [row for row in rows if row['fixture'] == name and row['pair'] == pair]
                if not chosen:
                    continue
                freqs = [row['median_run_frequency_khz'] for row in chosen]
                matched = len(chosen) >= 2 and all(v and v > 0 for v in freqs) and max(freqs) / min(freqs) - 1 <= .1
                pairs.append({'fixture': name, 'pair': pair, 'frequency_matched': matched,
                              'host_smt_noise_qualified': False,
                              'all_sample_quality_passed': all(not row['quality_failures'] and
                                                              not row['hard_failures'] for row in chosen)})
        save(args.out / 'summary.json', {
            'schema': 'uwvm-r5-nativetls-general-gc-measurement-v1',
            'measurement_family': args.measurement, 'complete': complete,
            'closure_before_ok': before_ok, 'closure_after_ok': after_ok,
            'execution_error': error, 'rows': len(rows), 'input_sha256': input_shas,
            'source_id_external': cold.SID, 'product_sha256': cold.PRODUCT_SHA,
            'semantic_pass_count': sum(row['semantic_receipt']['passed'] for row in rows),
            'hardware_quality_pass_count': sum(row.get('hardware_counting', {}).get('counter_quality_passed', False)
                                               for row in rows), 'pairs': pairs,
            'formal_acceptance': False, 'temperature_policy': 'observation_only',
            'host_smt_noise': 'unknown without independent actual observer',
            'limitations': ['Current nativeTLS/default ordinary R5 product, not old nonTLS S6e A/B or ROS.',
                            'Internal Wasm timer excludes startup/JIT; includes setup/GC/fields/checksum, not pure collector ROI.',
                            'Parent wait4 and whole-single-TID pure HW include startup/JIT; distinct measurement scopes.',
                            'Mutation phase_pending/no-attempt is field/lookup only, not collector qualification.',
                            'VTune sampling/MUX remains a separate measurement; no reference-cycle event or SW fallback added.']})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--out-plan', type=Path, required=True)
    execute = sub.add_parser('run')
    for name in ('plan', 'admission', 'perf-libraries', 'out'):
        execute.add_argument('--' + name, type=Path, required=True)
    execute.add_argument('--measurement', choices=('unprofiled', 'hardware'), required=True)
    execute.add_argument('--pairs', type=int, choices=(1, 2), default=1)
    execute.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    if args.mode == 'prepare':
        make_plan(args.out_plan)
    else:
        run(args)


if __name__ == '__main__':
    main()
