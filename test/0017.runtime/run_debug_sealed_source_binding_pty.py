#!/usr/bin/env python3
"""Real cross-module execution and restart checks of sealed source bindings."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
from run_debug_run_cli import Console
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);os.environ.update(TERM='dumb',NO_COLOR='1')
texts={
 'leaf':'(module (func (export "probe") (param i32) (result i32) (local i32) local.get 0 i32.const 7 i32.add local.tee 1))',
 'relay':'(module (import "binding-leaf" "probe" (func $leaf (param i32) (result i32))) (func (export "probe") (param i32) (result i32) local.get 0 call $leaf))',
 'main':'(module (import "binding-relay" "probe" (func $relay (param i32) (result i32))) (func (export "_start") (local i32) i32.const 35 call $relay local.set 0 local.get 0 i32.const 42 i32.ne if unreachable end))'}
files={}
for name,text in texts.items():
 wat=a.out.resolve()/(name+'.wat');wat.write_text(text);files[name]=wat.with_suffix('.wasm');subprocess.run([str(a.wasm_tools.resolve()),'parse',str(wat),'-o',str(files[name])],check=True)
replacement=a.out.resolve()/'leaf-body.bin';replacement.write_bytes(bytes.fromhex('01 01 7f 20 00 41 07 6a 22 01 0b'))
c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Wpre',str(files['leaf']),'binding-leaf','-Wpre',str(files['relay']),'binding-relay','--run',str(files['main'])]);actions=[];runs=[]
def ask(text,expect_error=False):
 at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,60)
 reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',reply).replace(b'\r',b'');actions.append({'command':text,'reply':reply.decode(errors='replace')});assert (b'error:' in reply)==expect_error,reply;return reply
try:
 c.wait(lambda b:b'(uwvm-debug) ' in b,0,120)
 listing=ask('info modules');ids={file.decode():int(identifier) for identifier,file in re.findall(rb'(?m)^  ([0-9]+) = ([^\n]+)',listing)}
 assert set(ids)=={'main.wasm','relay.wasm','leaf.wasm'},listing
 assert len(set(ids.values()))==3
 assert b'registered' in ask(f'bw {ids["leaf.wasm"]} 0 0')
 for index in range(3):
  main=ask('startw');assert b'module='+str(ids['main.wasm']).encode()+b' function=1' in main,main
  if index==2:
   changed=ask(f'replace {ids["leaf.wasm"]} 0 1 {replacement}');assert b'function replaced; generation 2' in changed,changed
   stale=ask(f'replace {ids["leaf.wasm"]} 0 1 {replacement}',True);assert b'function generation changed' in stale,stale
   assert b'registered' in ask(f'bw {ids["leaf.wasm"]} 0 0')
  stopped=ask('c');location=re.search(rb'thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+)',stopped);assert location,stopped
  thread,module,function,offset=map(int,location.groups());assert (module,function,offset)==(ids['leaf.wasm'],0,0),stopped
  if index==2:
   active=ask(f'replace {ids["leaf.wasm"]} 0 2 {replacement}',True);assert b'active' in active,active
  local=ask(f'locals wasm {thread} 0 0 2');assert re.findall(rb'^local ([0-9]+) (.*)$',local,re.M)==[(b'0',b'i32 = 35'),(b'1',b'i32 = 0')],local
  caller=ask(f'locals wasm {thread} 1 0 1');assert re.findall(rb'^local ([0-9]+) (.*)$',caller,re.M)==[(b'0',b'i32 = 35')],caller
  outer=ask(f'locals wasm {thread} 2 0 1');assert re.findall(rb'^local ([0-9]+) (.*)$',outer,re.M)==[(b'0',b'i32 = 0')],outer
  for _ in range(4):ask('nw')
  local=ask(f'locals wasm {thread} 0 0 2');assert re.findall(rb'^local ([0-9]+) (.*)$',local,re.M)==[(b'0',b'i32 = 35'),(b'1',b'i32 = 42')],local
  ask('c');status=ask('status');assert b'Program exited normally.' in status,status
  runs.append({'index':index,'module_ids':ids,'leaf_location':[module,function,offset],'correct_leaf_and_suspended_callers':True,'normal_exit':True,'generation2_replacement':index==2,'stale_and_active_replacement_rejected':index==2})
 c.finish()
 result={'passed':True,'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'fixtures':{name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in files.items()},'runs':runs,'actions':actions}
 (a.out/'result.json').write_text(json.dumps(result,indent=2));print('PASS three sealed source modules, exact leaf/caller locals, three reloads and committed replacement')
finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
