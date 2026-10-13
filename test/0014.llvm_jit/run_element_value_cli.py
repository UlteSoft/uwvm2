#!/usr/bin/env python3
"""Run actual Core 3 explicit element types: active/passive/declarative, empty vectors, table.init and indirect calls."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':(True,'(module (table 1 funcref) (elem $e funcref (ref.null func)) (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $e i32.const 0 table.get ref.is_null i32.eqz if unreachable end))'),
 'tuple':(True,'(module (table 2 funcref) (func $f) (elem (i32.const 0) funcref (ref.func $f) (ref.null func)) (func (export "_start") i32.const 0 table.get ref.is_null if unreachable end i32.const 1 table.get ref.is_null i32.eqz if unreachable end))'),
 'indirect':(True,'(module (type $t (func (result i32))) (table 1 funcref) (elem (i32.const 0) $f) (func $f (type $t) i32.const 42) (func (export "_start") i32.const 0 call_indirect (type $t) i32.const 42 i32.ne if unreachable end))'),
 'memory':(True,'(module (memory 1) (table 1 externref) (elem $e externref (ref.null extern)) (func (export "_start") i32.const 65532 i32.const 42 i32.store i32.const 0 i32.const 0 i32.const 1 table.init $e i32.const 0 table.get ref.is_null i32.eqz if unreachable end i32.const 65532 i32.load i32.const 42 i32.ne if unreachable end))'),
 'if':(True,'(module (table 1 funcref) (table $t 1 externref) (elem (table $t) (i32.const 0) externref (ref.null extern)) (func (export "_start") i32.const 0 table.get $t ref.is_null i32.eqz if unreachable end))'),
 'select':(True,'(module (table 2 funcref) (func $f) (elem $e func $f) (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $e elem.drop $e i32.const 0 table.get ref.is_null if unreachable end))'),
 'select-external':(True,'(module (elem declare externref (ref.null extern)) (func (export "_start")))'),
 'loop':(True,'(module (table 1 funcref) (elem $e funcref (ref.null func)) (func (export "_start") (local i32) i32.const 20000 local.set 0 loop i32.const 0 i32.const 0 i32.const 1 table.init $e local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end i32.const 0 table.get ref.is_null i32.eqz if unreachable end))'),
 'empty-passive':(True,'(module (elem funcref) (func (export "_start")))'),
 'empty-declarative':(True,'(module (elem declare externref) (func (export "_start")))'),
 'bad-table-init':(False,'(module (table 1 funcref) (elem $e externref (ref.null extern)) (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $e))'),
 'bad-active':(False,'(module (table 1 funcref) (elem (i32.const 0) externref (ref.null extern)) (func (export "_start")))'),
 'bad-expression':(False,'(module (elem externref (ref.null func)) (func (export "_start")))'),
 'bad-table-set':(False,'(module (table 1 externref) (elem declare funcref (ref.null func)) (func (export "_start") i32.const 0 ref.null func table.set))'),
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
  if sid==9:
   n=s.integer();body=leb(n)
   for _ in range(n):
    flags=s.integer();assert flags<8;active=not(flags&1);table=0;offset=b''
    if active:
     if flags&2:table=s.integer()
     start=s.pos;assert s.byte() in (0x41,0x42,0x23);s.integer();assert s.byte()==11;offset=s.data[start:s.pos]
    heap=0x70
    if flags&3:
     encoded=s.byte()
     if flags&4:heap=encoded
     else:assert encoded==0
    num=s.integer();expressions=bytearray()
    for _ in range(num):
     if flags&4:
      start=s.pos;assert s.byte() in (0xd0,0xd2,0x23);s.integer();assert s.byte()==11;expressions+=s.data[start:s.pos]
     else:expressions+=b'\xd2'+leb(s.integer())+b'\x0b'
    body+=leb(6 if active else (7 if flags&2 else 5))
    if active:body+=leb(table)+offset
    body+=bytes([0x63,heap])+leb(num)+expressions;count+=1
   assert s.pos==len(s.data)
  out.append(sid);out+=leb(len(body));out+=body
 assert count;return bytes(out),count

def raw_element(flags,type_bytes,tail=b''):
 sec=lambda i,b:bytes([i])+leb(len(b))+b
 return b'\0asm\1\0\0\0'+sec(9,bytes([1,flags])+type_bytes+tail)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 for name,(valid,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);short=a.output/(name+'-short.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(short)],check=True)
  data,count=expand(short.read_bytes());path=a.output/(name+'.wasm');path.write_bytes(data);cases.append((name,valid,path,count))
 for flags in (5,7):
  for label,t in [('truncated',b'\x63'),('padded-abstract',b'\x63\xf0\x7f'),('bad-heap',b'\x63\x7f'),('overflow',b'\x63\x80\x80\x80\x80\x10')]:
   name=str(flags)+'-'+label;path=a.output/(name+'.wasm');path.write_bytes(raw_element(flags,t));cases.append((name,False,path,1))
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
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS explicit element CLI:',len(rows),'runs',flush=True)
if __name__=='__main__':main()
