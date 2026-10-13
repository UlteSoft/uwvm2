#!/usr/bin/env python3
"""Run actual Core 3 explicit block/select encodings, keeping short encodings for codegen comparison."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':(True,'''(module (func $id (param funcref) (result funcref) block (result funcref) local.get 0 end)
 (func (export "_start") ref.null func call $id ref.is_null i32.eqz if unreachable end))'''),
 'tuple':(True,'''(module (func $id (param i32 funcref i64 externref) (result i32 funcref i64 externref)
 local.get 0 block (result funcref) local.get 1 end local.get 2 loop (result externref) local.get 3 end)
 (func (export "_start") i32.const 7 ref.null func i64.const 9 ref.null extern call $id
 ref.is_null i32.eqz if unreachable end i64.const 9 i64.ne if unreachable end ref.is_null i32.eqz if unreachable end i32.const 7 i32.ne if unreachable end))'''),
 'indirect':(True,'''(module (type $t (func (param externref) (result externref))) (table 1 funcref) (elem (i32.const 0) $id)
 (func $id (type $t) block (result externref) local.get 0 end)
 (func (export "_start") ref.null extern i32.const 0 call_indirect (type $t) ref.is_null i32.eqz if unreachable end))'''),
 'memory':(True,'''(module (memory 1) (func $id (param funcref i32) (result funcref i32)
 local.get 1 i32.const 42 i32.store block (result funcref) local.get 0 end local.get 1 i32.load)
 (func (export "_start") ref.null func i32.const 65532 call $id i32.const 42 i32.ne if unreachable end ref.is_null i32.eqz if unreachable end))'''),
 'if':(True,'''(module (func $f) (elem declare func $f) (func $choose (param i32) (result funcref)
 local.get 0 if (result funcref) ref.func $f else ref.null func end)
 (func (export "_start") i32.const 0 call $choose ref.is_null i32.eqz if unreachable end
 i32.const 1 call $choose ref.is_null if unreachable end))'''),
 'select':(True,'''(module (func $f) (elem declare func $f) (func $choose (param i32) (result funcref)
 ref.func $f ref.null func local.get 0 select (result funcref))
 (func (export "_start") i32.const 0 call $choose ref.is_null i32.eqz if unreachable end
 i32.const -1 call $choose ref.is_null if unreachable end))'''),
 'select-external':(True,'''(module (func (export "_start") ref.null extern ref.null extern i32.const -2147483648 select (result externref)
 ref.is_null i32.eqz if unreachable end))'''),
 'loop':(True,'''(module (func $id (param externref) (result externref) block (result externref) local.get 0 end)
 (func (export "_start") (local i32) i32.const 20000 local.set 0 loop
 ref.null extern call $id ref.is_null i32.eqz if unreachable end local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end))'''),
 'bad-select-hierarchy':(False,'''(module (func (export "_start") ref.null func ref.null extern i32.const 0 select (result funcref) drop))'''),
 'bad-block-result':(False,'''(module (func (export "_start") block (result funcref) ref.null extern end drop))'''),
 'bad-select-condition':(False,'''(module (func (export "_start") ref.null func ref.null func i64.const 0 select (result funcref) drop))'''),
 'bad-if-condition':(False,'''(module (func (export "_start") i64.const 0 if (result funcref) ref.null func else ref.null func end drop))'''),
}
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
  if sid==10:
   n=s.integer();body=leb(n)
   for _ in range(n):
    f=Reader(s.take(s.integer()));groups=f.integer()
    for _ in range(groups):f.integer();assert f.byte() in (0x7f,0x7e,0x7d,0x7c,0x7b,0x70,0x6f)
    code=bytearray(f.data[:f.pos])
    while f.pos<len(f.data):
     op=f.byte();code.append(op);start=f.pos
     if op in (2,3,4):
      first=f.data[f.pos];f.integer()
      if first in (0x70,0x6f):code.append(0x63);count+=1
     elif op==0x1c:
      arity=f.integer();assert arity==1;code+=f.data[start:f.pos];start=f.pos;first=f.byte()
      if first in (0x70,0x6f):code.append(0x63);count+=1
     elif op in (0x0c,0x0d,0x10,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x41,0x42,0xd0,0xd2):f.integer()
     elif op in (0x11,0x28,0x36):f.integer();f.integer()
     else:assert op in (0,5,11,0x1a,0x45,0x47,0x52,0x6a,0x6b,0xd1),(op,f.pos)
     code+=f.data[start:f.pos]
    body+=leb(len(code))+code
   assert s.pos==len(s.data)
  out.append(sid);out+=leb(len(body));out+=body
 assert count;return bytes(out),count

def corrupt_select(type_bytes,count=bytes([1])):
 sec=lambda i,b:bytes([i])+leb(len(b))+b
 # Polymorphic operands still require decoding and validating every type byte.
 code=bytes([0,0,0x1c])+count+type_bytes+bytes([0x1a,11])
 return b'\0asm\1\0\0\0'+sec(1,b'\1\x60\0\0')+sec(3,b'\1\0')+sec(7,b'\1\6_start\0\0')+sec(10,b'\1'+leb(len(code))+code)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 for name,(valid,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);short=a.output/(name+'-short.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(short)],check=True)
  data,count=expand(short.read_bytes());path=a.output/(name+'.wasm');path.write_bytes(data);cases.append((name,valid,path,count))
 for name,t,count in [('truncated',bytes([0x63]),bytes([1])),('padded-abstract',bytes([0x63,0xf0,0x7f]),bytes([1])),('bad-heap',bytes([0x63,0x7f]),bytes([1])),('wrong-arity',bytes([0x63,0x70,0x63,0x70]),bytes([2]))]:
  path=a.output/(name+'.wasm');path.write_bytes(corrupt_select(t,count));cases.append((name,False,path,1))
 configs=[]
 if a.uwvm:configs=[(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm.resolve(),a.ros)]+[('validator',[str(a.uwvm.resolve()),'-m','validation'])]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 assert configs;rows=[]
 for label,base in configs:
  for name,valid,path,count in cases:
   command=base+(['-W','function-references=y'] if label=='wasmtime' else ['-WFE-function-references','--run'])+[str(path)]
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
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS explicit block/select CLI:',len(rows),'runs',flush=True)
if __name__=='__main__':main()
