#!/usr/bin/env python3
"""Actual native run-to events: nearest breakpoint, exact PC, unchanged JIT bounds.
Fixture: debug_native_fast_breakpoint.wat. Compare --baseline with optimized.
The candidate interval contains only a straight-line numeric SIMD lowering;
stop-id delta measures published manager stops, not worker TF signals or a
branch skipping display rows.
"""
import argparse,json,os,re,time,hashlib
from pathlib import Path
from run_debug_run_cli import Console
p=argparse.ArgumentParser(description=__doc__)
for n in ['uwvm','wasm','out']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--baseline',action='store_true',help='Record old per-instruction stops without requiring batching')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
c=Console([str(a.uwvm.resolve()),'-Rdbg','-WFE-simd','-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]);actions=[];checks=[]
def ask(text,fail=False):
 b=c.command(text);b=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',b).replace(b'\r',b'');actions.append({'command':text,'reply':b.decode(errors='replace')})
 (a.out/'actions.json').write_text(json.dumps(actions,indent=2));assert fail or b'error:' not in b,b[-2000:];return b
def position():
 b=ask('status');return int(re.search(rb'stop-id (\d+)',b)[1]),int(re.search(rb'native-pc=(0x[0-9a-f]+)',b)[1],16)
def linear(row):
 text=row[4].decode().strip().lower();op=text.split()[0]
 if op.startswith(('call','j','ret','cb','tb')) or op in ['b','br','blr','bl','blraa','braa']:return False
 if not op.startswith(('mov','ldr','str','ldur','stur','add','sub','and','or','xor','eor','lea','fmov','nop','dup','umov','mul','pmul','pshuf','psrl','psll','padd','punp','zip','xtn','uzp','ushr','shl','orr','movi')):return False
 if re.match(r'\S+\s+(?:sp|wsp|x29|x30|rsp|esp|rbp|ebp)\s*,',text):return False
 return True
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);ask('startw');bp=ask('bw 0 0 4');bid=re.search(rb'breakpoint (\d+)',bp)[1].decode();ask('c');ask('delete '+bid);ask('ni')
 for iteration in range(1):
  for search in range(50):
   dis=ask('disas');rows=re.findall(rb'instruction (\d+) pc=(0x[0-9a-fA-F]+) function-offset=(\d+) bytes=([0-9a-fA-F ]+)  ([^\n]+)',dis)
   targets=[];ids=[]
   for ordinal,row in enumerate(rows[2:24],2):
    if not all(linear(x) for x in rows[:ordinal]):continue
    candidate=ask('bi '+row[2].decode(),True)
    if b'error:' in candidate:continue
    ids.append(re.search(rb'breakpoint (\d+)',candidate)[1].decode());targets.append(row)
    if len(ids)==2:break
   if ids:break
   ask('ni')
  else:raise AssertionError('fixture has no visible forward JIT target')
  row,bid=targets[0],ids[0]
  before,pc=position();t=time.perf_counter();hit=ask('c');elapsed=time.perf_counter()-t;after,actual=position()
  assert b'stopped: native breakpoint' in hit and b'native-breakpoint '+bid.encode() in hit,hit
  assert actual==int(row[1],16),hit
  checks.append({'target':actual,'stop_delta':after-before,'listing_ordinal':int(row[0]),'seconds':elapsed,'from':pc,'configured_targets':len(ids)})
  if not a.baseline:assert after-before==1,'linear native run should publish only one manager stop'
  ask('delete '+ids[0])
  if len(ids)==2:
   hit=ask('c');assert b'native-breakpoint '+ids[1].encode() in hit,hit
   assert position()[1]==int(targets[1][1],16),'second breakpoint must remain armed'
   ask('delete '+ids[1])
 assert b'error:' in ask('bi 65537',True)
 ask('nw');assert b'error:' in ask('bi 1',True),'cooperative pause must not acquire native breakpoint rights'
 ask('c');c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'checks':checks,'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest()},indent=2))
finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
