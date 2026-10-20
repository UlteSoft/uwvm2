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
 waiting=time.monotonic()
 while True:
  try:before=guard();assert before['conservative_committed_estimate']<54<<30,'ordinary committed54GiB admission';break
  except AssertionError as e:
   if str(e)not in ['conservative committed56GiB guard','ordinary committed54GiB admission']:raise
   assert time.monotonic()-waiting<120,'ordinary capacity120s deadline'
   (O/'kernel-admission-r1.json').write_text(json.dumps({'status':'WAITING_CAPACITY','commands_started':0,'reason':str(e),'wait_seconds':time.monotonic()-waiting})+'\n');time.sleep(10)
 (O/'kernel-runs-r1').mkdir();(O/'kernel-tmp-r1').mkdir()
 env={'PATH':'/usr/bin:/bin','TMPDIR':str(O/'kernel-tmp-r1'),'TZ':'UTC'}
 result={'status':'RUNNING','scope':'Same-source codec native/UWVM LLVM/Wasmtime functional equivalence only. Watchdog elapsed times are not latency or performance measurements. No native ROS, PMU or architecture qualification.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'kernel-result-r1.json').write_text(json.dumps(result,indent=2)+'\n')
 def run(label,argv,cwd,expected=0,deadline=180,limit=2<<30,input_bytes=None):
  waiting=time.monotonic()
  while True:
   try:state=guard();assert state['conservative_committed_estimate']<54<<30,'ordinary committed54GiB admission';break
   except AssertionError as e:
    if str(e)not in ['conservative committed56GiB guard','ordinary committed54GiB admission']:raise
    assert time.monotonic()-waiting<120,'per-command capacity120s deadline';save();time.sleep(10)
  d=O/'kernel-runs-r1'/label;d.mkdir()
  def prepare():
   assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
   os.sched_setaffinity(0,{19});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(180,180))
  row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'passed':False,'sampled_peak_own_rss':0,'processes':[],'log_directory':str(d),'resource_profile':'functional-kernel-single-target-conservative-capacity','sampled_RSS_limit':limit,'admission_state':state}
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
  prior=json.loads((O/'debug-r1-result.json').read_bytes());assert prior['status']=='ACTUAL_TINYGO_FIVE_OPT_THREE_REPOSITORY_SOURCE_DEBUG_CAPABILITIES_RECORDED'and len(prior['sessions'])==15 and prior['all_profiles_full_value_workflow']
  producer=json.loads((O/'producer-result-r1.json').read_bytes());assert producer['status']=='ACTUAL_TINYGO135_SELF_CONTAINED_EXAMPLE_PRODUCERS_AND9_BACKEND_REFUSALS_PASSED'
  result['source_DBG_parent']={'path':str(O/'debug-r1-result.json'),'sha256':sha((O/'debug-r1-result.json').read_bytes())};result['producer_parent']={'path':str(O/'producer-result-r1.json'),'sha256':sha((O/'producer-result-r1.json').read_bytes())}
  result['cases']=[];result['pins']={}
  result['resource_profile']={'conservative_committed_admission_below':54<<30,'active_below':56<<30,'hard_cgroup_limit_unchanged':64<<30,'one_target_at_a_time':True,'native_build_RSS_limit':512<<20,'native_target_RSS_limit':128<<20,'UWVM_LLVM_RSS_limit':2<<30,'Wasmtime_RSS_limit':1<<30,'sample_seconds':.02,'timing_and_PMU_qualification':False}
  result['watchdog_elapsed_seconds_are_not_performance_measurements']=True
  def pin(p,expected=None):
   p=Path(p);h=hashlib.sha256();n=0
   with p.open('rb')as f:
    while True:
     data=f.read(65536)
     if not data:break
     n+=len(data);h.update(data)
   identity={'bytes':n,'sha256':h.hexdigest()}
   if expected is not None:assert identity=={k:expected[k]for k in ['bytes','sha256']},str(p)
   result['pins'][str(p)]=identity;return identity
  assert os.uname().machine=='x86_64'
  for p in sorted((S/'sdk/tinygo').glob('*.go')):
   if not p.name.endswith('_test.go'):pin(p)
  pin(S/'sdk/examples/tinygo/codec_benchmark/main.go');pin(S/'sdk/examples/tinygo/go.mod');pin(S/'sdk/tinygo/go.mod')
  tools=Path('/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed')
  go=tools/'go/bin/go';pin(go,producer['tools'][str(go)])
  (O/'kernel-products-r1').mkdir();(O/'kernel-home-r1').mkdir()
  native=O/'kernel-products-r1/codec-native'
  env.update({'PATH':str(go.parent)+':/usr/bin:/bin','HOME':str(V/'root-language-tinygo-sdk-resources-20261019-a6/home'),'GOTOOLCHAIN':'local','GOPROXY':'off','GOSUMDB':'off','GOWORK':'off','GOENV':'off','GOFLAGS':'-mod=readonly','GOCACHE':str(V/'root-language-tinygo-sdk-resources-20261019-a6/cache'),'GOPATH':str(O/'gopath'),'GOMAXPROCS':'1','GOMEMLIMIT':'192MiB','GOOS':'linux','GOARCH':'amd64'})
  result['native_build_environment']=dict(env);save()
  row=run('build-native-same-source',[go,'build','-p=1','-trimpath','-o',native,'./codec_benchmark'],S/'sdk/examples/tinygo',limit=512<<20)
  native_identity=pin(native);result['native_product']={'path':str(native),**native_identity,'source_root':str(S)}
  env.clear();env.update({'PATH':'/usr/bin:/bin','HOME':str(O/'kernel-home-r1'),'TMPDIR':str(O/'kernel-tmp-r1'),'TZ':'UTC','GOMAXPROCS':'1'})
  def seed_for(n):
   seed=0x12345678
   for _ in range(n):seed=(seed*1664525+1013904223)&0xffffffff
   return seed
  def check(label,argv,n,engine,artifact=None,limit=2<<30,metadata=None):
   row=run(label,argv,S,limit=limit,input_bytes=b'GD')
   d=Path(row['log_directory']);expected=('READY_TINYGO_SDK_CODEC\nDONE_TINYGO_SDK_CODEC '+str(n)+' '+str(seed_for(n))+'\n').encode()
   assert(d/'stdout.log').read_bytes()==expected and(d/'stderr.log').read_bytes()==b''
   case={'label':label,'engine':engine,'iterations':n,'expected_final_seed':seed_for(n),'exact_complete_output':expected.decode(),'native_ROS_delivery':False,'performance_qualification':False,'metadata':metadata or{}}
   if artifact is not None:case['Wasm_product']={k:artifact[k]for k in ['path','bytes','sha256','backend','profile','kind']}
   result['cases'].append(case);save()
  for n in [1,17,65537]:check('native-'+str(n),[native,str(n)],n,'native-Go',limit=128<<20)
  products=[a for a in producer['artifacts']if a['backend']=='ros2'and a['kind']=='codec_benchmark'];assert len(products)==5 and{a['profile']for a in products}=={'0','1','2','s','z'}
  origin_path=Path('/tmp/root-language-debug-budget-20261019-r136/result.json');pin(origin_path);origin=json.loads(origin_path.read_bytes())
  result['LLVM_origin']={'path':str(origin_path),'identity':pin(origin_path),'native_ROS_imports_enabled':False,'frozen_manual_O2_g1':True}
  runtimes=origin['products'];assert len(runtimes)==4
  for runtime in runtimes:
   R=Path(runtime['binary']);pin(R,{'bytes':R.stat().st_size,'sha256':runtime['sha256']})
   flags=['-Rcm','full','-Rcc','jit']if runtime['repo']=='uwvm2'else['-Raot']
   flags+=['-Rct','0','-Rllvm-cache-path','disable','--wasip1-global-noinherit-system-environment']
   for artifact in products:
    wasm=Path(artifact['path']);pin(wasm,artifact);report=json.loads(Path(artifact['import_report']).read_bytes());assert report['status']=='PURE_WASI_IMPORTS_NO_ROS_CREDIT'and all(x['module']=='wasi_snapshot_preview1'for x in report['imports'])
    check(runtime['repo']+'-'+runtime['selection']+'-opt'+artifact['profile'],[R,*flags,'--run',wasm,'65537'],65537,'UWVM-LLVM',artifact,metadata={'repository':runtime['repo'],'language_debug_selection':runtime['selection'],'raw_Wasm_execution_not_disabled':True})
  W=Path('/home/macromodel/.wasmtime/bin/wasmtime');assert W.is_file();pin(W)
  row=run('wasmtime-version',[W,'--version'],S,limit=256<<20)
  version=(Path(row['log_directory'])/'stdout.log').read_text();assert version.startswith('wasmtime ')and not(Path(row['log_directory'])/'stderr.log').read_bytes();result['Wasmtime_version']=version.strip()
  for artifact in products:
   check('wasmtime-opt'+artifact['profile'],[W,'run','-C','cache=n,parallel-compilation=n',artifact['path'],'65537'],65537,'Wasmtime',artifact,limit=1<<30)
  result['unqualified_reference_engines']={'WAVM':'No re-admitted binary selected; not executed','WasmEdge':'No re-admitted binary selected; not executed','searched_candidate_paths':{s:Path(s).exists()for s in ['/home/macromodel/.wasmedge/bin/wasmedge','/home/macromodel/.local/bin/wavm','/opt/wavm/bin/wavm']}}
  assert len(result['cases'])==28 and len(result['commands'])==30
  for path,identity in list(result['pins'].items()):assert pin(path)==identity
  result['after']=guard();before_events=dict(l.split()for l in result['before']['memory_events'].splitlines());after_events=dict(l.split()for l in result['after']['memory_events'].splitlines());result['shared_memory_event_deltas']={k:int(after_events[k])-int(before_events[k])for k in before_events};assert all(result['shared_memory_event_deltas'][k]==0 for k in ['oom','oom_kill','oom_group_kill']);result['shared_max_event_attribution']='Unknown; shared file-cache reclaim may include charges caused by this functional cohort'
  result['status']='ACTUAL_SAME_SOURCE_NATIVE3_UWVM_LLVM20_WASMTIME5_CODEC_FUNCTIONAL_CASES_PASSED'
 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'functional_cases':len(result.get('cases',[]))}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
