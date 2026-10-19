#!/usr/bin/env python3
"""Native FFT ordinary versus GDB run: kernel time separate from startup/CLI."""
import argparse,hashlib,json,os,re,statistics,subprocess,time
from pathlib import Path
from run_debug_run_cli import Console
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--native',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--repeats',type=int,default=5)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='')
rows=[]
for mode in ['ordinary','gdb']:
 for i in range(a.repeats):
  started=time.perf_counter()
  if mode=='ordinary':log=subprocess.check_output([str(a.native.resolve())],timeout=30)
  else:
   c=Console(['/usr/bin/gdb','-q','-nx','--args',str(a.native.resolve())])
   def ask(command):
    at=len(c.log);os.write(c.fd,command.encode()+b'\n');return c.wait(lambda b:re.search(rb'\(gdb\) $',b),at,30)
   try:
    c.wait(lambda b:re.search(rb'\(gdb\) $',b),0,30);ask('set pagination off');ask('set confirm off');reply=ask('run')
    assert b'exited normally' in reply,reply
    log=bytes(c.log);c.finish()
   finally:c.close()
  ms=re.findall(rb'Time: ([0-9.]+) ms',log);sums=re.findall(rb'Checksum: ([^\r\n]+)',log);assert len(ms)==len(sums)==1,log
  (a.out/f'{mode}-{i}.bin').write_bytes(log);rows.append({'mode':mode,'kernel_ms':float(ms[0]),'process_s':time.perf_counter()-started,'checksum':sums[0].decode()})
  print(rows[-1],flush=True)
assert len({r['checksum'] for r in rows})==1
(a.out/'result.json').write_text(json.dumps({'passed':True,'samples':rows,'native_sha256':hashlib.sha256(a.native.read_bytes()).hexdigest(),'median_ms':{m:statistics.median(r['kernel_ms'] for r in rows if r['mode']==m) for m in ['ordinary','gdb']}},indent=2))
