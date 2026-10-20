"""Actual full validation, binary import and UWVM Go SDK consumer checks."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
A7=V/'root-language-tinygo-complete-examples-20261019-a7';O=V/'root-language-tinygo-existing-products-20261019-a8';S=A7/'source-r1'
GO=V/'root-language-rust-go-compiler-prefixes-20261019-r198/prefixes/go'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def sha(b):return hashlib.sha256(b).hexdigest()
def guard():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()=='68719476736'and(c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 stats=dict(l.split()for l in(c/'memory.stat').read_text().splitlines())
 current=int((c/'memory.current').read_text())
 reclaimable=max(0,min(current,int(stats.get('inactive_file','0')))-int(stats.get('file_dirty','0'))-int(stats.get('file_writeback','0'))-int(stats.get('unevictable','0')))
 committed=current-reclaimable
 assert current<=64<<30,'hard64GiB accounting'
 assert committed<56<<30,'conservative committed56GiB guard'
 events=dict(l.split()for l in(c/'memory.events').read_text().splitlines());assert all(events[k]=='0'for k in ['oom','oom_kill','oom_group_kill']),'shared OOM event'
 return {'memory_current':current,'memory_events':(c/'memory.events').read_text(),'conservative_reclaimable_file_bytes':reclaimable,'conservative_committed_estimate':committed}
def proc(pid):
 p=Path('/proc')/str(pid);raw=(p/'stat').read_text();fields=raw[raw.rfind(')')+2:].split()
 status=(p/'status').read_text();rss=next((int(l.split()[1])*1024 for l in status.splitlines()if l.startswith('VmRSS:')),None)
 return {'pid':pid,'pgrp':int(fields[2]),'start':int(fields[19]),'rss':rss,'cgroup':(p/'cgroup').read_text().strip()}
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 dependency=time.monotonic()
 while True:
  prerequisite=O/'producer-result-r1.json'
  if prerequisite.exists():
   try:parent=json.loads(prerequisite.read_bytes())
   except json.JSONDecodeError:time.sleep(.05);continue
   if parent['status']!='RUNNING':
    assert parent['status']=='ACTUAL_TINYGO135_SELF_CONTAINED_EXAMPLE_PRODUCERS_AND9_BACKEND_REFUSALS_PASSED','complete135 producer prerequisite '+parent.get('primary',parent['status']);break
  assert time.monotonic()-dependency<300,'producer prerequisite300s';time.sleep(1)
 waiting=time.monotonic()
 while True:
  try:before=guard();assert before['conservative_committed_estimate']<54<<30,'ordinary committed54GiB admission';break
  except AssertionError as e:
   if str(e)not in ['conservative committed56GiB guard','ordinary committed54GiB admission']:raise
   assert time.monotonic()-waiting<120,'ordinary capacity120s deadline'
   (O/'ordinary-admission-r3.json').write_text(json.dumps({'status':'WAITING_CAPACITY','commands_started':0,'reason':str(e),'wait_seconds':time.monotonic()-waiting})+'\n');time.sleep(10)
 (O/'ordinary-runs-r3').mkdir();(O/'ordinary-tmp-r3').mkdir()
 env={'PATH':'/usr/bin:/bin','TMPDIR':str(O/'ordinary-tmp-r3'),'TZ':'UTC'}
 result={'status':'RUNNING','scope':'Full validator, actual binary ROS signatures and actual UWVM ordinary Go SDK tests/default-denial example only. No positive native ROS, source DBG, performance or architecture credit.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'ordinary-result-r3.json').write_text(json.dumps(result,indent=2)+'\n')
 prior=json.loads((O/'ordinary-result-r2.json').read_bytes())
 assert prior['status']=='ACTUAL_AVAILABLE121_TINYGO224_GUEST_RUNS102_ARGS18_PREFLIGHT_REFUSALS_PASSED'and len(prior['commands'])==344
 reuse={r['label']:r for r in prior['commands']if r['passed']}
 result['reused_observation_parent']={'path':str(O/'ordinary-result-r2.json'),'sha256':sha((O/'ordinary-result-r2.json').read_bytes()),'failed_whole_cohort':False,'closed_observations':344,'reason':'Completed344 observations preserve their original resource profiles. Fresh single-process ordinary invocations use conservative committed below54GiB admission/below56GiB active, sampled child RSS64MiB and20ms observations. Compiler/LLVM-debugger/performance admission is unchanged.'}
 def normalize(argv):
  a=list(map(str,argv))
  for i,v in enumerate(a[:-1]):
   if v.endswith('-global-gateway-socket'):a[i+1]='<owned-absent-socket>'
  return a
 def run(label,argv,cwd,expected=0,deadline=60,limit=64<<20,input_bytes=None):
  if label in reuse:
   row=dict(reuse[label]);assert normalize(row['argv'])==normalize(argv) and row['cwd']==str(cwd) and row['expected_exit']==expected and row['complete_output'] and row['error']is None
   assert row.get('owned_input_hex')==(None if input_bytes is None else input_bytes.hex())
   d=Path(row.get('log_directory',str(A7/'ordinary-partial-runs-r3'/label)))
   for stream in ['stdout','stderr']:
    b=(d/(stream+'.log')).read_bytes();assert len(b)==row[stream+'_bytes'] and sha(b)==row[stream+'_sha256']
   row['log_directory']=str(d);row['reused_closed_observation']=True;result['commands'].append(row);save();return row
  waiting=time.monotonic()
  while True:
   try:state=guard();assert state['conservative_committed_estimate']<54<<30,'ordinary committed54GiB admission';break
   except AssertionError as e:
    if str(e)not in ['conservative committed56GiB guard','ordinary committed54GiB admission']:raise
    assert time.monotonic()-waiting<120,'per-command capacity120s deadline';save();time.sleep(10)
  d=O/'ordinary-runs-r3'/label;d.mkdir()
  def prepare():
   assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
   os.sched_setaffinity(0,{19});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
  row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'passed':False,'sampled_peak_own_rss':0,'processes':[],'log_directory':str(d),'resource_profile':'ordinary-single-process-64MiB-RSS-conservative-committed54-admit56-active-20ms','admission_state':state}
  result['commands'].append(row);save();start=time.monotonic();known={};p=None;error=None
  with(d/'stdout.log').open('wb')as out,(d/'stderr.log').open('wb')as err:
   try:
    p=subprocess.Popen(list(map(str,argv)),cwd=cwd,env=env,stdin=subprocess.DEVNULL if input_bytes is None else subprocess.PIPE,stdout=out,stderr=err,start_new_session=True,preexec_fn=prepare)
    row['pid']=p.pid;descriptor=os.pidfd_open(p.pid,0)
    if input_bytes is not None:
     assert p.stdin.write(input_bytes)==len(input_bytes);p.stdin.close();row['owned_input_hex']=input_bytes.hex()
    try:
     while p.poll()is None:
      sample=guard();row['sampled_shared_raw_peak']=max(row.get('sampled_shared_raw_peak',0),sample['memory_current']);row['sampled_committed_estimate_peak']=max(row.get('sampled_committed_estimate_peak',0),sample['conservative_committed_estimate']);total=0
      c=Path('/sys/fs/cgroup'+G)
      for pid in(c/'cgroup.procs').read_text().split():
       try:r=proc(int(pid))
       except(FileNotFoundError,ProcessLookupError):continue
       if r['pgrp']!=p.pid:continue
       assert r['cgroup']=='0::'+G
       old=known.get(r['pid'])
       if old:assert old['start']==r['start']
       known[r['pid']]=r
       if r['rss']is not None:total+=r['rss']
      row['sampled_peak_own_rss']=max(row['sampled_peak_own_rss'],total)
      assert total<=limit,'sampled own RSS guard'
      assert time.monotonic()-start<deadline,'command deadline'
      assert out.tell()+err.tell()<4<<20,'output bound'
      time.sleep(.02)
     row['exit']=p.wait(timeout=5)
    finally:os.close(descriptor)
   except BaseException as e:
    error=repr(e)
    if p is not None and p.poll()is None:
     # Signal only the newly owned process group after exact original leader check.
     try:
      r=proc(p.pid);assert r['pgrp']==p.pid and r['cgroup']=='0::'+G
      os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=5)
     except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait(timeout=5)
    row['exit']=None if p is None else p.returncode
  row['error']=error;row['elapsed_seconds']=time.monotonic()-start
  row['processes']=list(known.values());row['complete_output']=True
  for stream in['stdout','stderr']:
   b=(d/(stream+'.log')).read_bytes();row[stream+'_bytes']=len(b);row[stream+'_sha256']=sha(b)
  row['passed']=error is None and (row['exit']!=0 if expected=='clock-refusal' else row['exit']==expected)
  save();print(json.dumps({'label':label,'passed':row['passed'],'exit':row['exit'],'error':error,'peak_rss':row['sampled_peak_own_rss']}),flush=True)
  assert row['passed'],label
  return row
 try:
  import re
  base=V/'root-language-go-sdk-20261019-a3'
  producer=json.loads((O/'producer-result-r1.json').read_bytes())
  assert producer['status']=='ACTUAL_TINYGO135_SELF_CONTAINED_EXAMPLE_PRODUCERS_AND9_BACKEND_REFUSALS_PASSED'and len(producer['artifacts'])==135
  result['parent_failed_whole_cohort']=False;result['required_final_product_count']=135;result['individual_completed_products_checked']=135
  runtimes=json.loads((base/'runtime-restoration.json').read_bytes())['runtimes']
  result['scope']='Actual individually compiled TinyGo example default-disabled executions, pure-code checks and separate missing-Gateway startup refusals. A preflight refusal is not Guest permission-denial or native delivery evidence. Expected legacy application traps are refusals, not successful ROS applications. Coverage export never executed. Pure codec/benchmark loops are functional only, not timing evidence.'
  result['products']=producer['artifacts'];result['workflows']=[]
  result['producer_parent']={'path':str(O/'producer-result-r1.json'),'sha256':sha((O/'producer-result-r1.json').read_bytes()),'failed_whole_cohort':False,'complete_135_product_cohort':True}
  result['fresh_resource_profile']={'admission_conservative_committed_below':54<<30,'active_conservative_committed_below':56<<30,'reclaimable_definition':'max(0,min(memory.current,inactive_file)-file_dirty-file_writeback-unevictable)','sampled_process_group_RSS_at_most':64<<20,'sampling_interval_seconds':.02,'one_target_at_a_time':True,'hard_cgroup_limit_unchanged':64<<30,'new_subcgroup_created':False,'global_cache_kernel_or_cgroup_changes':False,'raw_memory_is_recorded_not_used_as_reclaimable_capacity_estimate':True,'compiler_debugger_performance_profiles_changed':False,'basis':'Prior completed344 observations include release/debug interpreters; bounded finite ordinary inputs; fixed finite ordinary inputs only; no performance qualification'}
  manifest=json.loads((A7/'source-inputs-r1.json').read_bytes());result['source_manifest_sha256']=sha((A7/'source-inputs-r1.json').read_bytes())
  for item in manifest['members']:assert sha((S/item['path']).read_bytes())==item['sha256']
  result['runtimes']=[{k:v for k,v in r.items()if k!='build_origin'}for r in runtimes]
  scratch=Path('/tmp')/('uwvm-tgo-a7-'+str(os.getpid()));scratch.mkdir(mode=0o700);sock=scratch/'absent.sock'
  result['scratch']=str(scratch)
  pure={'scheduling','sdk_codec_debug','codec_benchmark'}
  def zero_flags(backend):
   prefix='--'+backend+'-global-';values={'gateway-socket':str(sock),'gateway-uid':str(os.getuid()),'node-name':'tgo_a7_empty','namespace':'/tgo_a7','connect-timeout-ms':'3000','audit-deny-max-rate-hz':'100','audit-deny-burst':'16','audit-deny-max-records':'10000','audit-deny-max-bytes':'1048576'}
   if backend=='ros1':values.update({'master-uri':'http://127.0.0.1:11311','advertise-host':'127.0.0.1','transport':'tcpros'})
   else:values.update({'domain-id':'194','rmw-implementation':'rmw_fastrtps_cpp','security-mode':'disabled-test','keystore':'not-applicable','enclave':'not-applicable'})
   flags=[prefix+'enable']
   for key,value in values.items():flags.extend([prefix+key,value])
   return flags
  for runtime in runtimes:
   R=Path(runtime['path']);h=hashlib.sha256()
   with R.open('rb')as f:
    st=os.fstat(f.fileno());assert st.st_size==runtime['bytes']
    while True:
     chunk=f.read(65536)
     if not chunk:break
     h.update(chunk)
    assert h.hexdigest()==runtime['sha256']
    active=[]
    for pid in(Path('/sys/fs/cgroup'+G)/'cgroup.procs').read_text().split():
     try:ex=(Path('/proc')/pid/'exe').stat()
     except(FileNotFoundError,PermissionError,ProcessLookupError):continue
     if(ex.st_dev,ex.st_ino)==(st.st_dev,st.st_ino):active.append(int(pid))
    if not active:os.posix_fadvise(f.fileno(),0,0,os.POSIX_FADV_DONTNEED)
    result.setdefault('owned_runtime_read_cache_advice',[]).append({'path':str(R),'bytes':st.st_size,'sha256':h.hexdigest(),'device':st.st_dev,'inode':st.st_ino,'sampled_live_executable_matches':active,'advised':not active,'file_bytes_preserved':True,'future_open_race_disclosed':True});save()
   for artifact in producer['artifacts']:
    kind=artifact['kind']
    if kind=='coverage':continue
    wasm=Path(artifact['path']);assert sha(wasm.read_bytes())==artifact['sha256']
    backend=artifact['backend'];stem=runtime['mode']+'-'+backend+'-'+artifact['profile']+'-'+kind
    expected=0 if kind in pure or kind=='native_resources'else -4
    scopes=['disabled','missing-gateway']if kind=='native_resources'else['disabled']
    args=['deny']if kind=='native_resources'else['17']if kind=='codec_benchmark'else[]
    for scope in scopes:
     label=stem+'-'+scope;name='tgo_a7_'+backend+'_'+kind
     flags=['--wasip1-global-noinherit-system-environment','--wasm-set-main-module-name',name]
     if scope=='missing-gateway':flags+=zero_flags(backend)
     case_expected=126 if scope=='missing-gateway'else expected
     row=run(label,[R,*flags,'--run',wasm,*args],S,expected=case_expected,input_bytes=b'GD'if kind=='codec_benchmark'else None)
     d=Path(row.get('log_directory',str(O/'ordinary-runs-r3'/label)));out=(d/'stdout.log').read_bytes();err=(d/'stderr.log').read_bytes()
     cleaned=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',err)
     audit=[];other=[]
     for line in cleaned.decode().splitlines():
      if line.startswith('{'):audit.append(json.loads(line))
      else:other.append(line)
     if scope=='missing-gateway':
      assert not out and cleaned==('uwvm: [error] ROS policy: gateway preflight failed for '+backend+'/global/\n').encode(),(out,err)
     elif kind=='native_resources':
      assert out==b'PASS_TINYGO_ROS_RESOURCES_DEFAULT_DENIED\n'and not other
     elif kind=='sdk_codec_debug':assert not out and not err
     elif kind=='scheduling':
      assert out+err==b'owned deferred result 42\none local tick\n',(out,err)
     elif kind=='codec_benchmark':
      assert out==b'READY_TINYGO_SDK_CODEC\nDONE_TINYGO_SDK_CODEC 17 1267701127\n'and not err,(out,err)
     else:
      assert b'catch unreachable'in cleaned and b'Call stack:'in cleaned and b'func_idx='in cleaned,'actual application trap and bounded Wasm stack required'
      if artifact['profile']=='0':assert b'main.main'in cleaned,'unoptimized source frame required'
      if kind=='raw':assert b'open error 13\n'in out+err,'exact raw denied13'
      else:assert b'roswasm: capability call failed (inspect Code and HasRequired)'in out+err,'checked SDK error reached'
     assert not audit
     assert not sock.exists()
     result['workflows'].append({'label':label,'runtime':runtime['mode'],'backend':backend,'profile':artifact['profile'],'kind':kind,'scope':scope,'expected_exit':case_expected,'actual_exit':row['exit'],'Guest_executed':scope!='missing-gateway','unresolved_denials':len(audit),'native_ROS_delivery':False,'performance_qualification':False});save()
    if kind in {'native_resources','codec_benchmark'}:
     invalid=[('unknown',['unknown']),('extra',['deny','extra']),('oversize',['x'*2048])]if kind=='native_resources'else[('zero',['0']),('nondigit',['x']),('above-limit',['500000001'])]
     for label_suffix,arguments in invalid:
      label=stem+'-invalid-'+label_suffix;row=run(label,[R,'--wasip1-global-noinherit-system-environment','--run',wasm,*arguments],S,expected=2)
      d=Path(row.get('log_directory',str(O/'ordinary-runs-r3'/label)));assert(d/'stdout.log').read_bytes()==b''and(d/'stderr.log').read_bytes()==b''
  assert len(result['workflows'])==270 and len(result['commands'])==390
  result['fresh_commands']=sum(not r.get('reused_closed_observation',False)for r in result['commands']);assert result['fresh_commands']==46
  for item in manifest['members']:assert sha((S/item['path']).read_bytes())==item['sha256']
  assert not sock.exists()and not list(scratch.iterdir());scratch.rmdir()
  for artifact in producer['artifacts']:assert sha(Path(artifact['path']).read_bytes())==artifact['sha256']
  c=Path('/sys/fs/cgroup'+G);result['after']={'memory_current':int((c/'memory.current').read_text()),'memory_events':(c/'memory.events').read_text()}
  before_events=dict(l.split()for l in result['before']['memory_events'].splitlines());after_events=dict(l.split()for l in result['after']['memory_events'].splitlines());result['shared_memory_event_deltas']={k:int(after_events[k])-int(before_events[k])for k in before_events};assert all(result['shared_memory_event_deltas'][k]==0 for k in ['oom','oom_kill','oom_group_kill'])
  result['shared_max_events_attributable_to_this_cohort']=False;result['quiet_shared_resource_cohort']=all(v==0 for v in result['shared_memory_event_deltas'].values())
  result['status']='ACTUAL_TINYGO250_GUEST_EXECUTIONS120_ARGS20_MISSING_GATEWAY_PREFLIGHTS_PASSED'
 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary')}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
