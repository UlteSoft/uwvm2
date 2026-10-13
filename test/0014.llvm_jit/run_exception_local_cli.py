#!/usr/bin/env python3
"""Core 3 locally caught throw execution (not cross-function native exception qualification)."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CHECK='i32.const 42 i32.ne if unreachable end'
# Every value is checked after the exceptional edge. Interleaving 32 values of each
# scalar register class and v128 exceeds the ring capacity and exercises spill,
# tuple relocation, and recovery while preserving an older outer-stack value.
def large_payload_case():
 payload=[]
 for i in range(32):
  payload.extend([
   ('i32',f'i32.const {1000+i}',f'i32.const {1000+i} i32.ne if unreachable end'),
   ('i64',f'i64.const {9223372036854775000+i}',f'i64.const {9223372036854775000+i} i64.ne if unreachable end'),
   ('f32',f'f32.const {i}.5',f'f32.const {i}.5 f32.ne if unreachable end'),
   ('f64',f'f64.const {i}.25',f'f64.const {i}.25 f64.ne if unreachable end'),
   ('v128',f'v128.const i32x4 {i} {i+1} {2000+i} {i+3}',f'i32x4.extract_lane 2 i32.const {2000+i} i32.ne if unreachable end'),
  ])
 types=' '.join(x[0] for x in payload)
 values=' '.join(x[1] for x in payload)
 checks=' '.join(x[2] for x in reversed(payload))
 return f'(module (tag $t (param {types})) (func (export "_start") i32.const 42 block $out (result {types}) try_table (catch $t $out) f64.const -99 {values} throw $t end unreachable end {checks} {CHECK}))'

CASES={
 'large-payload':large_payload_case(),
 # The earlier branch executes; the subsequent caught throw must still validate
 # correctly with absent or partially materialized polymorphic payload operands.
 'dead-caught-missing':f'(module (tag $t (param i32 i64)) (func (export "_start") i32.const 42 block $out (result i32 i64) try_table (catch $t $out) i32.const 42 i64.const 99 br $out throw $t end unreachable end i64.const 99 i64.ne if unreachable end {CHECK} {CHECK}))',
 'dead-caught-partial':f'(module (tag $t (param i32 i64)) (func (export "_start") i32.const 42 block $out (result i32 i64) try_table (catch $t $out) i32.const 42 i64.const 99 br $out i64.const -7 throw $t end unreachable end i64.const 99 i64.ne if unreachable end {CHECK} {CHECK}))',
 'bad-dead-caught-partial':'(module (tag $t (param i32 i64)) (func (export "_start") block $out (result i32 i64) try_table (catch $t $out) i32.const 42 i64.const 99 br $out i32.const -7 throw $t end unreachable end drop drop))',

 'scalar':f'(module (tag $t (param i32)) (func (export "_start") block $out (result i32) try_table (catch $t $out) i32.const 42 throw $t end unreachable end {CHECK}))',
 'pair':f'(module (tag $t (param i32 i64)) (func (export "_start") block $out (result i32 i64) try_table (catch $t $out) i32.const 42 i64.const 9223372036854775807 throw $t end unreachable end i64.const 9223372036854775807 i64.ne if unreachable end {CHECK}))',
 'float-bits':'(module (tag $t (param f32 f64)) (func (export "_start") block $out (result f32 f64) try_table (catch $t $out) f32.const -0 f64.const nan:0x1234 throw $t end unreachable end i64.reinterpret_f64 i64.const 0x7ff0000000001234 i64.ne if unreachable end i32.reinterpret_f32 i32.const 0x80000000 i32.ne if unreachable end))',
 'vector':'(module (tag $t (param v128)) (func (export "_start") block $out (result v128) try_table (catch $t $out) v128.const i32x4 1 2 42 4 throw $t end unreachable end i32x4.extract_lane 2 '+CHECK+'))',
 'references':'(module (tag $t (param funcref externref)) (func (export "_start") block $out (result funcref externref) try_table (catch $t $out) ref.null func ref.null extern throw $t end unreachable end ref.is_null i32.eqz if unreachable end ref.is_null i32.eqz if unreachable end))',
 'function-label':f'(module (tag $t (param i32)) (func $f (result i32) try_table (catch $t 0) i32.const 42 throw $t end unreachable) (func (export "_start") call $f {CHECK}))',
 'try-results':f'(module (tag $t (param i32)) (func $f (param i32) (result i32) block $out (result i32) try_table (result i32) (catch $t $out) local.get 0 if i32.const 42 throw $t end i32.const 42 end end) (func (export "_start") i32.const 0 call $f {CHECK} i32.const 1 call $f {CHECK}))',
 'loop-all':'(module (tag $t (param i32)) (func (export "_start") (local i32) i32.const 5 local.set 0 loop $again try_table (catch_all $again) local.get 0 if local.get 0 i32.const 1 i32.sub local.tee 0 throw $t end end end local.get 0 if unreachable end))',
 'bad-tag':'(module (tag $t (param i32)) (func (export "_start") block $out (result i32) try_table (catch $t $out) i32.const 42 throw 42 end unreachable end drop))',
 'bad-catch-tag':'(module (func (export "_start") try_table (catch 42 0) end))',
 'bad-catch-label':'(module (tag $t) (func (export "_start") try_table (catch $t 42) end))',
 'bad-dead-payload':'(module (tag $t (param i64)) (func (export "_start") unreachable i32.const 0 throw $t))',
 'empty':'(module (tag $t) (func (export "_start") block $out try_table (catch $t $out) throw $t end unreachable end))',
 'all-discards':f'(module (tag $t (param i32 i64)) (func (export "_start") i32.const 42 block $out try_table (catch_all $out) f64.const 9 i32.const 12 i64.const 99 throw $t end unreachable end {CHECK}))',
 'distinct-tags':f'(module (tag $a (param i32)) (tag $b (param i32)) (func (export "_start") block $outer (result i32) try_table (catch $a $outer) block $inner (result i32) try_table (catch $b $inner) i32.const 42 throw $a end unreachable end drop unreachable end unreachable end {CHECK}))',
 'nearest':f'(module (tag $t (param i32)) (func (export "_start") block $outer (result i32) try_table (catch $t $outer) block $inner (result i32) try_table (catch $t $inner) i32.const 41 throw $t end unreachable end i32.const 1 i32.add br $outer end unreachable end {CHECK}))',
 'first-match':f'(module (tag $t (param i32)) (func (export "_start") block $outer (result i32) block $inner (result i32) try_table (catch $t $inner) (catch $t $outer) i32.const 41 throw $t end unreachable end i32.const 1 i32.add end {CHECK}))',
 'all-first':'(module (tag $t) (func (export "_start") (local i32) block $outer block $inner try_table (catch_all $inner) (catch $t $outer) throw $t end unreachable end i32.const 42 local.set 0 end local.get 0 '+CHECK+'))',
 'conditional':f'(module (tag $t (param i32)) (func $f (param i32) (result i32) block $out (result i32) try_table (catch $t $out) local.get 0 if i32.const 42 throw $t end i32.const 42 br $out end unreachable end) (func (export "_start") i32.const 0 call $f {CHECK} i32.const 1 call $f {CHECK}))',
 'memory':f'(module (tag $t (param i32)) (memory 1) (func (export "_start") block $out (result i32) try_table (catch $t $out) i32.const 65532 i32.const 42 i32.store i32.const 65532 i32.load throw $t end unreachable end {CHECK}))',
 'memory-loop':f'(module (tag $t (param i32)) (memory 1) (func (export "_start") (local i32) i32.const 1000 local.set 0 loop $again block $out (result i32) try_table (catch $t $out) i32.const 0 local.get 0 i32.store i32.const 0 i32.load throw $t end unreachable end local.set 0 local.get 0 i32.const 1 i32.sub local.tee 0 br_if $again end i32.const 0 i32.load i32.const 1 i32.ne if unreachable end))',
 'catch-loop-param':f'(module (tag $t (param i32)) (func (export "_start") (local i32) i32.const 5 loop $again (param i32) (result i32) local.tee 0 if try_table (catch $t $again) local.get 0 i32.const 1 i32.sub throw $t end end i32.const 42 end {CHECK}))',
 'side-effect':f'(module (tag $t (param i32)) (global $g (mut i32) (i32.const 0)) (func (export "_start") block $out (result i32) try_table (catch $t $out) i32.const 42 global.set $g global.get $g throw $t end unreachable end {CHECK} global.get $g {CHECK}))',
 'exited-handler':f'(module (tag $t (param i32)) (func (export "_start") block $outer (result i32) try_table (catch $t $outer) block $inner (result i32) try_table (catch $t $inner) i32.const 99 br $inner end unreachable end drop i32.const 42 throw $t end unreachable end {CHECK}))',
 'dead-throw':'(module (tag $t (param i32)) (func (export "_start") block br 0 throw $t end))',
 'trap':'(module (func (export "_start") block $out try_table (catch_all $out) unreachable end end))',
 'memory-trap':'(module (memory 1) (func (export "_start") block $out try_table (catch_all $out) i32.const 65536 i32.load drop end end))',
 'bad-payload':'(module (tag $t (param i64)) (func (export "_start") block $out (result i64) try_table (catch $t $out) i32.const 42 throw $t end unreachable end drop))',
 'bad-label-type':'(module (tag $t (param i32)) (func (export "_start") block $out (result i64) try_table (catch $t $out) i32.const 42 throw $t end unreachable end drop))',
 'bad-payload-order':'(module (tag $t (param i32 i64)) (func (export "_start") block $out (result i32 i64) try_table (catch $t $out) i64.const 1 i32.const 2 throw $t end unreachable end drop drop))',
 'bad-all-label':'(module (func (export "_start") block $out (result i32) try_table (catch_all $out) unreachable end unreachable end drop))',
 'bad-underflow':'(module (tag $t (param i32)) (func (export "_start") block $out (result i32) try_table (catch $t $out) throw $t end unreachable end drop))',
 'import-alias':f'(module (import "p" "t" (tag $a (param i32))) (import "p" "t" (tag $b (param i32))) (tag $local (param i32)) (func (export "_start") block $outer (result i32) try_table (catch $a $outer) block $inner (result i32) try_table (catch $local $inner) i32.const 42 throw $b end unreachable end drop unreachable end unreachable end {CHECK}))',
}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
 def encode(name,wat):
  src=a.out/(name+'.wat');src.write_text(wat);wasm=a.out/(name+'.wasm');subprocess.run([str(a.wasm_tools),'parse',str(src),'-o',str(wasm)],check=True);return wasm
 provider=encode('provider','(module (tag (export "t") (param i32)))')
 for name,wat in CASES.items():
  encode(name,wat)
  if name in ('memory','memory-loop'):encode(name+'-short',wat.replace('try_table (catch $t $out)','block').replace('throw $t','br $out'))
 configs=[]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime),'-C','cache=n','-W','exceptions=y']))
 if a.uwvm:configs += [(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm,a.ros)]+[('validator',[str(a.uwvm),'-m','validation'])]
 runs=[]
 for label,base in configs:
  for name in CASES:
   valid=not name.startswith('bad-');traps=name.endswith('trap');path=a.out/(name+'.wasm')
   preload=(['--preload','p='+str(provider)] if label=='wasmtime' else ['-Wpre',str(provider),'p']) if name.startswith('import') else []
   cmd=base+([] if label=='wasmtime' else ['-WFE-exceptions'])+preload+([] if label=='wasmtime' else ['--run'])+[str(path)]
   r=subprocess.run(cmd,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.out/(label+'-'+name+'.log')).write_bytes(log);runs.append(dict(configuration=label,case=name,command=cmd,exit=r.returncode));(a.out/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
   assert (r.returncode==0)==(valid and (not traps or label=='validator')),(label,name,log)
  print('PASS',label,len(CASES),'local exception cases',flush=True)
 if a.uwvm:
  for label,base in configs:
   if label=='wasmtime':continue
   for name in ('scalar','empty','all-first'):
    for flags in (['-WFD-exceptions'],['-WFE-function-references']):
     cmd=base+flags+['--run',str(a.out/(name+'.wasm'))];r=subprocess.run(cmd,capture_output=True,timeout=30);assert r.returncode!=0 and b'--wasm-feature-enable-exceptions' in r.stdout+r.stderr,(label,name,flags)
 (a.out/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(CASES),runs=len(runs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS Core 3 local exception execution:',len(runs),'runs')
if __name__=='__main__':main()
