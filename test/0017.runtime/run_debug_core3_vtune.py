#!/usr/bin/env python3
"""VTune hardware-only Core 3 sampling, inside the actual SSH Linux cgroup.

Use the same generated .wasm and its independent checksum for both binaries.
The target is pinned to one selected CPU; all target TIDs are checked.
Collection starts at the pre-clock _start stop. --post-kernel-gate ends it
at the fixture's post-result breakpoint, matching the Wasmtime sampler.
JIT startup and post-quit LLVM teardown are excluded. Timings from a profiled
run must never be substituted for the unprofiled paired throughput samples.
"""
import argparse,csv,hashlib,io,json,os,re,resource,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console
from run_debug_breakpoint_benchmark import exports
from debug_sampling_safety import SamplingSession, durable_write, sampling_options, sync_artifacts

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['uwvm','wasm','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--expected-cgroup',required=True)
 p.add_argument('--cpu',type=int,required=True)
 p.add_argument('--hybrid-pmu-companion',type=int,help='One opposite core type for VTune hybrid perf event validation; target stays pinned to --cpu.')
 p.add_argument('--expected-checksum',type=int,required=True)
 p.add_argument('--feature',action='append',default=[])
 p.add_argument('--ros',action='store_true')
 p.add_argument('--post-kernel-gate',action='store_true',help='Require a profile_end export and stop sampling before either runtime retires the guest.')
 p.add_argument('--vtune',type=Path,default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
 p.add_argument('--kind',choices=['hotspots','uarch-exploration'],default='hotspots')
 p.add_argument('--driverless-hotspots',action='store_true',help='Explicit perf hardware sampling without Intel SEP or stack collection; not a full microarchitecture preset.')
 p.add_argument('--drop-target-capabilities',action='store_true',help='Collector may have scoped CAP_PERFMON; the actual benchmark target must have none.')
 p.add_argument('--sampling-interval-ms',type=int,choices=[10,50,100],default=10,help='Bounded event sampling interval; explicit custom clock/retired SAV scales with it.')
 p.add_argument('--pmu-mode',choices=['detailed','summary'],default='summary',help='Microarchitecture defaults to summary counting; detailed explicitly selects hardware sampling.')
 p.add_argument('--timeout',type=int,default=240)
 p.add_argument('--min-kernel-ms',type=int,default=1000,help='Reject short kernels that cannot support useful hardware sampling.')
 p.add_argument('--expected-codegen',choices=['none','less'],default='less',help='Exact expected policy for this immutable sampled binary.')
 p.add_argument('--expected-pipeline',choices=['none','debug-local-ssa'],default='debug-local-ssa')
 a=p.parse_args();assert 1<=a.timeout<=600 and 100<=a.min_kernel_ms<=10000
 assert not a.driverless_hotspots or a.kind=='hotspots'
 assert os.sched_getaffinity(0)=={a.cpu}
 assert set(a.feature)<=set(['memory64','table64','multi-memory','extended-const','tail-call','exceptions','function-references','reference-types','gc','simd','relaxed-simd'])
 sampled_cpus=[a.cpu]
 if a.hybrid_pmu_companion is not None:
  def cpu_set(text):
   result=set()
   for part in text.strip().split(','):
    bounds=[int(v) for v in part.split('-')];result.update(range(bounds[0],bounds[-1]+1))
   return result
  core=cpu_set(Path('/sys/devices/cpu_core/cpus').read_text());atom=cpu_set(Path('/sys/devices/cpu_atom/cpus').read_text())
  assert (a.cpu in core and a.hybrid_pmu_companion in atom) or (a.cpu in atom and a.hybrid_pmu_companion in core)
  allowed=cpu_set(Path('/sys/fs/cgroup'+a.expected_cgroup+'/cpuset.cpus.effective').read_text())
  sampled_cpus=sorted([a.cpu,a.hybrid_pmu_companion]);assert set(sampled_cpus)<=allowed
 cg=Path('/sys/fs/cgroup'+a.expected_cgroup)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup
 assert (cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0'
 assert resource.getrlimit(resource.RLIMIT_NOFILE)[0]>=65536
 a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
 data={'passed':False,'kind':a.kind,'pmu_mode':a.pmu_mode if a.kind=='uarch-exploration' else None,'cgroup':a.expected_cgroup,'target_affinity':sorted(os.sched_getaffinity(0)),
       'sampler_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
       'sampling_helper_sha256':hashlib.sha256(Path(__file__).with_name('debug_sampling_safety.py').read_bytes()).hexdigest(),
       'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(a.wasm.read_bytes()).hexdigest(),
       'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
       'features':a.feature,'expected_checksum':a.expected_checksum,
       'post_kernel_gate':a.post_kernel_gate,
       'scope':'guest from exact pre-clock _start stop; excludes JIT/startup and post-quit LLVM teardown; includes output and guest exit'}
 if a.post_kernel_gate:data['scope']='exact pre-clock _start stop through post-result profile_end breakpoint; includes output, excludes compilation/startup, guest exit and runtime teardown'
 flags=['-Raot'] if a.ros else ['-Rcm','full','-Rcc','jit']
 target_argv=[str(a.uwvm.resolve()),'-Rdbg',*flags,'-Rct','0','-Rllvm-cache-path','disable','-Rclog','err',*['-WFE-'+f for f in a.feature],'--run',str(a.wasm.resolve())]
 data['target_argv']=target_argv
 data['collector_capabilities']={name:int(value,16) for name,value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',Path('/proc/self/status').read_text(),re.M)}
 c=vt=safety=None
 durable_write(a.out/'receipt.json',json.dumps(data,indent=2))
 try:
  safety=SamplingSession(a.out,sampled_cpus=sampled_cpus,sampling_interval_ms=a.sampling_interval_ms)
  launch=['/usr/bin/setpriv','--inh-caps=-all','--ambient-caps=-all',*target_argv] if a.drop_target_capabilities else target_argv
  data['launch_argv']=launch
  c=Console(launch)
  c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);entry=c.command('startw')
  symbols=exports(a.wasm);start=symbols['_start']
  assert f'function={start} byte-offset=0 '.encode() in entry,entry[-2000:]
  data.update(collect_function=start,collect_offset=0)
  if a.post_kernel_gate:
   end=symbols['profile_end'];bp=c.command(f'bw 0 {end} 0')
   assert not re.search(rb'error:|Undefined command',bp),bp[-2000:]
   assert re.search(rb'(?:Breakpoint|breakpoint) \d+',bp),bp[-2000:]
   data.update(end_function=end,end_offset=0)
  data['policies']=[[v.decode() for v in row] for row in re.findall(
      rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)',bytes(c.log))]
  assert data['policies'] and all(row==['session:debug',a.expected_pipeline,a.expected_codegen] for row in data['policies']),data['policies']
  assert Path(f'/proc/{c.pid}/cgroup').read_text().strip()=='0::'+a.expected_cgroup
  status=Path(f'/proc/{c.pid}/status').read_text()
  data['target_capabilities']={name:int(value,16) for name,value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',status,re.M)}
  assert data['target_capabilities'] and all(value==0 for value in data['target_capabilities'].values()),data['target_capabilities']
  data['actual_target_affinity']=sorted(os.sched_getaffinity(c.pid))
  assert data['actual_target_affinity']==[a.cpu],data['actual_target_affinity']
  data['target_task_affinities']={p.name:sorted(os.sched_getaffinity(int(p.name))) for p in Path(f'/proc/{c.pid}/task').iterdir()}
  assert all(v==[a.cpu] for v in data['target_task_affinities'].values())
  argv=[str(a.vtune),*(['-collect-with','runsa'] if a.driverless_hotspots else ['-collect',a.kind]),'-result-dir',str(a.out/'result'),'-target-pid',str(c.pid),'-target-duration-type','veryshort']
  options=sampling_options(a.kind,a.pmu_mode,a.cpu,a.sampling_interval_ms)
  options[5]=','.join(str(cpu) for cpu in sampled_cpus)
  data['sampled_cpus']=sampled_cpus
  data['sampling_interval_ms']=a.sampling_interval_ms
  # Intel sampling-interval multiplies the event's base SAV once.
  data['custom_sampling_period_base']=30000000 if a.driverless_hotspots else None
  data['custom_sampling_period']=30000000*a.sampling_interval_ms if a.driverless_hotspots else None
  if a.driverless_hotspots:
   options=options[:6]+['-knob','enable-stack-collection=false','-knob','enable-driverless-collection=true',
       '-knob',f'event-config=CPU_CLK_UNHALTED.REF_TSC:sa={30000000},INST_RETIRED.ANY:sa={30000000}']
  argv+=options
  data['hardware_backend']='perf driverless custom clock/retired sampling' if a.driverless_hotspots else 'VTune preset'
  data.update(argv=argv,target_pid=c.pid)
  with (a.out/'collect.log').open('w') as log:
   vt=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT);deadline=time.monotonic()+60
   while time.monotonic()<deadline and vt.poll() is None:
    safety.check()
    text=(a.out/'collect.log').read_text(errors='replace')
    if 'Collection started' in text or 'Collection resumed' in text:break
    time.sleep(.1)
   else:raise RuntimeError('hardware collection did not start: '+(a.out/'collect.log').read_text()[-2500:])
   at=len(c.log);os.write(c.fd,b'c\n')
   marker=b'Kernel ns: '
   reply=c.wait(safety.predicate(lambda b:marker in b and re.search(rb'(?:^|\n)\(uwvm-debug\) $',b)),at,min(a.timeout,30))
   times=re.findall(rb'Kernel ns: (\d+)',reply);sums=re.findall(rb'Checksum: ([^\r\n]+)',reply)
   assert len(times)==len(sums)==1,reply[-2000:]
   if a.post_kernel_gate:
    assert b'stopped: breakpoint' in reply and f'function={end} byte-offset=0 '.encode() in reply,reply[-2000:]
    assert b'Program exited' not in reply,reply[-2000:]
   else:assert b'Program exited' in reply,reply[-2000:]
   assert int(sums[0])==a.expected_checksum,(sums,a.expected_checksum)
   data.update(kernel_ms=int(times[0])/1e6,checksum=sums[0].decode())
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
   if a.post_kernel_gate:
    disabled=c.command('disable');assert b'error:' not in disabled,disabled[-2000:]
    exited=c.command('c');assert b'Program exited' in exited,exited[-2000:]
   c.finish()
  assert data['collect_exit']==0,(a.out/'collect.log').read_text()[-2500:]
  for report in ['summary','hotspots']:
   argv=[str(a.vtune),'-report',report,'-r',str(a.out/'result'),'-format','csv']
   if report=='hotspots':argv+=['-group-by','function','-limit','40']
   q=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   (a.out/(report+'.csv')).write_bytes(q.stdout);assert q.returncode==0,q.stdout[-2500:]
  data['artifacts']={name:hashlib.sha256((a.out/name).read_bytes()).hexdigest() for name in ['collect.log','stop.log','summary.csv','hotspots.csv']}
  data['metrics']=[{'level':int(cells[0]),'metric':cells[1],'value':cells[2]}
    for cells in csv.reader(io.StringIO((a.out/'summary.csv').read_text()),delimiter='\t')
    if len(cells)>=3 and cells[0].isdigit()]
  if a.kind=='hotspots':
   # Preserve the raw merged report, including progress. VTune may append
   # its TSV header to a carriage-return progress message on the same line.
   report=(a.out/'hotspots.csv').read_text();header=re.search(r'Function\t[^\r\n]+',report)
   assert header,'Hardware function report header is required'
   records=csv.reader(io.StringIO(report[header.start():]),delimiter='\t');fields=next(records)
   clock_key='Hardware Event Count:CPU_CLK_UNHALTED.REF_TSC' if a.driverless_hotspots else 'CPU Time'
   assert 'Function' in fields and clock_key in fields,fields
   table=[dict(zip(fields,cells)) for cells in records if len(cells)==len(fields)]
   data['sampled_function_clock_metric']=clock_key
   data['sampled_function_rows']=[row for row in table if re.fullmatch(r'[0-9.,]+',row[clock_key])
     and float(row[clock_key].replace(',',''))>0]
   assert data['sampled_function_rows'],'Nonzero actual hardware function samples are required'
   if a.driverless_hotspots:
    summary=(a.out/'summary.csv').read_text();event_header=re.search(r'Hardware Event Type\tCore Type\t[^\r\n]+',summary)
    assert event_header,'Actual event/sample count table required'
    events=csv.reader(io.StringIO(summary[event_header.start():]),delimiter='\t');event_fields=next(events)
    data['hardware_event_rows']=[dict(zip(event_fields,cells)) for cells in events if len(cells)==len(event_fields)]
    for event in ['INST_RETIRED.ANY','CPU_CLK_UNHALTED.REF_TSC']:
     assert sum(int(row['Hardware Event Sample Count:Self']) for row in data['hardware_event_rows'] if row['Hardware Event Type']==event)>=10,'At least 10 actual samples per event required'
  data['minimum_kernel_ms']=a.min_kernel_ms
  assert data['kernel_ms']>=a.min_kernel_ms,'Increase --kernel-repeats: this capture is too short to qualify'
  if a.kind=='uarch-exploration':
   retired=[row['value'] for row in data['metrics'] if row['metric'].strip()=='Instructions Retired']
   assert len(retired)==1 and float(retired[0].replace(',',''))>0,'Hardware retired-instruction counter is required'
  data['immutable_after']={'binary':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm':hashlib.sha256(a.wasm.read_bytes()).hexdigest()}
  assert data['immutable_after']=={'binary':data['binary_sha256'],'wasm':data['wasm_sha256']}
  safety.check(force=True)
  data['passed']=True
 except Exception as error:
  data['error']=repr(error);raise
 finally:
  if vt is not None and vt.poll() is None:
   vt.terminate()
   try:vt.wait(timeout=15)
   except subprocess.TimeoutExpired:vt.kill();vt.wait(timeout=5)
  if c is not None:
   durable_write(a.out/'transcript.bin',c.log);c.close()
  if safety is not None:
   data['kernel_alerts']=safety.events;safety.close()
  sync_artifacts(a.out)
  durable_write(a.out/'receipt.json',json.dumps(data,indent=2))
 print(json.dumps({k:data[k] for k in ['passed','kind','kernel_ms','checksum','binary_sha256']}),flush=True)

if __name__=='__main__':main()
