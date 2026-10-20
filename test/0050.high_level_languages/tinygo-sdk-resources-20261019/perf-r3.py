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
 assert current<60<<30,'shared current60GiB guard'
 assert current-int(stats.get('inactive_file','0'))<56<<30,'effective current56GiB guard'
 return {'memory_current':current,'memory_events':(c/'memory.events').read_text()}
def proc(pid):
 p=Path('/proc')/str(pid);raw=(p/'stat').read_text();fields=raw[raw.rfind(')')+2:].split()
 status=(p/'status').read_text();rss=next((int(l.split()[1])*1024 for l in status.splitlines()if l.startswith('VmRSS:')),None)
 return {'pid':pid,'pgrp':int(fields[2]),'start':int(fields[19]),'rss':rss,'cgroup':(p/'cgroup').read_text().strip()}

import importlib.util,selectors,stat,types,re
def require(value,message):
 if not value:raise ValueError(message)
def main():
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('--run-name',required=True);parser.add_argument('--full',action='store_true');parser.add_argument('--wrong-values',action='store_true');parser.add_argument('--iterations',type=int,required=True);parser.add_argument('--calibration',action='store_true');options=parser.parse_args();require(1<=options.iterations<=500000000,'bounded iterations')
 require(re.fullmatch(r'[a-z0-9-]{1,40}',options.run_name)is not None,'owned run name')
 base=V/'root-language-go-sdk-20261019-a3'
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=guard();start=time.monotonic();actors=[];owned_pins={}
 root=O/options.run_name;root.mkdir();scratch=Path('/tmp')/('uwvm-tinygo-a6-'+str(os.getpid()));scratch.mkdir(mode=0o700)
 result={'status':'RUNNING','scope':'Actual TinyGo ROS2 resource lessons and selected wrong-value controls against independent native peers; no ROS1, source DBG, performance, other architectures or full SDK parity.','before':before,'commands':[],'cases':[],'scratch':str(scratch),'all15_complete':False}
 def save():(O/(options.run_name+'-result.json')).write_text(json.dumps(result,indent=2)+'\n')
 helper=base/'native-recovery/helper.py';require(sha(helper.read_bytes())=='09940a0f6395fdbb13563d533384af1edc85d87e5d8bd1ea0145b1157432e42d','reviewed owner helper identity')
 owner=types.ModuleType('_go_sdk_native_owner');owner.__file__=str(helper);exec(compile(helper.read_bytes(),str(helper),'exec',dont_inherit=True),owner.__dict__)
 common=(base/'native-recovery/common.py').read_text();require(sha(common.encode())=='01980c5908b460c8eadc8931b8438ea481220902a6801ec6b4be815c0dc5d686','reviewed collector')
 scope={'selectors':selectors,'os':os,'time':time};exec(compile(common[common.index('class Drains:'):common.index('\ndef strict_json')],str(base/'native-recovery/common.py'),'exec'),scope);Drains=scope['Drains']
 result['helper_sha256']=sha(helper.read_bytes())
 def observe():
  state=guard();require(state['memory_events']==before['memory_events'],'memory event changed');require(time.monotonic()-start<3600,'cohort3600s deadline')
  total=0
  for a in actors:
   owner.remember_process(a['process']);a['drains'].poll();require(not a['drains'].failed(),'complete bounded PIPE')
   for m in owner.owned_process_record(a['process'])['members']:
    try:
     r=proc(m['identity']['pid'])
     if r['start']!=m['identity']['start']:continue
     require(r['cgroup']=='0::'+G,'owned process cgroup')
     total+=r['rss']or 0
    except(FileNotFoundError,ProcessLookupError):pass
  require(total<4<<30,'combined performance actors4GiB RSS')
  result['sampled_peak_combined_rss']=max(result.get('sampled_peak_combined_rss',0),total)
 def launch(label,argv,environment=None,held=False,stdin=False,expected=0):
  observe();folder=root/label;folder.mkdir();drains=Drains(folder);rd,wr=os.pipe()
  env={'PATH':'/usr/bin:/bin','HOME':str(root),'TMPDIR':str(O/'tmp'),'LC_ALL':'C.UTF-8',**(environment or{})}
  boot='import json,os,resource,sys;f=int(sys.argv[1]);os.sched_setaffinity(0,{20});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(180,180));assert os.read(f,1)==b"G";os.close(f);a=sys.argv[2:];a=json.load(open(a[1]))if a[0]=="--deferred-argv-file"else a;os.execve(a[0],a,dict(os.environ))'
  p=None
  try:
   p=subprocess.Popen([sys.executable,'-I','-S','-B','-c',boot,str(rd),*map(str,argv)],cwd=root,env=env,start_new_session=True,pass_fds=(rd,),stdin=subprocess.PIPE if stdin else subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
   drains.attach(p);owner.remember_process(p,force=True)
   row={'label':label,'argv':list(map(str,argv)),'environment':env,'pid':p.pid,'held_before_exec':held,'expected_exit':expected,'passed':False}
   a={'process':p,'gate':wr,'drains':drains,'folder':folder,'row':row,'final':False};actors.append(a);result['commands'].append(row);wr=None
   if not held:release(a)
   save();return a
  finally:
   os.close(rd)
   if wr is not None:os.close(wr)
   if p is None:drains.stop()
 def release(a):
  observe();os.write(a['gate'],b'G');os.close(a['gate']);a['gate']=None
 def marker(a,prefix):
  deadline=time.monotonic()+60
  while True:
   observe();b=(a['folder']/'stdout.log').read_bytes()
   if any(line.startswith(prefix.encode())for line in b.splitlines()):return
   require(a['process'].poll()is None,'actor exited before '+prefix);require(time.monotonic()<deadline,'marker deadline '+prefix);time.sleep(.02)
 def mapped(a,executable,sdk):
  text=(Path('/proc')/str(a['process'].pid)/'maps').read_text();require(len(text)<1<<20,'maps bound');rows={}
  for line in text.splitlines():
   fields=line.split(None,5)
   if len(fields)<6 or 'x'not in fields[1]or not fields[5].startswith('/'):continue
   p=Path(fields[5]);require(p.is_relative_to(sdk)or p==executable,'foreign executable mapped '+str(p))
   b=p.read_bytes();rows[str(p)]={'bytes':len(b),'sha256':sha(b)};owned_pins[str(p)]=rows[str(p)]
  a['row'].setdefault('mapping_snapshots',[]).append({'text':text,'sha256':sha(text.encode()),'physical_code_files':rows});save()
 def finish(a):
  if a['final']:return
  p=a['process'];row=a['row']
  if p.stdin is not None and not p.stdin.closed:p.stdin.close()
  if a['gate']is not None:os.close(a['gate']);a['gate']=None
  try:owner.terminate_group(p,term_timeout=3,kill_timeout=2)
  except BaseException as e:row['cleanup_error']=repr(e)
  deadline=time.monotonic()+3
  while not a['drains'].eof()and time.monotonic()<deadline:a['drains'].poll();time.sleep(.01)
  a['drains'].stop();row['ownership']=owner.owned_process_record(p);row['exit']=p.poll();row['complete_output']=a['drains'].complete();row['output']=a['drains'].evidence()
  for n in['stdout','stderr']:
   b=(a['folder']/(n+'.log')).read_bytes();row[n+'_bytes']=len(b);row[n+'_sha256']=sha(b)
  own=row['ownership'];row['passed']=row['exit']==row['expected_exit'] and row['complete_output']and own['closed']and not own['signals']and not own['retirement_errors']and not row.get('cleanup_error')
  a['final']=True;save()
 def wait(a):
  deadline=time.monotonic()+720
  while a['process'].poll()is None:observe();require(time.monotonic()<deadline,'actor720s deadline');time.sleep(.02)
  finish(a);require(a['row']['passed'],'actor failed '+a['row']['label'])
 def pin(p,expected=None):
  b=p.read_bytes();row={'bytes':len(b),'sha256':sha(b)}
  if expected:require(row['sha256']==expected,'selected input identity '+str(p))
  owned_pins[str(p)]=row;return row

 def credit(a,byte):
  require(a['process'].poll()is None and a['process'].stdin is not None and not a['process'].stdin.closed,'owned input credit')
  a['process'].stdin.write(byte);a['process'].stdin.flush();a['row'].setdefault('credits',[]).append({'byte':byte.decode(),'elapsed':time.monotonic()-start});save()
 def lines(a):return(a['folder']/'stdout.log').read_text().splitlines()
 def poll_guest(worker,positive,delivered,idle):
  for iteration in range(64):
   previous=len(lines(worker));credit(worker,b'P');deadline=time.monotonic()+60
   while True:
    observe();output=lines(worker)
    if positive and delivered in output:return
    if not positive and worker['process'].poll()is not None:
     finish(worker);require(worker['row']['passed'],'expected wrong-value Guest refusal');return
    if len(output)>previous:
     require(output[-1]==idle,'exact idle marker or complete delivery');break
    require(worker['process'].poll()is None,'Guest exited before complete event')
    require(time.monotonic()<deadline,'one finite Guest poll deadline');time.sleep(.02)
   time.sleep(.05)
  raise ValueError('64 exact owner poll credits exhausted')
 try:
  import fcntl,random
  lock=os.open('/tmp/uwvm-hll-benchmark.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
  deadline=time.monotonic()+1800
  while True:
   guard()
   try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);break
   except BlockingIOError:
    require(time.monotonic()<deadline,'existing benchmark lock wait1800s')
    print(json.dumps({'status':'WAITING_EXISTING_BENCHMARK_LOCK'}),flush=True);time.sleep(15)
  producer=json.loads((O/'probe-producer-result-r1.json').read_bytes())
  require(producer['status']=='ACTUAL_TINYGO_CODEC_PROBES_AND_SAME_SOURCE_NATIVE_BENCHMARK_BUILT_VALIDATED','source producer')
  native=next(a for a in producer['artifacts']if a['kind']=='codec-native')
  wasm=next(a for a in producer['artifacts']if a['kind']=='codec-benchmark'and a['backend']=='ros2')
  pin(Path(native['path']),native['sha256']);pin(Path(wasm['path']),wasm['sha256'])
  def seed_after(count):
   mult,add=1664525,1013904223;factor,offset=1,0;mask=0xffffffff
   while count:
    if count&1:factor=(mult*factor)&mask;offset=(mult*offset+add)&mask
    add=(mult*add+add)&mask;mult=(mult*mult)&mask;count>>=1
   return (factor*0x12345678+offset)&mask
  expected=['READY_TINYGO_SDK_CODEC','DONE_TINYGO_SDK_CODEC '+str(options.iterations)+' '+str(seed_after(options.iterations))]
  plans=[{'label':'native-go','argv':[native['path'],str(options.iterations)],'pin_files':{native['path']:native['sha256']},'expected_stable_lines':expected,'environment':{'GOMAXPROCS':'1'},'scope':'Same actual SDK codec benchmark source, native Go default optimization and runtime'}]
  origin=Path('/tmp/root-language-debug-budget-20261019-r136/result.json')
  for r in json.loads(origin.read_bytes())['products']:
   if r['selection']!='all':continue
   R=Path(r['binary']);pin(R,r['sha256'])
   mode=['-Rcm','full','-Rcc','jit']if r['repo']=='uwvm2'else['-Raot']
   plans.append({'label':r['repo'],'argv':[str(R),*mode,'-Rct','0','-Rllvm-cache-path','disable','--run',wasm['path'],str(options.iterations)],'pin_files':{str(R):r['sha256'],wasm['path']:wasm['sha256']},'expected_stable_lines':expected,'scope':'Same TinyGo opt2 Wasm; LLVM without debugger or JIT cache; no ROS import in codec kernel'})
  W=Path('/home/macromodel/.wasmtime/bin/wasmtime').resolve(strict=True);wp=pin(W)
  plans.append({'label':'wasmtime','argv':[str(W),'run','-C','cache=n,parallel-compilation=n',wasm['path'],str(options.iterations)],'pin_files':{str(W):wp['sha256'],wasm['path']:wasm['sha256']},'expected_stable_lines':expected,'scope':'Same TinyGo opt2 Wasm on installed Wasmtime; actual binary pinned'})
  sources=O/'perf-source-r1'
  for p in sources.iterdir():
   if p.is_file():pin(p)
  result['scope']='Finite same-source native Go versus four Wasm engines SDK String codec comparison. Supervisor G-to-observed-DONE monotonic wall time includes bounded polling/IO. Not full ROS latency or general language parity.'
  result['expected_complete_lines']=expected;result['iterations']=options.iterations;result['plans']=plans;result['runs']=[]
  result['calibration_only']=options.calibration;result['all15_complete']=False
  if not options.calibration:
   result['ended_own_dependencies']={}
   for name,status in [('native-full-r1-result.json','ACTUAL_TINYGO_NATIVE_RESOURCES_AND_SELECTED_WRONG_VALUES_PASSED'),('rollback-r2-result.json','ACTUAL_TINYGO10_PARTIAL_FACTORY_ROLLBACKS_PASSED'),('debug-r2-result.json','ACTUAL_TINYGO_MINIMAL_CODEC_SOURCE_DEBUG_SESSIONS_PASSED')]:
    path=O/name;d=json.loads(path.read_bytes());require(d['status']==status,'own prerequisite ended '+name);require(all(c['ownership']['closed']for c in d['commands']),'own prerequisite actors closed');result['ended_own_dependencies'][name]=sha(path.read_bytes())
  planfiles={}
  for plan in plans:
   f=root/(plan['label']+'.plan.json');f.write_text(json.dumps(plan,indent=2)+'\n');planfiles[plan['label']]=f
  phases=['calibration']if options.calibration else['warmup','sample0','sample1','sample2']
  for phase in phases:
   order=plans.copy();random.Random('tinygo-codec-'+phase).shuffle(order)
   for plan in order:
    label=phase+'-'+plan['label'];out=root/(label+'-data')
    actor=launch(label,[sys.executable,'-I','-B',sources/'profile_one.py','--plan',planfiles[plan['label']],'--out',out,'--kind','timed'])
    wait(actor);d=json.loads((out/'report.json').read_bytes());require(d['passed'],'actual exact codec output and timed owner');result['runs'].append({'phase':phase,'engine':plan['label'],'kind':'timed','kernel_seconds':d['kernel_seconds'],'cold_ready_seconds':d['cold_ready_seconds'],'report':str(out/'report.json'),'report_sha256':sha((out/'report.json').read_bytes())});save();print(json.dumps(result['runs'][-1]),flush=True)
  if not options.calibration:
   for plan in plans:
    for kind in ['hotspots','uarch-detailed']:
     label=kind+'-'+plan['label'];out=root/(label+'-data')
     actor=launch(label,[sys.executable,'-I','-B',sources/'profile_one.py','--plan',planfiles[plan['label']],'--out',out,'--kind',kind])
     wait(actor);d=json.loads((out/'report.json').read_bytes());require(d['passed'],'actual qualified hardware report');result['runs'].append({'phase':'hardware','engine':plan['label'],'kind':kind,'kernel_seconds':d['kernel_seconds'],'report':str(out/'report.json'),'report_sha256':sha((out/'report.json').read_bytes())});save();print(json.dumps(result['runs'][-1]),flush=True)
  for path,pin_value in owned_pins.items():require(pin(Path(path))==pin_value,'changed selected benchmark input')
  result['preserved_inputs']=owned_pins;result['after']=guard();require(result['after']['memory_events']==before['memory_events'],'final memory events')
  result['status']='ACTUAL_TINYGO_CODEC_CALIBRATION_PASSED'if options.calibration else'ACTUAL_TINYGO_NATIVE_FOUR_WASM_TIMINGS_AND10_HARDWARE_COLLECTIONS_PASSED'

 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  for actor in reversed(actors):
   try:finish(actor)
   except BaseException as e:result.setdefault('finalization_errors',[]).append(repr(e))
  if result.get('finalization_errors')and result['status']!='FAILED_RETAINED':result['status']='FAILED_FINALIZATION_RETAINED'
  if scratch.exists()and not list(scratch.iterdir()):scratch.rmdir()
  result['driver_sha256']=sha(Path(__file__).read_bytes());result['process_creation_monitor']='Sampled ancestry/group with retained pidfds; detached helpers escaping every observed anchor can be missed.';save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'cases':len(result['cases'])}),flush=True)
 if result['status']not in ['ACTUAL_TINYGO_CODEC_CALIBRATION_PASSED','ACTUAL_TINYGO_NATIVE_FOUR_WASM_TIMINGS_AND10_HARDWARE_COLLECTIONS_PASSED']:raise SystemExit(1)
if __name__=='__main__':main()
