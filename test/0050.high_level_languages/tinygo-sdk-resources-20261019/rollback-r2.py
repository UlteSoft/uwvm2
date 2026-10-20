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

import importlib.util,selectors,stat,types,re
def require(value,message):
 if not value:raise ValueError(message)
def main():
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('--run-name',required=True);parser.add_argument('--full',action='store_true');parser.add_argument('--wrong-values',action='store_true');parser.add_argument('--lessons',nargs='+',choices=['subscribe','trigger','set-bool','server','parameter']);options=parser.parse_args()
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
  require(total<1<<30,'combined actors1GiB RSS')
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
  deadline=time.monotonic()+100
  while a['process'].poll()is None:observe();require(time.monotonic()<deadline,'actor100s deadline');time.sleep(.02)
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
  built=json.loads((O/'native-build-result-r1.json').read_bytes());require(built['status']=='ACTUAL_TINYGO_NATIVE_RESOURCE_WITNESS_COMPILE_LINK_PASSED','native build admission')
  peer=Path(built['product']['path']);pin(peer,built['product']['sha256']);sdk=Path(built['SDK_root']);lib=built['library_path']
  native=json.loads((base/'native-recovery/native-origin.json').read_bytes());gateway=Path(native['binary']['path']);pin(gateway,native['binary']['sha256']);loader=sdk/'lib/x86_64-linux-gnu/ld-linux-x86-64.so.2';pin(loader,'41daf7836f7fce21a40ca63a00c5caf21b05a35a76ccc075d86f1716212cdf01')
  result['native_build_result_sha256']=sha((O/'native-build-result-r1.json').read_bytes());result['native_origin']=native;result['peer']=built['product'];result['loader']=owned_pins[str(loader)];result['resource_guards']={'cgroup_raw_max_exclusive':62<<30,'cgroup_effective_max_exclusive':56<<30,'combined_actor_RSS_max_exclusive':1<<30,'peer_deadline_seconds':90,'actor_wait_seconds':100,'whole_cohort_seconds':3600}
  producer=json.loads((O/'producer-result-r5.json').read_bytes());require(producer['status']=='ACTUAL_TINYGO_30_PRODUCERS_AND_NATIVE_MODELS_PASSED','new source-built TinyGo products')
  products=producer['artifacts'];result['TinyGo_producer_result_sha256']=sha((O/'producer-result-r5.json').read_bytes());runtimes=json.loads((base/'runtime-restoration.json').read_bytes())['runtimes']
  selected=[a for a in products if a['backend']=='ros2'and Path(a['path']).name=='resources.wasm'];require(len(selected)==5,'five source-bound TinyGo resource products')
  for row in json.loads((O/'source-formatted-r5.json').read_bytes())['members']:
   current=O/'source-r3'/row['path']
   require(pin(current)['sha256']==row['sha256'],'current TinyGo input differs from compiler product')
  policy=O/'source-r3/sdk/examples/tinygo/native_resources/policy.py'
  for p in(O/'source-r3').rglob('*'):
   if p.is_file():pin(p)
  choices=[(runtime,artifact)for runtime in runtimes for artifact in selected]
  result['scope']='Actual ROS2 serve-only partial factory rollback through the selected Gateway; no native peer delivery or failed-cleanup injection credit.'
  result['planned_cases']=len(choices)
  for index,(runtime,artifact)in enumerate(choices):
   case=root/('case'+str(index));case.mkdir();short=scratch/('c'+str(index));short.mkdir(mode=0o700);sock=short/'g.sock'
   ns='/uwvm_tinygo_rollback_a6_p'+str(os.getpid())+'/c'+str(index)
   R=Path(runtime['path']);wasm=Path(artifact['path']);pin(R,runtime['sha256']);pin(wasm,artifact['sha256'])
   argv_file=case/'worker-argv.json'
   worker=launch('c'+str(index)+'-tinygo',['--deferred-argv-file',argv_file],held=True,stdin=True)
   preparation=launch('c'+str(index)+'-policy',[sys.executable,'-S','-B',policy,'--lesson','rollback','--backend','2','--worker-uid',str(os.getuid()),'--worker-pid',str(worker['process'].pid),'--gateway-uid',str(os.getuid()),'--socket',sock,'--node-name','tinygo_resource_gateway','--namespace',ns,'--resource',ns,'--parameter-node',ns+'/sdk_parameter_node','--parameter-kind','string','--domain-id','194','--disabled-security-tutorial','--gateway-timeout-ms','15000','--out',case/'policy'])
   wait(preparation);plan=json.loads((case/'policy/policy.json').read_bytes())
   require('worker-pid '+str(worker['process'].pid)+'\n'in plan['Gateway_config_text'],'held actual PID')
   business=[str(R),*plan['Worker_arguments'],'--wasm-set-main-module-name','tinygo_resources','--run',str(wasm),'rollback']
   argv_file.write_text(json.dumps(business)+'\n');argv_file.chmod(0o600);worker['row']['target_argv']=business
   native_env={'AMENT_PREFIX_PATH':str(sdk/'opt/ros/humble'),'ROS_LOG_DIR':str(case/'logs'),'RMW_IMPLEMENTATION':'rmw_fastrtps_cpp','ROS_DOMAIN_ID':'194','ROS_SECURITY_ENABLE':'false'}
   gate=launch('c'+str(index)+'-gateway',[loader,'--inhibit-cache','--library-path',lib,gateway,case/'policy/gateway.conf'],native_env)
   deadline=time.monotonic()+20
   while not sock.exists():observe();require(gate['process'].poll()is None and time.monotonic()<deadline,'private socket readiness');time.sleep(.02)
   require(stat.S_ISSOCK(sock.lstat().st_mode)and stat.S_IMODE(sock.lstat().st_mode)==0o600,'actual private socket')
   mapped(gate,gateway,sdk);release(worker);marker(worker,'READY_TINYGO_ROS_ROLLBACK')
   require(str((Path('/proc')/str(worker['process'].pid)/'exe').resolve())==str(R.resolve()),'actual Runtime in held PID')
   credit(worker,b'A')
   for actor in [worker,gate]:wait(actor)
   require(lines(worker)==['READY_TINYGO_ROS_ROLLBACK','PASS_TINYGO_ROS_RESOURCES'],'exact rollback Guest protocol')
   stderr=(worker['folder']/'stderr.log').read_text().splitlines()
   require(all(line.startswith('{')for line in stderr),'no hidden rollback error');audit=[json.loads(line)for line in stderr]
   denied=[r for r in audit if r.get('event')=='unresolved-denial']
   require(len(denied)==2 and all(r['error']==13 and r['module']=='tinygo_resources'for r in denied),'ungranted alias and missing respond refused')
   creates=[r for r in audit if r.get('event')=='resource-create'];closes=[r for r in audit if r.get('event')=='resource-close']
   require(len(creates)==len(closes)==1,'one acquisition and one retirement')
   require(all(r['operation']=='service.advertise'and r['error']==0 for r in creates+closes),'exact serve owner cleanly retired')
   require(not any(r.get('event')=='allow'for r in audit),'no business operation before refused factory')
   require(all(r.get('error')in (0,13)for r in audit),'unexpected rollback errno')
   require(not sock.exists(),'Gateway socket retired');short.rmdir()
   result['cases'].append({'runtime':runtime['mode'],'profile':artifact['profile'],'wasm_sha256':artifact['sha256'],'runtime_sha256':runtime['sha256'],'policy':plan,'missing_respond_refused13':True,'one_create_one_close':True,'session_remained_open_before_explicit_shutdown':True,'native_peer_delivery':False,'worker_exit':0,'Gateway_exit':0});save()
   print(json.dumps({'rollback_case':index,'passed':True}),flush=True)
  for path,pin_value in owned_pins.items():require(pin(Path(path))==pin_value,'changed selected source/product/mapping input')
  result['preserved_inputs']=owned_pins;result['after']=guard();require(result['after']['memory_events']==before['memory_events'],'final memory events')
  result['status']='ACTUAL_TINYGO10_PARTIAL_FACTORY_ROLLBACKS_PASSED'
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
 if result['status']!='ACTUAL_TINYGO10_PARTIAL_FACTORY_ROLLBACKS_PASSED':raise SystemExit(1)
if __name__=='__main__':main()
