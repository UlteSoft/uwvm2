#!/usr/bin/env python3
"""Independent long-GC development diagnostic; never a qualified benchmark.

Retain exact cgroup/CPU/owned-PIDFD/deadline constraints. Temperature is an
observation only; frequency quality failures remain while the guest completes.
This does not modify or replace the existing qualified thermal watchdog.
"""
import argparse,decimal,hashlib,json,os,pathlib,re,resource,signal,statistics,subprocess,threading,time
P=pathlib.Path
BOOT='import os,signal,sys;os.sched_setaffinity(0,{0});fd=int(sys.argv[1]);go=os.read(fd,1);os.close(fd);\nif not go:sys.exit(0)\nos.kill(os.getpid(),signal.SIGSTOP);os.execvpe(sys.argv[2],sys.argv[2:],os.environ)'
def digest(path):
 with P(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(path,data):P(path).write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
def counts(path):return {k:int(v) for k,v in (line.split() for line in P(path).read_text().splitlines())}
def ident(pid):
 proc=P('/proc')/str(pid);stat=(proc/'stat').read_text();f=stat[stat.rfind(')')+2:].split()
 status=dict(line.split(':',1) for line in (proc/'status').read_text().splitlines() if ':' in line)
 return {'pid':pid,'birth':int(f[19]),'ppid':int(f[1]),'pgid':int(f[2]),'state':f[0],'uid':list(map(int,status['Uid'].split())),'cgroup':(proc/'cgroup').read_text(),'argv':(proc/'cmdline').read_bytes().split(b'\0')[:-1],'cpus':status['Cpus_allowed_list'].strip(),'rss':int(status['VmRSS'].split()[0])*1024 if 'VmRSS' in status else 0}
def json_ident(row):return dict(row,argv=[os.fsdecode(x) for x in row['argv']])
def telemetry():
 temps={}
 for zone in P('/sys/class/thermal').glob('thermal_zone*'):
  try:temps[zone.name+':'+(zone/'type').read_text().strip()]=int((zone/'temp').read_text())
  except (FileNotFoundError,PermissionError,ValueError):continue
 sensor=[v for k,v in temps.items() if k.split(':',1)[1]=='x86_pkg_temp' or k.split(':',1)[1].startswith('TCPU')]
 if not sensor:raise RuntimeError('No actual CPU/package temperature')
 freq=int(P('/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq').read_text())
 return {'time_ns':time.time_ns(),'temperatures':temps,'cpu_temperature':max(sensor),'scaling_cur_freq':freq,'cpu_stat':counts('/sys/fs/cgroup/cpu.stat'),'memory_events':counts('/sys/fs/cgroup/memory.events')}
def require(condition,text):
 if not condition:raise RuntimeError(text)
class Guard:
 def __init__(self):
  require(os.getuid()==1000,'Expected UID1000')
  require(os.sched_getaffinity(0)=={16},'Controller must use E-core16')
  self.cg=P('/proc/self/cgroup').read_text();self.self=ident(os.getpid());self.init=ident(1);self.events=counts('/sys/fs/cgroup/memory.events')
  fd=os.pidfd_open(os.getpid());os.close(fd)
  require(self.init['argv']==[b'sleep',b'infinity'],'Unexpected container init')
 def check(self,child=None,birth=None):
  require(P('/sys/fs/cgroup/memory.max').read_text().strip()==str(64<<30),'64GiB guard changed')
  require(P('/sys/fs/cgroup/memory.swap.max').read_text().strip()=='0','swap guard changed')
  require(P('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip()=='0,2,4,6,16-31','CPU admission changed')
  roster=set(map(int,P('/sys/fs/cgroup/cgroup.procs').read_text().split()))
  require(roster<=({1,os.getpid()}|({child} if child else set())),'Unowned concurrent cgroup process: '+str(roster))
  for original in (self.self,self.init):
   current=ident(original['pid']);require(all(current[k]==original[k] for k in ('birth','ppid','uid','cgroup','argv')),'Baseline process identity changed')
  now=counts('/sys/fs/cgroup/memory.events');require(all(now[k]==self.events[k] for k in ('oom','oom_kill')),'Cgroup OOM changed')
  require(int(P('/sys/fs/cgroup/memory.current').read_text())<56<<30,'Insufficient cgroup headroom')
  # Guest-only diagnostic writes bounded logs; no compile/download occurs.
  # This explicit 1GiB floor never alters the qualified watchdog's 16GiB floor.
  require(os.statvfs('/work').f_bavail*os.statvfs('/work').f_frsize>=1<<30,'Diagnostic disk floor1GiB')
  require(int(next(line.split()[1] for line in P('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')))*1024>=8<<30,'Physical memory headroom8GiB')
  if child:
   try:row=ident(child)
   except FileNotFoundError:return None
   require(row['birth']==birth and row['ppid']==os.getpid() and row['pgid']==child,'Child birth/ancestry/PGID changed')
   require(child in roster or row['state']=='Z','Live child missing from exact cgroup roster')
   require(row['uid']==[1000]*4 and row['cgroup']==self.cg,'Child UID/cgroup changed')
   require(row['rss']<=6<<30,'Own child RSS above6GiB')
   try:tasks=list((P('/proc')/str(child)/'task').iterdir())
   except FileNotFoundError:
    try:retired=ident(child)
    except FileNotFoundError:return None
    require(retired['birth']==birth and retired['state']=='Z','Missing task directory on still-live/reused child')
    return retired
   for task in tasks:
    try:
     values=dict(line.split(':',1) for line in (task/'status').read_text().splitlines() if ':' in line)
     require(values['Cpus_allowed_list'].strip()=='0','Guest TID escaped P0')
     require(list(map(int,values['Uid'].split()))==[1000]*4,'Guest TID UID changed')
    except FileNotFoundError:continue
   return row
  return None
 def admit(self):
  # User criteria: temperature is recorded, never an admission/quality failure.
  # All ownership, CPU, memory, disk and OOM constraints still reject admission.
  self.check();return telemetry()
def effective_argv(item):
 argv=list(item.get('diagnostic_argv',item['argv']))
 if not item['profile'].startswith('wasmtime49/'):
  require(argv.count('--run')==1,'Ambiguous product execution command')
  require('--log-verbose' not in argv,'Unexpected preexisting verbose flag')
  argv.insert(argv.index('--run'),'--log-verbose')
 return argv
def sample(item,label,pair,out,guard):
 before=guard.admit();log=out/(label+'.log');stop=threading.Event();points=[];errors=[];pidfd=None;process=None;watcher=None;watcher_started=False;admission=None;elapsed=None;usage=None;start=None;reaped=False;cancel=None
 readfd,writefd=os.pipe()
 try:
  with log.open('wb') as stream:
   process=subprocess.Popen(['python3','-c',BOOT,str(readfd),*effective_argv(item)],stdout=stream,stderr=subprocess.STDOUT,start_new_session=True,pass_fds=(readfd,))
   os.close(readfd);readfd=None
   # The private pipe leaves the original child blocked before STOP. If
   # pidfd_open fails, closing writefd gives EOF and safely exits that child.
   pidfd=os.pidfd_open(process.pid);first=ident(process.pid)
   os.write(writefd,b'x');os.close(writefd);writefd=None;end=time.monotonic()+5
   while first['state']!='T':
    require(time.monotonic()<end,'Own bootstrap failed to stop');time.sleep(.002);first=ident(process.pid)
   admission=guard.check(process.pid,first['birth']);require(admission is not None,'Missing actual own stopped child')
   admission=json_ident(admission);start=time.perf_counter_ns();signal.pidfd_send_signal(pidfd,signal.SIGCONT)
   def monitor():
    try:
     while not stop.wait(.02):
      guard.check(process.pid,first['birth']);points.append(telemetry())
      require(log.stat().st_size<=4<<20,'Output exceeds4MiB')
      require(time.perf_counter_ns()-start<=90_000_000_000,'90s sample timeout')
    except BaseException as failure:
     errors.append(str(failure))
     try:signal.pidfd_send_signal(pidfd,signal.SIGKILL)
     except ProcessLookupError:pass
   watcher=threading.Thread(target=monitor)
   oldmask=signal.pthread_sigmask(signal.SIG_BLOCK,{signal.SIGINT,signal.SIGTERM})
   try:watcher.start();watcher_started=True
   finally:signal.pthread_sigmask(signal.SIG_SETMASK,oldmask)
   _,status,usage=os.wait4(process.pid,0);elapsed=time.perf_counter_ns()-start;process.returncode=os.waitstatus_to_exitcode(status);reaped=True
 except BaseException as failure:
  errors.append(type(failure).__name__+': '+str(failure))
  if isinstance(failure,(KeyboardInterrupt,SystemExit)):
   cancel=failure;signal.signal(signal.SIGTERM,signal.SIG_IGN);signal.signal(signal.SIGINT,signal.SIG_IGN)
 finally:
  for fd in (readfd,writefd):
   if fd is not None:os.close(fd)
  stop.set()
  try:
   if watcher_started:watcher.join()
  except BaseException as failure:errors.append('Watcher retirement: '+type(failure).__name__+': '+str(failure))
  finally:
   if process is not None and not reaped:
    if pidfd is not None:
     try:signal.pidfd_send_signal(pidfd,signal.SIGKILL)
     except ProcessLookupError:pass
    # If no PIDFD was acquired, EOF on the private pipe is the sole exit path.
    _,status,usage=os.wait4(process.pid,0);process.returncode=os.waitstatus_to_exitcode(status);reaped=True
   if pidfd is not None:os.close(pidfd)
 try:after=telemetry()
 except BaseException as failure:
  after=before.copy();after['post_telemetry_failed']=type(failure).__name__+': '+str(failure);errors.append(after['post_telemetry_failed'])
  if isinstance(failure,(KeyboardInterrupt,SystemExit)):cancel=failure
 try:guard.check()
 except BaseException as failure:
  errors.append('Post-retirement guard: '+type(failure).__name__+': '+str(failure))
  if isinstance(failure,(KeyboardInterrupt,SystemExit)):cancel=failure
 reasons=errors[:]
 if process is None or process.returncode!=0:reasons.append('Guest execution failed')
 if elapsed is None or elapsed<100_000_000:reasons.append('Sub100ms or incomplete whole process sample')
 if not points:reasons.append('Missing in-interval frequency telemetry')
 peak=max(t['cpu_temperature'] for t in [before,*points,after])
 if any(before['cpu_stat'].get(k)!=after['cpu_stat'].get(k) for k in ('nr_throttled','throttled_usec')):reasons.append('CPU throttling changed')
 row={'label':label,'pair':pair,'fixture':item['fixture'],'profile':item['profile'],'argv':effective_argv(item),'base_argv':item['argv'],'hard_failures':errors[:],'admitted_process':admission,'exit':process.returncode if process else None,'wall_ns':elapsed,'user_seconds':usage.ru_utime if usage else None,'system_seconds':usage.ru_stime if usage else None,'maxrss_kib':usage.ru_maxrss if usage else None,'before':before,'during':points,'after':after,'peak_temperature':peak,'median_run_frequency_khz':statistics.median(p['scaling_cur_freq'] for p in points) if points else None,'log_sha256':digest(log),'inconclusive_reasons':reasons}
 row['semantic_receipt']=semantic_receipt(item,log,row['exit'])
 row['guest_execution_ns']=row['semantic_receipt'].get('guest_execution_ns')
 row['observed_guest_ns_per_iteration']=(row['guest_execution_ns']/row['semantic_receipt']['iterations'] if row['guest_execution_ns'] is not None else None)
 row['formal_acceptance']=False
 with (out/'raw.jsonl').open('a') as f:f.write(json.dumps(row,allow_nan=False)+'\n')
 print(json.dumps({k:row[k] for k in ('label','exit','wall_ns','peak_temperature','inconclusive_reasons')}),flush=True)
 if cancel is not None:raise cancel
 return row
def closure(plan,out,stage):
 for product,pin in plan['products'].items():
  for key,sha in (('binary','binary_sha256'),('runtime','runtime_sha256'),('build_json','build_json_sha256')):require(digest(pin[key])==pin[sha],product+' '+key+' changed')
  for command in pin['commands'].values():require(digest(command['path'])==command['sha256'],product+' compiler command sidecar changed')
  source=P(pin['source']);actual=subprocess.check_output(['python3',str(source/'tools/ci/wasm3_source_fingerprint.py'),str(source),str(out/(product+'-source-'+stage+'.json'))],text=True).strip()
  require(actual==pin['source_id'],product+' current source differs from built bytes')
 require(digest(plan['wasmtime']['path'])==plan['wasmtime']['sha256'],'Reference ELF changed')
 for item in plan['fixtures'].values():require(digest(item['path'])==item['sha256'],'Fixture changed')
# These immutable bytes were separately checked by the official validator,
# printed by the official tool, and executed by the pinned reference VM.
LONG_FIXTURES={
 'gc-allocation-ring-128000000':(128000000,2259414293,'5eb8cb2ab852164f339636ac21fb7b0284b3d6dee0236fe9cd11e2f04e27fd76'),
 'gc-allocation-ring-512000000':(512000000,1687964949,'4344aec55a95e51d86433ab6d57305528866f18583947a1def97e81b69d26d79'),
}
def semantic_receipt(item,log,exit_code):
 n,checksum,_=LONG_FIXTURES[item['fixture']]
 text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',log.read_text(errors='replace'))
 receipt={'iterations':n,'expected_checksum':checksum,'checksum_proof':'Exit zero from pinned _start which traps on a wrong checksum; checksum is not printed.',
          'exit_zero':exit_code==0,'passed':False,'formal_acceptance':False,'failures':[]}
 if item['profile'].startswith('wasmtime49/'):
  receipt['passed']=exit_code==0
  return receipt
 for label in ('gc-managed','gc-sealed-experimental'):
  matches=re.findall(r'^\['+re.escape(label)+r'\] ([^\r\n]+)',text,re.M)
  if len(matches)!=1:receipt['failures'].append('Expected exactly one '+label+' retirement receipt');continue
  receipt[label]={k:int(v) for k,v in re.findall(r'([a-z_]+)=([0-9]+)',matches[0])}
 managed=receipt.get('gc-managed',{});sealed=receipt.get('gc-sealed-experimental',{})
 if managed.get('allocations')!=n or sealed.get('committed_slots')!=n or sealed.get('remaining')!=0:
  receipt['failures'].append('Allocation/commit/remaining mismatch')
 if not (0<managed.get('collections',0) and 0<managed.get('reclaimed',0)<=n and managed.get('disabled')==0 and managed.get('reason')==0):
  receipt['failures'].append('Missing active managed collection/reclamation')
 if '[llvm-jit-full] owning-source=yes pending-plan=native object-cache=disabled body-fallback=no' not in text:
  receipt['failures'].append('Missing native owning-source/cache-disabled receipt')
 for field,label in (('guest_execution_ns','Total WASM execution time'),('whole_process_reported_ns','Total process time')):
  matches=re.findall(re.escape(label)+r': ([0-9]+(?:\.[0-9]+)?)s\.',text)
  if len(matches)==1:
   receipt[field]=int(decimal.Decimal(matches[0])*decimal.Decimal(1000000000))
  else:receipt['failures'].append('Missing/unexpected '+label)
 receipt['unreclaimed_allocation_count']=n-managed['reclaimed'] if 'reclaimed' in managed else None
 receipt['passed']=exit_code==0 and not receipt['failures']
 return receipt

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--plan',type=P,required=True);p.add_argument('--out',type=P,required=True)
 p.add_argument('--pairs',type=int,choices=(1,2,3),default=1)
 p.add_argument('--execute',action='store_true');a=p.parse_args()
 plan_sha=digest(a.plan);runner_sha=digest(__file__);plan=json.loads(a.plan.read_text())
 require(digest(a.plan)==plan_sha,'Plan changed while loading')
 require(plan['schema']=='uwvm-current-pcore-gc-eh-plan-v1','Wrong actual-plan schema')
 names=plan['timing']['gc_fixture_pair'];require(names==list(LONG_FIXTURES),'Diagnostic requires the checked 128M/512M pair')
 for name,(n,checksum,sha) in LONG_FIXTURES.items():
  f=plan['fixtures'][name];require(f['iterations']==n and f['checksum']==checksum and f['sha256']==sha,'Long fixture binding mismatch')
 require(not a.out.exists(),'New evidence path required');a.out.mkdir(parents=True)
 save(a.out/'plan.json',plan);(a.out/'runner.py').write_bytes(P(__file__).read_bytes())
 require(a.execute,'This diagnostic requires explicit --execute')
 def cancel(signum,frame):raise KeyboardInterrupt('Controlled cancellation signal '+str(signum))
 signal.signal(signal.SIGTERM,cancel);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 rows=[];groups=[];complete=False;closure_before_ok=False;closure_after_ok=False;execution_error=None
 try:
  closure(plan,a.out,'before');closure_before_ok=True
  # This is the existing hard admission preflight, not an old thermal receipt.
  subprocess.run(['bash',str(P(plan['products']['ordinary']['source'])/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
  guard=Guard();guard.check()
  selected=[r for r in plan['commands'] if r['fixture'] in names]
  profiles=list(dict.fromkeys(r['profile'] for r in selected))
  require(set(profiles)=={'ordinary/instruction/auto','ordinary/unwind/auto','ros/instruction/auto','ros/unwind/auto','wasmtime49/copying'},'Unexpected diagnostic profiles')
  require(len(selected)==len(names)*len(profiles),'Ambiguous/missing profile command')
  for pair in range(a.pairs):
   begin=(pair//2)%len(profiles);order=profiles[begin:]+profiles[:begin]
   if pair%2:order.reverse()
   pairrows=[]
   for profile in order:
    for name in (names if pair%2==0 else names[::-1]):
     item=next(r for r in selected if r['profile']==profile and r['fixture']==name)
     row=sample(item,str(pair)+'-'+name+'-'+profile.replace('/','-'),pair,a.out,guard)
     rows.append(row);pairrows.append(row)
     require(not row['hard_failures'],'Hard safety/ownership/deadline failure; diagnostic stopped')
     require(row['semantic_receipt']['passed'],'Guest/retirement semantic failure; diagnostic stopped')
   reasons=[];starts=[r['before']['cpu_temperature'] for r in pairrows];freqs=[r['median_run_frequency_khz'] for r in pairrows]
   if None in freqs or min(freqs)<=0 or max(freqs)/min(freqs)-1>.1:reasons.append('Pair frequency spread exceeds10% or missing')
   slopes={}
   for profile in profiles:
    low=next(r for r in pairrows if r['profile']==profile and r['fixture']==names[0]);high=next(r for r in pairrows if r['profile']==profile and r['fixture']==names[1])
    slopes[profile]={}
    for key in ('wall_ns','guest_execution_ns'):
     if low[key] is not None and high[key] is not None:
      delta=high[key]-low[key];slopes[profile][key+'_high_minus_low']=delta
      slopes[profile][key+'_observed_ns_per_iteration']=delta/(LONG_FIXTURES[names[1]][0]-LONG_FIXTURES[names[0]][0])
      if delta<=0:reasons.append(profile+' nonpositive '+key+' slope')
   groups.append({'pair':pair,'profile_order':order,'temperature_start_spread_observation':max(starts)-min(starts),'quality_failures':reasons,'unqualified_diagnostic_slopes':slopes,'formal_acceptance':False})
  closure(plan,a.out,'after');require(digest(a.plan)==plan_sha,'Plan changed during execution')
  require(digest(__file__)==runner_sha,'Diagnostic runner changed during execution')
  closure_after_ok=True;complete=True
 except BaseException as failure:
  execution_error=type(failure).__name__+': '+str(failure);raise
 finally:
  save(a.out/'summary.json',{'schema':'uwvm-long-gc-development-diagnostic-v1','independent_diagnostic_not_qualified_thermal_guard':True,
   'runner_sha256':runner_sha,'plan_sha256':plan_sha,'source_ids':{k:v['source_id'] for k,v in plan['products'].items()},
   'binary_sha256':{k:v['binary_sha256'] for k,v in plan['products'].items()},'rows':len(rows),'requested_pairs':a.pairs,'groups':groups,
   'complete':complete,'closure_before_ok':closure_before_ok,'closure_after_ok':closure_after_ok,'execution_error':execution_error,
   'completed_guest_count':sum(r['exit']==0 for r in rows),'semantic_pass_count':sum(r['semantic_receipt']['passed'] for r in rows),
   'quality_failure_rows':sum(bool(r['inconclusive_reasons']) for r in rows),'hard_failure_rows':sum(bool(r['hard_failures']) for r in rows),
   'diagnostic_disk_floor_bytes':1<<30,'sample_output_limit_bytes':4<<20,'temperature_rejection_enabled':False,
   'formal_acceptance':False,'formal_reasons':['Development diagnostic only: temperature is observation, frequency quality failures are retained, and no engine ranking or release acceptance is asserted.','Container-only observation does not establish host P0/SMT quiet; preserve external keeper host-load evidence.'],
   'not_full_release_qualification':True})
if __name__=='__main__':main()
