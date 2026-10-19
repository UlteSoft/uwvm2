"""One bounded gated native/Wasm kernel; stderr or stdout gates and tick/ns timers.

Called only by a bounded original-cgroup parent owner. A correctness-only plan
may select another CPU with --cpu; such a row cannot qualify performance. The actual native and Wasm
programs supply their own checked kernel timer and exact stable output.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,resource,signal,subprocess,sys,time


def main():
 p=argparse.ArgumentParser();p.add_argument('--plan',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--cpu',type=int,default=21);p.add_argument('--kind',choices=['timed','hotspots','uarch-detailed'],default='timed');a=p.parse_args();plan=json.loads(a.plan.read_bytes());a.out.mkdir();resource.setrlimit(resource.RLIMIT_CORE,(0,0));assert a.cpu in range(32) and (a.kind=='timed' or a.cpu==21);os.sched_setaffinity(0,{a.cpu});G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope';assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G;cg=Path('/sys/fs/cgroup')/G.lstrip('/');assert(cg/'memory.max').read_text().strip()==str(64<<30)and(cg/'memory.swap.max').read_text().strip()=='0'
 sys.path.insert(0,str(Path(__file__).parent));from owned_process import remember_process,terminate_group,owned_process_record
 from vtune_reports import hardware_reports
 from debug_sampling_safety import SamplingSession
 H=lambda b:hashlib.sha256(b).hexdigest()
 def hf(p):
  h=hashlib.sha256()
  with Path(p).open('rb')as f:
   while b:=f.read(65536):h.update(b)
  return h.hexdigest()
 pins={x:hf(x)for x in plan['pin_files']};assert all(v==plan['pin_files'][x]for x,v in pins.items());events=(cg/'memory.events').read_text();rate=Path('/proc/sys/kernel/perf_event_max_sample_rate').read_text();gate=plan.get('gate_stream','stderr');assert gate in ('stdout','stderr');assert a.cpu==21 or plan.get('performance_eligible') is False;r={'passed':False,'performance_eligible':plan.get('performance_eligible',True),'cpu':a.cpu,'kind':a.kind,'plan':plan,'input_hashes':pins,'before_memory_events':events,'perf_event_max_sample_rate_before':rate,'target':None,'collector':None,'scope':'Kernel inside READY/G/DONE/D; timer and DONE IO included, pre-gate producer initialization excluded; process time is separate','measurement_context':'formal sampling requires ended build/debug owners; target CPU recorded explicitly; shared host may run unrelated work; hardware collector also CPU21','source_language_debugger_claim':False};target=collector=safety=None;handles=[];all_tids=set();start=time.monotonic()
 def save():(a.out/'report.json').write_text(json.dumps(r,indent=2)+'\n')
 def observe():
  assert time.monotonic()-start<1800,'whole profile deadline';assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G and int((cg/'memory.current').read_text())<60<<30 and(cg/'memory.events').read_text()==events
  assert Path('/proc/sys/kernel/perf_event_max_sample_rate').read_text()==rate,'sampling rate changed'
  for proc in [target,collector]:
   if proc is None:continue
   remember_process(proc)
   if proc.poll()is None:assert Path('/proc/'+str(proc.pid)+'/cgroup').read_text().strip()=='0::'+G
  if target is not None and target.poll()is None:
   for t in Path('/proc/'+str(target.pid)+'/task').iterdir():
    try:assert os.sched_getaffinity(int(t.name))=={a.cpu};all_tids.add(int(t.name))
    except ProcessLookupError:pass
  for name in ['stdout','stderr']:
   path=a.out/(name+'.log')
   if path.exists():assert path.stat().st_size<2<<20
  if safety:safety.check()
 def until_marker(marker,timeout):
  deadline=time.monotonic()+timeout
  while time.monotonic()<deadline:
   observe();raw=(a.out/(gate+'.log')).read_bytes()
   if marker+b'\n'in raw:return
   assert target.poll()is None,'target ended before marker';time.sleep(.03)
  raise TimeoutError('profile marker '+repr(marker))
 def wait(proc,timeout):
  deadline=time.monotonic()+timeout
  while proc.poll()is None:
   observe();assert time.monotonic()<deadline,'bounded process wait';time.sleep(.05)
  return proc.returncode
 try:
  env=dict(os.environ);env.update(plan.get('environment',{}));env['TMPDIR']=str(a.out);env['TERM']='dumb';env['LC_ALL']='C'
  handles=[(a.out/(n+'.log')).open('wb')for n in ['stdout','stderr']];target=subprocess.Popen(plan['argv'],stdin=subprocess.PIPE,stdout=handles[0],stderr=handles[1],env=env,start_new_session=True);remember_process(target);r['target_pid']=target.pid;save();until_marker(b'UWVM_PROFILE_READY',900);r['cold_ready_seconds']=time.monotonic()-start;assert(a.out/(gate+'.log')).read_bytes()==b'UWVM_PROFILE_READY\n';assert(a.out/(('stdout'if gate=='stderr'else'stderr')+'.log')).read_bytes()==b'';observe();time.sleep(.05);observe();assert target.poll()is None and(a.out/(gate+'.log')).read_bytes()==b'UWVM_PROFILE_READY\n';r['ready_gate_blocked_before_release']=True
  if a.kind!='timed':
   safety=SamplingSession(a.out,sampled_cpus=[9,21],sampling_interval_ms=10);vtune='/opt/intel/oneapi/vtune/latest/bin64/vtune';r['vtune_sha256']=hf(vtune);options=['-duration','30','-knob','sampling-interval=10','-cpu-mask','9,21','-knob','enable-stack-collection=false']
   if a.kind=='hotspots':collect=['-collect-with','runsa'];options+=['-knob','enable-driverless-collection=true','-knob','event-config=CPU_CLK_UNHALTED.REF_TSC:sa=2000000,INST_RETIRED.ANY:sa=2000000']
   else:collect=['-collect','uarch-exploration'];options+=['-knob','pmu-collection-mode=detailed']
   argv=[vtune,*collect,'-result-dir',str(a.out/'result'),'-target-pid',str(target.pid),*options];r['collect_argv']=argv;log=(a.out/'collect.log').open('wb');handles.append(log);collector=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);remember_process(collector);deadline=time.monotonic()+60
   while time.monotonic()<deadline:
    observe();assert collector.poll()is None,'collector failed before attach';text=(a.out/'collect.log').read_text(errors='replace')
    if 'Collection started'in text or'Collection resumed'in text:break
    time.sleep(.1)
   else:raise TimeoutError('collector attach deadline')
  r['target_at_gate']={'pid':target.pid,'tids':sorted(all_tids),'cpu':a.cpu,'status':Path('/proc/'+str(target.pid)+'/status').read_text()};r['before_release_schedstat']=Path('/proc/'+str(target.pid)+'/schedstat').read_text();r['before_release_process_stat']=Path('/proc/'+str(target.pid)+'/stat').read_text();t=time.monotonic();target.stdin.write(b'G');target.stdin.flush();until_marker(b'UWVM_PROFILE_DONE',28);r['observed_gate_wall_seconds']=time.monotonic()-t;r['after_done_schedstat']=Path('/proc/'+str(target.pid)+'/schedstat').read_text();r['after_done_process_stat']=Path('/proc/'+str(target.pid)+'/stat').read_text();r['target_tids']=sorted(all_tids);assert(a.out/(gate+'.log')).read_bytes()==b'UWVM_PROFILE_READY\nUWVM_PROFILE_DONE\n';time.sleep(.05);observe();assert target.poll()is None and(a.out/(gate+'.log')).read_bytes()==b'UWVM_PROFILE_READY\nUWVM_PROFILE_DONE\n';r['done_gate_blocked_before_release']=True
  if a.kind!='timed':
   assert collector.poll()is None,'collector ended before DONE';stoplog=(a.out/'stop.log').open('wb');handles.append(stoplog);stop=subprocess.Popen([vtune,'-command','stop','-r',str(a.out/'result')],stdout=stoplog,stderr=subprocess.STDOUT,start_new_session=True);remember_process(stop)
   try:assert wait(stop,120)==0
   finally:terminate_group(stop)
   assert wait(collector,180)==0
  target.stdin.write(b'D');target.stdin.flush();target.stdin.close();assert wait(target,60)==0;r['target_exit']=target.returncode
  stdout=(a.out/'stdout.log').read_text();stderr=(a.out/'stderr.log').read_bytes();markers=b'UWVM_PROFILE_READY\nUWVM_PROFILE_DONE\n';assert stderr==(markers if gate=='stderr'else b''),stderr[-2000:];payload=stdout if gate=='stderr'else stdout.removeprefix(markers.decode());assert gate=='stderr'or stdout.startswith(markers.decode());lines=payload.splitlines();timer=[x for x in lines if re.fullmatch(plan['timer_pattern'],x)];assert len(timer)==1;assert [x for x in lines if x not in timer]==plan['expected_stable_lines'];m=re.fullmatch(plan['timer_pattern'],timer[0]);divisor=int(m.group('frequency'))if plan.get('kernel_time')=='ticks'else plan['timer_divisor'];value=int(m.group('ticks'))if plan.get('kernel_time')=='ticks'else int(m.group(1));assert divisor>0;r['kernel_seconds']=value/divisor;assert 0<r['kernel_seconds']<28;r['complete_stdout']=stdout;r['complete_stderr_sha256']=H(stderr)
  if a.kind!='timed':
   if a.kind=='hotspots':r['actual_event_configuration']=(a.out/'result/config/runsa.options').read_text()
   receipt=a.out/'collection-receipt.json';receipt.write_text(json.dumps({'target_pid':target.pid,'vtune_sha256':r['vtune_sha256'],'hybrid_pmu':{'target':21},'performance_eligible':True,'scope':r['scope'],'kind':a.kind,'target_tids':sorted(all_tids)},indent=2)+'\n');r.update(hardware_reports(vtune,a.out,a.kind,True,target_pid=target.pid,target_cpu=21,target_tids=sorted(all_tids),profile_receipt=receipt))
  assert {p:hf(p)for p in pins}==pins;r['after_memory_events']=(cg/'memory.events').read_text();assert r['after_memory_events']==events;r['perf_event_max_sample_rate_after']=Path('/proc/sys/kernel/perf_event_max_sample_rate').read_text();assert r['perf_event_max_sample_rate_after']==rate;r['passed']=True
 except BaseException as error:
  import traceback
  r.update(error=repr(error),traceback=traceback.format_exc())
 finally:
  for proc in [collector,target]:
   if proc is not None:
    try:terminate_group(proc)
    except BaseException as error:r.setdefault('cleanup_errors',[]).append(repr(error));r['passed']=False
  if safety:safety.close()
  for stream in handles:stream.close()
  if target is not None:r['target_exit']=target.returncode
  r['target_ownership']=owned_process_record(target)if target else None;r['collector_ownership']=owned_process_record(collector)if collector else None;r['process_wall_seconds']=time.monotonic()-start;save()
 print(json.dumps({'passed':r['passed'],'kernel_seconds':r.get('kernel_seconds'),'error':r.get('error'),'report':str(a.out/'report.json')}));return 0 if r['passed']else 1
if __name__=='__main__':raise SystemExit(main())
