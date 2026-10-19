#!/usr/bin/env python3
"""Real PTY breakpoint cost: cold table, disabled table, ignore and visible hits.

Use the SAME debug_breakpoint_perf.cc source for --wasm and --native (O2, -g,
-fno-inline). GDB and uwvm are compared to their OWN ordinary backend baseline.
Kernel CLOCK_MONOTONIC excludes breakpoint installation and launch. A hit case
includes prompt/continue round trips; ignore includes 9999 genuine ignored hits.
"""
import argparse, hashlib, json, os, re, statistics, subprocess, time
from pathlib import Path
from run_debug_run_cli import Console

def exports(path):
 data=path.read_bytes();at=8;end=len(data);found={}
 def number():
  nonlocal at
  n=shift=0
  while True:
   assert at<end and shift<35
   b=data[at];at+=1;n|=(b&127)<<shift
   if b<128:return n
   shift+=7
 while at<len(data):
  tag=data[at];at+=1;size=number();end=at+size;assert end<=len(data)
  if tag==7:
   for _ in range(number()):
    n=number();name=data[at:at+n].decode();at+=n;kind=data[at];at+=1;index=number()
    if kind==0:found[name]=index
   assert at==end
  at=end;end=len(data)
 return found

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['uwvm','wasm','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--native',type=Path);p.add_argument('--gdb',type=Path,default=Path('/usr/bin/gdb'))
 p.add_argument('--repeats',type=int,default=3);p.add_argument('--ros',action='store_true')
 p.add_argument('--case',action='append',choices=['none','cold256','disabled256','ignore9999','hits32'])
 a=p.parse_args();symbols=exports(a.wasm);a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='')
 result={'samples':[],'inputs':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [a.uwvm,a.wasm]+([a.native] if a.native else [])},
         'cgroup':Path('/proc/self/cgroup').read_text() if Path('/proc/self/cgroup').exists() else None,
         'note':'Each debugger uses its own backend baseline; hit timing includes PTY round trips. No profiler during timings.'}
 def save(): (a.out/'result.json').write_text(json.dumps(result,indent=2))
 modes=['wasm-ordinary','uwvm']+(['native-ordinary','gdb'] if a.native else [])
 for mode in modes:
  for case in (['none'] if 'ordinary' in mode else a.case or ['none','cold256','disabled256','ignore9999','hits32']):
   for rep in range(a.repeats):
    start=time.perf_counter(); name=f'{mode}-{case}-{rep}';count=0;table=b''
    flags=['-Raot'] if a.ros else ['-Rcm','full','-Rcc','jit']
    if 'ordinary' in mode:
     argv=[str(a.native.resolve())] if mode=='native-ordinary' else [str(a.uwvm.resolve()),*flags,'-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]
     reply=subprocess.check_output(argv,stderr=subprocess.STDOUT,timeout=120);(a.out/(name+'.bin')).write_bytes(reply)
    else:
     gdb=mode=='gdb'
     argv=[str(a.gdb),'-q','-nx','--args',str(a.native.resolve())] if gdb else [str(a.uwvm.resolve()),'-Rdbg',*flags,'-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]
     c=Console(argv);prompt=rb'\(gdb\) $' if gdb else rb'(?:^|\n)\(uwvm-debug\) $'
     def ask(text):
      at=len(c.log);os.write(c.fd,text.encode()+b'\n')
      b=c.wait(lambda b:re.search(prompt,b),at,120)
      assert not re.search(rb'error:|Undefined command|not defined|No symbol|not valid',b),b[-3000:]
      return b
     try:
      c.wait(lambda b:re.search(prompt,b),0,120)
      if gdb:
       ask('set pagination off');ask('set confirm off');ask('set print thread-events off')
      else:ask('startw')
      if case in ['cold256','disabled256']:
       for i in range(256):
        b=ask(f'break probe_cold{i}' if gdb else f'bw 0 {symbols["probe_cold"+str(i)]} 0')
        assert re.search(rb'(?:Breakpoint|breakpoint) \d+',b),b
       if case=='disabled256':ask('disable')
      if case in ['ignore9999','hits32']:
       b=ask('break probe_hot' if gdb else f'bw 0 {symbols["probe_hot"]} 0');bid=re.search(rb'(?:Breakpoint|breakpoint) (\d+)',b)[1].decode()
       if case=='ignore9999':ask('ignore '+bid+' 9999')
      started=time.perf_counter();reply=ask('run' if gdb else 'c')
      if case in ['ignore9999','hits32']:
       for _ in range(1 if case=='ignore9999' else 32):
        assert b'Breakpoint ' in reply if gdb else b'stopped: breakpoint' in reply,reply[-3000:]
        count+=1
        if count==(1 if case=='ignore9999' else 32):
         table=ask('info breakpoints');ask('disable')
        reply=ask('c')
       assert (b'hits=10000' if case=='ignore9999' else b'hits=32') in table if not gdb else True,table
      assert b'Time: ' in reply,reply[-3000:]
      wall=time.perf_counter()-started;c.finish()
     finally:(a.out/(name+'.bin')).write_bytes(c.log);c.close()
    ms=re.findall(rb'Time: ([0-9.]+) ms',reply);checksum=re.findall(rb'Checksum: ([^\r\n]+)',reply)
    assert len(ms)==len(checksum)==1 and checksum[0]==b'10000',reply
    row={'mode':mode,'case':case,'repeat':rep,'kernel_ms':float(ms[0]),'visible_stops':count,'total_s':time.perf_counter()-start,'argv':argv}
    if 'ordinary' not in mode:row['continue_to_exit_s']=wall
    result['samples'].append(row);save();print(mode,case,rep,row['kernel_ms'],flush=True)
 result['medians_ms']={f'{m}/{c}':statistics.median(x['kernel_ms'] for x in result['samples'] if x['mode']==m and x['case']==c) for m,c in {(x['mode'],x['case']) for x in result['samples']}}
 result['passed']=True;save()
if __name__=='__main__':main()
