#!/usr/bin/env python3
"""Real copied funcref, table mutation, ALL-park replacement and typed tail tests."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
from run_debug_run_cli import Console
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'0018.debugger'))
from run_wasm_operand_preview_cli import markers
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
wat=a.out.resolve()/'copied-ref.wat';wasm=wat.with_suffix('.wasm')
wat.write_text('''(module
 (type $unary (func (param i32) (result i32)))
 (table $table 1 (ref null $unary))
 (elem (i32.const 0) (ref $unary) (ref.func $old))
 (elem declare (ref $unary) (ref.func $other))
 (func $old (type $unary) (local i32) local.get 0 i32.const 7 i32.add local.tee 1)
 (func $other (type $unary) local.get 0 i32.const 9 i32.add)
 (func $tail (type $unary) local.get 0 i32.const 0 table.get $table return_call_ref $unary)
 (func (export "_start") (local $copied (ref null $unary)) (local $answer i32)
  i32.const 0 table.get $table local.set $copied
  i32.const 0 ref.func $other table.set $table
  nop
  i32.const 35 local.get $copied call_ref $unary local.set $answer
  local.get $answer i32.const 44 i32.ne if unreachable end
  i32.const 35 i32.const 0 table.get $table call_ref $unary
  i32.const 44 i32.ne if unreachable end
  i32.const 35 call $tail i32.const 44 i32.ne if unreachable end
  nop))''')
for argv in [[str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(wasm)],
             [str(a.wasm_tools.resolve()),'validate','--features','all',str(wasm)]]:
 subprocess.run(argv,check=True)
sites,_=markers(a.wasm_tools.resolve(),wasm);assert len(sites)==4 and len(sites[3])==2,sites
body=a.out.resolve()/'replacement.bin';body.write_bytes(bytes.fromhex('01 01 7f 20 00 41 09 6a 22 01 0b'))
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-WFE-function-references','-WFE-reference-types','-WFE-tail-call','--run',str(wasm)])
actions=[];runs=[]
def ask(text,error=False):
 at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,120)
 reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',reply).replace(b'\r',b'')
 actions.append({'command':text,'reply':reply.decode(errors='replace')});assert (b'error:' in reply)==error,reply;return reply
def stopped(reply,function,offset):
 match=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=(\d+)',reply)
 assert match and tuple(map(int,match.groups()[1:]))==(function,offset),reply;return match[1].decode()
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
 ask(f'bw 0 3 {sites[3][0]}');ask(f'bw 0 3 {sites[3][1]}')
 for iteration in range(3):
  ask('startw');ask('trace wasm '+('on' if iteration==2 else 'off'))
  thread=stopped(ask('c'),3,sites[3][0]);before=ask(f'locals wasm {thread} 0 0 2')
  assert b'function module=0 index=0' in before and b'local 1 i32 = 0' in before,before
  changed=ask(f'replace 0 0 1 {body}');assert b'function replaced; generation 2' in changed,changed
  stale=ask(f'replace 0 0 1 {body}',True);assert b'function generation changed' in stale,stale
  thread=stopped(ask('c'),3,sites[3][1]);after=ask(f'locals wasm {thread} 0 0 2')
  assert b'function module=0 index=0' in after and b'local 1 i32 = 44' in after,after
  if iteration==2:
   trace=ask('trace wasm read 0 128');assert b'opcode=0x14' in trace and b'opcode=0x15' in trace,trace
  ask('c');assert b'Program exited normally.' in ask('status')
  runs.append({'iteration':iteration,'copied_old_identity_preserved_after_table_set':True,'copied_reference_uses_current_replacement':44,'fresh_table_target':44,'typed_dynamic_tail':44,'stale_generation_rejected':True,'exact_current_locals':True,'full_trace_checked':iteration==2})
 c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'harness_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'runs':runs,'actions':actions},indent=2)+'\n');print('PASS copied reference/table mutation/current replacement/dynamic tail/three restarts/full trace')
finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
