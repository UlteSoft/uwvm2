#!/usr/bin/env python3
"""Four fixed R3c thread samples; immutable plain/PIDFD protocols, no counters.

Only the reviewed keeper may execute. This file does not compile, download,
connect through SSH, or modify the original guardian/sampler modules.
"""
import argparse
import decimal
import hashlib
import inspect
import json
import os
from pathlib import Path
import resource
import signal
import types

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
D = B/'builds/current-R3c-thread-build-cold-20261003-r4'
HERE = Path(__file__).resolve().parent
HELPER_SHA = 'a222edde5aad6bc9f7af2b835db86a7fb03af2ab9f08b9555bac49a73d3c1e71'
COLD_SHA = '9b02dd9197f50fc03eb9493b08b9110badb55d7c4e201c422e2446b242d020b9'
MANAGED_SHA = '23f58b55dcf8bf4e360c7e67ce8bba9d1383872694331b878f57b3daf607feeb'
R5_SHA = 'd96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb'
SAMPLER_SHA = '91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8'
HOST_SHA = '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5'
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
CAPTURE = 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'
CPUSET = '0,2,4,6,16-31'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path):
    return {'bytes': Path(path).stat().st_size, 'sha256': digest(path)}


def load(name, path, expected):
    data = Path(path).read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected, 'Frozen source changed: '+str(path))
    module = types.ModuleType(name)
    module.__file__ = str(path)
    exec(compile(data, str(path), 'exec'), module.__dict__)
    return module


def checked_replace(source, before, after):
    require(source.count(before) == 1, 'Reviewed private derivative anchor changed')
    return source.replace(before, after)


def process_cpu_allowed(entry, row):
    # Main/notifier starts with both CPUs, then the fixture pins itself to P0.
    return row['cpus'] in (('0',) if entry['cpu'] == '0' else ('0,2', '0')) and entry['cpu'] in ('0', '0,2')


def task_cpu_allowed(cpu, leader, tid, observed):
    if cpu == '0':
        return observed == '0'
    if cpu != '0,2':
        return False
    # Newly created waiter inherits the main affinity before its explicit P2
    # pin. Actual P2 observations are separately matched to printed waiter TIDs.
    return observed in (('0,2', '0') if tid == leader else ('0,2', '0', '2'))


def thread_guard_class(managed, host, base, sampler, out, environment):
    # Reuse the actual managed guard's full ownership/resource/env/retirement
    # checks. Only its two CPU-role call sites and strict worker CPU predicate
    # differ. No actual stat, argv, exe, task or PIDFD result is fabricated.
    source = inspect.getsource(managed.tid_snapshot)
    source = checked_replace(source, 'fields["Cpus_allowed_list"].strip()==cpu',
                             'task_cpu_allowed(cpu,int(proc.name),tid,fields["Cpus_allowed_list"].strip())')
    namespace = dict(managed.tid_snapshot.__globals__, task_cpu_allowed=task_cpu_allowed)
    exec(compile(source, '<private fixed thread TID observer>', 'exec'), namespace)
    source = inspect.getsource(managed.guard_class)
    source = checked_replace(source, 'row["cpus"]==entry["cpu"]=="0"', 'process_cpu_allowed(entry,row)')
    source = checked_replace(source, 'tid_snapshot(P("/proc")/str(pid),self.cg,"0",self.seen)',
                             'tid_snapshot(P("/proc")/str(pid),self.cg,entry["cpu"],self.seen)')
    namespace.update(process_cpu_allowed=process_cpu_allowed)
    exec(compile(source, '<private fixed thread roles>', 'exec'), namespace)
    return namespace['guard_class'](host, base, sampler, out, environment)


