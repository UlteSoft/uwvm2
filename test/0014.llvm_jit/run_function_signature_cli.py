#!/usr/bin/env python3
"""Execute explicit Core 3 nullable function signatures (0x63 heaptype), not encoder-shortened legacy aliases."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':'''(module (func $f) (elem declare func $f) (func $id (param funcref) (result funcref) local.get 0 ref.as_non_null)
 (func (export "_start") ref.func $f call $id ref.is_null if unreachable end))''',
 'external':'''(module (func $id (param externref) (result externref) local.get 0)
 (func (export "_start") ref.null extern call $id ref.is_null i32.eqz if unreachable end))''',
 'null-branches':'''(module (func $id (param funcref) (result funcref) local.get 0 br_on_non_null 0 ref.null func)
 (func (export "_start") ref.null func call $id ref.is_null i32.eqz if unreachable end))''',
 'tuple':'''(module (func $id (param i32 funcref i64 externref) (result i32 funcref i64 externref) local.get 0 local.get 1 local.get 2 local.get 3)
 (func (export "_start") i32.const 7 ref.null func i64.const 9 ref.null extern call $id
 ref.is_null i32.eqz if unreachable end i64.const 9 i64.ne if unreachable end ref.is_null i32.eqz if unreachable end i32.const 7 i32.ne if unreachable end))''',
 'indirect':'''(module (type $t (func (param externref) (result externref))) (table 1 funcref) (elem (i32.const 0) $id)
 (func $id (type $t) local.get 0) (func (export "_start") ref.null extern i32.const 0 call_indirect (type $t) ref.is_null i32.eqz if unreachable end))''',
 'loop':'''(module (func $id (param externref) (result externref) local.get 0) (func (export "_start") (local i32) i32.const 20000 local.set 0
 loop ref.null extern call $id ref.is_null i32.eqz if unreachable end local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end))''',
 'memory':'''(module (memory 1) (func $id (param funcref i32) (result funcref i32) local.get 1 i32.const 42 i32.store local.get 0 local.get 1 i32.load)
 (func (export "_start") ref.null func i32.const 65532 call $id i32.const 42 i32.ne if unreachable end ref.is_null i32.eqz if unreachable end))''',
 'block-signature':'''(module (type $t (func (param externref) (result externref)))
 (func (export "_start") ref.null extern block (type $t) end ref.is_null i32.eqz if unreachable end))''',
}
def leb(n):
 out=bytearray()
 while True:
  b=n&127;n>>=7;out.append(b|(128 if n else 0))
  if not n:return out
def expand(data):
 pos=8;out=bytearray(data[:8]);expanded=0
 def read():
  nonlocal pos
  n=0;shift=0
  while True:
   b=data[pos];pos+=1;n|=(b&127)<<shift
   if b<128:return n
   shift+=7
 while pos<len(data):
  sid=data[pos];pos+=1;size=read();end=pos+size;body=bytearray()
  if sid!=1:body.extend(data[pos:end]);pos=end
  else:
   count=read();body+=leb(count)
   for _ in range(count):
    assert data[pos]==0x60;pos+=1;body.append(0x60)
    for _ in range(2):
     count=read();body+=leb(count)
     for _ in range(count):
      t=data[pos];pos+=1
      if t in (0x70,0x6f):body.append(0x63);expanded+=1
      body.append(t)
   assert pos==end
  out.append(sid);out+=leb(len(body));out+=body
 assert expanded
 return out,expanded

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
 cases=[]
 for name,wat in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);short=a.output/(name+'-short.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(short)],check=True)
  data,count=expand(short.read_bytes());path=a.output/(name+'.wasm');path.write_bytes(data);cases.append((name,path,count))
 configs=[]
 if a.uwvm:
  configs=[(name,[x for x in command[:-1] if x!='-WFE-multi-memory']) for name,command in configurations(a.uwvm.resolve(),a.ros)];configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 assert configs;rows=[]
 for label,base in configs:
  for name,path,count in cases:
   command=base+(['-W','function-references=y'] if label=='wasmtime' else ['-WFE-function-references','--run'])+[str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log)
   rows.append(dict(config=label,case=name,expanded_types=count,exit=r.returncode,command=command));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert r.returncode==0,(label,name,log)
  print('PASS',label,flush=True)
 if a.uwvm:
  path=a.output/'identity.wasm';gates=[]
  for label,flags in [('default',[]),('disable',['-WFD-function-references']),('unrelated',['-WFE-tail-call']),('references-disabled',['-WFE-function-references','-WFD-reference-types']),('conflict',['-WFE-function-references','-WFD-function-references']),('legacy',['--wasm-feature-wasm2','-WFE-function-references'])]:
   command=[str(a.uwvm),'-m','validation',*flags,'--run',str(path)];r=subprocess.run(command,capture_output=True);log=r.stdout+r.stderr;(a.output/('gate-'+label+'.log')).write_bytes(log);assert r.returncode!=0,(label,log);gates.append(dict(case=label,exit=r.returncode,command=command))
  (a.output/'gates.json').write_text(json.dumps(gates,indent=2)+'\n');cache=a.output/'cache';cache.mkdir();rows_cache=[]
  for phase,flag in [('cold','-WFE-function-references'),('warm','-WFE-function-references'),('disabled','-WFD-function-references')]:
   compile_log=a.output/('cache-'+phase+'.compile.log');command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),flag,'--run',str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);(a.output/('cache-'+phase+'.log')).write_bytes(r.stdout+r.stderr);hit=compile_log.exists() and 'object-cache-hit' in compile_log.read_text();assert (r.returncode==0)==(phase!='disabled');assert phase!='warm' or hit;assert phase!='disabled' or not hit;rows_cache.append(dict(phase=phase,exit=r.returncode,hit=hit))
  (a.output/'cache.json').write_text(json.dumps(rows_cache,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n')
 print('PASS explicit function signatures:',len(rows),'runs',flush=True)
if __name__=='__main__':main()
