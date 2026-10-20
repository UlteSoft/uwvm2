"""Serial low-memory Go SDK producer checks in the original Linux cgroup."""
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
ZIG=V/'root-language-zig-tinygo-prefixes-20261019-r203/prefixes/zig/zig-x86_64-linux-0.14.1/zig'
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 waiting=time.monotonic()
 while True:
  try:
   before=guard();assert before['memory_current']<61<<30,'compiler reserve61GiB admission';break
  except AssertionError as e:
   if str(e)not in ['shared current62GiB guard','effective current56GiB guard','compiler reserve61GiB admission']:raise
   assert time.monotonic()-waiting<1200,'initial shared cgroup guard wait1200s exhausted'
   print(json.dumps({'status':'WAITING_ORIGINAL_SHARED_CAPACITY','reason':str(e),'wait_seconds':round(time.monotonic()-waiting)}),flush=True);time.sleep(15)
 (O/'build-runs-r5').mkdir();(O/'cache').mkdir(exist_ok=True);(O/'tmp').mkdir(exist_ok=True);(O/'gopath').mkdir(exist_ok=True)
 env={'PATH':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/go/bin:/usr/bin:/bin','HOME':str(O/'home'),'TMPDIR':str(O/'tmp'),'TZ':'UTC','GOTOOLCHAIN':'local','GOPROXY':'off','GOSUMDB':'off','GOWORK':'off','GOMAXPROCS':'1','GOMEMLIMIT':'192MiB','GOCACHE':str(O/'cache'),'GOPATH':str(O/'gopath'),'GOENV':'off','BINARYEN_CORES':'1','MAKEFLAGS':'-j1','OMP_NUM_THREADS':'1','TINYGOROOT':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/tinygo','GOROOT':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/go','GOFLAGS':'-mod=readonly','LD_LIBRARY_PATH':str(V/'host-compiler-r1/lib')+':'+str(V/'host-compiler-r1/lib/x86_64-unknown-linux-gnu')}

 (O/'home').mkdir(exist_ok=True)
 result={'status':'RUNNING','scope':'Actual gofmt and Go WASI Debug/Release builds only; no target/ROS/DBG/performance credit until separately executed.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'producer-result-r5.json').write_text(json.dumps(result,indent=2)+'\n')
 def run(label,argv,cwd,expected=0,deadline=900,limit=512<<20):
  guard();d=O/'build-runs-r5'/label;d.mkdir()
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
  result['scope']='Actual native Go model suite plus TinyGo five-optimization two-backend SDK resources/test/coverage producers. Raw coverage export is never executed.'
  result['tools']={str(p):{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}for p in[tiny,go,TOOLS/'go/bin/gofmt']}
  manifest=json.loads((O/'source-inputs-r3.json').read_bytes())
  for row in manifest['members']:assert sha((S/row['path']).read_bytes())==row['sha256']
  run('gofmt-new-files',[TOOLS/'go/bin/gofmt','-w',S/'sdk/tinygo/construction.go',S/'sdk/tinygo/construction_test.go',S/'sdk/examples/tinygo/native_resources/main.go',S/'sdk/examples/tinygo/sdk_codec_debug/main.go',S/'sdk/examples/tinygo/codec_benchmark/main.go'],S)
  (O/'source-formatted-r5.json').write_text(json.dumps({'members':[{'path':str(p.relative_to(S)),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}for p in sorted(S.rglob('*'))if p.is_file()]},indent=2)+'\n')
  run('native-model-suite',[go,'test','-p=1','-count=1','-json','./...'],S/'sdk/tinygo',limit=768<<20)
  validator=Path('/home/macromodel/Documents/uwvm-test-261018/workspaces/uwvm-hll-20261017/tools/wasm-tools-1.262.0/wasm-tools')
  checker=V/'root-language-go-sdk-20261019-a3/inspection-source/sdk/tools/check_wasm_imports.py'
  result['tools'].update({str(p):{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}for p in[validator,checker]})
  for backend in ['ros2','ros1']:
   for profile in ['0','1','2','s','z']:
    for kind in ['resources','coverage','sdk-tests']:
     folder=O/'products-r5'/(backend+'-opt'+profile);folder.mkdir(parents=True,exist_ok=True);wasm=folder/(kind+'.wasm')
     flags=['-p','1','-target=wasip1','-scheduler=none','-opt='+profile,'-panic=trap','-tags='+backend,'-o',wasm]
     if kind=='sdk-tests':
      flags=[f.replace('-scheduler=none','-scheduler=asyncify')if isinstance(f,str)else f for f in flags]
      argv=[tiny,'test','-c',*flags,'.'];cwd=S/'sdk/tinygo'
     else:argv=[tiny,'build',*flags,'./native_resources'if kind=='resources'else'./coverage'];cwd=S/'sdk/examples/tinygo'
     label=backend+'-opt'+profile+'-'+kind
     run('build-'+label,argv,cwd,limit=768<<20,deadline=1200)
     run('validate-'+label,[validator,'validate','--features','all',wasm],S)
     inspection=folder/(kind+'-imports.json')
     if kind!='sdk-tests':
      check=[sys.executable,'-S','-B',checker,wasm,'--backend',backend[-1],'--allow-module','wasi_snapshot_preview1','--report',inspection]
      if kind=='coverage':check+=['--require-all']
      run('imports-'+label,check,S)
     result['artifacts'].append({'path':str(wasm),'bytes':wasm.stat().st_size,'sha256':sha(wasm.read_bytes()),'backend':backend,'profile':profile,'kind':kind,'scheduler':'asyncify'if kind=='sdk-tests'else'none','import_report':None if kind=='sdk-tests'else str(inspection)});save()
  result['after']=guard();assert result['before']['memory_events']==result['after']['memory_events'];result['status']='ACTUAL_TINYGO_30_PRODUCERS_AND_NATIVE_MODELS_PASSED'

 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'products':len(result['artifacts'])}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
