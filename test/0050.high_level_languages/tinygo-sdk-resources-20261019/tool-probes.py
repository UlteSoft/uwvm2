"""Serial low-memory Go SDK producer checks in the original Linux cgroup."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-tinygo-sdk-resources-20261019-a6';S=O/'source-r2'
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
ZIG=V/'root-language-zig-tinygo-prefixes-20261019-r203/prefixes/zig/zig-x86_64-linux-0.14.1/zig'
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=guard();(O/'tool-runs').mkdir();(O/'cache').mkdir(exist_ok=True);(O/'tmp').mkdir(exist_ok=True);(O/'gopath').mkdir(exist_ok=True)
 env={'PATH':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/go/bin:/usr/bin:/bin','HOME':str(O/'home'),'TMPDIR':str(O/'tmp'),'TZ':'UTC','GOTOOLCHAIN':'local','GOPROXY':'off','GOSUMDB':'off','GOWORK':'off','GOMAXPROCS':'1','GOMEMLIMIT':'192MiB','GOCACHE':str(O/'cache'),'GOPATH':str(O/'gopath'),'GOENV':'off','LD_LIBRARY_PATH':str(V/'host-compiler-r1/lib')+':'+str(V/'host-compiler-r1/lib/x86_64-unknown-linux-gnu')}

 (O/'home').mkdir(exist_ok=True)
 result={'status':'RUNNING','scope':'Actual gofmt and Go WASI Debug/Release builds only; no target/ROS/DBG/performance credit until separately executed.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'tool-probe-result.json').write_text(json.dumps(result,indent=2)+'\n')
 def run(label,argv,cwd,expected=0,deadline=900,limit=512<<20):
  guard();d=O/'tool-runs'/label;d.mkdir()
  def prepare():
   os.sched_setaffinity(0,{18});resource.setrlimit(resource.RLIMIT_AS,(16<<30,16<<30));resource.setrlimit(resource.RLIMIT_CPU,(900,900))
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
  row['passed']=error is None and row['exit']==expected
  save();print(json.dumps({'label':label,'passed':row['passed'],'exit':row['exit'],'error':error,'peak_rss':row['sampled_peak_own_rss']}),flush=True)
  assert row['passed'],label
  guard()
  return row
 try:
  TOOLS=Path('/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed')
  tiny=TOOLS/'tinygo/bin/tinygo';go=TOOLS/'go/bin/go'
  result['scope']='Actual installed compiler versions and test/build help only; no producer or target qualification.'
  result['tools']={str(p):{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}for p in[tiny,go]}
  run('go-version',[go,'version'],O)
  run('tinygo-version',[tiny,'version'],O)
  run('tinygo-help-build',[tiny,'help','build'],O)
  run('tinygo-help-test',[tiny,'help','test'],O)
  result['after']=guard();assert result['after']['memory_events']==result['before']['memory_events'];result['status']='ACTUAL_TINYGO_INSTALLED_TOOL_PROBES_PASSED'

 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'products':len(result['artifacts'])}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
