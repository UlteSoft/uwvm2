#!/usr/bin/env python3
"""VTune hardware attach around the same complete FFT workload, excluding JIT startup.

Run serially through the existing 64 GiB/no-swap cgroup guard. The target
remains pinned to E cores. Both binaries must consume the exact same Wasm.
Collection is explicitly stopped after genuine guest exit, while the debugger
remains alive. LLVM teardown on quit is outside the hardware sampling window.
"""
import argparse,hashlib,json,os,re,resource,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['uwvm','wasm','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--expected-cgroup',required=True)
 p.add_argument('--vtune',type=Path,default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
 p.add_argument('--kind',choices=['hotspots','uarch-exploration'],default='hotspots')
 p.add_argument('--timeout',type=int,default=240)
 p.add_argument('--expected-codegen',choices=['none','less'],default='less',help='Exact expected policy for this immutable sampled binary.')
 p.add_argument('--expected-pipeline',choices=['none','debug-local-ssa'],default='debug-local-ssa')
 p.add_argument('--kernel-ns',action='store_true',help='Use integer Kernel ns / Checksum protocol from the multi-workload fixtures.')
 p.add_argument('--stop-at',help='Optional exact module-0 FUNCTION:OFFSET before the guest kernel clock starts, e.g. after fixture initialization.')
 a=p.parse_args();assert 1<=a.timeout<=600
 cg=Path('/sys/fs/cgroup'+a.expected_cgroup)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup
 assert (cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0'
 assert resource.getrlimit(resource.RLIMIT_NOFILE)[0]>=65536
 a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
 data={'passed':False,'kind':a.kind,'cgroup':a.expected_cgroup,'target_affinity':sorted(os.sched_getaffinity(0)),
       'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(a.wasm.read_bytes()).hexdigest(),
       'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
       'scope':'guest workload after startw, excluding JIT startup and debugger quit/post-quit LLVM teardown; includes guest initialization, output and exit'}
 c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rcm','full','-Rcc','jit','-Rct','0','-Rllvm-cache-path','disable','-Rclog','err','--run',str(a.wasm.resolve())]);vt=None
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);c.command('startw')
  if a.stop_at:
   function,offset=map(int,a.stop_at.split(':'));assert function>=0 and offset>=0
   bp=c.command(f'bw 0 {function} {offset}');assert re.search(rb'(?:Breakpoint|breakpoint) \d+',bp),bp[-2000:]
   stopped=c.command('c');assert b'stopped: breakpoint' in stopped and \
    f'function={function} byte-offset={offset} '.encode() in stopped,stopped[-2000:]
   disabled=c.command('disable');assert b'error:' not in disabled,disabled[-2000:]
   data.update(collect_function=function,collect_offset=offset,
    scope='guest after exact pre-clock stop; excludes fixture initialization/JIT startup and debugger quit/post-quit LLVM teardown; includes output and guest exit')
  data['policies']=[[v.decode() for v in row] for row in re.findall(
      rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)',bytes(c.log))]
  assert data['policies'] and all(row==['session:debug',a.expected_pipeline,a.expected_codegen] for row in data['policies']),data['policies']
  assert Path(f'/proc/{c.pid}/cgroup').read_text().strip()=='0::'+a.expected_cgroup
  data['actual_target_affinity']=sorted(os.sched_getaffinity(c.pid))
  assert data['actual_target_affinity']==list(range(16,32)),data['actual_target_affinity']
  argv=[str(a.vtune),'-collect',a.kind,'-result-dir',str(a.out/'result'),'-target-pid',str(c.pid)]
  argv+=['-knob','sampling-mode=hw','-knob','enable-stack-collection=true'] if a.kind=='hotspots' else ['-knob','pmu-collection-mode=summary']
  data.update(argv=argv,target_pid=c.pid)
  with (a.out/'collect.log').open('w') as log:
   vt=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT);deadline=time.monotonic()+60
   while time.monotonic()<deadline and vt.poll() is None:
    text=(a.out/'collect.log').read_text(errors='replace')
    if 'Collection started' in text or 'Collection resumed' in text:break
    time.sleep(.1)
   else:raise RuntimeError('hardware collection did not start: '+(a.out/'collect.log').read_text()[-2500:])
   at=len(c.log);os.write(c.fd,b'c\n')
   marker=b'Kernel ns: ' if a.kernel_ns else b'Time: '
   reply=c.wait(lambda b:marker in b and re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,a.timeout)
   times=re.findall(rb'Kernel ns: (\d+)' if a.kernel_ns else rb'Time: ([0-9.]+) ms',reply);sums=re.findall(rb'Checksum: ([^\r\n]+)',reply)
   assert len(times)==len(sums)==1 and b'Program exited' in reply,reply[-2000:]
   data.update(kernel_ms=float(times[0])/(1e6 if a.kernel_ns else 1),checksum=sums[0].decode())
   # Intel's documented -command stop targets this exact result directory.
   # Keep the real debugger alive until collection finalization completes,
   # then verify normal process retirement without sampling LLVM teardown.
   assert vt.poll() is None, 'collector stopped before genuine guest exit'
   stop_argv=[str(a.vtune),'-command','stop','-r',str(a.out/'result')]
   q=subprocess.run(stop_argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   (a.out/'stop.log').write_bytes(q.stdout)
   data.update(stop_argv=stop_argv,stop_exit=q.returncode)
   assert q.returncode==0,q.stdout[-2500:]
   data['collect_exit']=vt.wait(timeout=180)
   c.finish()
  assert data['collect_exit']==0,(a.out/'collect.log').read_text()[-2500:]
  for report in ['summary','hotspots']:
   argv=[str(a.vtune),'-report',report,'-r',str(a.out/'result'),'-format','csv']
   if report=='hotspots':argv+=['-group-by','function','-limit','40']
   q=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   (a.out/(report+'.csv')).write_bytes(q.stdout);assert q.returncode==0,q.stdout[-2500:]
  data['passed']=True
 finally:
  if vt is not None and vt.poll() is None:vt.terminate();vt.wait(timeout=15)
  (a.out/'transcript.bin').write_bytes(c.log);c.close();(a.out/'receipt.json').write_text(json.dumps(data,indent=2))
 print(json.dumps({k:data[k] for k in ['passed','kind','kernel_ms','checksum','binary_sha256']}),flush=True)

if __name__=='__main__':main()
