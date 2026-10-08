#!/usr/bin/env python3
"""Execute new ref.as_non_null syntax through actual VM modes and independent feature gates.

Typed heap declarations/call_ref are tested by the shared Core 3 typing fixtures;
this execution slice uses the currently integrated funcref/externref declarations.
Do not describe it as complete function-references or official WAST conformance.
"""
import argparse, hashlib, json, resource, subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':('success','''(module (func $f) (elem declare func $f)
  (func $identity (param funcref) (result funcref) local.get 0 ref.as_non_null ref.as_non_null)
  (func (export "_start") (local funcref)
   ref.func $f call $identity local.tee 0 ref.is_null if unreachable end
   i32.const 19 local.get 0 ref.as_non_null drop i32.const 23 i32.add i32.const 42 i32.ne if unreachable end))'''),
 'hot-loop':('success','''(module (func $f) (elem declare func $f)
  (func $identity (param funcref) (result funcref) local.get 0 ref.as_non_null)
  (func (export "_start") (local funcref i32)
   ref.func $f local.set 0 i32.const 20000 local.set 1
   loop local.get 0 call $identity ref.is_null if unreachable end
    local.get 1 i32.const 1 i32.sub local.tee 1 br_if 0 end))'''),
 'block':('success','''(module (func $f) (elem declare func $f)
  (func (export "_start") block (result funcref) ref.func $f ref.as_non_null br 0 end ref.is_null if unreachable end))'''),
 'select':('success','''(module (func $f) (elem declare func $f)
  (func (export "_start") ref.func $f ref.as_non_null ref.null func i32.const 1 select (result funcref) ref.is_null if unreachable end))'''),
 'null-func':('null reference','(module (func $checked (param funcref) local.get 0 ref.as_non_null drop) (func (export "_start") ref.null func call $checked nop))'),
 'null-extern':('null reference','(module (func $checked (param externref) local.get 0 ref.as_non_null drop) (func (export "_start") ref.null extern call $checked nop))'),
 'dead-bottom-func':('success','(module (func (result funcref) unreachable ref.as_non_null) (func (export "_start")))'),
 'dead-bottom-extern':('success','(module (func (result externref) unreachable ref.as_non_null) (func (export "_start")))'),
 'dead-bottom-local':('success','(module (func (result externref) (local externref) unreachable ref.as_non_null local.tee 0) (func (export "_start")))'),
 'dead-bottom-select':('success','(module (func (result externref) unreachable ref.as_non_null i32.const 0 select (result externref)) (func (export "_start")))'),
 'bad-number':('validation','(module (func (export "_start") i32.const 0 ref.as_non_null drop))'),
 'bad-underflow':('validation','(module (func (export "_start") ref.as_non_null drop))'),
 'bad-bottom-number':('validation','(module (func (export "_start") unreachable ref.as_non_null i32.eqz drop))'),
 'bad-bottom-local':('validation','(module (func (export "_start") (local externref) unreachable ref.as_non_null local.tee 0 i32.eqz drop))'),
 'bad-untyped-select':('validation','(module (func (export "_start") unreachable ref.as_non_null i32.const 0 select drop))'),
 'bad-typed-select':('validation','(module (func (export "_start") unreachable ref.as_non_null i32.const 0 select (result i32) drop))'),
}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');p.add_argument('--only');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 for name,(outcome,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);path=source.with_suffix('.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(path)],check=True);cases.append((name,outcome,path))
 configs=[]
 if a.uwvm:
  configs=[(label,[x for x in command[:-1] if x!='-WFE-multi-memory']) for label,command in configurations(a.uwvm.resolve(),a.ros)]
  configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 if a.only:configs=[c for c in configs if c[0]==a.only]
 assert configs;rows=[]
 for label,base in configs:
  for name,outcome,path in cases:
   command=base+(['-W','function-references=y'] if label=='wasmtime' else ['-WFE-function-references','--run'])+[str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log)
   success=outcome=='success' or (label=='validator' and outcome!='validation');passed=(r.returncode==0)==success
   if not success:
    if outcome=='validation':passed=passed and any(x in log.lower() for x in [b'validat',b'type mismatch',b'invalid input webassembly code'])
    else:
     passed=passed and b'null reference' in log.lower()
     if label!='wasmtime':passed=passed and b'Call stack:' in log
   rows.append(dict(configuration=label,case=name,exit=r.returncode,passed=passed,command=command));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
   if not passed:raise RuntimeError(label+' '+name+'\n'+log.decode(errors='replace'))
  print('PASS',label,flush=True)
 if a.uwvm and not a.only:
  executable=str(a.uwvm.resolve());path=a.output/'identity.wasm';gate_rows=[]
  for name,flags in [('default',[]),('disabled',['-WFD-function-references']),('unrelated',['-WFE-tail-call']),('conflict',['-WFE-function-references','-WFD-function-references']),('legacy-conflict',['--wasm-feature-wasm2','-WFE-function-references'])]:
   c=[executable,'-m','validation',*flags,'--run',str(path)];r=subprocess.run(c,capture_output=True);log=r.stdout+r.stderr;(a.output/('gate-'+name+'.log')).write_bytes(log);assert r.returncode!=0,(name,log)
   if name=='disabled':assert b'WebAssembly 3.0' in log and b'--wasm-feature-enable-function-references' in log
   if 'conflict' in name:assert b'conflict' in log.lower(),log
   gate_rows.append(dict(case=name,exit=r.returncode,command=c))
  (a.output/'gates.json').write_text(json.dumps(gate_rows,indent=2)+'\n')
  cache=a.output/'cache';cache.mkdir();cache_rows=[]
  for phase,switch in [('cold','-WFE-function-references'),('warm','-WFE-function-references'),('disabled','-WFD-function-references')]:
   compile_log=a.output/('cache-'+phase+'.compile.log');c=[executable]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),switch,'--run',str(path)]
   r=subprocess.run(c,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/('cache-'+phase+'.log')).write_bytes(log);hit=compile_log.exists() and 'object-cache-hit' in compile_log.read_text()
   assert (r.returncode==0)==(phase!='disabled'),log
   assert phase!='warm' or hit
   assert phase!='disabled' or (not hit and b'--wasm-feature-enable-function-references' in log)
   cache_rows.append(dict(phase=phase,exit=r.returncode,hit=hit))
  (a.output/'cache.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n')
 print('PASS ref.as_non_null CLI',len(rows),'runs',flush=True)
if __name__=='__main__':main()
