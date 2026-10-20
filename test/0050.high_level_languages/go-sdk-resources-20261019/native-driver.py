"""Actual full validation, binary import and UWVM Go SDK consumer checks."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-go-sdk-20261019-a3';S=O/'source'
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
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,128<<30));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=guard();start=time.monotonic();actors=[];owned_pins={}
 root=O/'native-cases';root.mkdir();scratch=Path('/tmp')/('uwvm-go-a3-'+str(os.getpid()));scratch.mkdir(mode=0o700)
 result={'status':'RUNNING','scope':'Actual official-Go ROS2 String publication with independent native graph/QoS/GID/value/quiet/retirement witness. No new resources-example positive credit, source DBG, performance, other backend/architecture or complete SDK parity.','before':before,'commands':[],'cases':[],'scratch':str(scratch),'all15_complete':False}
 def save():(O/'native-result.json').write_text(json.dumps(result,indent=2)+'\n')
 helper=O/'native-recovery/helper.py';require(sha(helper.read_bytes())=='09940a0f6395fdbb13563d533384af1edc85d87e5d8bd1ea0145b1157432e42d','reviewed owner helper identity')
 owner=types.ModuleType('_go_sdk_native_owner');owner.__file__=str(helper);exec(compile(helper.read_bytes(),str(helper),'exec',dont_inherit=True),owner.__dict__)
 common=(O/'native-recovery/common.py').read_text();require(sha(common.encode())=='01980c5908b460c8eadc8931b8438ea481220902a6801ec6b4be815c0dc5d686','reviewed collector')
 scope={'selectors':selectors,'os':os,'time':time};exec(compile(common[common.index('class Drains:'):common.index('\ndef strict_json')],str(O/'native-recovery/common.py'),'exec'),scope);Drains=scope['Drains']
 result['helper_sha256']=sha(helper.read_bytes())
 def observe():
  state=guard();require(state['memory_events']==before['memory_events'],'memory event changed');require(time.monotonic()-start<240,'cohort240s deadline')
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
 def launch(label,argv,environment=None,held=False,stdin=False):
  observe();folder=root/label;folder.mkdir();drains=Drains(folder);rd,wr=os.pipe()
  env={'PATH':'/usr/bin:/bin','HOME':str(root),'TMPDIR':str(O/'consumer-tmp'),'LC_ALL':'C.UTF-8',**(environment or{})}
  boot='import os,resource,sys;f=int(sys.argv[1]);os.sched_setaffinity(0,{20});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(120,120));assert os.read(f,1)==b"G";os.close(f);os.execve(sys.argv[2],sys.argv[2:],dict(os.environ))'
  p=None
  try:
   p=subprocess.Popen([sys.executable,'-I','-S','-B','-c',boot,str(rd),*map(str,argv)],cwd=root,env=env,start_new_session=True,pass_fds=(rd,),stdin=subprocess.PIPE if stdin else subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
   drains.attach(p);owner.remember_process(p,force=True)
   row={'label':label,'argv':list(map(str,argv)),'environment':env,'pid':p.pid,'held_before_exec':held,'passed':False}
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
  deadline=time.monotonic()+30
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
  own=row['ownership'];row['passed']=row['exit']==0 and row['complete_output']and own['closed']and not own['signals']and not own['retirement_errors']and not row.get('cleanup_error')
  a['final']=True;save()
 def wait(a):
  deadline=time.monotonic()+45
  while a['process'].poll()is None:observe();require(time.monotonic()<deadline,'actor45s deadline');time.sleep(.02)
  finish(a);require(a['row']['passed'],'actor failed '+a['row']['label'])
 def pin(p,expected=None):
  b=p.read_bytes();row={'bytes':len(b),'sha256':sha(b)}
  if expected:require(row['sha256']==expected,'selected input identity '+str(p))
  owned_pins[str(p)]=row;return row
 try:
  built=json.loads((O/'native-build-result-r6.json').read_bytes());require(built['status']=='ACTUAL_GO_NATIVE_ROS2_WITNESS_COMPILE_LINK_PASSED','native build admission')
  peer=Path(built['product']['path']);pin(peer,built['product']['sha256']);sdk=Path(built['SDK_root']);lib=built['library_path']
  native=json.loads((O/'native-recovery/native-origin.json').read_bytes());gateway=Path(native['binary']['path']);pin(gateway,native['binary']['sha256']);loader=sdk/'lib/x86_64-linux-gnu/ld-linux-x86-64.so.2';pin(loader,'41daf7836f7fce21a40ca63a00c5caf21b05a35a76ccc075d86f1716212cdf01')
  result['native_build_result_sha256']=sha((O/'native-build-result-r6.json').read_bytes());result['native_origin']=native;result['peer']=built['product'];result['loader']=owned_pins[str(loader)]
  spec=importlib.util.spec_from_file_location('_go_sdk_policy',O/'policy-source/sdk/tools/example_policy.py');policy=importlib.util.module_from_spec(spec);spec.loader.exec_module(policy)
  pin(O/'policy-source/sdk/tools/example_policy.py')
  products=json.loads((O/'producer-final-result.json').read_bytes())['artifacts'];runtimes=json.loads((O/'runtime-restoration.json').read_bytes())['runtimes']
  result['runtimes']=[{k:v for k,v in r.items()if k!='build_origin'}for r in runtimes]
  for index,(runtime,artifact)in enumerate((r,a)for r in runtimes for a in products if a['backend']=='ros2'and Path(a['path']).name=='telemetry.wasm'):
   case=root/('case'+str(index));case.mkdir();short=scratch/('c'+str(index));short.mkdir(mode=0o700);sock=short/'g.sock';ns='/uwvm_go_sdk_a3_p'+str(os.getpid())+'/c'+str(index)
   R=Path(runtime['path']);wasm=Path(artifact['path']);pin(R,runtime['sha256']);pin(wasm,artifact['sha256'])
   native_env={'AMENT_PREFIX_PATH':str(sdk/'opt/ros/humble'),'ROS_LOG_DIR':str(case/'logs'),'RMW_IMPLEMENTATION':'rmw_fastrtps_cpp','ROS_DOMAIN_ID':'193','ROS_SECURITY_ENABLE':'false'}
   witness=launch('c'+str(index)+'-peer',[loader,'--inhibit-cache','--library-path',lib,peer,ns],native_env)
   marker(witness,'READY_NATIVE_TELEMETRY ');mapped(witness,peer,sdk)
   args=types.SimpleNamespace(backend=2,socket=sock,gateway_uid=os.getuid(),worker_uid=os.getuid(),worker_pid=1,node_name='roswasm_language_gateway',namespace=ns,domain_id=193,disabled_security_tutorial=True,master_uri=None,advertise_host=None,operation=['publish'],resource=ns+'/telemetry',gateway_timeout_ms=15000)
   plan=policy.policy(args);argv=[R,'--wasip1-global-grant','monotonic-clock','--wasip1-global-grant','realtime-clock',*plan['Worker_arguments'],'--wasm-set-main-module-name','go_telemetry','--run',wasm,'publish']
   worker=launch('c'+str(index)+'-go',argv,held=True,stdin=True);args.worker_pid=worker['process'].pid;plan=policy.policy(args)
   conf=case/'gateway.conf';conf.write_text(plan['Gateway_config_text']);conf.chmod(0o600);(case/'policy.json').write_text(json.dumps(plan,indent=2)+'\n')
   gate=launch('c'+str(index)+'-gateway',[loader,'--inhibit-cache','--library-path',lib,gateway,conf],native_env)
   deadline=time.monotonic()+20
   while not sock.exists():observe();require(gate['process'].poll()is None,'Gateway exited before socket');require(time.monotonic()<deadline,'socket20s deadline');time.sleep(.02)
   require(stat.S_ISSOCK(sock.lstat().st_mode)and stat.S_IMODE(sock.lstat().st_mode)==0o600,'private socket mode');mapped(gate,gateway,sdk)
   release(worker);marker(worker,'READY_ROS_PUBLISH');marker(witness,'MATCHED_NATIVE_TELEMETRY ')
   mapped(gate,gateway,sdk);mapped(witness,peer,sdk)
   worker['process'].stdin.write(b'P');worker['process'].stdin.flush();marker(worker,'PUBLISHED_ROS_VALUE');marker(witness,'DELIVERED_NATIVE_TELEMETRY ')
   worker['process'].stdin.write(b'A');worker['process'].stdin.flush();worker['process'].stdin.close()
   for a in[worker,gate,witness]:wait(a)
   require((worker['folder']/'stdout.log').read_bytes()==b'READY_ROS_PUBLISH\nPUBLISHED_ROS_VALUE\nPASS_ROS_PUBLISH\n','exact complete Go protocol')
   audit=[json.loads(l)for l in(worker['folder']/'stderr.log').read_bytes().splitlines()];denied=[r for r in audit if r.get('event')=='unresolved-denial']
   require(len(denied)==1 and denied[0]['error']==13 and denied[0]['ros']=='ros2'and denied[0]['module']=='go_telemetry','actual forbidden-alias denial13')
   require(all(r.get('error')in(0,13)for r in audit),'unexpected Host errno')
   rows=(witness['folder']/'stdout.log').read_text().splitlines();require(len(rows)==4,'four native phases');parsed=[]
   for phase,line in zip(['READY','MATCHED','DELIVERED','PASS'],rows):
    match=re.fullmatch(phase+'_NATIVE_TELEMETRY namespace='+re.escape(ns)+r' gateway=roswasm_language_gateway gid=([0-9a-f]{48}) warmup=([01]) delivered=([01])',line);require(match is not None,'native exact phase');parsed.append({'phase':phase,'gid':match[1],'warmup':int(match[2]),'delivered':int(match[3])})
   require(parsed[0]['gid']=='0'*48 and parsed[1]['gid']!='0'*48 and len({r['gid']for r in parsed[1:]})==1 and[r['delivered']for r in parsed]==[0,0,1,1],'native exact GID/value association')
   require((witness['folder']/'stderr.log').read_bytes()==b'','native witness stderr')
   require(not sock.exists(),'Gateway socket retired')
   short.rmdir()
   result['cases'].append({'runtime_mode':runtime['mode'],'producer_profile':artifact['profile'],'wasm_sha256':artifact['sha256'],'runtime_sha256':runtime['sha256'],'namespace':ns,'policy':plan,'native_protocol':parsed,'full_native_value':'bounded WasmROS telemetry','quiet_min_seconds':2,'native_publisher_count_at_exit':0,'worker_exit':worker['row']['exit'],'Gateway_exit':gate['row']['exit'],'peer_exit':witness['row']['exit'],'native_positive_delivery':True});save();print(json.dumps({'case':index,'actual_native_delivery':True,'runtime':runtime['mode'],'producer':artifact['profile']}),flush=True)
  for p,row in owned_pins.items():require(sha(Path(p).read_bytes())==row['sha256'],'changed native selected input')
  result['preserved_inputs']=owned_pins;result['after']=guard();require(result['after']['memory_events']==before['memory_events'],'final memory events')
  result['status']='ACTUAL_GO_FOUR_NATIVE_ROS2_TELEMETRY_DELIVERIES_PASSED'
 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  for a in reversed(actors):
   try:finish(a)
   except BaseException as e:result.setdefault('finalization_errors',[]).append(repr(e))
  if result.get('finalization_errors')and result['status']!='FAILED_RETAINED':result['status']='FAILED_FINALIZATION_RETAINED'
  if scratch.exists()and not list(scratch.iterdir()):scratch.rmdir()
  result['driver_sha256']=sha(Path(__file__).read_bytes());result['process_creation_monitor']='Sampled ancestry/group with retained pidfds; fully detached short helpers can be missed.';save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'cases':len(result['cases'])}),flush=True)
 if not result['status'].endswith('DELIVERIES_PASSED'):raise SystemExit(1)
if __name__=='__main__':main()
