#!/usr/bin/env python3
"""Attach VTune hardware CLI sampling after a genuine Wasm entry stop.

Intel CLI: collect hotspots (sampling-mode=hw, enable-stack-collection=true)
or uarch-exploration. Reports and exact command/limits/CPU/cgroup are retained.
The fixture must run longer than --seconds; Ctrl+C then quit retires our child.
Do not run concurrent compiles or other benchmarks. Requires 64GiB, swap0 cgroup.
"""
import argparse,hashlib,json,os,re,resource,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console
p=argparse.ArgumentParser(description=__doc__)
for n in ['uwvm','wasm','out']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--vtune',type=Path,default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
p.add_argument('--kind',choices=['hotspots','uarch-exploration'],required=True)
p.add_argument('--seconds',type=int,default=15);p.add_argument('--expected-cgroup',required=True)
a=p.parse_args();assert 1<=a.seconds<=60
cg=Path('/sys/fs/cgroup'+a.expected_cgroup)
assert Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup
assert (cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0'
assert resource.getrlimit(resource.RLIMIT_NOFILE)[0]>=65536
a.out.mkdir(parents=True,exist_ok=False);os.environ.update(NO_COLOR='1',TERM='dumb')
data={'passed':False,'kind':a.kind,'cgroup':a.expected_cgroup,'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
      'target_affinity':sorted(os.sched_getaffinity(0)),'nofile':resource.getrlimit(resource.RLIMIT_NOFILE),
      'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(a.wasm.read_bytes()).hexdigest()}
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]);vt=None
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);c.command('startw')
 assert Path(f'/proc/{c.pid}/cgroup').read_text().strip()=='0::'+a.expected_cgroup
 argv=[str(a.vtune),'-collect',a.kind,'-duration',str(a.seconds),'-result-dir',str(a.out/'result'),'-target-pid',str(c.pid)]
 if a.kind=='hotspots':argv+=['-knob','sampling-mode=hw','-knob','enable-stack-collection=true']
 else:argv+=['-knob','pmu-collection-mode=summary']
 data.update(argv=argv,target_pid=c.pid)
 with (a.out/'collect.log').open('w') as f:
  vt=subprocess.Popen(argv,stdout=f,stderr=subprocess.STDOUT);deadline=time.monotonic()+60
  while time.monotonic()<deadline and vt.poll() is None:
   log=(a.out/'collect.log').read_text(errors='replace')
   if 'Collection started' in log or 'Collection resumed' in log:break
   time.sleep(.1)
  else:raise RuntimeError('VTune did not start: '+(a.out/'collect.log').read_text()[-3000:])
  at=len(c.log);os.write(c.fd,b'c\n');data['collection_exit']=vt.wait(timeout=180)
 assert data['collection_exit']==0,(a.out/'collect.log').read_text()[-3000:]
 # The profiling fixture should still execute, proving startup was excluded.
 c.wait(lambda b:len(b)>0,at,5)
 assert b'Program exited' not in bytes(c.log[at:]),'increase fixture repetitions for sampling window'
 at=len(c.log);os.write(c.fd,b'\x03');c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,30);c.finish()
 for report in ['summary','hotspots']:
  args=[str(a.vtune),'-report',report,'-r',str(a.out/'result'),'-format','csv']
  if report=='hotspots':args+=['-group-by','function','-limit','40']
  q=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120);(a.out/(report+'.csv')).write_bytes(q.stdout);assert q.returncode==0,q.stdout[-3000:]
 data['passed']=True
finally:
 if vt is not None and vt.poll() is None:vt.terminate();vt.wait(timeout=15)
 (a.out/'transcript.bin').write_bytes(c.log);c.close();(a.out/'receipt.json').write_text(json.dumps(data,indent=2))
