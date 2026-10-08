#!/usr/bin/env python3
"""Focused Core 3 table64 execution, validation, independent gates and signed cache.

Original pinned call_indirect64/table_size64 modules and all their assertions are
wrapped without modifying their declarations or instructions. Other table WAST
files are not claimed by this focused runner; host-reference tests live in the
integrated C++ fixture. Run only on Linux inside the required test cgroup.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from run_wasm3_multi_memory import configurations
from run_wasm3_multi_memory_spec import wrap

PIN = '10d212453286717b4bc59151810acd66ba2d0334'
HASHES = {
    'call_indirect64': '1624a745dc4ea54431e736204bb991e815391852d62f60bd967734d0d74058ec',
    'table_size64': '30d08e2f8e62ba0fd1462223da9cb947c50a561faa3ca16e51f55fa3c46ce25b',
    'table_copy_mixed': '897f55132bcb1d98a8ae98ceff3023825d374eef61eba1c8daf77c5990a32444',
}
# outcome is success, validation, or a required trap diagnostic substring.
CASES = {
 'operations': ('success', [], '''(module
 (table $wide i64 3 6 funcref) (table $small 3 6 funcref) (table $external i64 0 2 externref)
 (type $result (func (result i64)))
 (func $answer (type $result) i64.const 73)
 (elem $passive func $answer $answer)
 (func (export "_start")
  i64.const 0 i32.const 0 i32.const 2 table.init $wide $passive
  i64.const 1 call_indirect $wide (type $result) i64.const 73 i64.ne if unreachable end
  i32.const 0 i64.const 1 i32.const 1 table.copy $small $wide
  i64.const 2 i32.const 0 i32.const 1 table.copy $wide $small
  i64.const 2 call_indirect $wide (type $result) i64.const 73 i64.ne if unreachable end
  i64.const 0 ref.null func i64.const 1 table.fill $wide
  i64.const 0 table.get $wide ref.is_null i32.eqz if unreachable end
  i64.const 0 ref.func $answer table.set $wide
  i64.const 1 i64.const 0 i64.const 2 table.copy $wide $wide
  i64.const 2 call_indirect $wide (type $result) i64.const 73 i64.ne if unreachable end
  ref.null func i64.const 2 table.grow $wide i64.const 3 i64.ne if unreachable end
  ref.null func i64.const 4294967296 table.grow $wide i64.const -1 i64.ne if unreachable end
  table.size $wide i64.const 5 i64.ne if unreachable end
  ref.null extern i64.const 1 table.grow $external i64.const 0 i64.ne if unreachable end
  i64.const 0 table.get $external ref.is_null i32.eqz if unreachable end
  elem.drop $passive i64.const 5 i32.const 0 i32.const 0 table.init $wide $passive))'''),
 'indirect-arguments': ('success', [], '''(module
 (type $signature (func (param i64 i64 i64 i64 i64 i64 i64 i64) (result i64)))
 (func $sum (type $signature) local.get 0 local.get 1 i64.add local.get 2 i64.add local.get 3 i64.add
  local.get 4 i64.add local.get 5 i64.add local.get 6 i64.add local.get 7 i64.add)
 (table $wide i64 1 funcref) (elem (table $wide) (i64.const 0) func $sum)
 (func (export "_start") i64.const 1 i64.const 2 i64.const 3 i64.const 4 i64.const 5 i64.const 6 i64.const 7 i64.const 8
  i64.const 0 call_indirect $wide (type $signature) i64.const 36 i64.ne if unreachable end))'''),
 'tail-indirect': ('success', ['tail-call'], '''(module
 (type $signature (func (param i64) (result i64)))
 (table $wide i64 1 funcref) (elem (table $wide) (i64.const 0) func $count)
 (func $count (type $signature) local.get 0 i64.eqz if i64.const 91 return end
  local.get 0 i64.const 1 i64.sub i64.const 0 return_call_indirect $wide (type $signature))
 (func (export "_start") i64.const 10000 call $count i64.const 91 i64.ne if unreachable end))'''),
 'high-get': ('table access out of bounds', [], '(module (table i64 1 funcref) (func (export "_start") i64.const 4294967296 table.get drop))'),
 'high-indirect': ('call_indirect: table index out of bounds', [], '''(module (type $void (func)) (func $f)
 (table i64 1 funcref) (elem (i64.const 0) func $f)
 (func (export "_start") i64.const 4294967296 call_indirect (type $void)))'''),
 'high-empty-copy': ('table access out of bounds', [], '''(module (table $wide i64 1 funcref) (table $small 1 funcref)
 (func (export "_start") i32.const 0 i64.const 4294967296 i32.const 0 table.copy $small $wide))'''),
 'high-empty-fill': ('table access out of bounds', [], '(module (table i64 1 funcref) (func (export "_start") i64.const -1 ref.null func i64.const 0 table.fill))'),
 'bad-selector': ('validation', [], '(module (table i64 1 funcref) (func (export "_start") i32.const 0 call_indirect))'),
 'bad-get': ('validation', [], '(module (table i64 1 funcref) (func (export "_start") i32.const 0 table.get drop))'),
 'bad-init-length': ('validation', [], '(module (table i64 1 funcref) (elem $e func) (func (export "_start") i64.const 0 i32.const 0 i64.const 0 table.init $e))'),
 'bad-copy-length': ('validation', [], '(module (table $a i64 1 funcref) (table $b 1 funcref) (func (export "_start") i64.const 0 i32.const 0 i64.const 0 table.copy $a $b))'),
}

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path)
 p.add_argument('--wat2wasm',required=True);p.add_argument('--wast2json');p.add_argument('--upstream',type=Path)
 p.add_argument('--ros',action='store_true');p.add_argument('--configuration');a=p.parse_args()
 root=Path(__file__).resolve().parents[2]
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=False)
 cases=[]
 for name,(outcome,features,wat) in CASES.items():
  source=a.output/(name+'.wat');source.write_text(wat);binary=source.with_suffix('.wasm')
  subprocess.run([a.wat2wasm,'--enable-memory64','--enable-tail-call','--no-check',str(source),'-o',str(binary)],check=True)
  cases.append(dict(name=name,outcome=outcome,features=['table64',*features],path=binary,assertions=0))
 if a.upstream:
  assert a.wast2json
  for name,sha in HASHES.items():
   source=(a.upstream/(name+'.wast')).read_bytes();assert hashlib.sha256(source).hexdigest()==sha
   path=a.output/(name+'.wast');path.write_bytes(source);spec_json=path.with_suffix('.json')
   subprocess.run([a.wast2json,'--enable-memory64',str(path),'-o',str(spec_json)],check=True)
   commands=json.loads(spec_json.read_text())['commands'];modules=[];invalids=[]
   for cmd in commands:
    if cmd['type']=='module':modules.append((cmd,[]))
    elif cmd['type']=='assert_return':modules[-1][1].append(cmd if cmd['expected'] else dict(cmd,type='action'))
    elif cmd['type']=='assert_invalid':invalids.append(cmd)
    else:raise RuntimeError('unhandled pinned upstream command '+cmd['type'])
   for i,(module,assertions) in enumerate(modules):
    if name=='table_copy_mixed':
     # Upstream only validates these four functions; execute each as additional coverage.
     assertions=[dict(type='action',action=dict(type='invoke',field=f,args=[]),expected=[]) for f in ('test32','test64','test_64to32','test_32to64')]
    binary=a.output/(name+f'-wrapped-{i}.wasm');binary.write_bytes(wrap((a.output/module['filename']).read_bytes(),assertions))
    cases.append(dict(name=name+str(i),outcome='success',features=['table64'],path=binary,assertions=len(assertions)))
   for i,cmd in enumerate(invalids):cases.append(dict(name=name+f'-invalid-{i}',outcome='validation',features=['table64'],path=a.output/cmd['filename'],assertions=0,validation_only=True))
  (a.output/'upstream.json').write_text(json.dumps(dict(commit=PIN,sha256=HASHES,scope='Three original table64 WAST files; other files are not covered by this runner.'),indent=2)+'\n')
 configs=[]
 if a.uwvm:
  configs=[(name,[x for x in cmd[:-1] if x!='-WFE-multi-memory']) for name,cmd in configurations(a.uwvm.resolve(),a.ros)]
  configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n']))
 if a.configuration:configs=[c for c in configs if c[0]==a.configuration]
 assert configs;rows=[]
 for label,base in configs:
  for case in cases:
   if case.get('validation_only') and label not in ('validator','wasmtime'):continue
   switches=sum((['-W',('memory64' if f=='table64' else f)+'=y'] for f in case['features']),[]) if label=='wasmtime' else ['-WFE-'+f for f in case['features']]
   command=base+switches+([] if label=='wasmtime' else ['--run'])+[str(case['path'])]
   run=subprocess.run(command,capture_output=True,timeout=90);log=run.stdout+run.stderr
   (a.output/(label+'-'+case['name']+'.log')).write_bytes(log)
   outcome=case['outcome'];expected_success=outcome=='success' or (label=='validator' and outcome!='validation')
   passed=(run.returncode==0)==expected_success
   if not expected_success:
    if outcome=='validation':passed=passed and any(t in log.lower() for t in (b'validat',b'parsing error',b'type mismatch'))
    else:passed=passed and (outcome.encode() in log if label!='wasmtime' else b'out of bounds table access' in log)
   rows.append(dict(configuration=label,case=case['name'],passed=passed,exit=run.returncode,command=command))
   (a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
   if not passed:raise RuntimeError(label+' '+case['name']+'\n'+log.decode(errors='replace'))
  print('PASS',label,flush=True)
 if a.uwvm and not a.configuration:
  binary=next(c['path'] for c in cases if c['name']=='operations')
  for flags in ([],['-WFE-memory64'],['-WFD-table64']):
   run=subprocess.run([str(a.uwvm.resolve()),'-m','validation',*flags,'--run',str(binary)],capture_output=True,timeout=30)
   assert run.returncode!=0 and b'--wasm-feature-enable-table64' in run.stdout+run.stderr
  # Exercise a genuine signed object-cache hit, then forbid the same module with a narrower policy.
  cache=a.output/'cache';cache.mkdir();cache_rows=[]
  base=[str(a.uwvm.resolve())]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])
  for phase,switch in [('cold','-WFE-table64'),('warm','-WFE-table64'),('disabled','-WFD-table64')]:
   compilation=a.output/('cache-'+phase+'.compile.log')
   command=base+['-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compilation),switch,'--run',str(binary)]
   run=subprocess.run(command,capture_output=True,timeout=60);log=run.stdout+run.stderr
   (a.output/('cache-'+phase+'.log')).write_bytes(log);hit=compilation.exists() and 'object-cache-hit' in compilation.read_text()
   assert (run.returncode==0)==(phase!='disabled')
   assert phase!='warm' or hit
   assert phase!='disabled' or (not hit and b'--wasm-feature-enable-table64' in log)
   cache_rows.append(dict(phase=phase,exit=run.returncode,cache_hit=hit))
  (a.output/'cache-checks.json').write_text(json.dumps(cache_rows,indent=2)+'\n')
 (a.output/'summary.json').write_text(json.dumps(dict(passed=True,runs=len(rows),cases=len(cases),configurations=len(configs),official_assertions=sum(c['assertions'] for c in cases),binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest() if a.uwvm else None),indent=2)+'\n')
 print('PASS table64 CLI runs=',len(rows),flush=True)
if __name__=='__main__':main()
