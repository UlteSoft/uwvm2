#!/usr/bin/env python3
"""Verify zero-hit breakpoint cost in Wasmtime's native and Wasm paths."""
import argparse,hashlib,json,os,re,socket,statistics,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console
from run_debug_wasmtime_comparison import ask,finish_batch,wasm_run


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['wasmtime','lldb','wasm','out']:p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--repeats',type=int,default=5);a=p.parse_args();a.out.mkdir(exist_ok=False)
 assert os.sched_getaffinity(0)=={16};os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='')
 result={'passed':False,'wasm_sha256':hashlib.sha256(a.wasm.read_bytes()).hexdigest(),'samples':[]}
 modes=['ordinary','debug-o2','gdb-o2','gdb-cold256','gdb-disabled256','guest-instrument','guest-lldb','guest-cold256','guest-disabled256']
 for rep in range(a.repeats):
  for mode in modes if rep%2==0 else list(reversed(modes)):
   file=a.out/f'{mode}-{rep}.bin';begin=time.perf_counter()
   if mode in ['ordinary','debug-o2','gdb-o2','guest-instrument','guest-lldb']:
    row=wasm_run(a.wasmtime,a.lldb,a.wasm,mode,file)
   elif mode.startswith('gdb-'):
    c=Console(['/usr/bin/gdb','-q','-nx','--args',str(a.wasmtime),'run','-C','cache=n','-O','opt-level=2','-D','debug-info',str(a.wasm)])
    try:
     c.wait(lambda b:re.search(rb'\(gdb\) $',b),0,30)
     for cmd in ['set pagination off','set confirm off','set breakpoint pending on']:ask(c,cmd)
     for i in range(256):ask(c,f'break probe_cold{i}')
     ask(c,'break __jit_debug_register_code');reply=ask(c,'run')
     assert b'Breakpoint 257' in reply,reply[-2000:]
     # Let the JIT notification complete before requiring resolved sites.
     # Returning from this registration helper precedes guest execution.
     ask(c,'finish')
     table=ask(c,'info breakpoints');assert b'<PENDING>' not in table,table[-3000:]
     assert len(re.findall(rb'^\s*\d+\s+breakpoint',table,re.M))==257
     ask(c,'delete 257')
     if mode=='gdb-disabled256':ask(c,'disable')
     reply=ask(c,'continue');assert b'exited normally' in reply,reply[-2000:]
     c.finish();output=bytes(c.log)
    finally:file.write_bytes(c.log);c.close()
    row={'kernel_ns':round(float(re.findall(rb'Time: ([0-9.]+) ms',output)[0])*1e6),'checksum':re.findall(rb'Checksum: (\d+)',output)[0].decode(),'process_s':time.perf_counter()-begin}
   else:
    with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
    serverfile=file.with_suffix('.server.bin');c=None
    with serverfile.open('xb') as f:
     server=subprocess.Popen([str(a.wasmtime),'run','-C','cache=n','-O','opt-level=2','-g',str(port),str(a.wasm)],stdout=f,stderr=subprocess.STDOUT)
     try:
      deadline=time.monotonic()+60
      while b'Debugger listening' not in serverfile.read_bytes():
       assert server.poll() is None and time.monotonic()<deadline;time.sleep(.02)
      commands=[f'process connect --plugin wasm connect://127.0.0.1:{port}',*[f'breakpoint set -n probe_cold{i}' for i in range(256)],'breakpoint list']
      if mode=='guest-disabled256':commands+=['breakpoint disable','breakpoint list']
      commands+=['continue','breakpoint list']
      c=Console([str(a.lldb),'--no-lldbinit','--batch',*[x for cmd in commands for x in ['-o',cmd]]])
      c.wait(lambda b:b'exited with status = 0' in b,0,180);finish_batch(c);server.wait(timeout=10);assert server.returncode==0
      output=bytes(c.log)
      assert len(re.findall(rb'^Breakpoint \d+: where =',output,re.M))==256,output[-4000:]
      assert b'hit count = 1' not in output
      program=serverfile.read_bytes();row={'kernel_ns':round(float(re.findall(rb'Time: ([0-9.]+) ms',program)[0])*1e6),'checksum':re.findall(rb'Checksum: (\d+)',program)[0].decode(),'process_s':time.perf_counter()-begin}
     finally:
      if c is not None:file.write_bytes(c.log);c.close()
      if server.poll() is None:server.terminate()
      try:server.wait(timeout=5)
      except subprocess.TimeoutExpired:server.kill();server.wait()
   assert row['checksum']=='10000',row
   result['samples'].append({'mode':mode,'repeat':rep,**row});(a.out/'result.json').write_text(json.dumps(result,indent=2));print(mode,rep,row['kernel_ns']/1e6,'ms',flush=True)
 result['medians_ms']={m:statistics.median(x['kernel_ns']/1e6 for x in result['samples'] if x['mode']==m) for m in modes};result['passed']=True;(a.out/'result.json').write_text(json.dumps(result,indent=2))


if __name__=='__main__':main()
