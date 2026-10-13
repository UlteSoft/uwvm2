#!/usr/bin/env python3
"""Dynamic local/import call_ref targets, same-epoch replacement and restarts."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
from run_debug_run_cli import Console
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'0018.debugger'))
from run_wasm_operand_preview_cli import markers
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
texts={'provider':'(module (func (export "probe") (param i32) (result i32) (local i32) local.get 0 i32.const 7 i32.add local.tee 1))',
'main':'''(module (type $unary (func (param i32) (result i32)))
 (import "identity-provider" "probe" (func $provider (type $unary)))
 (elem declare func $provider $local)
 (func $local (type $unary) local.get 0 i32.const 9 i32.add)
 (func (export "_start") (local $target (ref null $unary)) (local $answer i32)
  ref.func $local local.set $target i32.const 33 local.get $target call_ref $unary
  i32.const 42 i32.ne if unreachable end
  ref.func $provider local.set $target i32.const 35 local.get $target call_ref $unary local.set $answer
  local.get $answer i32.const 42 i32.ne if unreachable end
  nop
  i32.const 35 local.get $target call_ref $unary local.set $answer
  local.get $answer i32.const 44 i32.ne if unreachable end))'''}
files={}
for name,text in texts.items():
 wat=a.out.resolve()/(name+'.wat');wat.write_text(text);files[name]=wat.with_suffix('.wasm')
 subprocess.run([str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(files[name])],check=True)
points,_=markers(a.wasm_tools.resolve(),files['main']);assert len(points[2])==1,points
replacement=a.out.resolve()/'provider-body.bin';replacement.write_bytes(bytes.fromhex('01 01 7f 20 00 41 09 6a 22 01 0b'))
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-WFE-function-references','-WFE-reference-types','-Wpre',str(files['provider']),'identity-provider','--run',str(files['main'])]);actions=[];runs=[]
def ask(text,error=False):
 at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,120)
 reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',reply).replace(b'\r',b'')
 actions.append({'command':text,'reply':reply.decode(errors='replace')});assert (b'error:' in reply)==error,reply;return reply
def stopped(reply,module,function,offset):
 match=re.search(rb'thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+)',reply)
 assert match and tuple(map(int,match.groups()[1:]))==(module,function,offset),reply;return match[1].decode()
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
 listing=ask('info modules');ids={f.decode():int(i) for i,f in re.findall(rb'(?m)^  ([0-9]+) = ([^\n]+)',listing)}
 assert set(ids)=={'main.wasm','provider.wasm'},listing
 main,provider=ids['main.wasm'],ids['provider.wasm']
 ask(f'bw {provider} 0 0');ask(f'bw {main} 2 {points[2][0]}')
 for iteration in range(3):
  ask('startw');thread=stopped(ask('c'),provider,0,0)
  values=ask(f'locals wasm {thread} 0 0 2');assert re.findall(rb'^local ([0-9]+) (.*)$',values,re.M)==[(b'0',b'i32 = 35'),(b'1',b'i32 = 0')],values
  thread=stopped(ask('c'),main,2,points[2][0]);values=ask(f'locals wasm {thread} 0 0 2')
  # Inspection names the actual owned import slot; execution below verifies its provider leaf.
  assert f'function module={main} index=0'.encode() in values and b'local 1 i32 = 42' in values,values
  # The first call already populated any mapping cache. Its callee is retired,
  # while THIS same invocation and its target reference remain live.
  changed=ask(f'replace {provider} 0 1 {replacement}');assert b'function replaced; generation 2' in changed,changed
  ask(f'bw {provider} 0 0')
  stale=ask(f'replace {provider} 0 1 {replacement}',True);assert b'function generation changed' in stale,stale
  thread=stopped(ask('c'),provider,0,0)
  values=ask(f'locals wasm {thread} 0 0 2');assert b'local 0 i32 = 35' in values and b'local 1 i32 = 0' in values,values
  active=ask(f'replace {provider} 0 2 {replacement}',True);assert b'active' in active,active
  ask('c');assert b'Program exited normally.' in ask('status')
  runs.append({'iteration':iteration,'local_target_result':42,'import_target_before':42,'same_invocation_replacement_result':44,'stale_and_active_replacement_rejected':True,'exact_current_reference_and_locals':True})
 c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'harness_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'wasm_sha256':{n:hashlib.sha256(f.read_bytes()).hexdigest() for n,f in files.items()},'runs':runs,'actions':actions},indent=2));print('PASS dynamic call_ref local/import target, same-invocation replacement after a warm lookup, exact values, stale/active rejection and 3 restarts')
finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
