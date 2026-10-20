"""Actual full validation, binary import and UWVM Go SDK consumer checks."""
from pathlib import Path
import datetime,hashlib,json,os,resource,signal,subprocess,sys,time,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
O=V/'root-language-tinygo-complete-examples-20261019-a7';S=O/'source'
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
 prerequisite=json.loads((O/'readback-partial-r2.json').read_bytes())
 require(prerequisite['status']=='INDEPENDENT_INERT_TINYGO_PARTIAL_EXAMPLE_READBACK_PASSED'and prerequisite['products']==121,'independent completed-product import and closed-log readback')
 waiting=time.monotonic()
 while True:
  try:
   before=guard();require(before['memory_current']<61<<30,'debug reserve61GiB admission');break
  except (ValueError,AssertionError) as e:
   if str(e)not in ['shared current62GiB guard','effective current56GiB guard','debug reserve61GiB admission']:raise
   require(time.monotonic()-waiting<1200,'debug capacity wait1200s');print(json.dumps({'status':'WAITING_DEBUG_CAPACITY','reason':str(e)}),flush=True);time.sleep(15)
 start=time.monotonic();actors=[];owned_pins={}
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
  require(total<3<<30,'combined debug actors3GiB RSS')
  result['sampled_peak_combined_rss']=max(result.get('sampled_peak_combined_rss',0),total)
 def launch(label,argv,environment=None,held=False,stdin=False,expected=0):
  observe();folder=root/label;folder.mkdir();drains=Drains(folder);rd,wr=os.pipe()
  env={'PATH':'/usr/bin:/bin','HOME':str(root),'TMPDIR':str(O/'tmp'),'LC_ALL':'C.UTF-8',**(environment or{})}
  boot='import json,os,resource,sys;f=int(sys.argv[1]);os.sched_setaffinity(0,{20});resource.setrlimit(resource.RLIMIT_AS,(128<<30,128<<30));resource.setrlimit(resource.RLIMIT_CPU,(300,300));assert os.read(f,1)==b"G";os.close(f);a=sys.argv[2:];a=json.load(open(a[1]))if a[0]=="--deferred-argv-file"else a;os.execve(a[0],a,dict(os.environ))'
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
  base=V/'root-language-go-sdk-20261019-a3'
  origin=json.loads(Path('/tmp/root-language-debug-budget-20261019-r136/result.json').read_bytes());runtimes=[{'path':r['binary'],'sha256':r['sha256'],'mode':r['repo']}for r in origin['products']if r['selection']=='all'];result['LLVM_build_origin_sha256']=sha(Path('/tmp/root-language-debug-budget-20261019-r136/result.json').read_bytes())
  producer=json.loads((O/'producer-result-r2.json').read_bytes());require(producer['status']=='FAILED_RETAINED' and len(producer['artifacts'])==121,'individually completed source-bound TinyGo products');result['producer_parent']={'path':str(O/'producer-result-r2.json'),'sha256':sha((O/'producer-result-r2.json').read_bytes()),'failed_whole_cohort':True,'selected_completed_probes_only':True};result['independent_input_parent']={'path':str(O/'readback-partial-r2.json'),'sha256':sha((O/'readback-partial-r2.json').read_bytes()),'ordinary_full_prerequisite_claimed':False}
  source=O/'source-r1/sdk/examples/tinygo/sdk_codec_debug/main.go'
  line=next(i for i,s in enumerate(source.read_text().splitlines(),1)if '// TINYGO_SDK_CODEC_READY'in s)
  result['scope']='Actual TinyGo five-optimization source-debug capability observations on three LLVM products. Missing optimized locations are explicitly unavailable, not fabricated values; no ROS import, native callback or performance credit.'
  products=[a for a in producer['artifacts']if a['kind']=='sdk_codec_debug'and a['backend']=='ros2']
  result['source']={'path':str(source),'line':line,'identity':pin(source)};result['sessions']=[]
  for runtime in runtimes if options.full else runtimes[:1]:
   for artifact in products if options.full else products[:1]:
    waiting=time.monotonic()
    while True:
     try:require(guard()['memory_current']<61<<30,'debug reserve61GiB admission');break
     except (ValueError,AssertionError) as e:
      if str(e)not in ['shared current62GiB guard','effective current56GiB guard','debug reserve61GiB admission']:raise
      require(time.monotonic()-waiting<1200,'per-session debug capacity wait');time.sleep(10)
    R=Path(runtime['path']);wasm=Path(artifact['path']);pin(R,runtime['sha256']);pin(wasm,artifact['sha256'])
    actor=launch(runtime['mode']+'-opt'+artifact['profile']+'-'+artifact['backend'],[R,'-Rdbg','-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-cache-path','disable','--run',wasm],stdin=True)
    cursor=0;actions=[];session={'runtime':runtime['mode'],'profile':artifact['profile'],'backend':artifact['backend'],'wasm_sha256':artifact['sha256'],'runtime_sha256':runtime['sha256'],'actions':actions,'source_breakpoint':False,'value_available':False,'source_step':False,'stale_read_refused':False,'normal_exit':False};result['sessions'].append(session);save()
    def prompt(command=None):
     nonlocal cursor
     if command is not None:credit(actor,(command+'\n').encode())
     deadline=time.monotonic()+120
     while True:
      observe();rawout=(actor['folder']/'stdout.log').read_bytes();pos=rawout.find(b'(uwvm-debug) ',cursor)
      if pos>=0:
       reply=rawout[cursor:pos+13];cursor=pos+13;actions.append({'command':command,'reply':reply.decode(errors='replace')});save();return reply
      require(actor['process'].poll()is None,'debug console exited early')
      require(time.monotonic()<deadline,'debug prompt120s deadline');time.sleep(.02)
    initial=prompt();require(b'uwvm-debug'in initial,'actual console')
    require(b'prepared; no Wasm instruction executed'in prompt('status'),'prepared source launch')
    bp=prompt('breaksource 0 '+str(source)+':'+str(line));found=re.search(rb'breakpoint (\d+)',bp)
    if found is not None:
     session['source_breakpoint']=True;prompt('continue')
     for _ in range(64):
      stopped=prompt('wait')
      if b'stopped: breakpoint'in stopped:break
      require(b'guest exited:'not in stopped and b'Program exited'not in stopped,'program exited before requested source breakpoint')
     else:raise ValueError('source breakpoint stop bound')
     match=re.search(rb'thread (\d+) module=',stopped);require(match is not None,'actual stopped thread');thread=int(match[1])
     trace=prompt('bt '+str(thread));sid=re.search(rb'stop-id (\d+)',trace);require(sid is not None,'actual stop identity');stop_id=int(sid[1])
     require(str(source).encode()in trace,'actual compiler source file')
     session['exact_source_line']=(str(source)+':'+str(line)+':').encode()in trace
     command='print '+str(thread)+' '+str(stop_id)+' state.Code';value=prompt(command)
     require(value==prompt(command),'same-stop read-only value or refusal')
     if re.search(rb'(?:, value=| = [iu]32=)13(?: |\r|\n)',value)is not None:session['value_available']=True;session['value']=13
     else:
      require(b'error: source value unavailable'in value,'unexpected source value result')
      session['value_unavailable_reason']=value.decode(errors='replace')
     prompt('delete '+found[1].decode())
     stepped=prompt('step source '+str(thread)+' into')
     for _ in range(64):
      if b'stopped:'in stepped or b'Program exited'in stepped or b'guest exited:'in stepped:break
      require(b'error:'not in stepped,'unexpected step refusal');stepped=prompt('wait')
     else:raise ValueError('step completion bound')
     if b'stopped: selected participant step'in stepped:
      newer=prompt('bt '+str(thread));new_sid=re.search(rb'stop-id (\d+)',newer);require(new_sid is not None and int(new_sid[1])>stop_id,'source step must advance stop')
      session['source_step']=True
      stale=prompt(command);require(b'error:'in stale and b'source-value stop='not in stale,'stale value must be refused');session['stale_read_refused']=True
     else:
      require(b'Program exited normally.'in stepped or b'guest exited: 0'in stepped,'step may only finish normally')
      session['source_step_exited_normally']=True
    else:
     require(b'error:'in bp,'source breakpoint has neither ID nor explicit refusal')
     session['source_breakpoint_refusal']=bp.decode(errors='replace')
    if not session.get('source_step_exited_normally'):
     prompt('continue')
     for _ in range(64):
      exited=prompt('wait')
      if b'guest exited:'in exited or b'Program exited'in exited:
       require(b'guest exited: 0'in exited or b'Program exited normally.'in exited,'actual codec probe must complete');break
     else:raise ValueError('debug Guest exit bound')
    credit(actor,b'quit\n');wait(actor);session['normal_exit']=True
    if artifact['profile']=='0':
     require(all(session[k]for k in ['source_breakpoint','exact_source_line','value_available','source_step','stale_read_refused']),'opt0 baseline must remain fully available')
    session['finite_full_value_workflow']=all(session.get(k,False)for k in ['source_breakpoint','exact_source_line','value_available','source_step','stale_read_refused','normal_exit'])
    save();print(json.dumps({'session':len(result['sessions']),'runtime':runtime['mode'],'profile':artifact['profile'],'finite_full_value_workflow':session['finite_full_value_workflow'],'normal_exit':True}),flush=True)
  require(len(result['sessions'])==(15 if options.full else 1),'complete selected debug matrix')
  result['all_profiles_full_value_workflow']=all(s['finite_full_value_workflow']for s in result['sessions'])
  result['preserved_inputs']=owned_pins
  for path,identity in owned_pins.items():require(pin(Path(path))==identity,'changed debug input')
  result['after']=guard();require(result['after']['memory_events']==before['memory_events'],'debug memory events');result['status']='ACTUAL_TINYGO_FIVE_OPT_THREE_REPOSITORY_SOURCE_DEBUG_CAPABILITIES_RECORDED'
 except BaseException as e:
  result['status']='FAILED_RETAINED';result['primary']=repr(e);result['traceback']=traceback.format_exc()
 finally:
  for actor in reversed(actors):
   try:finish(actor)
   except BaseException as e:result.setdefault('finalization_errors',[]).append(repr(e))
  if scratch.exists()and not list(scratch.iterdir()):scratch.rmdir()
  result['driver_sha256']=sha(Path(__file__).read_bytes());save()
 print(json.dumps({'status':result['status'],'primary':result.get('primary'),'sessions':len(result.get('sessions',[]))}),flush=True)
 if result['status']!='ACTUAL_TINYGO_FIVE_OPT_THREE_REPOSITORY_SOURCE_DEBUG_CAPABILITIES_RECORDED':raise SystemExit(1)
if __name__=='__main__':main()
