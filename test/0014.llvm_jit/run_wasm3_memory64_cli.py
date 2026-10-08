#!/usr/bin/env python3
"""Focused binary-memory64 acceptance, including ordinary lazy/tiered frontends.

WAT inputs contain Core 3 i64 memory declarations/offsets and threads syntax.
Every executable positive case checks results in guest code. The optional pinned
upstream load64 WAST preserves its original module and assertion sequence.
"""
import argparse, hashlib, json, resource, subprocess
from pathlib import Path
from run_wasm3_multi_memory import configurations
from run_wasm3_multi_memory_spec import wrap

OFFICIAL_LOAD64_SHA256 = '9309409e3471fcbd846829d54e12e742f8cd97aeb3650ffe7c0f622a36b06838'
OFFICIAL_LOAD64_COMMIT = '10d212453286717b4bc59151810acd66ba2d0334'

CASES = {
 'scalar': (True, ['memory64'], '''(module (memory i64 1 2) (data (i64.const 16) "abcd")
 (func (export "_start")
  i64.const 16 i32.load i32.const 0x64636261 i32.ne if unreachable end
  i64.const 31 i64.const 0x123456789abcdef i64.store
  i64.const 31 i64.load i64.const 0x123456789abcdef i64.ne if unreachable end
  memory.size i64.const 1 i64.ne if unreachable end
  i64.const 1 memory.grow i64.const 1 i64.ne if unreachable end
  memory.size i64.const 2 i64.ne if unreachable end
  i64.const 0x100000000 memory.grow i64.const -1 i64.ne if unreachable end))'''),
 'simd': (True, ['memory64'], '''(module (memory i64 1)
 (func (export "_start")
  i64.const 17 v128.const i32x4 1 2 3 4 v128.store
  i64.const 17 v128.load v128.const i32x4 1 2 3 4 i32x4.eq i32x4.all_true
  if else unreachable end))'''),
 'bulk': (True, ['memory64','multi-memory'], '''(module (memory $wide i64 1) (memory $narrow 1)
 (data $passive "hello")
 (func (export "_start")
  i64.const 9 i32.const 0 i32.const 5 memory.init $wide $passive
  i32.const 13 i64.const 9 i32.const 5 memory.copy $narrow $wide
  i64.const 23 i32.const 13 i32.const 5 memory.copy $wide $narrow
  i64.const 23 i32.load8_u $wide i32.const 104 i32.ne if unreachable end
  i64.const 23 i32.const 97 i64.const 5 memory.fill $wide
  i64.const 27 i32.load8_u $wide i32.const 97 i32.ne if unreachable end
  data.drop $passive i64.const 65536 i32.const 0 i32.const 0 memory.init $wide $passive))'''),
 'atomic': (True, ['memory64','threads'], '''(module (memory i64 1 1 shared)
 (func (export "_start")
  i64.const 0 i64.const 7 i64.atomic.store
  i64.const 0 i64.const 7 i64.const 9 i64.atomic.rmw8.cmpxchg_u
  i64.const 7 i64.ne if unreachable end
  i64.const 0 i64.atomic.load i64.const 9 i64.ne if unreachable end
  i64.const 0 i64.const 9 i64.const 0 memory.atomic.wait64 i32.const 2 i32.ne if unreachable end
  i64.const 0 i32.const 1 memory.atomic.notify i32.eqz if else unreachable end atomic.fence))'''),
 'wide-offset': (True, ['memory64','threads'], '''(module (memory i64 1 1 shared)
 (func $finish (result i32) i32.const 23)
 (func $scalar (result i32)
  i32.const 0 if i64.const 1 i64.load offset=18446744073709551615 drop end call $finish)
 (func $simd (result i32)
  i32.const 0 if i64.const 1 v128.load offset=18446744073709551615 drop end call $finish)
 (func $atomic (result i32)
  i32.const 0 if i64.const 1 i64.atomic.load offset=18446744073709551615 drop end call $finish)
 (func (export "_start")
  call $scalar i32.const 23 i32.ne if unreachable end
  call $simd i32.const 23 i32.ne if unreachable end
  call $atomic i32.const 23 i32.ne if unreachable end))'''),
 'bad-address': (False, ['memory64'], '(module (memory i64 1) (func (export "_start") i32.const 0 i32.load drop))'),
 'bad-grow': (False, ['memory64'], '(module (memory i64 1) (func (export "_start") i32.const 0 memory.grow drop))'),
 'bad-offset': (False, ['memory64'], '(module (memory 1) (func (export "_start") i32.const 0 i32.load offset=4294967296 drop))'),
 'bad-init-length': (False, ['memory64'], '(module (memory i64 1) (data $d "x") (func (export "_start") i64.const 0 i32.const 0 i64.const 1 memory.init $d))'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path)
 p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--wat2wasm',required=True)
 p.add_argument('--ros',action='store_true');p.add_argument('--jit-only',action='store_true');p.add_argument('--configuration')
 p.add_argument('--official-load64',type=Path);p.add_argument('--wast2json');a=p.parse_args()
 root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=False)
 cases=[]
 for name,(valid,features,wat) in CASES.items():
  path=a.output/(name+'.wat');path.write_text(wat);binary=path.with_suffix('.wasm')
  command=[a.wat2wasm,'--enable-memory64','--enable-multi-memory','--enable-threads','--no-check',str(path),'-o',str(binary)]
  subprocess.run(command,check=True);cases.append(dict(name=name,valid=valid,features=features,path=binary,assertions=None))
 if a.official_load64:
  assert a.wast2json;source=a.official_load64.read_bytes();assert hashlib.sha256(source).hexdigest()==OFFICIAL_LOAD64_SHA256
  (a.output/'load64.wast').write_bytes(source)
  # Resolve/pin the upstream file outside this runner; record the exact bytes tested.
  spec_json=a.output/'load64.json'
  subprocess.run([a.wast2json,'--enable-memory64','--enable-multi-memory',str(a.output/'load64.wast'),'-o',str(spec_json)],check=True)
  commands=json.loads(spec_json.read_text())['commands'];modules=[];text_only=[]
  for command in commands:
   if command['type']=='module':modules.append((command,[]))
   elif command['type'] in ('assert_return','action'):
    assert modules;modules[-1][1].append(command)
   elif command['type']=='assert_malformed' and command['module_type']=='text':text_only.append(command)
   elif command['type']!='assert_invalid':raise RuntimeError('unhandled official command '+command['type'])
  for i,(module,assertions) in enumerate(modules):
   binary=a.output/f'official-load64-{i}.wasm';binary.write_bytes(wrap((a.output/module['filename']).read_bytes(),[dict(c,type="action") if c['type']=='assert_return' and not c['expected'] else c for c in assertions]))
   cases.append(dict(name=f'official-load64-{i}',valid=True,features=['memory64','multi-memory'],path=binary,assertions=len(assertions)))
  # Invalid original modules run through explicit validation in every selected product.
  for i,command in enumerate(c for c in commands if c['type']=='assert_invalid'):
   cases.append(dict(name=f'official-invalid-{i}',valid=False,features=['memory64','multi-memory'],path=a.output/command['filename'],assertions=None,validation_only=True))
  (a.output/'upstream.json').write_text(json.dumps(dict(url='https://github.com/WebAssembly/spec/blob/'+OFFICIAL_LOAD64_COMMIT+'/test/core/memory64/load64.wast',sha256=hashlib.sha256(source).hexdigest(),excluded_text_parser_cases=text_only),indent=2)+'\n')
 configs=[]
 if a.uwvm:
  configs=[(name,[x for x in cmd[:-1] if x!='-WFE-multi-memory']) for name,cmd in configurations(a.uwvm.resolve(),a.ros)]
  if a.jit_only:configs=[c for c in configs if c[0].startswith('jit-')]
  configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 if a.configuration:configs=[c for c in configs if c[0]==a.configuration]
 assert configs;rows=[]
 for label,base in configs:
  for case in cases:
   if case.get('validation_only') and label not in ('validator','wasmtime'):continue
   switches=sum((['-W',f'{f}=y'] for f in case['features']),[]) if label=='wasmtime' else ['-WFE-'+f for f in case['features']]
   if label=='wasmtime' and 'threads' in case['features']:switches+=['-W','shared-memory=y']
   command=base+switches+([] if label=='wasmtime' else ['--run'])+[str(case['path'])]
   result=subprocess.run(command,capture_output=True,timeout=90);log=result.stdout+result.stderr
   (a.output/(label+'-'+case['name']+'.log')).write_bytes(log)
   passed=(result.returncode==0)==case['valid']
   if not case['valid']:passed=passed and any(x in log.lower() for x in (b'validat',b'parsing error',b'type mismatch',b'offset out of range'))
   rows.append(dict(configuration=label,case=case['name'],exit=result.returncode,passed=passed,command=command))
   (a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
   if not passed:raise RuntimeError(label+' '+case['name']+'\n'+log.decode(errors='replace'))
  print('PASS',label,flush=True)
 if a.uwvm and not a.configuration:
  for name in ('scalar','atomic'):
   case=next(c for c in cases if c['name']==name)
   for flags in ([],['-WFD-memory64']):
    result=subprocess.run([str(a.uwvm.resolve()),'-m','validation',*flags,'--run',str(case['path'])],capture_output=True,timeout=30)
    assert result.returncode!=0 and b'--wasm-feature-enable-memory64' in result.stdout+result.stderr
 if a.uwvm and not a.configuration:
  # The original u64 values must survive rendered CLI diagnostics too.
  from run_wasm3_multi_memory_spec import leb
  for name,minimum,maximum,needle in [('limit', (1<<48)+1, (1<<48)+1, str((1<<48)+1)), ('descending',1<<40,(1<<40)-1,str((1<<40)-1))]:
   payload=b'\x01\x05'+leb(minimum)+leb(maximum);wasm=a.output/('diagnostic-'+name+'.wasm')
   wasm.write_bytes(b'\0asm\1\0\0\0\x05'+leb(len(payload))+payload)
   result=subprocess.run([str(a.uwvm.resolve()),'-m','validation','-WFE-memory64','--run',str(wasm)],capture_output=True,timeout=30)
   log=result.stdout+result.stderr;(a.output/('diagnostic-'+name+'.log')).write_bytes(log)
   assert result.returncode!=0 and needle.encode() in log
  # A genuine cache hit may never bypass a subsequently disabled feature.
  cache=a.output/'memory64-cache';cache.mkdir();binary=next(c['path'] for c in cases if c['name']=='scalar')
  base=[str(a.uwvm.resolve())]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])
  cache_rows=[]
  for phase,flag in [('cold','-WFE-memory64'),('warm','-WFE-memory64'),('disabled','-WFD-memory64')]:
   compilation=a.output/('cache-'+phase+'.compile.log')
   command=base+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compilation),flag,'--run',str(binary)]
   result=subprocess.run(command,capture_output=True,timeout=60);log=result.stdout+result.stderr
   (a.output/('cache-'+phase+'.log')).write_bytes(log);compiled=compilation.read_text() if compilation.exists() else ''
   hit='object-cache-hit' in compiled
   assert (result.returncode==0)==(phase!='disabled')
   assert phase!='warm' or hit
   assert phase!='disabled' or (not hit and b'--wasm-feature-enable-memory64' in log)
   cache_rows.append(dict(phase=phase,exit=result.returncode,cache_hit=hit))
  (a.output/'cache-checks.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,runs=len(rows),cases=len(cases),configurations=len(configs),official_assertions=sum(c['assertions'] or 0 for c in cases),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n')
 print('PASS memory64 CLI runs=',len(rows),flush=True)
if __name__=='__main__':main()
