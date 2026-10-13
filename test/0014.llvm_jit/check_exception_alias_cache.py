#!/usr/bin/env python3
"""Change imported tag aliasing under an unchanged importer, with cold/warm persistent JIT objects."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--wasmtime',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ros',action='store_true');p.add_argument('--cross-function',action='store_true',help='Throw from the imported provider so native propagation and runtime tag matching are exercised');a=p.parse_args()
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
def encode(name,wat):
 s=a.out/(name+'.wat');s.write_text(wat);o=a.out/(name+'.wasm');subprocess.run([str(a.wasm_tools),'parse',str(s),'-o',str(o)],check=True);return o
raise_function='(func (export "raise") (param i32) local.get 0 throw $b)' if a.cross_function else ''
providers={
 'alias':encode('alias','(module (tag $b (export "a") (export "b") (param i32)) (func (export "expected") (result i32) i32.const 42) '+raise_function+')'),
 'distinct':encode('distinct','(module (tag (export "a") (param i32)) (tag $b (export "b") (param i32)) (func (export "expected") (result i32) i32.const 41) '+raise_function+')')}
throw_import='(import "p" "raise" (func $raise (param i32)))' if a.cross_function else ''
throw_instruction='call $raise' if a.cross_function else 'throw $b'
importer=encode('importer',f'''(module (import "p" "a" (tag $a (param i32))) (import "p" "b" (tag $b (param i32))) (import "p" "expected" (func $expected (result i32))) {throw_import}
 (func (export "_start") block $outer (result i32) block $inner (result i32) try_table (catch $a $inner) (catch $b $outer) i32.const 41 {throw_instruction} end unreachable end i32.const 1 i32.add end call $expected i32.ne if unreachable end))''')
for name,provider in providers.items():
 c=[str(a.wasmtime),'-C','cache=n','-W','exceptions=y','--preload','p='+str(provider),str(importer)];r=subprocess.run(c,capture_output=True,timeout=30);(a.out/('wasmtime-'+name+'.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0
rows=[]
for mode in (('full',) if a.ros else ('full','lazy','lazy+verification')):
 for policy in ('instruction','unwind'):
  cache=a.out/(mode+'-'+policy+'-cache');cache.mkdir()
  for step,name in enumerate(('alias','alias','distinct','distinct','alias')):
   log=a.out/f'{mode}-{policy}-{step}.compile.log';command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm',mode])+['-WFE-exceptions','-Rllvm-call-stack',policy,'-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(log),'-Wpre',str(providers[name]),'p','--run',str(importer)]
   r=subprocess.run(command,capture_output=True,timeout=60);(a.out/f'{mode}-{policy}-{step}.log').write_bytes(r.stdout+r.stderr);hit=log.exists() and 'object-cache-hit' in log.read_text();rows.append(dict(mode=mode,policy=policy,step=step,provider=name,exit=r.returncode,cache_hit=hit,command=command));(a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert r.returncode==0,(mode,policy,step,r.stderr)
   assert step not in (1,3,4) or hit,(mode,policy,step,'expected warm cache hit')
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,cross_function=a.cross_function,runs=len(rows),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),importer_sha256=hashlib.sha256(importer.read_bytes()).hexdigest()),indent=2)+'\n')
print('PASS',len(rows),'cross-function' if a.cross_function else 'local', 'tag alias topology cold/warm cache executions')
