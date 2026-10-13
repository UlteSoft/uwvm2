#!/usr/bin/env python3
"""Core 3 tag declarations, import/export index space, feature gates and cache policy.

This qualifies declarations and instantiation. It does not claim try_table/throw execution.
"""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
from run_external_value_cli import leb,Reader
CASES={
 'empty-payload':'(module (tag (param)) (func (export "_start")))',
 'numeric-payload':'(module (tag (param i32 i64 f32 f64)) (func (export "_start") i64.const 42 i64.const 42 i64.ne if unreachable end))',
 'vector-payload':'(module (tag (param v128)) (func (export "_start") v128.const i32x4 1 2 3 4 i32x4.extract_lane 2 i32.const 3 i32.ne if unreachable end))',
 'reference-payload':'(module (tag (param funcref externref)) (func (export "_start") ref.null extern ref.is_null i32.eqz if unreachable end))',
 'same-signatures':'(module (type $t (func (param i32))) (tag (export "a") (type $t)) (tag (export "b") (type $t)) (func (export "_start")))',
 'unused-index':'(module (type (func (result i32))) (type $t (func (param i64))) (tag (type $t)) (func (export "_start")))',
 'memory':'(module (tag (param i32)) (memory 1) (func (export "_start") i32.const 65532 i32.const 42 i32.store i32.const 65532 i32.load i32.const 42 i32.ne if unreachable end))',
 'memory-loop':'(module (tag (param i32)) (memory 1) (func (export "_start") (local i32) i32.const 1000 local.set 0 loop i32.const 0 local.get 0 i32.store local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end i32.const 0 i32.load i32.const 1 i32.ne if unreachable end))',
 'import':'(module (import "p" "t" (tag (param i32 i64))) (func (export "_start")))',
 'import-alias':'(module (import "p" "t" (tag $a (param i32 i64))) (import "p" "t" (tag $b (param i32 i64))) (export "a" (tag $a)) (export "b" (tag $b)) (func (export "_start")))',
 'import-local':'(module (import "p" "t" (tag (param i32 i64))) (tag $local (param i32 i64)) (export "local" (tag $local)) (func (export "_start")))',
}
def section(i,b):return bytes([i])+leb(len(b))+b
def module(tags,types=b'\1\x60\0\0'):return b'\0asm\1\0\0\0'+section(1,types)+section(13,tags)
def strip_tags(data):
 r=Reader(data);out=bytearray(r.take(8))
 while r.pos<len(data):
  sid=r.byte();body=r.take(r.integer())
  if sid!=13:out+=section(sid,body)
 return bytes(out)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
 provider=a.output/'provider.wasm';w=a.output/'provider.wat';w.write_text('(module (tag (export "t") (param i32 i64)))');subprocess.run([str(a.wasm_tools),'parse',str(w),'-o',str(provider)],check=True)
 cases=[]
 for name,wat in CASES.items():
  src=a.output/(name+'.wat');src.write_text(wat);path=a.output/(name+'.wasm');subprocess.run([str(a.wasm_tools),'parse',str(src),'-o',str(path)],check=True);cases.append((name,True,path))
  if name in ('memory','memory-loop'):(a.output/(name+'-short.wasm')).write_bytes(strip_tags(path.read_bytes()))
 binary={
  'empty-section':(True,module(b'\0')+section(3,b'\0')), # corrected below: section 3 must precede tag.
  'bad-attribute':(False,module(b'\1\1\0')),
  'bad-type-index':(False,module(b'\1\0\1')),
  'bad-results':(False,module(b'\1\0\0',b'\1\x60\0\1\x7f')),
  'truncated-count':(False,module(b'\x80')),
  'truncated-type':(False,module(b'\1\0\x80')),
  'overflow-type':(False,module(b'\1\0\x80\x80\x80\x80\x10')),
  'duplicate-section':(False,module(b'\0')+section(13,b'\0')),
  'trailing-data':(False,module(b'\0\0')),
  'wrong-order':(False,b'\0asm\1\0\0\0'+section(1,b'\1\x60\0\0')+section(6,b'\0')+section(13,b'\1\0\0')),
 }
 # Add an empty tag vector to an executable module without changing the function index space.
 r=Reader((a.output/'empty-payload.wasm').read_bytes());empty=bytearray(r.take(8))
 while r.pos<len(r.data):
  sid=r.byte();body=r.take(r.integer());empty+=section(sid,b'\0' if sid==13 else body)
 binary['empty-section']=(True,bytes(empty))
 for name,(valid,data) in binary.items():path=a.output/(name+'.wasm');path.write_bytes(data);cases.append((name,valid,path))
 configs=[]
 if a.uwvm:configs=[(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm.resolve(),a.ros)]+[('validator',[str(a.uwvm.resolve()),'-m','validation'])]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 rows=[]
 for label,base in configs:
  for name,valid,path in cases:
   preload=(['--preload','p='+str(provider)] if label=='wasmtime' else ['-Wpre',str(provider),'p']) if name.startswith('import') else []
   command=base+(['-W','exceptions=y'] if label=='wasmtime' else ['-WFE-exceptions'])+preload+([] if label=='wasmtime' else ['--run'])+[str(path)]
   result=subprocess.run(command,capture_output=True,timeout=60);log=result.stdout+result.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log);rows.append(dict(configuration=label,case=name,valid=valid,exit=result.returncode,command=command));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert (result.returncode==0)==valid,(label,name,log)
  print('PASS',label,flush=True)
 if a.uwvm:
  for case in ('empty-section','empty-payload','import'):
   preload=['-Wpre',str(provider),'p'] if case=='import' else []
   for label,flags in [('default',[]),('disabled',['-WFD-exceptions']),('unrelated',['-WFE-function-references'])]:
    command=[str(a.uwvm),'-m','validation',*preload,*flags,'--run',str(a.output/(case+'.wasm'))];r=subprocess.run(command,capture_output=True,timeout=30);log=r.stdout+r.stderr;(a.output/('gate-'+case+'-'+label+'.log')).write_bytes(log);assert r.returncode!=0 and b'--wasm-feature-enable-exceptions' in log,(case,label,log)
  r=subprocess.run([str(a.uwvm),'-WFE-exceptions','-WFD-exceptions','--run',str(a.output/'memory.wasm')],capture_output=True);assert r.returncode!=0 and b'conflicts' in r.stdout+r.stderr
  for case in ('same-signatures','import-alias'):
   command=[str(a.uwvm),'-m','section-details','-WFE-exceptions','--run',str(a.output/(case+'.wasm'))];r=subprocess.run(command,capture_output=True);log=r.stdout+r.stderr;(a.output/('details-'+case+'.log')).write_bytes(log);assert r.returncode==0 and b'tag[' in log,(case,log)
  cache=a.output/'cache';cache.mkdir();cache_rows=[]
  for phase,flag in [('cold','-WFE-exceptions'),('warm','-WFE-exceptions'),('disabled','-WFD-exceptions')]:
   compile_log=a.output/('cache-'+phase+'.compile.log');command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),flag,'--run',str(a.output/'memory.wasm')];r=subprocess.run(command,capture_output=True,timeout=60);(a.output/('cache-'+phase+'.log')).write_bytes(r.stdout+r.stderr);hit=compile_log.exists() and 'object-cache-hit' in compile_log.read_text();assert (r.returncode==0)==(phase!='disabled');assert phase!='warm' or hit;assert phase!='disabled' or not hit;cache_rows.append(dict(phase=phase,exit=r.returncode,hit=hit))
  (a.output/'cache.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS Core 3 tag declarations:',len(rows),'runs')
if __name__=='__main__':main()
