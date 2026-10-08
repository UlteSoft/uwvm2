#!/usr/bin/env python3
"""Independent current S6e GeneralGC adapter; sole Linux keeper executes.

Reuses frozen ownership/counting code, never its old fixed-workload main.
No build, SSH, administration, software counters, sampling, or cgroup mutation.
"""
import argparse, collections, decimal, hashlib, json, os, pathlib, re, resource, select, shlex
import signal, statistics, threading, time, types
P = pathlib.Path
HOST_SHA = '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5'
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
SOURCE_ID = 'sha256:6e05059af0b6c2a46ed4476d82920d91636dcda66dc6e59360e0560a4fe05131'
PRODUCT_SHA = 'b5f39dfe70e08a308a0cc415dd3e904386dd069d24a0cf330c75f860c0026830'
GENERATOR_SHA = 'eb6048d1d1fe7ad0f3264af41cc742b5a89ee17c2ca720c891ac5571fef9471e'
RESPONSE_SHA = 'b3ad1de4c162c22476fb8cbb974c52d23818bfd1f30c17328229c52449224287'
RESPONSE_BYTES = 8724
PROFILE = 'ordinary/unwind/S6e'
FAMILIES = ('mutable-struct', 'reference-cycle', 'numeric-array', 'reference-array')
PHASES = ('allocate', 'mutate')
SIX_SET32 = ('COMPACT_NUMERIC', 'MANAGED_NUMERIC_PAGE', 'SEALED_COMPACT_CURSOR',
            'SEALED_LOCAL_TABLE', 'PENDING_NUMERIC_FUSED_CATCH', 'PACKED_NUMERIC_ARRAYS', 'NUMERIC_STRUCT_SET32')

def require(condition, reason):
    if not condition: raise RuntimeError(reason)

