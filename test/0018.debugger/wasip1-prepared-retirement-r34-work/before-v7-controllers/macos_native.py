from pathlib import Path
import sys,json,hashlib,os,subprocess,time,signal,resource,re,struct,platform,ctypes,importlib.util
D=Path(__file__).parent
spec=importlib.util.spec_from_file_location('owned_mac',D/'macos_monitor.py');m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
repo=sys.argv[1];assert repo in ('uwvm2','uwvm2-ros')
O=D/'macos'/repo;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
q=json.loads((O/'qualified.json').read_text());assert q['passed'] and not q['actual_native_execution']
inputs={**{i['binary']:i['binary_sha256'] for i in q['fixtures'].values()},**{i['wasm']:i['wasm_sha256'] for i in q['fixtures'].values()}}
assert all(sha(O/n)==h for n,h in inputs.items())
assert sys.platform=='darwin' and platform.machine()=='arm64'
for n in (i['binary'] for i in q['fixtures'].values()):
 with (O/n).open('rb') as f:magic,cpu=struct.unpack('<II',f.read(8));assert magic==0xfeedfacf and cpu==0x100000c
 os.chmod(O/n,0o700)
ramdisk=json.loads((D/'active-ramdisk.json').read_text());assert ramdisk['passed'] and ramdisk['capacity_bytes']==1<<30 and ramdisk['mountpoint']==str(D/'macos')
assert O.stat().st_dev==(D/'macos').stat().st_dev and O.stat().st_dev!=D.stat().st_dev
assert os.statvfs(D).f_bavail*os.statvfs(D).f_frsize>384<<20
ram_bytes=ramdisk['capacity_bytes']
profile='(version 1)(allow default)(deny process-fork)'
native=m.MacNative();rows=[];limit=2<<30;reserve=(128<<20)+ram_bytes
# Independent negative control: the fixed inherited profile rejects fork.
probe_log=O/'nofork-probe.log';probe_status=None;probe_peak=0
with probe_log.open('wb') as f:
 probe=subprocess.Popen(['/usr/bin/sandbox-exec','-p',profile,sys.executable,'-c','import os; os.fork()'],stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
 identity=native.birth(native.bsd(probe.pid));start=time.monotonic()
 try:
  while probe_status is None:
   current=native.bsd(probe.pid);assert native.birth(current)==identity
   assert all(p==probe.pid for p in native.members(probe.pid))
   probe_peak=max(probe_peak,native.rss(probe.pid,current));assert probe_peak+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve<limit
   waited,raw,probe_usage=os.wait4(probe.pid,os.WNOHANG)
   if waited:probe_status=os.waitstatus_to_exitcode(raw);probe.returncode=probe_status;break
   assert time.monotonic()-start<10;time.sleep(.005)
 finally:
  if probe_status is None:
   current=native.bsd(probe.pid,missing=True)
   if current is not None:
    assert native.birth(current)==identity
    if current.status!=5:os.kill(probe.pid,signal.SIGKILL)
   os.wait4(probe.pid,0)
probe_upper=max(probe_peak,probe_usage.ru_maxrss)+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve
assert probe_status!=0 and b'Operation not permitted' in probe_log.read_bytes() and probe_upper<limit
(O/'nofork-probe.json').write_text(json.dumps(dict(passed=True,exit=probe_status,aggregate_peak_upper_bytes=probe_upper,limit_bytes=limit,pid_reaped=True,log_sha256=sha(probe_log)),indent=2)+'\n')

def stable_rss(pid,expected):
 for attempt in range(40):
  current=native.bsd(pid,missing=True)
  if current is None:return 0
  assert native.birth(current)==expected
  if current.status==5:return 0
  try:return native.rss(pid,current)
  except RuntimeError as error:
   if 'bytes=0, errno=3,' not in str(error):raise
   # Kernel task accounting can retire before BSD/ZOMB accounting. Only
   # genuine ESRCH is retried, with the same PID/birth/UID/PGID each time.
   if attempt==39:raise
   time.sleep(.001)
 raise RuntimeError('unreachable bounded RSS retry')

cases=[]
for fixture,item in q['fixtures'].items():
 for policy in ('instruction','unwind'):
  counted=fixture=='debug_wasip1_prepared_retirement'
  if counted:pattern=r'^CHECKPOINT_WASIP1_PREPARED_RETIREMENT policy='+policy+r' checks=([0-9]+) .*whole_restore=0 PASS$'
  else:pattern=r'^CHECKPOINT_'+('PREPARED_RETIREMENT' if fixture=='debug_checkpoint_prepared_retirement' else 'NATIVE_COHORT_RETIREMENT')+r' policy='+policy+r' actual_workers=([0-9]+) .*whole_restore=0 PASS$'
  cases.append(dict(label=fixture+'-'+policy,argv=[str(O/item['binary']),str(O/item['wasm']),policy],checks=None if counted else 2,counted=counted,pattern=pattern))
(O/'spec.json').write_text(json.dumps(dict(scope='R34 actual old native cohort retirement and candidate closed-ready/discard; original unstripped qualified arm64 binaries',cases=cases,limit_bytes=limit),indent=2)+'\n')
try:
 for case in cases:
  assert os.statvfs(D).f_bavail*os.statvfs(D).f_frsize>384<<20
  log=O/(case['label']+'.log');assert not log.exists();start=time.monotonic();status=None;identity=None;peak=0
  with log.open('wb') as output:
   proc=subprocess.Popen(['/usr/bin/sandbox-exec','-p',profile,*case['argv']],stdout=output,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_FSIZE,(1<<30,1<<30)))
   try:
    first=native.bsd(proc.pid);identity=native.birth(first);assert identity.uid==os.getuid() and identity.pgid==proc.pid
    while status is None:
     current=native.bsd(proc.pid);assert native.birth(current)==identity
     members=native.members(proc.pid);assert all(p==proc.pid for p in members),'unexpected process outside no-fork contract'
     total=stable_rss(proc.pid,identity);peak=max(peak,total)
     assert total+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve<limit
     waited,raw,usage=os.wait4(proc.pid,os.WNOHANG)
     if waited:status=os.waitstatus_to_exitcode(raw);proc.returncode=status;break
     assert time.monotonic()-start<300;assert log.stat().st_size<16<<20;time.sleep(.005)
   finally:
    if status is None:
     current=native.bsd(proc.pid,missing=True)
     if current is not None:
      assert identity is not None and native.birth(current)==identity
      if current.status!=5:os.kill(proc.pid,signal.SIGKILL)
     os.wait4(proc.pid,0)
  upper=max(usage.ru_maxrss,peak)+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve
  assert status==0,log.read_text(errors='replace')[-10000:];assert upper<limit
  text=log.read_text();found=re.search(case['pattern'],text,re.M);assert found and (int(found[1])>=1 if case['counted'] else int(found[1])==case['checks']);case['checks']=int(found[1])
  assert all(sha(O/n)==h for n,h in inputs.items())
  row=dict(label=case['label'],argv=case['argv'],checks=case['checks'] if case['counted'] else None,exit=status,passed=True,seconds=time.monotonic()-start,sampled_aggregate_peak_rss_bytes=peak,aggregate_peak_upper_bytes=upper,limit_bytes=limit,external_controller_reserve_bytes=128<<20,owned_ramdisk_capacity_bytes=ram_bytes,identities=[identity.__dict__],log=str(log),log_sha256=sha(log),pid_reaped=True)
  rows.append(row);(O/'native-execution.json').write_text(json.dumps(dict(passed=len(rows)==6,rows=rows),indent=2)+'\n');print(repo,case['label'],'PASS',case['checks'],'upper',upper,flush=True)
 receipt=dict(passed=True,os='macos',host_arch=platform.machine(),host_os=platform.platform(),execution_arch='arm64 native',qualified_sha256=sha(O/'qualified.json'),source_manifest_sha256=q['source_manifest_sha256'],input_sha256=inputs,execution_sha256=sha(O/'native-execution.json'),monitor_sha256=sha(D/'macos_monitor.py'),controller_sha256=sha(Path(__file__)),rows=rows,counted_assertions=sum(r['checks'] or 0 for r in rows),limit_bytes=limit,owned_ramdisk_bytes=ram_bytes,physical_disk_reserve_bytes=384<<20,maximum_physical_native_metadata_bytes=8<<20,nofork_probe_sha256=sha(O/'nofork-probe.log'),proof_scope='Actual old execution drain and physical OS join retain candidate; ready keeps admission closed; explicit discard reopens original; full world publication/replay remains unavailable')
 (O/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
finally:native.close()