def spawn_view(hw, r5, cpu, receipts):
    require(cpu in ('0', '0,2'), 'Only fixed thread affinities are admissible')
    if cpu == '0':
        return r5.stopped_environment_view(hw, receipts)
    source = inspect.getsource(hw.spawn_stopped)
    source = checked_replace(source, "'cpu': '0' if role == 'guest' else '16'",
                             "'cpu': '0,2' if role == 'guest' else '16'")
    source = checked_replace(source, "argv[:3] == ['taskset', '-c', '0']",
                             "argv[:3] == ['taskset', '-c', '0,2']")
    namespace = dict(hw.spawn_stopped.__globals__)
    exec(compile(source, '<private fixed parked spawn>', 'exec'), namespace)
    changed = types.SimpleNamespace(**dict(hw.__dict__, spawn_stopped=namespace['spawn_stopped']))
    return r5.stopped_environment_view(changed, receipts)


def native_checksum(worker):
    mask = (1 << 32)-1
    seed = 0x12345+worker
    values = [(i+seed) & mask for i in range(1024)]
    for p in range(32):
        values = [((((v << 7) | (v >> 25)) & mask) ^ (seed+p)) + 0x9e3779b9 & mask for v in values]
    checksum = 0
    for value in values:
        checksum = ((((checksum << 5) | (checksum >> 27)) & mask)+value) & mask
    return checksum


def semantic(item, log, exit_code):
    result = dict(passed=False, failures=[], internal_rows=[], collector_roi=False,
                  single_tid_hardware_qualified=False, formal_acceptance=False)
    try:
        require(type(exit_code) is int and exit_code == 0, 'Actual fixture failed')
        lines = Path(log).read_text(errors='replace').splitlines()
        rows = [json.loads(line, parse_float=decimal.Decimal) for line in lines if line.startswith('{')]
        require(all(type(row) is dict for row in rows), 'Invalid actual thread JSON')
        if item['kind'] == 'creation':
            require(len(rows) == 108 and lines.count('PASS real VM thread entry/checksum/markers; shared memory + atomic.fence; creation counter not instrumented') == 1,
                    'Actual timed creation cells/probe-free marker differ')
            keys = ('wall_ns','thread_constructor_sum_ns','join_sum_ns','start_latency_ns','admission_ns','guest_interval_ns','release_ns')
            expected = {(path, workers, sample, threaded) for path in ('std_native','vm_full_entry','vm_raw_entry')
                        for workers in (1,4) for sample in range(9) for threaded in (False,True)}
            found = set()
            checksums = {workers: sum(native_checksum(i) for i in range(workers))*1024 for workers in (1,4)}
            for row in rows:
                require(set(row) == {'path','workers','threaded','sample','rounds','checksum','native_creations',*keys}, 'Creation JSON shape')
                require(type(row['workers']) is type(row['sample']) is type(row['rounds']) is int and type(row['threaded']) is bool,
                        'Creation discrete types')
                cell = (row['path'],row['workers'],row['sample'],row['threaded'])
                require(cell in expected and cell not in found, 'Unexpected/duplicate creation cell')
                found.add(cell)
                require(row['rounds'] == 1024 and type(row['checksum']) is int and row['checksum'] == checksums[row['workers']] and
                        type(row['native_creations']) is int and row['native_creations'] == 0, 'Creation checksum/probe/profile differs')
                for key in keys:
                    require(type(row[key]) in (int,decimal.Decimal), 'Creation timer type invalid')
                    value = decimal.Decimal(row[key]); require(value.is_finite() and value >= 0, 'Creation timer invalid')
                require(row['wall_ns'] > 0 and row['guest_interval_ns'] > 0, 'Actual creation/work timer absent')
                if not row['threaded']:
                    require(row['thread_constructor_sum_ns'] == row['join_sum_ns'] == 0, 'Synchronous fixture claims thread creation')
            require(found == expected, 'Missing creation cell')
            result['scope'] = 'Per-round creation/work/join/TLS-drain wall; per-worker admission/body/release; warmup/JIT excluded'
        else:
            require(len(rows) == 1152 and lines.count('PASS Linux parked guest wait32 -> guest notify; stat S/futex wchan before notify=1; 9 samples x 128 wakeups, 2 warmups') == 1,
                    'Actual parked samples/marker differ')
            expected = {(sample, r) for sample in range(9) for r in range(128)}
            found = set(); waiters = {}
            keys = {'policy','sample','round','wake_ns','notify_call_ns','qualification_ns','qualification_probes',
                    'empty_notify_retries','waiter_tid','parked_task_state','notify_return','parked_wchan'}
            for row in rows:
                require(set(row) == keys and all(type(row[k]) is int for k in keys-{'policy','parked_task_state','parked_wchan'}), 'Parked shape/types differ')
                cell = (row['sample'],row['round']); require(cell in expected and cell not in found, 'Parked duplicate/unknown cell'); found.add(cell)
                require(row['policy'] == item['trace'] and row['parked_task_state'] == 'S' and row['notify_return'] == 1 and
                        type(row['parked_wchan']) is str and 'futex' in row['parked_wchan'] and row['waiter_tid'] > 0,
                        'Missing actual parked-futex/notify/policy proof')
                require(row['wake_ns'] > 0 and row['notify_call_ns'] >= 0 and row['qualification_ns'] > 0 and
                        row['qualification_probes'] > 0 and row['empty_notify_retries'] >= 0, 'Invalid parked timers/observations')
                require(waiters.setdefault(row['sample'],row['waiter_tid']) == row['waiter_tid'], 'Waiter changed within sample')
            require(found == expected, 'Missing parked cell')
            result['actual_printed_waiter_tids'] = sorted(set(waiters.values()))
            result['scope'] = 'Actual parked notify-to-return and notify-call ROI; qualification/create/setup excluded'
        result.update(passed=True, internal_rows=[{k: str(v) if isinstance(v,decimal.Decimal) else v for k,v in row.items()} for row in rows])
    except (RuntimeError,ValueError,OSError,decimal.InvalidOperation,TypeError) as error:
        result['failures'].append(type(error).__name__+': '+str(error))
    return result


