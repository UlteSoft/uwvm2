from pathlib import Path
import sys,json,os,re,signal,argparse,hashlib
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));sys.path.insert(0,str(R/'source/call-ref-identity-c10'));import native as N
from fixture_converter import wat_to_wasm,leb
from run_debug_run_cli import Console
p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(exist_ok=False,parents=True);os.environ.update(TERM='dumb',NO_COLOR='1');results=[]
for ty in ['i8','i16','i32','f32']:
 D=a.out/ty;D.mkdir();v=lambda x:'i32.const '+str(x)+(' f32.reinterpret_i32' if ty=='f32' else '');get=('array.get_u' if ty in ['i8','i16'] else 'array.get')+' $A'+(' i32.reinterpret_f32' if ty=='f32' else '');mask=255 if ty=='i8' else 65535 if ty=='i16' else 0xffffffff;expected=0xff800001&mask
 wat='(module (type $A (array (mut '+ty+'))) (func (export "_start") (local $a (ref null $A)) i32.const 257 array.new_default $A local.set $a local.get $a i32.const 256 '+v(17)+' array.set $A nop local.get $a i32.const 256 '+get+' i32.const '+str(expected)+' i32.ne if unreachable end local.get $a i32.const 255 '+v(29)+' array.set $A local.get $a i32.const 255 '+get+' i32.const 29 i32.ne if unreachable end))'
 wp=D/'main.wat';wp.write_text(wat);wasm=D/'main.wasm';wat_to_wasm(wp,wasm)
 b=wasm.read_bytes();pos=8;point=None
 while pos<len(b):
  sec=b[pos];n,pos=leb(b,pos+1);end=pos+n
  if sec==10:
   count,pos=leb(b,pos);assert count==1;size,pos=leb(b,pos);groups,pos=leb(b,pos)
   for _ in range(groups):
    _,pos=leb(b,pos);k=b[pos];pos+=1
    if k in [0x63,0x64]:_,pos=leb(b,pos)
   first=pos
   while pos<end:
    off=pos;op=b[pos];pos+=1
    if op==1:point=off-first;break
    if op in [0x20,0x21,0x41]:_,pos=leb(b,pos)
    elif op==0xfb:
     sub,pos=leb(b,pos);assert sub in [7,14];_,pos=leb(b,pos)
    else:assert op==0xbe,(ty,hex(op))
  pos=end
 assert point is not None
 c=Console([str(a.uwvm),*(['-Rcc','jit','-Rcm','full'] if a.uwvm.parent.name=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3']),'-Rdbg','-Rct','0','-WFE-gc','-Rllvm-cache-path','disable','--run',str(wasm)]);actions=[]
 def ask(text,error=False):
  at=len(c.log);os.write(c.fd,(text+'\n').encode());reply=c.wait(lambda b:re.search(rb'(?:^|\n)\(uwvm-debug\) $',b),at,120);reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',reply).replace(b'\r',b'');actions.append(dict(command=text,reply=reply.decode(errors='replace')));assert (b'error:' in reply)==error,reply;return reply
 try:
  assert Path('/proc/'+str(c.pid)+'/cgroup').read_text().strip()=='0::'+N.CG;c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);listing=ask('info modules');mid=int(re.search(rb'(?m)^  ([0-9]+) = main.wasm',listing)[1]);ask(f'bw {mid} 0 {point}')
  for iteration in range(3):
   ask('startw');reply=ask('c');m=re.search(rb'thread (\d+) module=(\d+) function=(\d+) byte-offset=(\d+)',reply);assert m and tuple(map(int,m.groups()[1:]))==(mid,0,point),reply;t=m[1].decode();view=ask(f'locals wasm {t} 0 0 1');assert b'array #' in view and b'members=257' in view,view
   kind='f32' if ty=='f32' else 'i32';ask(f'set wasm member {t} locals 0 0 at 256 bits {kind} ff800001');ask(f'set wasm member {t} locals 0 0 at 257 bits {kind} 2a',True);ask(f'set wasm member {t} locals 0 0 at 256 bits i64 2a',True);ask('c');assert b'Program exited normally.' in ask('status');results.append(dict(type=ty,restart=iteration,pass_=True,debug_write_bits=0xff800001,wasm_sha=N.sha(wasm),point=point))
  c.finish()
 finally:(D/'transcript.bin').write_bytes(c.log);(D/'actions.json').write_text(json.dumps(actions,indent=2));c.close()
 print('C21_DEBUG_STORE',ty,flush=True)
(a.out/'result.json').write_text(json.dumps(dict(results=results,cells=len(results),ELF_sha=N.sha(a.uwvm),scope='Actual full JIT debugger fallback: inspect mutable array after Wasm write, raw typed member write, invalid kind/bounds rejected, Wasm validates debug write and performs another store; 3 restarts each numeric32storage type.'),indent=2))
