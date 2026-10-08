#!/usr/bin/env python3
"""Latest Core 3 try_table syntax: zero-handler execution, structural typing, precise feature gates.
Nonempty handler execution is qualified separately once backend exceptional edges are implemented.
"""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
from run_external_value_cli import leb
CASES={
 'empty':'(module (func (export "_start") try_table end))',
 'result':'(module (func (export "_start") try_table (result i32) i32.const 42 end i32.const 42 i32.ne if unreachable end))',
 'multi':'(module (func (export "_start") try_table (result i32 i64) i32.const 42 i64.const 7 end i64.const 7 i64.ne if unreachable end i32.const 42 i32.ne if unreachable end))',
 'params':'(module (func (export "_start") i32.const 41 try_table (param i32) (result i32) i32.const 1 i32.add end i32.const 42 i32.ne if unreachable end))',
 'nested':'(module (func (export "_start") try_table (result i32) try_table (result i32) i32.const 42 br 1 end end i32.const 42 i32.ne if unreachable end))',
 'outer-branch':'(module (func (export "_start") block (result i32) try_table i32.const 42 br 1 end unreachable end i32.const 42 i32.ne if unreachable end))',
 'unreachable-valid':'(module (func (export "_start") block br 0 try_table (result i32) unreachable end drop end))',
 'reference':'(module (func (export "_start") try_table (result (ref null extern)) ref.null extern end ref.is_null i32.eqz if unreachable end))',
 'call':'(module (func $f (result i32) i32.const 42) (func (export "_start") try_table (result i32) call $f end i32.const 42 i32.ne if unreachable end))',
 'memory':'(module (memory 1) (func (export "_start") try_table i32.const 65532 i32.const 42 i32.store i32.const 65532 i32.load i32.const 42 i32.ne if unreachable end end))',
 'memory-loop':'(module (memory 1) (func (export "_start") (local i32) try_table i32.const 1000 local.set 0 loop i32.const 0 local.get 0 i32.store local.get 0 i32.const 1 i32.sub local.tee 0 br_if 0 end end i32.const 0 i32.load i32.const 1 i32.ne if unreachable end))',
 'trap':'(module (func (export "_start") try_table unreachable end))',
 'memory-trap':'(module (memory 1) (func (export "_start") try_table i32.const 65536 i32.load drop end))',
 'bad-result':'(module (func (export "_start") try_table (result i64) i32.const 1 end drop))',
 'bad-else':'(module (func (export "_start") try_table else end))',
}
def sec(i,b):return bytes([i])+leb(len(b))+b
def binary(body):
 body=b'\0'+body
 return b'\0asm\1\0\0\0'+sec(1,b'\1\x60\0\0')+sec(3,b'\1\0')+sec(7,b'\1\6_start\0\0')+sec(10,b'\1'+leb(len(body))+body)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 for name,wat in CASES.items():
  src=a.output/(name+'.wat');src.write_text(wat);path=a.output/(name+'.wasm')
  if name=='bad-else':path.write_bytes(binary(b'\x1f\x40\0\x05\x0b\x0b'))
  else:subprocess.run([str(a.wasm_tools),'parse',str(src),'-o',str(path)],check=True)
  cases.append((name,not name.startswith('bad-'),name.endswith('trap'),path))
  if name in ('memory','memory-loop'):
   src=a.output/(name+'-short.wat');src.write_text(wat.replace('try_table','block'));subprocess.run([str(a.wasm_tools),'parse',str(src),'-o',str(a.output/(name+'-short.wasm'))],check=True)
 for name,body,valid in [
  ('padded-zero',b'\x1f\x40\x80\x80\x80\x80\0\x0b\x0b',True),
  ('bad-count',b'\x1f\x40\xff\xff\xff\xff\x0f\x0b\x0b',False),
  ('bad-kind',b'\x1f\x40\1\x80\0\0\x0b\x0b',False),
  ('bad-missing-count',b'\x1f\x40',False),
  ('bad-overflow-count',b'\x1f\x40\x80\x80\x80\x80\x10\x0b\x0b',False),
  ('bad-missing-end',b'\x1f\x40\0\x0b',False)]:
  path=a.output/(name+'.wasm');path.write_bytes(binary(body));cases.append((name,valid,False,path))
 configs=[]
 if a.uwvm:configs=[(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm.resolve(),a.ros)]+[('validator',[str(a.uwvm.resolve()),'-m','validation'])]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 rows=[]
 for label,base in configs:
  for name,valid,traps,path in cases:
   cmd=base+(['-W','exceptions=y'] if label=='wasmtime' else ['-WFE-exceptions','-WFE-function-references'])+([] if label=='wasmtime' else ['--run'])+[str(path)]
   r=subprocess.run(cmd,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log);rows.append(dict(configuration=label,case=name,valid=valid,traps=traps,exit=r.returncode,command=cmd));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');expect=valid and (not traps or label=='validator');assert (r.returncode==0)==expect,(label,name,log)
  print('PASS',label,flush=True)
 if a.uwvm:
  # The untouched complete legacy selector reports unknown opcodes; scoped flags must name exceptions.
  for label,base in configs:
   for flags in ([],['-WFD-exceptions'],['-WFE-function-references']):
    r=subprocess.run(base+flags+['--run',str(a.output/'empty.wasm')],capture_output=True,timeout=30);assert r.returncode!=0 and (b'--wasm-feature-enable-exceptions' in r.stdout+r.stderr or (not flags and b'Illegal opcode base' in r.stdout+r.stderr)),(label,flags,r.stdout+r.stderr)
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS Core 3 zero-handler try_table:',len(rows),'runs')
if __name__=='__main__':main()
