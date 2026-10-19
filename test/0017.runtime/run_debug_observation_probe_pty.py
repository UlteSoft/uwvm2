#!/usr/bin/env python3
"""Exercise cold numeric breakpoint sites with actual loop values and policy changes."""
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path
from run_debug_run_cli import Console

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'0018.debugger'))
from run_wasm_operand_preview_cli import markers

p=argparse.ArgumentParser(description=__doc__)
for name in ('uwvm','wasm-tools','out'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
wat=a.out.resolve()/'numeric-loop.wat';wasm=wat.with_suffix('.wasm')
wat.write_text('''(module (func (export "_start") (local i32)
  loop local.get 0 i32.const 1 i32.add nop local.tee 0
  i32.const 100 i32.lt_u br_if 0 end local.get 0 nop drop))''')
subprocess.run([str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(wasm)],check=True)
sites,_=markers(a.wasm_tools.resolve(),wasm);assert len(sites)==1 and len(sites[0])==2,sites
os.environ.update(TERM='dumb',NO_COLOR='1')
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rclog','err','--run',str(wasm)])
actions=[]
def cmd(text):
    at=len(c.log);os.write(c.fd,text.encode()+b'\n')
    reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,60)
    assert b'error:' not in reply,reply[-3000:]
    actions.append({'command':text,'reply':reply.decode(errors='replace')});return reply
def values(local,operand,offset):
    reply=cmd('status');location=re.search(rb'thread (\d+) module=0 function=0 byte-offset=(\d+)',reply)
    assert location and int(location[2])==offset,reply
    t=location[1].decode()
    reply=cmd(f'locals wasm {t} 0 0 1');assert f'i32 = {local}'.encode() in reply,reply
    reply=cmd(f'operands {t} 0 0 1');assert f'i32 = {operand}'.encode() in reply,reply
try:
    c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);cmd('startw')
    reply=cmd(f'bw 0 0 {sites[0][0]}');loop=re.search(rb'breakpoint (\d+)',reply)[1].decode()
    reply=cmd(f'bw 0 0 {sites[0][1]}');final=re.search(rb'breakpoint (\d+)',reply)[1].decode()
    cmd(f'ignore {loop} 39');cmd('c');values(39,40,sites[0][0])
    cmd(f'disable {loop}');cmd('c');values(100,100,sites[0][1])
    cmd('c');cmd('startw');cmd(f'enable {loop}');cmd(f'ignore {loop} 59')
    cmd('c');values(59,60,sites[0][0])
    # This deliberately has no DWARF. Source-expression conditions must refuse
    # evaluation and retain the actual next hit, never silently assume false.
    cmd(f'condition {loop} 0');reply=cmd('c')
    assert b'breakpoint-condition' in reply and b'unavailable' in reply,reply
    values(60,61,sites[0][0]);cmd(f'condition {loop}');cmd(f'disable {loop}')
    cmd('c');values(100,100,sites[0][1])
    cmd(f'delete {loop}');cmd(f'disable {final}');cmd('c')
    c.finish()
    (a.out/'result.json').write_text(json.dumps({'passed':True,'actions':actions,'sites':sites,
        'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest()},indent=2))
    print('PASS cold numeric loop, current values, ignore/disable/enable/condition/delete and restart')
finally:
    (a.out/'console.bin').write_bytes(c.log);(a.out/'actions.json').write_text(json.dumps(actions,indent=2));c.close()