def digest(path):
    with P(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()

def pin(path):
    path = P(path).resolve(strict=True)
    require(path.is_file(), 'Not an actual file: '+str(path))
    return {'path':str(path), 'bytes':path.stat().st_size, 'sha256':digest(path)}

def verify_pin(value):
    require(isinstance(value,dict) and isinstance(value.get('path'),str) and P(value['path']).is_absolute() and
            '..' not in P(value['path']).parts and type(value.get('bytes')) is int and value['bytes'] >= 0 and
            isinstance(value.get('sha256'),str) and re.fullmatch('[0-9a-f]{64}',value['sha256']), 'Incomplete actual pin')
    require(P(value['path']).stat().st_size == value['bytes'] and digest(value['path']) == value['sha256'], 'Actual file changed: '+value['path'])

def save(path, value): P(path).write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')

def read_pin(value): verify_pin(value); return json.loads(P(value['path']).read_text())

def add_pin(table, path, expected):
    require(P(path).is_absolute() and isinstance(expected,dict) and 'sha256' in expected and 'bytes' in expected, 'Missing actual dependency pin')
    value = dict(path=path,sha256=expected['sha256'],bytes=expected['bytes'])
    verify_pin(value)
    require(path not in table or table[path] == value, 'Contradictory actual dependency: '+path)
    table[path] = value

def absolute_pin_maps(value, table):
    # Read real immutable build maps, not the display-only truncated shape file.
    if isinstance(value,dict):
        for key,item in value.items():
            if isinstance(key,str) and P(key).is_absolute() and isinstance(item,dict) and 'sha256' in item and 'bytes' in item:
                add_pin(table,key,item)
            absolute_pin_maps(item,table)
    elif isinstance(value,list):
        for item in value: absolute_pin_maps(item,table)

def response_tokens_contract(tokens):
    # This is the actual already-compiled S6e response, not a new compiler
    # profile. Only its release define and no-PCH-timestamp forwarding pair
    # are neutral exceptions; UWVM definitions/undefines remain forbidden.
    index=0
    while index<len(tokens):
        token=tokens[index]
        if token=='-Xclang':
            require(index+1<len(tokens) and tokens[index+1]=='-fno-pch-timestamp', 'Response compiler forwarding override')
            index+=2; continue
        if token.startswith('-D'):
            require(token=='-DNDEBUG','Response definition/profile override')
        else:
            require(not token.startswith(('-U','-B','@','/D','/U')) and token!='-Xpreprocessor', 'Response profile override/nested response')
        index+=1

def response_contract(data):
    require(len(data)==RESPONSE_BYTES and hashlib.sha256(data).hexdigest()==RESPONSE_SHA,
            'Different actual S6e response bytes')
    response_tokens_contract(shlex.split(data.decode(),posix=True))

def recipe(argv, files):
    require(isinstance(argv,list) and all(isinstance(x,str) for x in argv), 'Missing actual compile argv')
    actual = [x for x in argv if x.startswith('-DUWVM_EXPERIMENTAL_')]
    require(sorted(actual) == sorted('-DUWVM_EXPERIMENTAL_'+name+'=1' for name in SIX_SET32), 'Not the exact current six-plus-SET32 profile')
    require(not any(x.startswith('-DUWVM2_BUILD_SOURCE_ID') for x in argv), 'This baseline is externally bound, not single-TU embedded')
    require(not any(x in ('-D','-U','-Xclang','-Xpreprocessor') or x.startswith(('-U','-B')) for x in argv), 'Compiler profile override')
    for argument in argv:
        if not argument.startswith('@'): continue
        value = pin(argument[1:]); files[value['path']] = value
        response_contract(P(value['path']).read_bytes())

def fixture_contract(definition):
    require(isinstance(definition,dict) and definition.get('schema') == 'uwvm-general-gc-fixture-v1', 'Wrong general fixture')
    require(definition.get('family') in FAMILIES and definition.get('phase') in PHASES and
            definition.get('iterations') in (1000000,2000000) and definition.get('root_ring') == 1024 and
            definition.get('compact_numeric_eligible') is False, 'Wrong workload/candidate profile')
    expected = definition['expected']; n = definition['iterations']; k = {'mutable-struct':1,'reference-cycle':2,'numeric-array':1,'reference-array':3}[definition['family']]
    require(expected['guest_planned_allocations'] == k*(n if definition['phase']=='allocate' else 1024) and
            expected['final_root_group_count'] == 1024 and expected['final_reachable_objects'] == 1024*k and
            type(expected['return_checksum_u32']) is int and 0 <= expected['return_checksum_u32'] < 1<<32, 'Workload operation/checksum contract')
    require(expected.get('collection_count') is None and expected.get('reclaimed_count') is None, 'No invented runtime GC counts')

def prepare(cold, runtime_bind, runtime_receipts, output):
    require(not output.exists(), 'New plan path required')
    summary = json.loads((cold/'summary.json').read_text()); closure = json.loads((cold/'closure-before.json').read_text())
    commands = dict(json.loads((cold/'commands.json').read_text()))
    require(summary['source_id_external'] == SOURCE_ID and summary['embedded_source_id_enabled'] is False and
            summary['full_fixture_qualification'] is True and summary['cold_semantic_passed'] is True and
            summary['source_product_tools_before_after_equal'] is True and summary['all_official_roundtrip_bytes_equal'] is True, 'Cold qualification missing')
    actual = closure['product']; require(actual['source_id_external'] == SOURCE_ID and actual['product']['sha256'] == PRODUCT_SHA, 'Wrong current product')
    build = read_pin(actual['closure_after']); require(build['passed'] is True and build['source_dependency_before_after_equal'] is True and
            build['all_actual_dependencies_before_bound'] is True and build['target_config_site_before_present'] is True and
            build['source_id_external'] == SOURCE_ID and build['output']['sha256'] == PRODUCT_SHA and build['same_task_r3_runtime_reused'] is True and
            build['embedded_build_source_id_enabled'] is False and build['cache_required_explicitly_disabled'] is True, 'Actual eleven-stage closure failed')
    rb, rr = pin(runtime_bind), pin(runtime_receipts)
    require(rb['sha256'] == build['runtime']['artifact_bind_sha256'] and rr['sha256'] == build['runtime']['origin_receipts_sha256'], 'Actual runtime origin not bound')
    runtime = read_pin(rb); require(build['runtime']['origin_runtime_returncode'] == 0 and build['runtime']['source_id'] == SOURCE_ID, 'Runtime origin failed')
    source = commands['source-before'][2]; files = {}
    absolute_pin_maps(build,files); absolute_pin_maps(runtime,files)
    require(len(files) >= 2701, 'Missing full actual main/host/runtime dependency maps')
    for argv in [build['components'][name]['actual_compile_proof']['actual_argv'] for name in ('main','host-api')] + [build['runtime']['actual_compile_argv']]:
        recipe(argv,files)
    add_pin(files,build['runtime']['object_path'],build['runtime']['object'])
    files[build['runtime']['dependency_file_path']] = dict(path=build['runtime']['dependency_file_path'],sha256=build['runtime']['dependency_file_sha256'],bytes=P(build['runtime']['dependency_file_path']).stat().st_size)
    verify_pin(files[build['runtime']['dependency_file_path']])
    for name in ('main','host-api'):
        proof=build['components'][name]['actual_compile_proof']; require(proof['returncode'] == 0, 'Component compile failed')
        argv=proof['actual_argv']; require(argv.count('-o')==1,'Ambiguous component output'); add_pin(files,argv[argv.index('-o')+1],build['components'][name]['output'])
        require(argv.count('-MF')==1,'Missing actual component MD'); md=pin(argv[argv.index('-MF')+1]); require(md['sha256']==build['components'][name]['dependency_file_sha256'],'Component MD changed'); files[md['path']]=md
    require(build['actual_link_proof']['returncode'] == 0,'Actual link failed')
    tools=closure['tools']['tools']; tool_pins=[]
    for key,value in tools.items(): verify_pin(value); tool_pins.append(value)
    wt=next(v for v in tool_pins if P(v['path']).name=='wasmtime'); fixture_rows={}; selected=[]; environments=[]
    for cell in summary['cells']:
        definition=cell['fixture_manifest']; fixture_contract(definition)
        name=definition['family']+'-'+definition['phase']+'-'+str(definition['iterations'])
        require(name not in fixture_rows and cell['official_roundtrip_bytes_equal'] and cell['wasmtime_start_exit0'] and cell['product_unwind_exit0_selfchecks'], 'Incomplete/duplicate cold cell')
        manifest=pin(cold/'fixtures'/(name+'.json')); require(read_pin(manifest)==definition,'Manifest differs from cold oracle')
        for value in (cell['wasm'],cell['wat'],cell['roundtrip']): verify_pin(value)
        require(cell['wasm']['sha256']==cell['roundtrip']['sha256']==definition['wasm_sha256'],'Different original bytes')
        fixture_rows[name]=dict(cell['wasm'],manifest=manifest,definition=definition)
        for profile,suffix in ((PROFILE,'ordinary-unwind'),('wasmtime49/copying','wasmtime-start')):
            original=commands[name+'-'+suffix]; require(original[:1]==['env'] and original[1].startswith('LD_LIBRARY_PATH=') and original[2]=='RAYON_NUM_THREADS=1','Unknown cold environment')
            environments.append(original[1][len('LD_LIBRARY_PATH='):]); argv=original[3:]
            require(argv[-1]==cell['wasm']['path'],'Different cold fixture argument')
            if profile==PROFILE:
                require(argv[0]==actual['product']['path'] and '--log-verbose' in argv and '-Rclog' not in argv,'Wrong product baseline flags')
                argv=argv[:argv.index('--run')]+['-Rclog','err']+argv[argv.index('--run'):]
            selected.append(dict(fixture=name,profile=profile,argv=['taskset','-c','0',*argv]))
    require(len(fixture_rows)==16 and len(selected)==32 and len(set(environments))==1,'Expected sixteen exact long cases and environment')
    product_pin=actual['product']; verify_pin(product_pin)
    provenance=[pin(cold/name) for name in ('summary.json','closure-before.json','commands.json','source-before.json','source-after.json')]+[actual['closure_after'],actual['origin_receipts'],rb,rr]
    require(read_pin(provenance[3])==read_pin(provenance[4]),'Cold source changed')
    plan=dict(schema='uwvm-current-s6e-general-gc-plan-v1',execute_ready=True,source=source,source_id=SOURCE_ID,product=product_pin,
              profile='six experimental GC macros plus SET32 r5; not default-off, not shared-status candidate',runtime_reused=True,
              embedded_source_id_enabled=False,LD_LIBRARY_PATH=environments[0],wasmtime=wt,tools=tool_pins,files=list(files.values()),
              provenance=provenance,fixtures=fixture_rows,commands=selected,formal_acceptance=False,temperature_policy='observation_only',
              measurement_boundary='whole process/guest including startup and JIT; allocation and mutate distinct, not pure collector ROI')
    save(output,plan)

def validate_plan(plan):
    require(plan.get('schema')=='uwvm-current-s6e-general-gc-plan-v1' and plan.get('execute_ready') is True and
            plan.get('source_id')==SOURCE_ID and plan['product']['sha256']==PRODUCT_SHA and plan['embedded_source_id_enabled'] is False and
            plan.get('formal_acceptance') is False and plan.get('temperature_policy')=='observation_only', 'Different or placeholder current plan')
    require(len(plan['fixtures'])==16 and len(plan['commands'])==32 and len(plan['files'])>=2701,'Incomplete current plan')
    expected=set()
    for name,value in plan['fixtures'].items():
        d=value['definition']; fixture_contract(d); require(name==d['family']+'-'+d['phase']+'-'+str(d['iterations']) and value['sha256']==d['wasm_sha256'],'Fixture name/bytes mismatch')
        expected.update((name,p) for p in (PROFILE,'wasmtime49/copying'))
    require({(r['fixture'],r['profile']) for r in plan['commands']}==expected,'Duplicate/missing paired commands')
    for item in plan['commands']:
        argv=item['argv']; require(argv[:3]==['taskset','-c','0'] and argv[-1]==plan['fixtures'][item['fixture']]['path'],'Not the original P0 fixture')
        if item['profile']==PROFILE:
            require(argv == ['taskset','-c','0',plan['product']['path'],
                '-Rcc','jit','-Rcm','full','-Rllvm-full-policy','pb-o3','-Rllvm-call-stack','unwind',
                '-Rllvm-exception-dispatch','auto','-Rllvm-cache-path','disable','-Rct','0','-WFE-gc',
                '--wasm-feature-enable-reference-types','--wasm-feature-enable-function-references',
                '--log-verbose','-Rclog','err','--run',argv[-1]],'Changed current original policy/feature/metrics command')
            for option,value in (('-Rcc','jit'),('-Rcm','full'),('-Rllvm-full-policy','pb-o3'),('-Rllvm-call-stack','unwind'),('-Rllvm-exception-dispatch','auto'),('-Rllvm-cache-path','disable'),('-Rct','0'),('-Rclog','err')):
                require(argv.count(option)==1 and argv[argv.index(option)+1]==value,'Changed actual policy/metrics argv')
            require(argv.count('--log-verbose')==1 and argv.count('--run')==1 and '-WFE-gc' in argv and
                    '--wasm-feature-enable-reference-types' in argv and '--wasm-feature-enable-function-references' in argv,'Missing current GC features/verbose')
        else:
            require(argv[3:]==[plan['wasmtime']['path'],'-C','cache=n,collector=copying','-W',
                'all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y',argv[-1]],'Changed Wasmtime same-byte copying command')

def semantic(plan,item,log,exit_code):
    definition=plan['fixtures'][item['fixture']]['definition']; fixture_contract(definition)
    result=dict(iterations=definition['iterations'],phase=definition['phase'],family=definition['family'],expected=definition['expected'],
                exit_zero=exit_code==0,passed=False,collector_qualified=False,mutation_lookup_qualified=False,failures=[],formal_acceptance=False,
                checksum_proof='Pinned self-checking _start exit0; independent exact-byte Wasmtime run checksum is in cold receipt, not a printed dynamic counter.')
    if item['profile'].startswith('wasmtime49/'):
        result['passed']=exit_code==0; return result
    text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',log.read_text(errors='replace'))
    matches=re.findall(r'^\[gc-managed\] ([^\r\n]+)',text,re.M)
    if len(matches)!=1: result['failures'].append('Missing or ambiguous actual managed retirement counters')
    else:
        counts={k:int(v) for k,v in re.findall(r'([a-z_]+)=([0-9]+)',matches[0])}; result['gc-managed']=counts
        complete=all(k in counts for k in ('allocations','attempts','collections','reclaimed','roots_requested','disabled','reason'))
        common=complete and counts['allocations']==definition['expected']['guest_planned_allocations'] and counts['roots_requested']==1 and counts['disabled']==0
        if definition['phase']=='allocate':
            result['collector_qualified']=common and counts['reason']==0 and counts['collections']>0 and counts['reclaimed']>0 and counts['reclaimed']<=counts['allocations']
            if not result['collector_qualified']: result['failures'].append('Allocation collector counters/roots/disabled/reason qualification failed')
        else:
            result['mutation_lookup_qualified']=common and counts['attempts']==0 and counts['collections']==0 and counts['reclaimed']==0 and counts['reason']==2
            result['mutation_contract']='No collection attempt; phase_pending=2 is accepted only for field/lookup measurement, never collector qualification.'
            if not result['mutation_lookup_qualified']: result['failures'].append('Initialization-only mutation counters differ from no-attempt cold contract')
    if text.count('[llvm-jit-full] owning-source=yes pending-plan=native object-cache=disabled body-fallback=no')!=1:
        result['failures'].append('Missing native-owned/cache-disabled/no-body-fallback witness')
    for key,label in (('guest_execution_ns','Total WASM execution time'),('whole_process_reported_ns','Total process time')):
        values=re.findall(re.escape(label)+r': ([0-9]+(?:\.[0-9]+)?)s\.',text)
        if len(values)==1: result[key]=int(decimal.Decimal(values[0])*decimal.Decimal(1000000000))
        else: result['failures'].append('Missing or ambiguous '+label)
    result['passed']=exit_code==0 and not result['failures']
    result['collector_qualified']=result['collector_qualified'] and result['passed']
    result['mutation_lookup_qualified']=result['mutation_lookup_qualified'] and result['passed']
    return result

def closure(host,plan,libraries,out,stage):
    validate_plan(plan)
    for value in [plan['product'],plan['wasmtime'],*plan['tools'],*plan['files'],*plan['provenance']]: verify_pin(value)
    for value in plan['fixtures'].values(): verify_pin(value); require(read_pin(value['manifest'])==value['definition'],'Fixture manifest changed')
    require(host.host_source_id(plan['source'],out/('source-'+stage+'.json'))==SOURCE_ID,'Actual current source changed')
    require(os.environ.get('LD_LIBRARY_PATH','')==plan['LD_LIBRARY_PATH']==libraries['LD_LIBRARY_PATH'] and
            os.environ.get('RAYON_NUM_THREADS')=='1' and not os.environ.get('LD_PRELOAD') and not os.environ.get('LD_AUDIT'),'Actual loader/thread environment differs')
    for value in libraries['files']:
        verify_pin(value); require(str(P(value['path']).resolve(strict=True))==value['real_path'],'Actual DSO real path changed')

def short_identity_observation(reaped,exit_code,elapsed,errors):
    # A missed 20ms observation is never executable identity proof. Retain a
    # complete short guest only as unqualified diagnostic data; any observed
    # ownership/argv/cgroup failure or a longer missing witness stays hard.
    return reaped is True and type(exit_code) is int and exit_code==0 and \
           type(elapsed) is int and 0<elapsed<100_000_000 and not errors

def plain_role(item):
    return 'reference' if item['profile']=='wasmtime49/copying' else 'guest'

def actual_mm_stat(text):
    # Indices relative to state (proc_pid_stat field 3). Read actual fields;
    # never invent terminal state, replace argv, or infer MM from RSS alone.
    fields=text[text.rfind(')')+2:].split()
    require(len(fields)>=49,'Incomplete actual owned stat')
    names=('vsize','rss_pages','startcode','endcode','startstack','start_data',
           'end_data','start_brk','arg_start','arg_end','env_start','env_end')
    indices=(20,21,23,24,25,42,43,44,45,46,47,48)
    return dict(raw=text,pid=int(text.split('(',1)[0]),birth=int(fields[19]),
        state=fields[0],ppid=int(fields[1]),pgid=int(fields[2]),
        mm=dict(zip(names,(int(fields[index]) for index in indices))))

def same_owned_stat(entry,stat,parent):
    return stat['pid']==entry['process'].pid and stat['birth']==entry['birth'] and \
        stat['ppid']==parent and stat['pgid']==entry['process'].pid

def cleared_owned_row(entry,row,cg,parent):
    pid=entry['process'].pid
    return row['pid']==pid and row['birth']==entry['birth'] and row['ppid']==parent and row['pgid']==pid and \
        row['uid']==[1000]*4 and row['cgroup']==cg and row['cpus']==entry['cpu'] and row['argv']==[] and row['rss']==0

def clear_mm_retirement_candidate(entry,row,stat,cg,parent):
    # Profiler early retirement remains hard under the frozen SIGINT contract.
    actual=entry['actual_exec']; pid=entry['process'].pid
    return entry['role'] in ('guest','reference') and type(entry['pidfd']) is int and entry['pidfd']>=0 and \
        isinstance(actual,dict) and actual.get('pid')==pid and actual.get('birth')==entry['birth'] and \
        actual.get('ppid')==parent and actual.get('pgid')==pid and actual.get('uid')==[1000]*4 and \
        actual.get('cgroup')==cg and actual.get('cpus')==entry['cpu']=='0' and \
        actual.get('argv')==[os.fsdecode(v) for v in entry['exec_argv']] and \
        actual.get('executable')==str(entry['expected_exe']) and \
        cleared_owned_row(entry,row,cg,parent) and same_owned_stat(entry,stat,parent) and \
        len(stat['mm'])==12 and all(v==0 for v in stat['mm'].values())

def observed_guard(host,base,admission,out):
    # The frozen guard still receives actual proc fields. Only an already
    # authenticated cleared-MM guest can defer observation until real terminal
    # proof. No live mismatch, replacement argv or synthetic Z is accepted.
    original=types.SimpleNamespace(**base.__dict__); recent=collections.deque(maxlen=64)
    lock=threading.Lock(); check_lock=threading.RLock(); owner=[None]; failures=[0]; retirements=[0]
    def ident(pid):
        row=base.ident(pid); guard=owner[0]
        entry=next((e for e in guard.owned if not e['reaped'] and e['process'].pid==pid),None) if guard else None
        if entry is not None:
            allowed=[entry['exec_argv']] if entry['actual_exec'] is not None else entry['allowed_argv']
            record=dict(observed_ns=time.perf_counter_ns(),role=entry['role'],original_birth=entry['birth'],
                row=base.json_ident(row),argv_allowed=row['argv'] in allowed,
                expected_argv=[os.fsdecode(v) for v in entry['exec_argv']],original_pidfd=entry['pidfd'])
            if not record['argv_allowed']:
                def ready():
                    try: return bool(select.select([entry['pidfd']],[],[],0)[0]) if entry['pidfd'] is not None else None
                    except (OSError,ValueError,TypeError) as error: return dict(error=type(error).__name__+': '+str(error))
                record['pidfd_readable_before_second_stat']=ready()
                try:
                    record['second_stat']=actual_mm_stat((P('/proc')/str(pid)/'stat').read_text())
                except (OSError,ValueError,IndexError) as error: record['second_stat_error']=type(error).__name__+': '+str(error)
                record['pidfd_readable_after_second_stat']=ready()
            with lock: recent.append(record)
            if not record['argv_allowed'] and 'second_stat' in record and \
               clear_mm_retirement_candidate(entry,row,record['second_stat'],guard.cg,os.getpid()):
                original_fd=entry['pidfd']; started_ns=time.perf_counter_ns(); deadline=started_ns+2_000_000_000
                audit=dict(started_ns=started_ns,deadline_ns=deadline,original_pidfd=original_fd,
                    original_birth=entry['birth'],initial_observation=base.json_ident(row),
                    initial_actual_stat=record['second_stat'],observations=0,terminal_confirmed=False)
                record['bounded_retirement_wait']=audit
                def terminal_receipt():
                    require(entry['pidfd']==original_fd,'Original owned PIDFD changed before terminal confirmation')
                    require(audit['finished_ns']<=deadline,'Cleared-MM terminal confirmation exceeded2s')
                    audit['terminal_confirmed']=True
                    with lock: retirements[0]+=1; number=retirements[0]
                    if number<=16:
                        save(out/('owned-retirement-'+str(number)+'-'+str(started_ns)+'.json'),
                            dict(role=entry['role'],actual_exec=entry['actual_exec'],observation=audit,
                                 actual_fields_only=True,original_host_guard_sha256=HOST_SHA))
                while True:
                    require(entry['pidfd']==original_fd,'Original owned PIDFD changed during terminal observation')
                    require(time.perf_counter_ns()<=deadline,'Cleared-MM owned terminal observation exceeded2s')
                    ready_before=bool(select.select([original_fd],[],[],0)[0]); audit['observations']+=1
                    try:
                        current=base.ident(pid)
                        audit['last_observation']=dict(time_ns=time.perf_counter_ns(),row=base.json_ident(current),
                            pidfd_readable_before=ready_before)
                        # Reject a returned live mismatch before another proc
                        # read can disappear; an absent later stat cannot erase
                        # an actual wrong UID/argv/ancestry/CPU observation.
                        require(cleared_owned_row(entry,current,guard.cg,os.getpid()),
                                'Owned identity/argv changed before terminal stat observation')
                        stat=actual_mm_stat((P('/proc')/str(pid)/'stat').read_text())
                    except FileNotFoundError as missing:
                        # wait4 may run on the other thread. Preserve the real
                        # missing read; only original FD + actual absent stat/Z
                        # lets the unchanged guard handle FileNotFoundError.
                        try: stat=actual_mm_stat((P('/proc')/str(pid)/'stat').read_text())
                        except FileNotFoundError:
                            audit['last_observation']=dict(time_ns=time.perf_counter_ns(),actual_stat_absent=True,
                                missing_path=str(missing.filename),pidfd_readable=bool(select.select([original_fd],[],[],0)[0]))
                            if audit['last_observation']['pidfd_readable']:
                                audit.update(terminal_kind='actual_stat_absent',finished_ns=time.perf_counter_ns())
                                terminal_receipt(); raise missing
                        else:
                            require(same_owned_stat(entry,stat,os.getpid()) and all(v==0 for v in stat['mm'].values()),
                                    'Owned identity/MM changed after partial terminal read')
                            ready_after=bool(select.select([original_fd],[],[],0)[0])
                            audit['last_observation']=dict(time_ns=time.perf_counter_ns(),actual_stat=stat,
                                partial_missing_path=str(missing.filename),pidfd_readable=ready_after)
                            if stat['state']=='Z' and ready_after:
                                audit.update(terminal_kind='actual_stat_Z_partial_read',finished_ns=time.perf_counter_ns())
                                terminal_receipt(); raise missing
                    else:
                        ready_after=bool(select.select([original_fd],[],[],0)[0])
                        audit['last_observation']=dict(time_ns=time.perf_counter_ns(),row=base.json_ident(current),actual_stat=stat,
                            pidfd_readable_before=ready_before,pidfd_readable_after=ready_after)
                        require(clear_mm_retirement_candidate(entry,current,stat,guard.cg,os.getpid()),
                                'Owned identity/argv/MM changed during terminal observation')
                        if current['state']==stat['state']=='Z' and ready_after:
                            audit.update(terminal_kind='actual_Z',finished_ns=time.perf_counter_ns())
                            terminal_receipt(); return current # Actual unmodified row, not a substituted state.
                    time.sleep(min(.005,max(0,(deadline-time.perf_counter_ns())/1_000_000_000)))
        return row
    original.ident=ident; guard=host.HostGuard(original,admission); owner[0]=guard; actual_check=guard.check
    def check_serialized():
        try:
            actual_check()
            # Plain references can use several Wasmtime compiler TIDs, all on
            # P0 in the exact scope. Product guest/counter single-TID rules in
            # the original guard remain unchanged.
            for entry in guard.owned:
                if entry['reaped'] or entry['role']!='reference': continue
                pid=entry['process'].pid
                try: tasks=list((P('/proc')/str(pid)/'task').iterdir())
                except FileNotFoundError:
                    require(bool(select.select([entry['pidfd']],[],[],0)[0]),'Missing reference tasks not proved retired');continue
                for task in tasks:
                    try: current=(task/'cgroup').read_text()
                    except FileNotFoundError:
                        require(bool(select.select([entry['pidfd']],[],[],0)[0]),'Reference TID cgroup vanished before proved retirement');continue
                    require(current==guard.cg,'Reference TID escaped exact target cgroup')
        except BaseException as failure:
            with lock:
                failures[0]+=1; number=failures[0]; records=list(recent)
            if number<=16:
                try: save(out/('guard-rejection-'+str(number)+'-'+str(time.perf_counter_ns())+'.json'),
                    dict(error=type(failure).__name__+': '+str(failure),observations=records,
                         observation_only=True,original_guard_bytes_unchanged=True,identity_rejection_weakened=False,original_host_guard_sha256=HOST_SHA))
                except BaseException: pass # Failure remains the original hard rejection.
            raise
    def checked():
        # No new check/admission can pass a pending terminal observation. wait4
        # can reap the original child; its FD stays open until watcher join.
        with check_lock: return check_serialized()
    guard.check=checked
    return guard

def plain_sample(base,hw,guard,item,out,receipt):
    guard.check(); before=base.telemetry(); log=out/'guest.log'; guest=None; watcher=None; started=False; reaped=False
    stop=threading.Event(); points=[]; errors=[]; usage=None; elapsed=None; begin=None; cancel=None
    try:
        with log.open('wb') as stream:
            guest=hw.spawn_stopped(base,guard,'guest',item['argv'],stream)
            # Bootstrap admission remains guest/P0. Only plain reference mode
            # changes its multiplicity role before execution, never CPU/argv.
            guest['role']=plain_role(item)
            guard.check(); begin=time.perf_counter_ns(); hw.signal_owned(guest,signal.SIGCONT)
            def monitor():
                try:
                    while not stop.wait(.02):
                        guard.check(); points.append(base.telemetry())
                        require(log.stat().st_size<=4<<20,'Output exceeds4MiB')
                        require(time.perf_counter_ns()-begin<=90_000_000_000,'90s guest deadline')
                except BaseException as error:
                    errors.append(type(error).__name__+': '+str(error)); hw.signal_owned(guest,signal.SIGKILL)
            watcher=threading.Thread(target=monitor); oldmask=signal.pthread_sigmask(signal.SIG_BLOCK,{signal.SIGINT,signal.SIGTERM})
            try: watcher.start(); started=True
            finally: signal.pthread_sigmask(signal.SIG_SETMASK,oldmask)
            _,status,usage=os.wait4(guest['process'].pid,0); elapsed=time.perf_counter_ns()-begin
            guest['process'].returncode=os.waitstatus_to_exitcode(status); guest['reaped']=reaped=True
    except BaseException as error:
        errors.append(type(error).__name__+': '+str(error))
        if isinstance(error,(KeyboardInterrupt,SystemExit)):
            cancel=error; signal.signal(signal.SIGTERM,signal.SIG_IGN); signal.signal(signal.SIGINT,signal.SIG_IGN)
    finally:
        stop.set()
        try:
            if started: watcher.join()
        except BaseException as error: errors.append('Watcher retirement: '+str(error))
        finally:
            if guest:
                if not reaped:
                    hw.signal_owned(guest,signal.SIGKILL); _,status,usage=os.wait4(guest['process'].pid,0)
                    guest['process'].returncode=os.waitstatus_to_exitcode(status); guest['reaped']=True
                if guest['pidfd'] is not None: os.close(guest['pidfd']); guest['pidfd']=None
    try: after=base.telemetry(); guard.check()
    except BaseException as error:
        errors.append('Post-retirement: '+str(error)); after=dict(before,post_failed=str(error))
        if isinstance(error,(KeyboardInterrupt,SystemExit)): cancel=error
    quality=[]
    if elapsed is None or elapsed<100_000_000: quality.append('Sub100ms or incomplete whole guest')
    if not points: quality.append('Missing in-window frequency')
    if guest and guest['actual_exec'] is None:
        if short_identity_observation(reaped,guest['process'].returncode,elapsed,errors):
            quality.append('Unqualified short guest: original loaded executable/argv was not observed; identity remains unknown')
        else: errors.append('Missing actual original guest executable/argv receipt')
    if any(before['cpu_stat'].get(k)!=after['cpu_stat'].get(k) for k in ('nr_throttled','throttled_usec')): quality.append('CPU throttling changed')
    row=dict(profile=item['profile'],fixture=item['fixture'],argv=item['argv'],guest_exit=guest['process'].returncode if guest else None,
        guest_wall_ns=elapsed,user_seconds=usage.ru_utime if usage else None,system_seconds=usage.ru_stime if usage else None,
        maxrss_kib=usage.ru_maxrss if usage else None,guest_admission=guest['admitted'] if guest else None,
        execution_role=guest['role'] if guest else None,
        guest_actual_exec=guest['actual_exec'] if guest else None,
        execution_identity_qualified=guest is not None and guest['actual_exec'] is not None and not errors,
        before=before,during=points,after=after,
        median_run_frequency_khz=statistics.median(p['scaling_cur_freq'] for p in points) if points else None,
        hard_failures=errors,quality_failures=quality,temperature_policy='observation_only',formal_acceptance=False,
        semantic_receipt=receipt(item,log,guest['process'].returncode if guest else None),log_sha256=digest(log) if log.exists() else None)
    save(out/'raw.json',row)
    if cancel: raise cancel
    return row

def main():
    parser=argparse.ArgumentParser(description=__doc__); sub=parser.add_subparsers(dest='mode',required=True)
    prep=sub.add_parser('prepare'); prep.add_argument('--cold-build',type=P,required=True); prep.add_argument('--runtime-bind',type=P,required=True); prep.add_argument('--runtime-receipts',type=P,required=True); prep.add_argument('--out-plan',type=P,required=True)
    run=sub.add_parser('run')
    for key in ('plan','admission','perf-libraries','out'): run.add_argument('--'+key,type=P,required=True)
    run.add_argument('--measurement',choices=('unprofiled','hardware'),required=True); run.add_argument('--pairs',type=int,choices=(1,2),default=1); run.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    if args.mode=='prepare': prepare(args.cold_build,args.runtime_bind,args.runtime_receipts,args.out_plan); return
    here=P(__file__).parent; host_bytes=(here/'run_current_host_pcore_hw_counting.py').read_bytes()
    require(hashlib.sha256(host_bytes).hexdigest()==HOST_SHA,'Frozen host dependency changed')
    host=types.ModuleType('uwvm_frozen_host_counting'); host.__file__=str(here/'run_current_host_pcore_hw_counting.py'); exec(compile(host_bytes,host.__file__,'exec'),host.__dict__)
    hw,hw_path=host.fixed_module('uwvm_frozen_hw_protocol','run_current_pcore_hw_counting.py',HW_SHA)
    original,base_path=host.fixed_module('uwvm_frozen_base_parser','run_current_pcore_diagnostic.py',BASE_SHA)
    plan=json.loads(args.plan.read_text()); validate_plan(plan)
    admission=json.loads(args.admission.read_text()); libraries=json.loads(args.perf_libraries.read_text()); host.validate_raw_inventory(admission,libraries)
    require(digest(admission['trusted_wrapper']['path'])==admission['trusted_wrapper']['sha256'],'Actual trusted scope admission wrapper changed')
    input_shas={str(p):digest(p) for p in (P(__file__),args.plan,args.admission,args.perf_libraries,here/'run_current_host_pcore_hw_counting.py',hw_path,base_path,P(admission['trusted_wrapper']['path']))}
    require(not args.out.exists(),'Fresh output directory required'); args.out.mkdir(parents=True)
    save(args.out/'plan.json',plan); save(args.out/'admission.json',admission); save(args.out/'perf-libraries.json',libraries)
    for name,path in (('runner.py',P(__file__)),('host.py',here/'run_current_host_pcore_hw_counting.py'),('protocol.py',hw_path),('base.py',base_path)): (args.out/name).write_bytes(path.read_bytes())
    require(args.execute,'Explicit reviewed --execute required'); resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def cancel(signum,frame): raise KeyboardInterrupt('Controlled cancellation '+str(signum))
    signal.signal(signal.SIGTERM,cancel)
    guard=observed_guard(host,original,admission,args.out); adapted=host.HostBase(original,guard.root)
    # Frozen measure code receives a narrow value adapter; old modules/main and
    # old raw fixture tables remain unchanged. No invented old admission pass.
    base=types.SimpleNamespace(**original.__dict__); base.telemetry=adapted.telemetry; base.effective_argv=lambda item:list(item['argv'])
    receipt=lambda item,log,code:semantic(plan,item,log,code); base.semantic_receipt=receipt
    aux=types.SimpleNamespace(semantic_receipt=receipt)
    rows=[]; error=None; before_ok=after_ok=complete=False; pmu_before=pmu_after=None
    try:
        closure(host,plan,libraries,args.out,'before'); guard.check(); before_ok=True
        selected=plan['commands'] if args.measurement=='unprofiled' else [r for r in plan['commands'] if r['profile']==PROFILE and plan['fixtures'][r['fixture']]['definition']['iterations']==2000000]
        if args.measurement=='hardware':
            pmu_before=host.pmu_receipt(); save(args.out/'pmu-before.json',pmu_before)
            perf=P(admission['host_perf']['path']).resolve(strict=True)
            require(perf.read_bytes()[:4]==b'\x7fELF' and digest(perf)==admission['host_perf']['sha256'],'Actual perf ELF changed')
            require(any(v['real_path']==str(perf) and v['sha256']==digest(perf) for v in libraries['files']),'Perf absent from actual DSO closure')
        for pair in range(args.pairs):
            for index,item in enumerate(selected if pair%2==0 else selected[::-1]):
                out=args.out/(str(pair)+'-'+str(index)+'-'+item['fixture']+'-'+item['profile'].replace('/','-')); out.mkdir()
                if args.measurement=='unprofiled': row=plain_sample(base,hw,guard,item,out,receipt)
                else:
                    signals=[]; globals_=dict(hw.measure.__globals__,EVENT_GROUP=host.RAW_GROUP,
                        parse_stat=lambda stat,debug='':host.normalized_math(hw,stat,debug),
                        signal_owned=lambda entry,sig:host.submit_owned_signal(hw,base,guard,entry,sig,signals))
                    measure=types.FunctionType(hw.measure.__code__,globals_,'general_gc_hw_measure'); row=measure(base,aux,guard,item,perf,out)
                    row['owned_perf_sigint_receipts']=signals[:]
                    log=out/(row['label']+'.perf.log'); attrs=host.final_attributes(log.read_text(errors='replace'),row['guest_admission']['pid']) if row['guest_admission'] else dict(passed=False,failures=['No actual guest'])
                    counts=row['hardware_counting']; prior=counts['failures'][:]; counts['final_guest_attributes']=attrs
                    counts['failures']=[x for x in prior if not x.startswith('Awaiting actual final-guest')]+attrs['failures']
                    completion=host.perf_completion(row); row['actual_perf_completion']=completion
                    counts['counter_quality_passed']=counts.get('math_quality_passed',False) and attrs['passed'] and completion['passed']
                    counts['failures']+=completion['failures']; row['quality_failures']=[x for x in row['quality_failures'] if x not in prior]+counts['failures']
                row['pair']=pair; row['measurement_family']=args.measurement; row['source_id_external']=SOURCE_ID; row['host_smt_noise']='unknown without independent actual observer'; row['input_sha256']=input_shas
                save(out/'qualified.json',row); rows.append(row)
                with (args.out/'rows.jsonl').open('a') as stream: stream.write(json.dumps(row,allow_nan=False)+'\n')
                require(not row['hard_failures'],'Actual hard ownership/resource/deadline failure')
                require(row['semantic_receipt']['passed'],'Actual guest/managed-counter qualification failure')
                if args.measurement=='hardware':
                    require(row['actual_perf_completion']['passed'] and row['hardware_counting']['counter_quality_passed'],
                        'Actual perf protocol/final attributes/exact enabled-running counts failed')
        closure(host,plan,libraries,args.out,'after'); guard.check()
        if args.measurement=='hardware': pmu_after=host.pmu_receipt(); save(args.out/'pmu-after.json',pmu_after); require(pmu_before==pmu_after,'Actual PMU changed')
        require(all(digest(path)==sha for path,sha in input_shas.items()),'Immutable execution input changed'); after_ok=complete=True
    except BaseException as failure: error=type(failure).__name__+': '+str(failure); raise
    finally:
        pairs=[]
        for name in plan['fixtures']:
            for pair in range(args.pairs):
                chosen=[r for r in rows if r['fixture']==name and r['pair']==pair]
                if not chosen: continue
                freqs=[r['median_run_frequency_khz'] for r in chosen]; matched=len(chosen)>=2 and all(v and v>0 for v in freqs) and max(freqs)/min(freqs)-1<=.1
                pairs.append(dict(fixture=name,pair=pair,frequency_matched=matched,host_smt_noise_qualified=False,
                    all_sample_quality_passed=all(not r['quality_failures'] and not r['hard_failures'] for r in chosen)))
        save(args.out/'summary.json',dict(schema='uwvm-current-general-gc-measurement-v1',measurement_family=args.measurement,complete=complete,
            closure_before_ok=before_ok,closure_after_ok=after_ok,execution_error=error,rows=len(rows),input_sha256=input_shas,source_id_external=SOURCE_ID,
            product_sha256=PRODUCT_SHA,semantic_pass_count=sum(r['semantic_receipt']['passed'] for r in rows),pairs=pairs,
            hardware_quality_pass_count=sum(r.get('hardware_counting',{}).get('counter_quality_passed',False) for r in rows),
            formal_acceptance=False,temperature_policy='observation_only',host_smt_noise='unknown unless separate real observer receipt is provided',
            limitations=['Whole guest includes startup and JIT, not pure collector ROI.','S6e six-plus-SET32 current baseline only; no default/ROS/ranking claim.',
                'Mutation phase_pending/no-attempt is field/lookup measurement, never collector qualification.','VTune sampling/MUX is a separate family, not these exact raw counts.']))
if __name__=='__main__': main()