def helper_namespace():
    path = D/'closure.py'; require(digest(path) == HELPER_SHA, 'Actual r4 source helper differs')
    source = path.read_text(); boundary='\nmode=sys.argv[1]\n'
    require(source.count(boundary) == 1, 'Actual r4 top-level mode boundary differs')
    namespace = {'__name__':'thread_actual_r4_binding_only'}
    exec(compile(source.split(boundary,1)[0],str(path),'exec'),namespace)
    return namespace


def closure(namespace, out, stage, r5):
    require(digest(D/'cold-after.json') == COLD_SHA, 'Actual r4 functional closure changed')
    after = json.loads((D/'cold-after.json').read_bytes()); before = json.loads((D/'cold-before.json').read_bytes())
    require(after['passed'] is True and after['actual_build_stages'] == after['actual_cold_stages'] == 6 and
            after['timed_ELF_creation_probe_undefined'] is True and after['actual_symbols_qualified'] is True,
            'Missing actual creator/official validation/probe-free qualification')
    require(namespace['prepared']() == before['prepared'] and namespace['inputs']() == before['inputs'], 'Actual current source/SDK/tool inputs changed')
    require(json.loads((D/'source-before.json').read_bytes()) == json.loads((D/'source-after.json').read_bytes()) == before['source_fingerprint'], 'Actual source fingerprint changed')
    objects = {}; elfs = {}
    for name in namespace['NAMES']:
        bound = json.loads((D/(name+'.executable-bind.json')).read_bytes())
        objects[name] = namespace['object_check'](name)
        require(pin(D/name) == bound['output'] and objects[name] == bound['object'] and namespace['stage'](name+'-link') == bound['link'], 'Actual linked fixture/MD differs')
        elfs[name] = pin(D/name)
    for label,proof in after['cold_proofs'].items(): require(namespace['stage'](label) == proof, 'Actual creator/official cold proof differs')
    record = {'source':before['source_fingerprint']['source_id'],'objects':objects,'ELFs':elfs,
              'inputs':namespace['inputs'](),'prepared':namespace['prepared'](),'actual_cold_after':pin(D/'cold-after.json')}
    r5.save(out/('closure-'+stage+'.json'),record)
    return record


