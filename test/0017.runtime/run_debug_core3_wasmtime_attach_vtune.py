#!/usr/bin/env python3
"""Attach VTune at a verified Wasm/LLDB pre-clock breakpoint, stop before guest exit."""
import argparse,csv,hashlib,io,json,os,re,resource,signal,socket,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console
from run_debug_core3_wasmtime_vtune import digest,saved_run,check_target
from debug_sampling_safety import SamplingSession,durable_write,sampling_options,sync_artifacts

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['source-root','wasmtime','lldb','fixtures','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--case',required=True);p.add_argument('--expected-cgroup',required=True);p.add_argument('--cpu',type=int,required=True)
 p.add_argument('--hybrid-pmu-companion',type=int,required=True);p.add_argument('--kind',choices=['hotspots','uarch-exploration'],required=True)
 p.add_argument('--pmu-mode',choices=['summary'],default='summary');p.add_argument('--driverless-hotspots',action='store_true');p.add_argument('--drop-target-capabilities',action='store_true')
 p.add_argument('--vtune',type=Path,default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'));p.add_argument('--timeout',type=int,default=240);p.add_argument('--min-kernel-ms',type=int,default=1000)
 a=p.parse_args();os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='');assert not a.driverless_hotspots or a.kind=='hotspots';assert 1<=a.timeout<=600 and 100<=a.min_kernel_ms<=10000;assert a.drop_target_capabilities and (a.kind!='hotspots' or a.driverless_hotspots)
 subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup and os.sched_getaffinity(0)=={a.cpu}
 cg=Path('/sys/fs/cgroup'+a.expected_cgroup);assert (cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0'
 def verified_command(console,text,timeout,safety,marker=None):
  offset=len(console.log);os.write(console.fd,text.encode()+b'\n')
  reply=console.wait(safety.predicate(lambda data:(re.search(rb'\(lldb\) $',data) if marker is None else marker in data and b'(lldb) ' in data)),offset,min(timeout,30))
  assert not re.search(rb'(?:^|\n)error:|invalid command',reply),reply[-3000:]
  return reply
 def cpu_set(text):
  result=set()
  for part in text.strip().split(','):
   bounds=[int(v) for v in part.split('-')];result.update(range(bounds[0],bounds[-1]+1))
  return result
 core=cpu_set(Path('/sys/devices/cpu_core/cpus').read_text());atom=cpu_set(Path('/sys/devices/cpu_atom/cpus').read_text());assert (a.cpu in core and a.hybrid_pmu_companion in atom) or (a.cpu in atom and a.hybrid_pmu_companion in core)
 sampled=sorted([a.cpu,a.hybrid_pmu_companion]);assert set(sampled)<=cpu_set((cg/'cpuset.cpus.effective').read_text());assert resource.getrlimit(resource.RLIMIT_NOFILE)[0]>=65536
 row=next(r for r in json.loads((a.fixtures/'manifest.json').read_text()) if r['name']==a.case);assert row['validation']=='passed' and row['profile_gates']=={'before':'start','after':'profile_end'}
 wasm=(a.fixtures/(a.case+'.wasm')).resolve();assert digest(wasm)==row['wasm_sha256'];a.out.mkdir(parents=True,exist_ok=False)
 paths=[a.wasmtime.resolve(),a.lldb.resolve(),a.vtune.resolve(),wasm,a.fixtures/'manifest.json',Path(__file__).resolve(),a.source_root/'test/0017.runtime/run_debug_core3_wasmtime_vtune.py',a.source_root/'test/0017.runtime/debug_sampling_safety.py']
 before={str(f):digest(f) for f in paths};data={'passed':False,'hardware_capture_passed':False,'jit_symbol_attribution':False,'kind':a.kind,'pmu_mode':a.pmu_mode if a.kind=='uarch-exploration' else None,'case':row,'cgroup':a.expected_cgroup,'sampled_cpus':sampled,'immutable_before':before,'scope':'VTune attaches after verified pre-clock start breakpoint; stops at actual post-result profile_end before guest exit; excludes compilation/startup/exit/teardown','notes':['Counter comparison uses an actual Wasm/LLDB plugin session. Native GNU GDB is a separate capability baseline.','Hardware counts qualify independently from attribution of private StoreCode JIT function names.','Profiled kernel time is not unprofiled throughput.'],'boot':Path('/proc/sys/kernel/random/boot_id').read_text().strip()}
 target=console=collector=owner=safety=None
 durable_write(a.out/'receipt.json',json.dumps(data,indent=2))
 try:
  safety=SamplingSession(a.out,sampled_cpus=sampled)
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  argv=[str(a.wasmtime.resolve()),'run','-C','cache=n','-O','opt-level=2','-W','memory64=y','-g',f'127.0.0.1:{port}',str(wasm)];launch=['/usr/bin/setpriv','--inh-caps=-all','--ambient-caps=-all',*argv];data.update(target_argv=argv,launch_argv=launch)
  log=a.out/'server.bin'
  with log.open('xb') as stream:target=subprocess.Popen(launch,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT)
  owner=os.pidfd_open(target.pid);deadline=time.monotonic()+a.timeout
  while b'Debugger listening' not in log.read_bytes():
   safety.check();assert target.poll() is None,log.read_bytes()[-3000:];assert time.monotonic()<deadline;time.sleep(.05)
  ticks=Path(f'/proc/{target.pid}/stat').read_text().rsplit(')',1)[1].split()[19];data.update(target_pid=target.pid,target_task_affinities=check_target(target.pid,a,ticks))
  data['target_capabilities']={n:int(v,16) for n,v in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',Path(f'/proc/{target.pid}/status').read_text(),re.M)}
  packets=a.out/'packets.log';console=Console([str(a.lldb.resolve()),'--no-lldbinit']);console.wait(safety.predicate(lambda b:re.search(rb'\(lldb\) $',b)),0,a.timeout)
  for command in ['settings set auto-confirm true','settings set stop-disassembly-display never','log enable gdb-remote packets -f '+json.dumps(str(packets))]:verified_command(console,command,a.timeout,safety)
  connected=verified_command(console,f'process connect --plugin wasm connect://127.0.0.1:{port}',a.timeout,safety,b'stop reason = signal SIGTRAP');assert b'Process 1 stopped' in connected,connected[-3000:]
  reply=verified_command(console,'breakpoint set -n start',a.timeout,safety,b'Breakpoint 1:');address=re.search(rb'Breakpoint 1: .*address = (0x[0-9a-fA-F]+)',reply);assert address,reply
  reply=verified_command(console,'breakpoint set -n profile_end',a.timeout,safety,b'Breakpoint 2:');assert re.search(rb'Breakpoint 2: .*where = .*`profile_end',reply),reply
  reply=verified_command(console,'continue',a.timeout,safety,b'stop reason = breakpoint');assert b'stop reason = breakpoint 1.1' in reply,reply
  reply=verified_command(console,'register read pc',a.timeout,safety,b'pc =');pc=re.search(rb'pc = (0x[0-9a-fA-F]+)',reply);assert pc and int(pc[1],16)==int(address[1],16),reply
  data['pre_clock_wasm_pc']=int(pc[1],16);verified_command(console,'thread backtrace',a.timeout,safety);assert b'qWasmCallStack' in packets.read_bytes();verified_command(console,'breakpoint delete --force 1',a.timeout,safety)
  options=sampling_options(a.kind,a.pmu_mode,a.cpu);options[5]=','.join(map(str,sampled))
  if a.driverless_hotspots:options=options[:6]+['-knob','enable-stack-collection=false','-knob','enable-driverless-collection=true','-knob','event-config=CPU_CLK_UNHALTED.REF_TSC:sa=30000000,INST_RETIRED.ANY:sa=30000000']
  collect=[str(a.vtune),*(['-collect-with','runsa'] if a.driverless_hotspots else ['-collect',a.kind]),'-result-dir',str(a.out/'result'),'-target-pid',str(target.pid),'-target-duration-type','veryshort',*options];data['collect_argv']=collect
  with (a.out/'collect.log').open('xb') as stream:collector=subprocess.Popen(collect,stdout=stream,stderr=subprocess.STDOUT)
  deadline=time.monotonic()+60
  while b'Collection started' not in (a.out/'collect.log').read_bytes():
   safety.check();assert collector.poll() is None,(a.out/'collect.log').read_bytes()[-3000:];assert time.monotonic()<deadline;time.sleep(.1)
  reply=verified_command(console,'continue',a.timeout,safety,b'`profile_end');assert b'stop reason = breakpoint 2.1' in reply and b'`profile_end' in reply,reply
  output=log.read_bytes();times,sums=re.findall(rb'Kernel ns: (\d+)',output),re.findall(rb'Checksum: (\d+)',output);assert len(times)==len(sums)==1 and int(sums[0])==row['expected_checksum'],output
  data.update(kernel_ms=int(times[0])/1e6,checksum=int(sums[0]),post_result_task_affinities=check_target(target.pid,a,ticks));stop=[str(a.vtune),'-command','stop','-r',str(a.out/'result')];saved_run(stop,a.out/'stop.log');data['stop_argv']=stop;data['collect_exit']=collector.wait(timeout=180);assert data['collect_exit']==0
  assert target.poll() is None,'Attach collection must retain its real stopped target'
  verified_command(console,'breakpoint delete --force 2',a.timeout,safety);reply=verified_command(console,'continue',a.timeout,safety,b'exited with status = 0');assert b'exited with status = 0' in reply,reply;console.finish();assert target.wait(timeout=30)==0
  for report in ['summary','hotspots']:
   command=[str(a.vtune),'-report',report,'-r',str(a.out/'result'),'-format','csv']+(['-group-by','function','-limit','100'] if report=='hotspots' else []);saved_run(command,a.out/(report+'.csv'))
  summary=(a.out/'summary.csv').read_text();data['metrics']=[{'level':int(c[0]),'metric':c[1],'value':c[2]} for c in csv.reader(io.StringIO(summary),delimiter='\t') if len(c)>=3 and c[0].isdigit()]
  if a.driverless_hotspots:
   h=re.search(r'Hardware Event Type\tCore Type\t[^\r\n]+',summary);assert h;events=csv.reader(io.StringIO(summary[h.start():]),delimiter='\t');fields=next(events);data['hardware_event_rows']=[dict(zip(fields,c)) for c in events if len(c)==len(fields)]
   for event in ['INST_RETIRED.ANY','CPU_CLK_UNHALTED.REF_TSC']:assert sum(int(v['Hardware Event Sample Count:Self']) for v in data['hardware_event_rows'] if v['Hardware Event Type']==event)>=10
  else:
   retired=[v['value'] for v in data['metrics'] if v['metric'].strip()=='Instructions Retired'];assert len(retired)==1 and float(retired[0].replace(',',''))>0
  report=(a.out/'hotspots.csv').read_text();h=re.search(r'Function\t[^\r\n]+',report);table=[]
  if h:
   records=csv.reader(io.StringIO(report[h.start():]),delimiter='\t');fields=next(records);table=[dict(zip(fields,c)) for c in records if len(c)==len(fields)]
  data['sampled_kernel_rows']=[v for v in table if re.search(r'(?:^|\W)kernel(?:\W|$)',v.get('Function','')) and any(float(value.replace(',',''))>0 for key,value in v.items() if key and ('CPU Time' in key or 'Hardware Event Count:CPU_CLK_UNHALTED.REF_TSC'==key) and re.fullmatch(r'[0-9.,]+',value))];data['jit_symbol_attribution']=bool(data['sampled_kernel_rows'])
  assert data['kernel_ms']>=a.min_kernel_ms
  data['immutable_after']={str(f):digest(f) for f in paths};assert data['immutable_after']==before;safety.check(force=True);data.update(passed=True,hardware_capture_passed=True)
  if not data['jit_symbol_attribution']:data['notes'].append('Actual private StoreCode kernel has no named sampled row; do not claim named Wasm JIT hotspot attribution.')
 except BaseException as error:data['error']=repr(error);raise
 finally:
  if console is not None:durable_write(a.out/'transcript.bin',console.log);console.close()
  if collector is not None and collector.poll() is None:
   collector.terminate()
   try:collector.wait(timeout=15)
   except subprocess.TimeoutExpired:collector.kill();collector.wait(timeout=5)
  if target is not None and target.poll() is None:
   if owner is not None:signal.pidfd_send_signal(owner,signal.SIGTERM)
   try:target.wait(timeout=5)
   except subprocess.TimeoutExpired:signal.pidfd_send_signal(owner,signal.SIGKILL);target.wait(timeout=5)
  if owner is not None:os.close(owner)
  if safety is not None:data['kernel_alerts']=safety.events;safety.close()
  sync_artifacts(a.out);durable_write(a.out/'receipt.json',json.dumps(data,indent=2))
 print(json.dumps({k:data[k] for k in ['passed','hardware_capture_passed','jit_symbol_attribution','kernel_ms']}),flush=True)

if __name__=='__main__':main()
