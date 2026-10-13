#!/usr/bin/env python3
"""Sample an ordinary native kernel or a real GDB inferior, never GDB itself.

The fixture must raise SIGSTOP before its monotonic kernel clock starts and
again after printing its result when UWVM_PROFILE_GATE=1. These two control
stops are outside the timed kernel. Collect serially in the actual 64 GiB
cgroup. The PMU window includes the resume, output and final signal stop.
"""
import argparse,hashlib,json,os,re,resource,signal,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['native','out']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--expected-cgroup',required=True)
 p.add_argument('--gdb',action='store_true')
 p.add_argument('--kind',choices=['hotspots','uarch-exploration'],default='hotspots')
 p.add_argument('--vtune',type=Path,default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
 a=p.parse_args();cg=Path('/sys/fs/cgroup'+a.expected_cgroup)
 expected='0::'+a.expected_cgroup
 assert Path('/proc/self/cgroup').read_text().strip()==expected
 assert (cg/'memory.max').read_text().strip()==str(64<<30)
 assert (cg/'memory.swap.max').read_text().strip()=='0'
 assert sorted(os.sched_getaffinity(0))==list(range(16,32))
 assert resource.getrlimit(resource.RLIMIT_NOFILE)[0]>=65536
 a.out.mkdir(parents=True,exist_ok=False)
 os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='',UWVM_PROFILE_GATE='1')
 argv=['/usr/bin/gdb','-q','-nx','--args',str(a.native.resolve())] if a.gdb else [str(a.native.resolve())]
 data={'passed':False,'mode':'gdb-inferior' if a.gdb else 'native-ordinary','kind':a.kind,'argv':argv,
  'native_sha256':hashlib.sha256(a.native.read_bytes()).hexdigest(),'cgroup':a.expected_cgroup,
  'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
  'scope':'native inferior only; begins at SIGSTOP before kernel clock; ends after result output and SIGSTOP. Does not sample GDB controller, startup or quit.'}
 c=Console(argv);vt=None;inferior_owner=None
 def ask(text):
  at=len(c.log);os.write(c.fd,text.encode()+b'\n')
  b=c.wait(lambda b:re.search(rb'\(gdb\) $',b),at,120)
  assert not re.search(rb'Undefined command|No symbol|Cannot access|Error:',b),b[-2000:]
  return b
 def stopped(pid,timeout=30):
  until=time.monotonic()+timeout
  while time.monotonic()<until:
   state=Path(f'/proc/{pid}/status').read_text()
   if re.search(r'^State:\s+[Tt]',state,re.M):return
   time.sleep(.02)
  raise AssertionError('fixture did not stop outside kernel')
 try:
  if a.gdb:
   c.wait(lambda b:re.search(rb'\(gdb\) $',b),0,60)
   for cmd in ['set pagination off','set confirm off','set print thread-events off','handle SIGSTOP stop print nopass']:ask(cmd)
   reply=ask('run');assert b'SIGSTOP' in reply,reply[-2000:]
   reply=ask('info inferiors');pid=int(re.search(rb'\*\s+1\s+process\s+(\d+)',reply)[1])
  else:
   pid=c.pid;stopped(pid)
  inferior_owner=os.pidfd_open(pid)
  assert Path(f'/proc/{pid}/cgroup').read_text().strip()==expected
  data.update(target_pid=pid,controller_pid=c.pid,actual_target_affinity=sorted(os.sched_getaffinity(pid)))
  assert data['actual_target_affinity']==list(range(16,32))
  collect=[str(a.vtune),'-collect',a.kind,'-result-dir',str(a.out/'result'),'-target-pid',str(pid)]
  collect+=['-knob','sampling-mode=hw','-knob','enable-stack-collection=true'] if a.kind=='hotspots' else ['-knob','pmu-collection-mode=summary']
  data['collect_argv']=collect
  with (a.out/'collect.log').open('w') as log:
   vt=subprocess.Popen(collect,stdout=log,stderr=subprocess.STDOUT)
   until=time.monotonic()+60
   while time.monotonic()<until and vt.poll() is None:
    s=(a.out/'collect.log').read_text(errors='replace')
    if 'Collection started' in s or 'Collection resumed' in s:break
    time.sleep(.1)
   else:raise AssertionError('native hardware collection did not start: '+s[-2000:])
   if a.gdb:
    reply=ask('c');assert b'SIGSTOP' in reply,reply[-2000:]
   else:
    os.kill(pid,signal.SIGCONT)
    reply=c.wait(lambda b:b'Checksum: ' in b and re.search(rb'Checksum: \d+\r?\n',b),0,120)
    stopped(pid)
   ns=re.findall(rb'Kernel ns: (\d+)',reply);cs=re.findall(rb'Checksum: (\d+)',reply)
   assert len(ns)==len(cs)==1,reply[-2000:]
   data.update(kernel_ms=int(ns[0])/1e6,checksum=cs[0].decode())
   assert vt.poll() is None,'collector exited before post-kernel control stop'
   stop=[str(a.vtune),'-command','stop','-r',str(a.out/'result')]
   q=subprocess.run(stop,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   (a.out/'stop.log').write_bytes(q.stdout);data.update(stop_argv=stop,stop_exit=q.returncode)
   assert q.returncode==0,q.stdout[-2000:]
   data['collect_exit']=vt.wait(timeout=180);assert data['collect_exit']==0
  if a.gdb:
   assert b'exited normally' in ask('c');c.finish()
  else:
   os.kill(pid,signal.SIGCONT);got,status=os.waitpid(pid,0);c.reaped=True
   assert got==pid and os.waitstatus_to_exitcode(status)==0
  for report in ['summary','hotspots']:
   qargs=[str(a.vtune),'-report',report,'-r',str(a.out/'result'),'-format','csv']
   if report=='hotspots':qargs+=['-group-by','function','-limit','40']
   q=subprocess.run(qargs,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   (a.out/(report+'.csv')).write_bytes(q.stdout);assert q.returncode==0,q.stdout[-2000:]
  data['passed']=True
 finally:
  if vt is not None and vt.poll() is None:vt.terminate();vt.wait(timeout=15)
  (a.out/'transcript.bin').write_bytes(c.log)
  if inferior_owner is not None:
   try:signal.pidfd_send_signal(inferior_owner,signal.SIGKILL)
   except ProcessLookupError:pass
   finally:os.close(inferior_owner)
  c.close()
  (a.out/'receipt.json').write_text(json.dumps(data,indent=2))
 print(json.dumps({k:data[k] for k in ['passed','mode','kind','kernel_ms','checksum']}),flush=True)

if __name__=='__main__':main()
