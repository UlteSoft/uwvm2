"""Actual full validation, binary import and UWVM Go SDK consumer checks."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-tinygo-sdk-resources-20261019-a6';S=O/'source-r3'
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
 assert current<62<<30,'shared current62GiB guard'
 assert current-int(stats.get('inactive_file','0'))<56<<30,'effective current56GiB guard'
 return {'memory_current':current,'memory_events':(c/'memory.events').read_text()}
def proc(pid):
 p=Path('/proc')/str(pid);raw=(p/'stat').read_text();fields=raw[raw.rfind(')')+2:].split()
 status=(p/'status').read_text();rss=next((int(l.split()[1])*1024 for l in status.splitlines()if l.startswith('VmRSS:')),None)
 return {'pid':pid,'pgrp':int(fields[2]),'start':int(fields[19]),'rss':rss,'cgroup':(p/'cgroup').read_text().strip()}
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=guard();(O/'ordinary-runs-r2').mkdir();(O/'ordinary-tmp-r2').mkdir()
 env={'PATH':'/usr/bin:/bin','TMPDIR':str(O/'ordinary-tmp-r2'),'TZ':'UTC'}
 result={'status':'RUNNING','scope':'Full validator, actual binary ROS signatures and actual UWVM ordinary Go SDK tests/default-denial example only. No positive native ROS, source DBG, performance or architecture credit.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'ordinary-result-r2.json').write_text(json.dumps(result,indent=2)+'\n')
 def run(label,argv,cwd,expected=0,deadline=300,limit=1<<30):
  guard();d=O/'ordinary-runs-r2'/label;d.mkdir()
  def prepare():
   os.sched_setaffinity(0,{19});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(300,300))
  row={'label':label,'argv':list(map(str,argv)),'cwd':str(cwd),'expected_exit':expected,'passed':False,'sampled_peak_own_rss':0,'processes':[]}
  result['commands'].append(row);save();start=time.monotonic();known={};p=None;error=None
  with(d/'stdout.log').open('wb')as out,(d/'stderr.log').open('wb')as err:
   try:
    p=subprocess.Popen(list(map(str,argv)),cwd=cwd,env=env,stdin=subprocess.DEVNULL,stdout=out,stderr=err,start_new_session=True,preexec_fn=prepare)
    row['pid']=p.pid;descriptor=os.pidfd_open(p.pid,0)
    try:
     while p.poll()is None:
      guard();total=0
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
      assert out.tell()+err.tell()<32<<20,'output bound'
      time.sleep(.15)
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
  guard()
  return row
 try:
  base=V/'root-language-go-sdk-20261019-a3'
  producer=json.loads((O/'producer-result-r5.json').read_bytes())
  assert producer['status']=='ACTUAL_TINYGO_30_PRODUCERS_AND_NATIVE_MODELS_PASSED'
  runtimes=json.loads((base/'runtime-restoration.json').read_bytes())['runtimes']
  result['scope']='Actual TinyGo five-profile two-backend resource default denial and compiled SDK test execution on two frozen interpreter Runtime products. Coverage export never executed. No native, DBG or performance credit.'
  result['target_workflows']=[];result['products']=producer['artifacts']
  result['runtimes']=[{k:v for k,v in r.items()if k!='build_origin'}for r in runtimes]
  for r in runtimes:
   R=Path(r['path']);assert sha(R.read_bytes())==r['sha256']
   for a in producer['artifacts']:
    if a['kind']=='coverage':continue
    wasm=Path(a['path']);assert sha(wasm.read_bytes())==a['sha256']
    label=r['mode']+'-'+a['backend']+'-'+a['profile']+'-'+a['kind']
    flags=[] if a['kind']=='resources' else ['--wasip1-global-grant','monotonic-clock','--wasip1-global-grant','realtime-clock']
    arguments=['deny']if a['kind']=='resources'else['-test.v','-test.skip','TestExecutorResetsGuardAfterCallbackPanic']
    row=run('ordinary-'+label,[R,*flags,'--run',wasm,*arguments],S)
    out=(O/'ordinary-runs-r2'/row['label']/'stdout.log').read_bytes();err=(O/'ordinary-runs-r2'/row['label']/'stderr.log').read_bytes()
    if a['kind']=='resources':assert out==b'PASS_TINYGO_ROS_RESOURCES_DEFAULT_DENIED\n'and not err
    else:
     assert b'TestExecutorResetsGuardAfterCallbackPanic'not in out
     assert b'PASS'in out and b'FAIL'not in out+err,(out[-4000:],err[-4000:])
    result['target_workflows'].append({'label':row['label'],'runtime':r['mode'],'backend':a['backend'],'producer':a['profile'],'kind':a['kind'],'explicit_WASI_grants':flags,'passed':True});save()
    if a['kind']=='resources':
     for case,args in [('unknown',['unknown']),('extra',['deny','extra']),('oversize',['x'*2048])]:
      row=run('arguments-'+label+'-'+case,[R,'--run',wasm,*args],S,expected=2)
      assert(O/'ordinary-runs-r2'/row['label']/'stdout.log').read_bytes()==b''
  result['panic_trap_controls']=[]
  for r in runtimes:
   a=next(a for a in producer['artifacts']if a['kind']=='sdk-tests'and a['backend']=='ros2'and a['profile']=='0')
   label='intentional-panic-'+r['mode']
   row=run(label,[r['path'],'--wasip1-global-grant','monotonic-clock','--wasip1-global-grant','realtime-clock','--run',a['path'],'-test.v','-test.run','TestExecutorResetsGuardAfterCallbackPanic'],S,expected=-4)
   out=(O/'ordinary-runs-r2'/label/'stdout.log').read_bytes();err=(O/'ordinary-runs-r2'/label/'stderr.log').read_bytes()
   assert b'=== RUN   TestExecutorResetsGuardAfterCallbackPanic'in out and b'--- PASS:'not in out
   assert b'catch unreachable'in err and b'TestExecutorResetsGuardAfterCallbackPanic'in err
   result['panic_trap_controls'].append({'label':label,'expected_trap':True,'native_Go_recovery_fixture_not_Wasm_supported':True})
  result['after']=guard();assert result['before']['memory_events']==result['after']['memory_events']
  result['status']='ACTUAL_TINYGO40_ORDINARY_WORKFLOWS_AND60_ARGUMENT_REFUSALS_PASSED'

 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary')}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
