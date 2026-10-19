#!/usr/bin/env python3
"""Real numeric locals across loop PHIs, call entry, suspended caller and NaN/v128 bits."""
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path
from run_debug_run_cli import Console
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'0018.debugger'))
from run_wasm_operand_preview_cli import markers
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
wat=a.out.resolve()/'numeric-locals.wat';wasm=wat.with_suffix('.wasm')
wat.write_text('''(module
 (func $leaf (param i32) (result i32) local.get 0 nop i32.const 7 i32.add)
 (func (export "_start") (local $i i32) (local i64 f32 f64 v128)
  i64.const -9223372036854775785 local.set 1
  i32.const 0x7f812345 f32.reinterpret_i32 local.set 2
  i64.const 0x7ff0123456789abc f64.reinterpret_i64 local.set 3
  v128.const i64x2 0x1020304050607080 0xa0b0c0d0e0f00102 local.set 4
  loop local.get $i nop call $leaf drop
   local.get $i i32.const 1 i32.add local.tee $i i32.const 3 i32.lt_u br_if 0
  end nop))''')
subprocess.run([str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(wasm)],check=True)
sites,_=markers(a.wasm_tools.resolve(),wasm);assert len(sites[0])==1 and len(sites[1])==2,sites
c=Console([str(a.uwvm.resolve()),'-Rdbg','-WFE-simd','-Rct','0','-Rllvm-cache-path','disable','-Rclog','err','--run',str(wasm)]);actions=[]
def command(text):
 reply=c.command(text);assert b'error:' not in reply,reply[-3000:]
 actions.append({'command':text,'reply':reply.decode(errors='replace')});return reply
def locals_at(frame,index,callee=False):
 reply=command(f'locals wasm 1 {frame} 0 {1 if callee else 5}')
 expected=[f'i32 = {index}'] if callee else [f'i32 = {index}','i64 = -9223372036854775785',
  'f32 = bits=0x7f812345','f64 = bits=0x7ff0123456789abc','v128 = bytes=80706050403020100201f0e0d0c0b0a0']
 rows=[x.decode() for x in re.findall(rb'^local \d+ ([^\r\n]+)',reply,re.M)];assert rows==expected,(rows,expected)
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
 for function,offsets in sites.items():
  for offset in offsets:assert b'registered' in command(f'bw 0 {function} {offset}')
 for index in range(3):
  for function in [1,0]:
   reply=command('c');assert f'function={function} byte-offset={sites[function][0]} '.encode() in reply,reply
   locals_at(0,index,function==0)
   if function==0:locals_at(1,index)
   operands=command('operands 1 0 0 1');assert f'operand 0 i32 = {index}'.encode() in operands,operands
 reply=command('c');assert f'function=1 byte-offset={sites[1][1]} '.encode() in reply,reply
 locals_at(0,3);command('c');assert b'Program exited normally.' in command('status');c.finish()
 policies=[[x.decode() for x in row] for row in re.findall(rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)',c.log)]
 assert policies and all(row==['session:debug','debug-local-ssa','less'] for row in policies),policies
 (a.out/'result.json').write_text(json.dumps({'passed':True,'sites':sites,'policies':policies,
  'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'actions':len(actions)},indent=2))
 print('PASS actual numeric locals/PHIs/callee/caller, exact sNaN/v128 bits and terminal retirement')
finally:
 (a.out/'transcript.bin').write_bytes(c.log);(a.out/'actions.json').write_text(json.dumps(actions,indent=2));c.close()
