"""Serial low-memory Go SDK producer checks in the original Linux cgroup."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-tinygo-complete-examples-20261019-a7';S=O/'source-r1'
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
 admission={'status':'WAITING_CAPACITY','commands_started':0,'required_raw_current_below':61<<30}
 def admit_save():(O/'admission-r5.json').write_text(json.dumps(admission,indent=2)+'\n')
 admit_save()
 while True:
  try:
   before=guard();assert before['memory_current']<61<<30,'compiler reserve61GiB admission';break
  except AssertionError as e:
   if str(e)not in ['shared current62GiB guard','effective current56GiB guard','compiler reserve61GiB admission']:raise
   if time.monotonic()-waiting>=1200:
    admission.update(status='NOT_RUN_CAPACITY',reason=str(e),wait_seconds=time.monotonic()-waiting);admit_save()
    (O/'producer-result-r5.json').write_text(json.dumps({'status':'NOT_RUN_CAPACITY','commands':[],'artifacts':[],'admission':admission,'all15_complete':False},indent=2)+'\n');return
   admission.update(reason=str(e),wait_seconds=time.monotonic()-waiting);admit_save()
   print(json.dumps({'status':'WAITING_ORIGINAL_SHARED_CAPACITY','reason':str(e),'wait_seconds':round(time.monotonic()-waiting)}),flush=True);time.sleep(15)
 admission.update(status='ADMITTED',before=before,wait_seconds=time.monotonic()-waiting);admit_save()
 (O/'build-runs-r5').mkdir();(O/'cache').mkdir(exist_ok=True);(O/'tmp').mkdir(exist_ok=True);(O/'gopath').mkdir(exist_ok=True)
 env={'PATH':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/go/bin:/usr/bin:/bin','HOME':str(V/'root-language-tinygo-sdk-resources-20261019-a6/home'),'TMPDIR':str(O/'tmp'),'TZ':'UTC','GOTOOLCHAIN':'local','GOPROXY':'off','GOSUMDB':'off','GOWORK':'off','GOMAXPROCS':'1','GOMEMLIMIT':'192MiB','GOCACHE':str(V/'root-language-tinygo-sdk-resources-20261019-a6/cache'),'GOPATH':str(O/'gopath'),'GOENV':'off','BINARYEN_CORES':'1','MAKEFLAGS':'-j1','OMP_NUM_THREADS':'1','TINYGOROOT':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/tinygo','GOROOT':'/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed/go','GOFLAGS':'-mod=readonly','LD_LIBRARY_PATH':str(V/'host-compiler-r1/lib')+':'+str(V/'host-compiler-r1/lib/x86_64-unknown-linux-gnu')}

 (O/'home').mkdir(exist_ok=True)
 result={'status':'RUNNING','scope':'Actual gofmt and Go WASI Debug/Release builds only; no target/ROS/DBG/performance credit until separately executed.','before':before,'commands':[],'artifacts':[],'all15_complete':False,'new_performance_credit':False}
 def save():(O/'producer-result-r5.json').write_text(json.dumps(result,indent=2)+'\n')
 def run(label,argv,cwd,expected=0,deadline=900,limit=512<<20):
  wait_started=time.monotonic()
  if label.startswith(('build-','refuse-','invalid-selection-','independent-invalid-go-')):
   while True:
    try:
     state=guard();assert state['memory_current']<61<<30,'per-compiler reserve61GiB';break
    except AssertionError as e:
     if str(e)not in ['shared current62GiB guard','effective current56GiB guard','per-compiler reserve61GiB']:raise
     assert time.monotonic()-wait_started<1200,'per-command capacity wait1200s'
     print(json.dumps({'status':'WAITING_COMMAND_CAPACITY','label':label,'reason':str(e)}),flush=True);time.sleep(15)
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
  row['passed']=error is None and (row['exit']!=0 if expected=='compile-refusal'else row['exit']==expected)
  save();print(json.dumps({'label':label,'passed':row['passed'],'exit':row['exit'],'error':error,'peak_rss':row['sampled_peak_own_rss']}),flush=True)
  assert row['passed'],label
  guard()
  return row
 try:
  TOOLS=Path('/home/macromodel/.local/share/uwvm-ros-extension/20261014-sdk-toolchains-recovery-r1/installed')
  tiny=TOOLS/'tinygo/bin/tinygo';go=TOOLS/'go/bin/go'
  validator=O/'validation-tools/wasm-tools'
  checker=S/'sdk/tools/check_wasm_imports.py'
  result['scope']='Complete committed self-contained TinyGo SDK example build/validation/import matrix. Optional owned-RMW generated-source route explicitly excluded pending genuine staged records. Compilation is not native ROS, DBG or performance evidence.'
  result['tools']={str(t):{'bytes':t.stat().st_size,'sha256':sha(t.read_bytes())}for t in[tiny,go,validator,checker]}
  manifest=json.loads((O/'source-inputs-r1.json').read_bytes())
  for row in manifest['members']:assert sha((S/row['path']).read_bytes())==row['sha256']
  result['source_manifest_sha256']=sha((O/'source-inputs-r1.json').read_bytes())
  lessons=['raw','typed','node','resources','servers','events_tf_lifecycle','scheduling','action_parameters','tf_graph_clock','unified_events','native_resources','coverage','sdk_codec_debug','codec_benchmark']
  result['lessons']=lessons;result['refusals']=[]
  parents=[('build-runs-r1',json.loads((O/'producer-result-r1.json').read_bytes())),('build-runs-r2',json.loads((O/'producer-result-r2.json').read_bytes()))]
  earlier=parents[-1][1];assert earlier['status']=='FAILED_RETAINED'and len(earlier['artifacts'])==121
  for _,parent in parents:assert parent['source_manifest_sha256']==result['source_manifest_sha256']and {Path(k).name:v for k,v in parent['tools'].items()}=={Path(k).name:v for k,v in result['tools'].items()}
  result['validator_relocation']=json.loads((O/'validator-restoration-r1.json').read_bytes())
  result['reused_artifact_parents']=[{'result':str(O/('producer-result-r'+str(i+1)+'.json')),'sha256':sha((O/('producer-result-r'+str(i+1)+'.json')).read_bytes()),'failed_whole_cohort':True,'reason':parent.get('primary')}for i,(_,parent)in enumerate(parents)]
  result['reused_commands']=[]
  for artifact in earlier['artifacts']:
   assert sha(Path(artifact['path']).read_bytes())==artifact['sha256']
   suffix=artifact['backend']+'-opt'+artifact['profile']+'-'+artifact['kind']
   for prefix in ['build-','validate-','imports-']:
    matches=[(folder,row)for folder,parent in parents for row in parent['commands']if row['label']==prefix+suffix and row['passed']and row['exit']==0 and row['error']is None]
    assert len(matches)==1,(prefix,suffix)
    folder,row=matches[0];assert row['complete_output']
    for stream in ['stdout','stderr']:assert sha((O/folder/row['label']/(stream+'.log')).read_bytes())==row[stream+'_sha256']
    result['reused_commands'].append({'log_directory':str(O/folder/row['label']),'command':row})
   result['artifacts'].append(artifact)
  already={(a['backend'],a['profile'],a['kind'])for a in result['artifacts']}
  try:
   env['GOOS']='wasip1';env['GOARCH']='wasm'
   row=run('independent-excluded-go-list',[go,'list','-e','-json','-tags=tinygo,ros1','./events_tf_lifecycle'],S/'sdk/examples/tinygo',limit=768<<20)
   selected=json.loads((O/'build-runs-r5'/row['label']/'stdout.log').read_bytes())
   assert selected.get('IgnoredGoFiles')==['main.go']and not selected.get('GoFiles')and'build constraints exclude all Go files'in selected['Error']['Err']
   result['independent_excluded_file_selection']=selected
   for name,tags in [('neither','tinygo'),('both','tinygo,ros1,ros2')]:
    output=O/('go-refused-'+name+'.wasm')
    row=run('independent-invalid-go-'+name,[go,'build','-p=1','-tags='+tags,'-o',output,'./raw'],S/'sdk/examples/tinygo',expected='compile-refusal',limit=768<<20)
    text=b''.join((O/'build-runs-r5'/row['label']/(stream+'.log')).read_bytes()for stream in ['stdout','stderr'])
    assert b'select_exactly_one_of_ros1_or_ros2'in text and not output.exists()
  finally:env.pop('GOOS',None);env.pop('GOARCH',None)
  save()
  result['explicit_unqualified_generated_route']={'entry':'owned_rmw_service','reason':'Requires actual source-pair-generated customtypes/bounded_records.go and independent optional control ABI/native platform closure; absent generated inputs are not silently fabricated','compiled':False}
  result['owned_cache_reuse']={'HOME':env['HOME'],'GOCACHE':env['GOCACHE'],'legacy_evidence_sources_or_products_modified':False}
  pure={'scheduling','sdk_codec_debug','codec_benchmark'}
  for backend in ['ros2','ros1']:
   for profile in ['0','1','2','s','z']:
    for lesson in lessons:
     folder=O/'products-r5'/(backend+'-opt'+profile);folder.mkdir(parents=True,exist_ok=True)
     wasm=folder/(lesson+'.wasm');label=backend+'-opt'+profile+'-'+lesson
     if(backend,profile,lesson)in already:continue
     argv=[tiny,'build','-p','1','-target=wasip1','-scheduler=none','-opt='+profile,'-panic=trap','-tags='+backend,'-o',wasm,'./'+lesson]
     if backend=='ros1'and lesson=='events_tf_lifecycle':
      row=run('refuse-'+label,argv,S/'sdk/examples/tinygo',expected='compile-refusal',limit=768<<20)
      err=(O/'build-runs-r5'/row['label']/'stderr.log').read_text()
      assert err=='package uwvm2.local/roswasm-examples/events_tf_lifecycle\n\n',repr(err)
      assert not wasm.exists();result['refusals'].append({'label':label,'reason':'documented ROS2-only main','passed':True});save();continue
     run('build-'+label,argv,S/'sdk/examples/tinygo',limit=768<<20,deadline=1200)
     run('validate-'+label,[validator,'validate','--features','all',wasm],S)
     report=folder/(lesson+'-imports.json')
     if lesson in pure:
      script="import runpy,json,pathlib,sys;d=runpy.run_path(sys.argv[1]);p=pathlib.Path(sys.argv[2]);rows=d['inspect'](p.read_bytes());assert all(r['module']=='wasi_snapshot_preview1' for r in rows);pathlib.Path(sys.argv[3]).write_text(json.dumps({'status':'PURE_WASI_IMPORTS_NO_ROS_CREDIT','imports':rows},indent=2));print('PASS pure imports without ROS credit')"
      run('imports-'+label,[sys.executable,'-I','-S','-B','-c',script,checker,wasm,report],S)
     else:
      check=[sys.executable,'-S','-B',checker,wasm,'--backend',backend[-1],'--allow-module','wasi_snapshot_preview1','--report',report]
      if lesson=='coverage':check+=['--require-all']
      run('imports-'+label,check,S)
     result['artifacts'].append({'path':str(wasm),'bytes':wasm.stat().st_size,'sha256':sha(wasm.read_bytes()),'backend':backend,'profile':profile,'kind':lesson,'scheduler':'none','import_report':str(report),'pure_no_ROS_credit':lesson in pure});save()
  for profile in ['0','z']:
   for name,tags in [('neither',''),('both','ros1,ros2')]:
    wasm=O/('refused-'+name+'-'+profile+'.wasm')
    argv=[tiny,'build','-p','1','-target=wasip1','-scheduler=none','-opt='+profile,'-panic=trap','-o',wasm]
    if tags:argv+=['-tags='+tags]
    argv+=['./raw'];label='invalid-selection-'+name+'-'+profile
    row=run(label,argv,S/'sdk/examples/tinygo',expected='compile-refusal',limit=768<<20)
    err=(O/'build-runs-r5'/label/'stderr.log').read_text()
    assert ('select_exactly_one_of_ros1_or_ros2'in err or err.startswith('package uwvm2.local/roswasm')) and not wasm.exists(),err
    result['refusals'].append({'label':label,'reason':'invalid explicit backend selection','passed':True});save()
  assert len(result['artifacts'])==135 and len(result['refusals'])==9
  for row in manifest['members']:assert sha((S/row['path']).read_bytes())==row['sha256']
  result['after']=guard();assert result['before']['memory_events']==result['after']['memory_events']
  result['status']='ACTUAL_TINYGO135_SELF_CONTAINED_EXAMPLE_PRODUCERS_AND9_BACKEND_REFUSALS_PASSED'

 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'products':len(result['artifacts'])}),flush=True)
 if result['status']=='FAILED_RETAINED':raise SystemExit(1)
if __name__=='__main__':main()
