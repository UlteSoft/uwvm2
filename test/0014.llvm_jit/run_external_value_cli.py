#!/usr/bin/env python3
"""Run actual Core 3 explicit table/global/import declarations and bounded binary failures."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':(True,'(module (global $g funcref (ref.null func)) (func (export "_start") global.get $g ref.is_null i32.eqz if unreachable end))'),
 'tuple':(True,'(module (global $f (mut funcref) (ref.null func)) (global $e (mut externref) (ref.null extern)) (func $f) (elem declare func $f) (func (export "_start") ref.func $f global.set $f global.get $f ref.is_null if unreachable end global.get $e ref.is_null i32.eqz if unreachable end))'),
 'indirect':(True,'(module (type $t (func (result i32))) (table 1 funcref) (elem (i32.const 0) $f) (func $f (type $t) i32.const 42) (func (export "_start") i32.const 0 call_indirect (type $t) i32.const 42 i32.ne if unreachable end))'),
 'memory':(True,'(module (memory 1) (global $g (mut funcref) (ref.null func)) (func (export "_start") i32.const 65532 i32.const 42 i32.store global.get $g ref.is_null i32.eqz if unreachable end i32.const 65532 i32.load i32.const 42 i32.ne if unreachable end))'),
 'if':(True,'(module (table 2 externref) (func (export "_start") i32.const 1 table.get ref.is_null i32.eqz if unreachable end i32.const 0 ref.null extern table.set i32.const 0 table.get ref.is_null i32.eqz if unreachable end))'),
 'select':(True,'(module (table 2 5 funcref) (func $f) (elem declare func $f) (func (export "_start") i32.const 0 ref.func $f i32.const 2 table.fill i32.const 1 table.get ref.is_null if unreachable end ref.null func i32.const 1 table.grow i32.const 2 i32.ne if unreachable end i32.const 2 table.get ref.is_null i32.eqz if unreachable end))'),
 'select-external':(True,'(module (global $e (mut externref) (ref.null extern)) (func (export "_start") global.get $e ref.null extern i32.const 1 select (result externref) global.set $e global.get $e ref.is_null i32.eqz if unreachable end))'),
 'loop':(True,'(module (global $g (mut externref) (ref.null extern)) (func (export "_start") (local i32) i32.const 20000 local.set 0 loop global.get $g global.set $g local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end global.get $g ref.is_null i32.eqz if unreachable end))'),
 'bad-global-set':(False,'(module (global $g (mut funcref) (ref.null func)) (func (export "_start") ref.null extern global.set $g))'),
 'bad-table-set':(False,'(module (table 1 externref) (func (export "_start") i32.const 0 ref.null func table.set))'),
 'bad-immutable':(False,'(module (global $g externref (ref.null extern)) (func (export "_start") ref.null extern global.set $g))'),
 'bad-initializer':(False,'(module (global $g funcref (ref.null extern)) (func (export "_start")))'),
 'import-table-func':(True,'(module (import "m" "t" (table 1 funcref)) (func (export "_start") i32.const 0 table.get ref.is_null i32.eqz if unreachable end))'),
 'import-table-extern':(True,'(module (import "m" "e" (table 1 externref)) (func (export "_start") i32.const 0 table.get ref.is_null i32.eqz if unreachable end))'),
 'import-global-func':(True,'(module (import "m" "g" (global (mut funcref))) (func (export "_start") global.get 0 ref.is_null i32.eqz if unreachable end ref.null func global.set 0))'),
 'import-global-extern':(True,'(module (import "m" "x" (global (mut externref))) (func (export "_start") global.get 0 ref.is_null i32.eqz if unreachable end ref.null extern global.set 0))'),
}
PROVIDER='(module (table (export "t") 1 funcref) (table (export "e") 1 externref) (global (export "g") (mut funcref) (ref.null func)) (global (export "x") (mut externref) (ref.null extern)))'

def leb(n):
 out=bytearray()
 while True:
  b=n&127;n>>=7;out.append(b|(128 if n else 0))
  if not n:return out
class Reader:
 def __init__(self,data):self.data=data;self.pos=0
 def byte(self):b=self.data[self.pos];self.pos+=1;return b
 def integer(self):
  n=0;shift=0
  while True:
   b=self.byte();n|=(b&127)<<shift
   if b<128:return n
   shift+=7
 def take(self,n):b=self.data[self.pos:self.pos+n];self.pos+=n;assert self.pos<=len(self.data);return b

def expand(data):
 r=Reader(data);out=bytearray(r.take(8));count=0
 while r.pos<len(data):
  sid=r.byte();s=Reader(r.take(r.integer()));body=bytearray(s.data)
  if sid in (2,4,6):
   n=s.integer();body=leb(n)
   for _ in range(n):
    kind=1 if sid==4 else 3
    if sid==2:
     start=s.pos
     for _ in range(2):s.take(s.integer())
     kind=s.byte();body+=s.data[start:s.pos]
    assert kind in (1,3)
    t=s.byte()
    if t in (0x70,0x6f):body.append(0x63);count+=1
    body.append(t);start=s.pos
    if kind==1:
     flags=s.integer();assert flags in (0,1);s.integer()
     if flags&1:s.integer()
    else:
     s.byte()
     if sid==6:
      assert s.byte() in (0xd0,0xd2,0x23);s.integer();assert s.byte()==11
    body+=s.data[start:s.pos]
   assert s.pos==len(s.data)
  out.append(sid);out+=leb(len(body));out+=body
 assert count;return bytes(out),count

def raw_declaration(table,type_bytes,tail=b''):
 sec=lambda i,b:bytes([i])+leb(len(b))+b
 return b'\0asm\1\0\0\0'+sec(4 if table else 6,b'\1'+type_bytes+tail)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 provider_wat=a.output/'provider.wat';provider_wat.write_text(PROVIDER);provider=a.output/'provider.wasm';subprocess.run([str(a.wasm_tools),'parse',str(provider_wat),'-o',str(provider)],check=True)
 for name,(valid,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);short=a.output/(name+'-short.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(short)],check=True)
  data,count=expand(short.read_bytes());path=a.output/(name+'.wasm');path.write_bytes(data);cases.append((name,valid,path,count))
 for table in (False,True):
  for label,t in [('truncated',b'\x63'),('padded-abstract',b'\x63\xf0\x7f'),('bad-heap',b'\x63\x7f'),('overflow',b'\x63\x80\x80\x80\x80\x10')]:
   name=('table-' if table else 'global-')+label;path=a.output/(name+'.wasm');path.write_bytes(raw_declaration(table,t));cases.append((name,False,path,1))
 configs=[]
 if a.uwvm:configs=[(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm.resolve(),a.ros)]+[('validator',[str(a.uwvm.resolve()),'-m','validation'])]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 assert configs;rows=[]
 for label,base in configs:
  for name,valid,path,count in cases:
   preload=(['--preload','m='+str(provider)] if label=='wasmtime' else ['-Wpre',str(provider),'m']) if name.startswith('import-') else []
   command=base+preload+(['-W','function-references=y'] if label=='wasmtime' else ['-WFE-function-references','--run'])+[str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log)
   rows.append(dict(configuration=label,case=name,valid=valid,expanded_types=count,exit=r.returncode,command=command));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert (r.returncode==0)==valid,(label,name,log)
   if not valid:assert any(x in log.lower() for x in (b'validat',b'parsing error',b'invalid input webassembly code',b'failed to parse webassembly module')),(label,name,log)
  print('PASS',label,flush=True)
 if a.uwvm:
  gates=[]
  for case in ('identity','select'):
   for label,flags in [('default',[]),('disabled',['-WFD-function-references']),('unrelated',['-WFE-tail-call']),('reference-types-disabled',['-WFE-function-references','-WFD-reference-types'])]:
    command=[str(a.uwvm),'-m','validation',*flags,'--run',str(a.output/(case+'.wasm'))];r=subprocess.run(command,capture_output=True);log=r.stdout+r.stderr;(a.output/('gate-'+case+'-'+label+'.log')).write_bytes(log);assert r.returncode!=0,(case,label,log)
    if label=='unrelated':assert b'--wasm-feature-enable-function-references' in log,(case,label,log)
    gates.append(dict(case=case,policy=label,exit=r.returncode,command=command))
  (a.output/'gates.json').write_text(json.dumps(gates,indent=2)+'\n');cache=a.output/'cache';cache.mkdir();cache_rows=[]
  for phase,flag in [('cold','-WFE-function-references'),('warm','-WFE-function-references'),('disabled','-WFD-function-references')]:
   compile_log=a.output/('cache-'+phase+'.compile.log');command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),flag,'--run',str(a.output/'select.wasm')]
   r=subprocess.run(command,capture_output=True,timeout=60);(a.output/('cache-'+phase+'.log')).write_bytes(r.stdout+r.stderr);hit=compile_log.exists() and 'object-cache-hit' in compile_log.read_text();assert (r.returncode==0)==(phase!='disabled');assert phase!='warm' or hit;assert phase!='disabled' or not hit;cache_rows.append(dict(phase=phase,exit=r.returncode,hit=hit))
  (a.output/'cache.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS explicit table/global/import CLI:',len(rows),'runs',flush=True)
if __name__=='__main__':main()
