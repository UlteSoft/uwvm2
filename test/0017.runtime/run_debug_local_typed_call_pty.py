#!/usr/bin/env python3
"""Real local typed-call stop, suspended caller, disabled probe and trace checks."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
from run_debug_run_cli import Console
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'0018.debugger'))
from run_wasm_operand_preview_cli import markers
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
wat=a.out.resolve()/'local-call.wat';wasm=wat.with_suffix('.wasm')
wat.write_text("""(module
 (func $leaf (param i32) (result i32) (local i32)
  local.get 0 i32.const 7 i32.add local.tee 1)
 (func (export "_start") (local i32)
  i32.const 9 local.set 0 i32.const 123 i32.const 35 nop call $leaf
  i32.const 42 i32.ne if unreachable end
  i32.const 123 i32.ne if unreachable end
  local.get 0 i32.const 9 i32.ne if unreachable end))""")
subprocess.run([str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(wasm)],check=True)
sites,_=markers(a.wasm_tools.resolve(),wasm);assert len(sites)==2 and len(sites[1])==1,sites
call_offset=sites[1][0]+1
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(wasm)]);actions=[];checks=[]
def ask(text):
 at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,60)
 reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',reply).replace(b'\r',b'');assert b'error:' not in reply,reply
 actions.append({'command':text,'reply':reply.decode(errors='replace')});return reply
def values(command,kind,want):
 reply=ask(command);got=re.findall(('^'+kind+r' ([0-9]+) (.*)$').encode(),reply,re.M)
 assert got==[(str(i).encode(),('i32 = '+str(v)).encode()) for i,v in enumerate(want)],(command,reply)
def position(reply,function,offset):
 match=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=(\d+)',reply)
 assert match and (int(match[2]),int(match[3]))==(function,offset),reply
 return match[1].decode()
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
 leaf=re.search(rb'breakpoint (\d+)',ask('bw 0 0 0'))[1].decode()
 call=re.search(rb'breakpoint (\d+)',ask(f'bw 0 1 {call_offset}'))[1].decode()
 for mode in ['disabled-call','before-call','ignored-call','full-trace']:
  ask('startw');ask('trace wasm off')
  if mode in ['disabled-call','full-trace']:ask('disable '+call)
  else:ask('enable '+call)
  if mode=='ignored-call':ask(f'ignore {call} 1')
  if mode=='full-trace':ask('trace wasm on')
  reply=ask('c')
  if mode=='before-call':
   thread=position(reply,1,call_offset)
   values(f'locals wasm {thread} 0 0 1','local',[9]);values(f'operands {thread} 0 0 2','operand',[123,35]);reply=ask('c')
  thread=position(reply,0,0)
  values(f'locals wasm {thread} 0 0 2','local',[35,0]);values(f'locals wasm {thread} 1 0 1','local',[9])
  values(f'operands {thread} 1 0 1','operand',[123])
  ask('c');assert b'Program exited normally.' in ask('status')
  if mode=='full-trace':
   trace=ask('trace wasm read 0 128')
   assert re.search(rb'wasm-event .*function=1 .*byte-offset='+str(call_offset).encode()+rb' .*before-instruction opcode=0x10',trace),trace
  checks.append({'mode':mode,'exact_call_inputs':mode=='before-call','exact_leaf_and_suspended_caller':True,'normal_checksum_exit':True})
 c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'call_offset':call_offset,'checks':checks,'actions':actions},indent=2));print('PASS local typed call inputs, suspended prefix, disabled/ignored probes and full trace')
finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
