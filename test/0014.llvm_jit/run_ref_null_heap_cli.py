#!/usr/bin/env python3
"""Core 3 ref.null concrete function heaps through execution, initialization and feature gates."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
CASES={
 'identity':(True,[],'''(module (type $t (func)) (func (export "_start") ref.null $t ref.is_null i32.eqz if unreachable end))'''),
 'global':(True,[],'''(module (type $t (func)) (global $g funcref (ref.null $t)) (func (export "_start") global.get $g ref.is_null i32.eqz if unreachable end))'''),
 'table-initializer':(True,['-WFE-table-initializer'],'''(module (type $t (func)) (table 2 funcref (ref.null $t)) (func (export "_start") i32.const 1 table.get 0 ref.is_null i32.eqz if unreachable end))'''),
 'table64-initializer':(True,['-WFE-table-initializer','-WFE-table64'],'''(module (type $t (func)) (table i64 2 funcref (ref.null $t)) (func (export "_start") i64.const 1 table.get 0 ref.is_null i32.eqz if unreachable end))'''),
 'active-element':(True,[],'''(module (type $t (func)) (table 1 funcref) (elem (i32.const 0) funcref (ref.null $t)) (func (export "_start") i32.const 0 table.get 0 ref.is_null i32.eqz if unreachable end))'''),
 'passive-element':(True,[],'''(module (type $t (func)) (table 1 funcref) (elem $e funcref (ref.null $t)) (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $e i32.const 0 table.get 0 ref.is_null i32.eqz if unreachable end))'''),
 'declared-element':(True,[],'''(module (type $t (func)) (elem declare funcref (ref.null $t)) (func (export "_start")))'''),
 'extended-global':(True,['-WFE-extended-const'],'''(module (type $t (func)) (global $a funcref (ref.null $t)) (global $b funcref (global.get $a)) (func (export "_start") global.get $b ref.is_null i32.eqz if unreachable end))'''),
 'multi-byte':(True,[],'''(module '''+''.join('(type $t%d (func))'%i for i in range(129))+'''(func (export "_start") ref.null 128 ref.is_null i32.eqz if unreachable end ref.null 11 ref.is_null i32.eqz if unreachable end))'''),
 'dead-index':(False,[],'''(module (func (export "_start") unreachable ref.null 999 drop))'''),
 'bad-global-heap':(False,[],'''(module (global funcref (ref.null 999)) (func (export "_start")))'''),
 'bad-global-hierarchy':(False,[],'''(module (type $t (func)) (global externref (ref.null $t)) (func (export "_start")))'''),
 'bad-element-hierarchy':(False,[],'''(module (type $t (func)) (elem externref (ref.null $t)) (func (export "_start")))'''),
 'bad-result':(False,[],'''(module (type $t (func)) (func $bad (result i32) ref.null $t) (func (export "_start") call $bad drop))'''),
}
def leb(n):
 out=bytearray()
 while True:
  b=n&127;n>>=7;out.append(b|(128 if n else 0))
  if not n:return out
def binary(heap):
 # Empty function type at index zero and an exported [] -> [] body. The heap is raw signed-33 bytes.
 sec=lambda i,b:bytes([i])+leb(len(b))+b
 code=bytes([0,0xd0])+bytes(heap)+bytes([0xd1,0x45,0x04,0x40,0,0x0b,0x0b])
 return b'\0asm\1\0\0\0'+sec(1,b'\1\x60\0\0')+sec(3,b'\1\0')+sec(7,b'\1\6_start\0\0')+sec(10,b'\1'+leb(len(code))+code)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--ros',action='store_true');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False);cases=[]
 for name,(valid,flags,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);path=source.with_suffix('.wasm');subprocess.run([str(a.wasm_tools),'parse',str(source),'-o',str(path)],check=True);cases.append((name,valid,flags,path))
 for name,valid,heap in [('padded-zero',True,[0x80,0x80,0x80,0x80,0]),('max-positive',False,[0xff,0xff,0xff,0xff,0x0f]),('overflow',False,[0x80,0x80,0x80,0x80,0x10]),('padded-abstract',False,[0xf0,0x7f])]:
  path=a.output/(name+'.wasm');path.write_bytes(binary(heap));cases.append((name,valid,[],path))
 configs=[]
 if a.uwvm:configs=[(n,[x for x in c[:-1] if x!='-WFE-multi-memory']) for n,c in configurations(a.uwvm.resolve(),a.ros)]+[('validator',[str(a.uwvm.resolve()),'-m','validation'])]
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 assert configs;rows=[]
 for label,base in configs:
  for name,valid,flags,path in cases:
   command=base+(['-W','function-references=y','-W','memory64=y'] if label=='wasmtime' else ['-WFE-function-references',*flags,'--run'])+[str(path)]
   r=subprocess.run(command,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/(label+'-'+name+'.log')).write_bytes(log)
   rows.append(dict(configuration=label,case=name,valid=valid,exit=r.returncode,command=command));(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n');assert (r.returncode==0)==valid,(label,name,log)
   if not valid:assert any(x in log.lower() for x in (b'validat',b'parsing error',b'invalid input webassembly code',b'failed to parse webassembly module')),(label,name,log)
  print('PASS',label,flush=True)
 if a.uwvm:
  # Default/explicitly-disabled Core 2 routes intentionally retain the untouched legacy pure validator.
  # It rejects the new immediate as an invalid reference type; a Core 3 route reports the scoped feature.
  gates=[]
  for case in ('identity','global','active-element','table-initializer'):
   path=a.output/(case+'.wasm')
   for label,flags in [('default',[]),('disabled',['-WFD-function-references']),('unrelated',['-WFE-tail-call'])]:
    extras=['-WFE-table-initializer'] if case=='table-initializer' else []
    command=[str(a.uwvm),'-m','validation',*extras,*flags,'--run',str(path)];r=subprocess.run(command,capture_output=True);log=r.stdout+r.stderr;(a.output/('gate-'+case+'-'+label+'.log')).write_bytes(log);assert r.returncode!=0 and (b'--wasm-feature-enable-function-references' in log or (case=='identity' and label in ('default','disabled') and b'invalid wasm1.1 reference type immediate' in log.lower())),(case,label,log);gates.append(dict(case=case,policy=label,exit=r.returncode,command=command))
  (a.output/'gates.json').write_text(json.dumps(gates,indent=2)+'\n');cache=a.output/'cache';cache.mkdir();cache_rows=[]
  for phase,flag in [('cold','-WFE-function-references'),('warm','-WFE-function-references'),('disabled','-WFD-function-references')]:
   compile_log=a.output/('cache-'+phase+'.compile.log');command=[str(a.uwvm)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),flag,'--run',str(a.output/'identity.wasm')]
   r=subprocess.run(command,capture_output=True,timeout=60);log=r.stdout+r.stderr;(a.output/('cache-'+phase+'.log')).write_bytes(log);hit=compile_log.exists() and 'object-cache-hit' in compile_log.read_text();assert (r.returncode==0)==(phase!='disabled');assert phase!='warm' or hit;assert phase!='disabled' or (not hit and (b'--wasm-feature-enable-function-references' in log or b'invalid wasm1.1 reference type immediate' in log.lower()));cache_rows.append(dict(phase=phase,exit=r.returncode,hit=hit))
  (a.output/'cache.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,cases=len(cases),runs=len(rows),configs=len(configs),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n');print('PASS ref.null heap CLI:',len(rows),'runs',flush=True)
if __name__=='__main__':main()
