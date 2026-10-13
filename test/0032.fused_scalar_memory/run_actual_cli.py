#!/usr/bin/env python3
"""Focused actual CLI differential gate for typed scalar Core3 memory events.

Run only in the existing Linux 64GiB/swap0 cgroup through the sole keeper.
The output is an exclusive new evidence namespace; no existing receipt is edited.
The imported-memory byte-after-failure witness requires a real component observer
and is expressly not certified by a subprocess exit code in this CLI runner.
"""
import argparse, hashlib, json, resource, subprocess, sys
from pathlib import Path

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path)
 p.add_argument('--wat2wasm',type=Path,required=True);p.add_argument('--wast2json',type=Path,required=True)
 p.add_argument('--ros',action='store_true');p.add_argument('--configuration');p.add_argument('--combine-matrix',action='store_true')
 p.add_argument('--source-id',required=True);a=p.parse_args()
 root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent
 # Reuse the existing actual-membership/resource admission gate; this is not a
 # new native owner/guardian or a substitute for the keeper's birth/PIDFD checks.
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=False)
 sys.path.insert(0,str(root/'test/0014.llvm_jit'))
 from run_wasm3_multi_memory import configurations
 from run_wasm3_multi_memory_spec import wrap
 source=json.loads((here/'source-oracle-manifest.json').read_text())
 for record in source['files']:
  path=here/record['path'];assert path.stat().st_size==record['size'] and digest(path)==record['sha256']
 tools=[]
 for kind,path in [('wat2wasm',a.wat2wasm),('wast2json',a.wast2json),('uwvm',a.uwvm),('wasmtime',a.wasmtime)]:
  if path is None:continue
  path=path.resolve();v=subprocess.run([str(path),'--version'],capture_output=True,timeout=30)
  if v.returncode:raise RuntimeError(kind+' version command failed')
  (a.output/(kind+'-version.log')).write_bytes(v.stdout+v.stderr)
  tools.append({'role':kind,'path':str(path),'sha256':digest(path),'version_sha256':digest(a.output/(kind+'-version.log'))})
 cases=[]
 for file in ('scalar_memory32_memory64_all23.wast','scalar_memory64_boundary.wast'):
  namespace=a.output/file.removesuffix('.wast');namespace.mkdir()
  target=namespace/'commands.json'
  subprocess.run([str(a.wast2json.resolve()),'--enable-memory64','--enable-multi-memory',str(here/'fixtures'/file),'-o',str(target)],check=True)
  commands=json.loads(target.read_text())['commands'];module=None;assertions=[]
  for number,c in enumerate(commands):
   if c['type']=='module':
    if module is not None and assertions:raise RuntimeError('unexpected multiple module fixture')
    module=namespace/c['filename']
   elif c['type']=='assert_return':assertions.append(c)
   elif c['type']=='assert_trap':
    assert module is not None
    # Both supplied trap exports return i32. An unexpected return0 succeeds and
    # therefore fails the expected runtime-trap row; another value traps with
    # unreachable, which is rejected by the memory-specific diagnostic check.
    binary=namespace/('trap-'+str(number)+'.wasm')
    binary.write_bytes(wrap(module.read_bytes(),[dict(c,type='assert_return',expected=[{'type':'i32','value':'0'}])]))
    cases.append({'name':file+'-trap-'+str(number),'path':binary,'expect':'memory-trap','assertions':1})
   elif c['type']=='assert_invalid':cases.append({'name':file+'-invalid-'+str(number),'path':namespace/c['filename'],'expect':'invalid','assertions':0})
   else:raise RuntimeError('unhandled real WAST command '+c['type'])
  if assertions:
   assert module is not None
   binary=namespace/'assertions.wasm';binary.write_bytes(wrap(module.read_bytes(),assertions))
   cases.append({'name':'all23-memory32-memory64','path':binary,'expect':'valid','assertions':len(assertions)})
 for leaf in json.loads((here/'fixtures/oracle.json').read_text())['malformed_binary_cases']:
  cases.append({'name':Path(leaf['path']).stem,'path':here/'fixtures'/leaf['path'],'expect':'invalid','assertions':0})
 # Preserve all 32 original functions and their original indices. The appended
 # assertion _start is index32 and calls the real exported void entry index0;
 # invalid function31 still lies beyond the <=16-function adjacent warmup.
 source_wat=here/'fixtures/unused_invalid_function_32.wat';binary=a.output/'unused-original32.wasm'
 subprocess.run([str(a.wat2wasm.resolve()),'--enable-memory64','--enable-multi-memory','--no-check',str(source_wat),'-o',str(binary)],check=True)
 start=a.output/'unused-original32-plus-entry.wasm'
 start.write_bytes(wrap(binary.read_bytes(),[{'type':'action','action':{'type':'invoke','field':'entry','args':[]},'expected':[]}]))
 cases.append({'name':'unused-invalid-original32','path':start,'expect':'invalid','assertions':0})
 configs=[]
 if a.uwvm:
  for name,base in configurations(a.uwvm.resolve(),a.ros):
   command=[x for x in base[:-1] if x!='-WFE-multi-memory']
   command+=['-Rct','0','-WFE-memory64','-WFE-multi-memory','--run']
   configs.append((name,command))
  configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation','-WFE-memory64','-WFE-multi-memory','--run']))
 if a.combine_matrix:
  if a.ros or not a.uwvm:raise RuntimeError('combine matrix requires ordinary product')
  configs=[x for x in configs if not x[0].startswith('int-')]
  for mode in ('full','lazy','lazy+verification'):
   for level in ('disable','soft','heavy','extra'):
    for no_delay in (False,True):
     command=[str(a.uwvm.resolve()),'-Rcc','int','-Rcm',mode,'-Rct','0','-Rint-op-conbine-level',level]
     if no_delay:command+=['-Rint-no-delay-local']
     configs.append((f'int-{mode}-{level}-'+('no-delay' if no_delay else 'delay'),command+['-WFE-memory64','-WFE-multi-memory','--run']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n','-W','memory64=y','-W','multi-memory=y']))
 if a.configuration:configs=[x for x in configs if x[0]==a.configuration]
 if not configs:raise RuntimeError('no actual configurations selected')
 rows=[]
 def execute(name,case,command,expect):
  result=subprocess.run(command+[str(case['path'])],capture_output=True,timeout=90)
  log=result.stdout+result.stderr;label=name+'-'+case['name'];(a.output/(label+'.log')).write_bytes(log)
  low=log.lower()
  if expect=='valid':good=result.returncode==0
  elif expect=='invalid':good=result.returncode>0 and any(x in low for x in (b'validat',b'parsing error',b'type mismatch',b'alignment',b'memarg',b'offset'))
  else:good=(result.returncode>0 or result.returncode in (-4,-6)) and any(x in low for x in (b'out of bounds memory',b'out-of-bounds memory',b'memory access violation',b'memory access out of bounds',b'memory out of bounds'))
  rows.append({'configuration':name,'case':case['name'],'expected':expect,'exit':result.returncode,'passed':good,'command':command+[str(case['path'])],
   'wasm_sha256':digest(case['path']),'log_sha256':digest(a.output/(label+'.log')),'assertions':case['assertions']})
  (a.output/'rows.json').write_text(json.dumps(rows,indent=2)+'\n')
  if not good:raise RuntimeError(label+' actual result violates '+expect+'; see preserved original log')
 for name,command in configs:
  for case in cases:
   if case['expect']=='memory-trap' and name=='validator':execute(name,case,command,'valid');continue
   if case['expect']=='memory-trap' and name!='wasmtime':
    # A compile/validation failure is never accepted as a runtime trap.
    execute(name+'-trap-validation',case,[str(a.uwvm.resolve()),'-m','validation','-WFE-memory64','-WFE-multi-memory','--run'],'valid')
   execute(name,case,command,case['expect'])
 if a.uwvm:
  valid=next(x for x in cases if x['expect']=='valid')
  for feature in ('memory64','multi-memory'):
   flags=['-WFE-memory64','-WFE-multi-memory'];flags.remove('-WFE-'+feature);flags+=['-WFD-'+feature]
   execute('validator-disabled-'+feature,valid,[str(a.uwvm.resolve()),'-m','validation',*flags,'--run'],'invalid')
 summary={'source_id':a.source_id,'passed':True,'tools':tools,'runner_sha256':digest(__file__),'helper_pins':{name:digest(root/'test/0014.llvm_jit'/name) for name in ('run_wasm3_multi_memory.py','run_wasm3_multi_memory_spec.py')},
  'source_oracle_manifest_sha256':digest(here/'source-oracle-manifest.json'),'runs':len(rows),'rows':rows,
  'pre_effect_imported_provider_byte_witness_passed':False,'pre_effect_reason':'requires separate actual retained provider memory component observer; CLI exit never certifies post-failure byte state',
  'performance_qualified':False,'generated_asm_checked':False}
 (a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({'passed':True,'runs':len(rows),'pre_effect_byte_witness':False,'performance':False}))
if __name__=='__main__':main()
