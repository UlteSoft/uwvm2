#!/usr/bin/env python3
"""First-decode Core3 i32 actual CLI gate; only the admitted Linux64GiB keeper."""
import argparse, hashlib, json, resource, subprocess, sys
from pathlib import Path
def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('output',type=Path);p.add_argument('--uwvm',type=Path,required=True)
 p.add_argument('--wast2json',type=Path,required=True);p.add_argument('--wasmtime',type=Path)
 p.add_argument('--ros',action='store_true');p.add_argument('--configuration')
 p.add_argument('--combine-matrix',action='store_true');p.add_argument('--source-id',required=True)
 a=p.parse_args();root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=False)
 source=json.loads((here/'source-oracle-manifest.json').read_text())
 for record in source['files']:
  file=here/record['path']
  if file.stat().st_size!=record['size'] or digest(file)!=record['sha256']:raise RuntimeError('fixture source drift')
 sys.path.insert(0,str(root/'test/0014.llvm_jit'))
 from run_wasm3_multi_memory import configurations
 from run_wasm3_multi_memory_spec import wrap
 tools=[]
 for role,path in [('uwvm',a.uwvm),('wast2json',a.wast2json),('wasmtime',a.wasmtime)]:
  if path is None:continue
  path=path.resolve();actual=subprocess.run([str(path),'--version'],capture_output=True,timeout=30)
  log=actual.stdout+actual.stderr;(a.output/(role+'-version.log')).write_bytes(log)
  if actual.returncode:raise RuntimeError(role+' actual version command failed')
  tools.append({'role':role,'path':str(path),'sha256':digest(path),'version_sha256':digest(a.output/(role+'-version.log'))})
 commands_json=a.output/'commands.json'
 subprocess.run([str(a.wast2json.resolve()),'--enable-gc','--enable-function-references','--enable-tail-call',
   str(here/'fixtures/i32_numeric_core3.wast'),'-o',str(commands_json)],check=True)
 cases=[];module=None;returns=[]
 def flush():
  if module is not None and returns:
   binary=a.output/('assertions-'+str(len(cases))+'.wasm')
   binary.write_bytes(wrap(module.read_bytes(),returns))
   cases.append({'name':'all18-core3-return-bits','path':binary,'expect':'valid','assertions':len(returns)})
  elif module is not None:
   cases.append({'name':'unused-polymorphic-numeric','path':module,'expect':'valid','assertions':0})
 for number,command in enumerate(json.loads(commands_json.read_text())['commands']):
  kind=command['type']
  if kind=='module':
   flush();module=a.output/command['filename'];returns=[]
  elif kind=='assert_return':returns.append(command)
  elif kind=='assert_trap':
   if module is None:raise RuntimeError('trap without actual preceding module')
   binary=a.output/('trap-'+str(number)+'.wasm')
   binary.write_bytes(wrap(module.read_bytes(),[dict(command,type='assert_return',expected=[{'type':'i32','value':'0'}])]))
   cases.append({'name':'numeric-trap-'+str(number),'path':binary,'expect':'overflow' if 'overflow' in command['text'] else 'divide-zero','assertions':1})
  elif kind=='assert_invalid':
   cases.append({'name':'typed-invalid-'+str(number),'path':a.output/command['filename'],'expect':'invalid','assertions':0})
  else:raise RuntimeError('unsupported actual WAST command '+kind)
 flush()
 if sum(case['assertions'] for case in cases if case['expect']=='valid')!=52:raise RuntimeError('original positive assertions missing')
 configs=[];features=['-WFE-gc','-WFE-function-references','-WFE-tail-call']
 for label,base in configurations(a.uwvm.resolve(),a.ros):
  command=[x for x in base[:-1] if x!='-WFE-multi-memory']
  configs.append((label,command+['-Rct','0',*features,'--run']))
 configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation',*features,'--run']))
 if a.combine_matrix:
  if a.ros:raise RuntimeError('combine matrix requires ordinary product')
  configs=[row for row in configs if not row[0].startswith('int-')]
  for mode in ('full','lazy','lazy+verification'):
   for level in ('disable','soft','heavy','extra'):
    for no_delay in (False,True):
     command=[str(a.uwvm.resolve()),'-Rcc','int','-Rcm',mode,'-Rct','0','-Rint-op-conbine-level',level]
     if no_delay:command+=['-Rint-no-delay-local']
     configs.append(('int-'+mode+'-'+level+('-no-delay' if no_delay else '-delay'),command+features+['--run']))
 if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n','-W','gc=y','-W','function-references=y','-W','tail-call=y']))
 if a.configuration:configs=[row for row in configs if row[0]==a.configuration]
 if not configs:raise RuntimeError('no actual configurations selected')
 rows=[]
 def execute(label,case,command,expected):
  actual=subprocess.run(command+[str(case['path'])],capture_output=True,timeout=90)
  log=actual.stdout+actual.stderr;name=label+'-'+case['name'];out=a.output/(name+'.log');out.write_bytes(log);lower=log.lower()
  if expected=='valid':passed=actual.returncode==0
  elif expected=='invalid':
   passed=actual.returncode>0 and any(word in lower for word in (b'validat',b'parsing error',b'type mismatch',b'operand stack underflow'))
   passed=passed and b'there are no errors' not in lower and b'(null)' not in lower
  else:
   phrases=(b'integer divide by zero',b'integer division by zero',b'divide-by-zero') if expected=='divide-zero' else (b'integer overflow',b'integer division overflow')
   passed=(actual.returncode>0 or actual.returncode in (-4,-6)) and any(word in lower for word in phrases)
  rows.append({'configuration':label,'case':case['name'],'expect':expected,'returncode':actual.returncode,'passed':passed,
   'command':command+[str(case['path'])],'wasm_sha256':digest(case['path']),'log_sha256':digest(out),'assertions':case['assertions']})
  (a.output/'rows.json').write_text(json.dumps(rows,indent=2)+'\n')
  if not passed:raise RuntimeError(name+' actual result violates '+expected)
 for label,command in configs:
  for case in cases:
   trap=case['expect'] in ('divide-zero','overflow')
   if trap and label=='validator':execute(label,case,command,'valid');continue
   if trap and label!='wasmtime':
    execute(label+'-trap-static',case,[str(a.uwvm.resolve()),'-m','validation',*features,'--run'],'valid')
   execute(label,case,command,case['expect'])
 if not a.configuration:
  positive=next(case for case in cases if case['expect']=='valid' and case['assertions']==52)
  for feature in ('gc','tail-call'):
   flags=[flag for flag in features if flag!='-WFE-'+feature]+['-WFD-'+feature]
   execute('disabled-'+feature,positive,[str(a.uwvm.resolve()),'-m','validation',*flags,'--run'],'invalid')
 summary={'passed':True,'source_id':a.source_id,'tools':tools,'runs':len(rows),'rows':rows,'return_assertions':52,
  'runner_sha256':digest(__file__),'source_oracle_sha256':digest(here/'source-oracle-manifest.json'),
  'helper_pins':{leaf:digest(root/'test/0014.llvm_jit'/leaf) for leaf in ('run_wasm3_multi_memory.py','run_wasm3_multi_memory_spec.py')},
  'generated_asm_checked':False,'performance_qualified':False,'module_build_qualified':False}
 (a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps({'passed':True,'runs':len(rows),'asm':False,'performance':False}))
if __name__=='__main__':main()