def commands(out):
    items = []
    for trace in ('unwind','instruction'):
        items.append(dict(label='thread-timing-'+trace,kind='creation',trace=trace,profile='ordinary/'+trace+'/thread-create',
            fixture='thread-create',cpu='0',argv=['taskset','-c','0',str(D/'thread-timed'),trace,'9','1024',str(out/('timed-'+trace+'.wasm'))]))
        items.append(dict(label='parked-timing-'+trace,kind='parked',trace=trace,profile='ordinary/'+trace+'/thread-parked',
            fixture='thread-parked',cpu='0,2',argv=['taskset','-c','0,2',str(D/'parked'),trace]))
    return items


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('admission','perf-libraries','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--execute',action='store_true'); args = parser.parse_args()
    require(args.execute and not args.out.exists(), 'Reviewed keeper execution and fresh output required')
    args.out.mkdir(parents=True)
    host = load('_thread_host',HERE/'run_current_host_pcore_hw_counting.py',HOST_SHA)
    hw = load('_thread_hw_protocol',HERE/'run_current_pcore_hw_counting.py',HW_SHA)
    original = load('_thread_base',HERE/'run_current_pcore_diagnostic.py',BASE_SHA)
    sampler = load('_thread_plain',HERE/'run_current_general_gc.py',SAMPLER_SHA)
    managed = load('_thread_managed',HERE/'run_managed_general_measurement.py',MANAGED_SHA)
    r5 = load('_thread_environment',HERE/'run_general_gc_R5_nativeTLS.py',R5_SHA)
    namespace = helper_namespace()
    admission = json.loads(args.admission.read_bytes()); libraries = json.loads(args.perf_libraries.read_bytes())
    host.validate_raw_inventory(admission,libraries)
    require(digest(admission['trusted_wrapper']['path']) == admission['trusted_wrapper']['sha256'], 'Actual current wrapper changed')
    for value in libraries['files']:
        require(Path(value['real_path']).stat().st_size == value['bytes'] and digest(value['real_path']) == value['sha256'], 'Actual static DSO closure changed')
    environment = dict(os.environ)
    require(environment.get('RAYON_NUM_THREADS') == '1' and environment.get('UWVM_TEST_CPUSET') == CPUSET and
            environment.get('PYTHONDONTWRITEBYTECODE') == '1', 'Actual fixed launch environment differs')
    loader = next(v.split('=',1)[1] for v in namespace['prepared']()['prefix'] if v.startswith('LD_LIBRARY_PATH='))
    require(environment.get('LD_LIBRARY_PATH') == loader and not any(environment.get(k) for k in ('LD_PRELOAD','LD_AUDIT','LD_DEBUG')), 'Loader environment differs')
    environment.pop(CAPTURE,None)
    guard_type = thread_guard_class(managed,host,original,sampler,args.out,environment)
    guard = guard_type(admission); adapted = host.HostBase(original,guard.root)
    base = types.SimpleNamespace(**dict(original.__dict__,telemetry=adapted.telemetry))
    private_plain = types.FunctionType(sampler.plain_sample.__code__,dict(sampler.plain_sample.__globals__,plain_role=lambda item:'reference'),'thread_plain_sample')
    dependency_files = tuple(HERE/name for name in ('run_managed_general_measurement.py','run_general_gc_R5_nativeTLS.py',
        'run_current_general_gc.py','run_current_host_pcore_hw_counting.py','run_current_pcore_hw_counting.py','run_current_pcore_diagnostic.py'))
    inputs = {str(p):digest(p) for p in (Path(__file__),args.admission,args.perf_libraries,D/'closure.py',D/'cold-after.json',
        Path(admission['trusted_wrapper']['path']),*dependency_files)}
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def cancel(sig,frame): raise KeyboardInterrupt('Controlled thread cancellation '+str(sig))
    signal.signal(signal.SIGTERM,cancel)
    rows=[]; error=None; before_ok=after_ok=complete=False
    try:
        before = closure(namespace,args.out,'before',r5); guard.check(); before_ok=True
        selected = commands(args.out); r5.save(args.out/'commands.json',selected)
        for index,item in enumerate(selected):
            out = args.out/(str(index)+'-'+item['label']); out.mkdir()
            receipts=[]; current_hw=spawn_view(hw,r5,item['cpu'],receipts)
            current_base=types.SimpleNamespace(**dict(base.__dict__,BOOT=original.BOOT.replace('{0}','{0,2}') if item['cpu']=='0,2' else original.BOOT))
            unknown_before=guard.unknown_count; guard.tids.clear(); guard.seen={}
            row=private_plain(current_base,current_hw,guard,item,out,semantic)
            row.update(measurement_family='multi-TID-unprofiled-only',thread_role=item['kind'],child_environment_receipts=receipts,
                actual_task_observations=list(guard.tids),vanished_TID_coverage_unknown_count=guard.unknown_count-unknown_before,
                source_id_external=before['source'],input_sha256=inputs,host_smt_noise='unknown without independent observer')
            if row['vanished_TID_coverage_unknown_count']: row['quality_failures'].append('Unqualified complete worker coverage: vanished TIDs retained as unknown')
            if item['kind']=='parked' and row['semantic_receipt']['passed']:
                printed=set(row['semantic_receipt']['actual_printed_waiter_tids'])
                observed={task['tid'] for snapshot in guard.tids for task in snapshot['actual_tids']
                          if any(line.startswith('Cpus_allowed_list:') and line.split(':',1)[1].strip()=='2'
                                 for line in task['status_raw'].splitlines())}
                row['printed_waiters_observed_on_P2']=sorted(printed & observed)
                row['printed_waiters_CPU_coverage_unknown']=sorted(printed-observed)
                if printed-observed: row['quality_failures'].append('Printed waiter CPU witness missing; bounded observer coverage remains unknown')
            if item['kind']=='creation':
                require(pin(args.out/('timed-'+item['trace']+'.wasm')) == pin(D/('qualifier-'+item['trace']+'.wasm')), 'Actual timed/generated validated Wasm bytes differ')
            row['timing_quality_passed']=not row['hard_failures'] and not row['quality_failures'] and row['semantic_receipt']['passed']
            r5.save(out/'qualified.json',row); rows.append(row)
            with (args.out/'rows.jsonl').open('a') as stream: stream.write(json.dumps(row,allow_nan=False)+'\n')
            require(not row['hard_failures'] and row['semantic_receipt']['passed'], 'Actual thread ownership/resource/selfcheck failure')
        require(closure(namespace,args.out,'after',r5) == before, 'Actual source/MD/ELF closure changed')
        guard.check(); require(all(digest(p)==v for p,v in inputs.items()), 'Actual frozen measurement input changed')
        after_ok=complete=True
    except BaseException as failure:
        error=type(failure).__name__+': '+str(failure); raise
    finally:
        r5.save(args.out/'summary.json',dict(complete=complete,closure_before_passed=before_ok,closure_after_passed=after_ok,
            actual_plain_rows=len(rows),semantics_pass_count=sum(r['semantic_receipt']['passed'] for r in rows),
            timing_quality_pass_count=sum(r.get('timing_quality_passed',False) for r in rows),error=error,formal_acceptance=False,
            temperature_policy='observation_only',hardware_counter_path=False,whole_wait4_scope='All process TIDs including stopped bootstrap; GO-to-reap wall separately',
            actual_thread_snapshot_coverage='Vanished workers are unknown, never fabricated retirement or CPU proof'))


if __name__ == '__main__':
    main()
