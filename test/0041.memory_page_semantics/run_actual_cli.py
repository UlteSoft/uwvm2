#!/usr/bin/env python3
"""Finite actual Core3 memory.size/grow shared typed-stack gate; native admission is keeper-only."""
import argparse,hashlib,json,resource,subprocess,sys
from pathlib import Path
def pin(path):
    p=Path(path);data=p.read_bytes();return {'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('output',type=Path);p.add_argument('--repository',type=Path,required=True)
    p.add_argument('--source-id',required=True);p.add_argument('--wast2json',type=Path,required=True)
    p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--ros',action='store_true')
    p.add_argument('--configuration',default='full',help='full, all, or an exact existing configuration')
    a=p.parse_args();repo=a.repository.resolve();here=Path(__file__).resolve().parent
    subprocess.run(['bash',str(repo/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(repo/'test/0014.llvm_jit'))
    from run_wasm3_multi_memory import configurations
    from run_wasm3_multi_memory_spec import wrap
    oracle=json.loads((here/'fixtures/oracle.json').read_text())
    fixture=here/'fixtures/pages.wast'
    if pin(fixture)['sha256']!=oracle['wast_sha256']:raise RuntimeError('fixture source pin mismatch')
    tools=[]
    for role,path in (('wast2json',a.wast2json),('uwvm',a.uwvm),('wasmtime',a.wasmtime)):
        if path is None:continue
        path=path.resolve();result=subprocess.run([str(path),'--version'],capture_output=True,timeout=30)
        log=output/(role+'-version.log');log.write_bytes(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(role+' version command failed')
        tools.append({'role':role,'path':str(path),**pin(path),'version_log':pin(log)})
    commands=output/'commands.json'
    # WABT's documented enable-all is used only by the assembler. Actual VM
    # features below stay precise; this does not grant any guest feature/authority.
    subprocess.run([str(a.wast2json.resolve()),'--enable-all',str(fixture),'-o',str(commands)],check=True)
    groups=[];named={};invalid=[]
    for command in json.loads(commands.read_text())['commands']:
        kind=command['type']
        if kind=='module':
            groups.append({'module':output/command['filename'],'returns':[]})
            if 'name' in command:
                if command['name'] in named:raise RuntimeError('duplicate actual module name')
                named[command['name']]=groups[-1]
        elif kind=='assert_return':
            action=command['action']
            if action['type']!='invoke':raise RuntimeError('unexpected page action')
            target=named[action['module']] if 'module' in action else groups[-1]
            target['returns'].append(command)
        elif kind=='assert_invalid':invalid.append(output/command['filename'])
        else:raise RuntimeError('unexpected actual WAST command '+kind)
    if len(groups)!=oracle['valid_modules'] or [len(g['returns']) for g in groups]!=oracle['assertions_per_module'] or len(invalid)!=oracle['invalid_modules']:
        raise RuntimeError('actual WAST command oracle mismatch')
    cells=[];mixed=None
    for index,group in enumerate(groups):
        executable=output/('page-assertions-'+str(index)+'.wasm')
        executable.write_bytes(wrap(group['module'].read_bytes(),group['returns']))
        cells.append(('page-module-'+str(index),executable,True))
        if index==2:mixed=executable
    cells += [('unused-invalid-'+str(i),file,False) for i,file in enumerate(invalid)]
    features=['-WFE-memory64','-WFE-multi-memory','-WFE-reference-types','-WFE-function-references']
    configs=[]
    if a.uwvm:
        for name,command in configurations(a.uwvm.resolve(),a.ros):
            if a.configuration=='full' and not any(name.startswith(x) for x in ('int-full-','jit-full-')):continue
            if a.configuration not in ('full','all',name):continue
            command=[x for x in command[:-1] if x!='-WFE-multi-memory']
            configs.append((name,command+['-Rct','0',*features,'--run']))
        configs.append(('validator',[str(a.uwvm.resolve()),'-m','validation',*features,'--run']))
    if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime.resolve()),'-C','cache=n','-W','memory64=y','-W','multi-memory=y','-W','function-references=y']))
    if not configs:raise RuntimeError('no actual product selected')
    rows=[]
    def execute(configuration,name,file,valid,base):
        command=base+[str(file)];result=subprocess.run(command,capture_output=True,timeout=90)
        log=output/(configuration+'-'+name+'.log');log.write_bytes(result.stdout+result.stderr)
        low=(result.stdout+result.stderr).lower()
        good=result.returncode==0 if valid else result.returncode>0 and any(x in low for x in (b'validat',b'parsing error',b'type mismatch',b'operand',b'invalid',b'disabled'))
        if not valid and (b'there are no errors' in low or b'(null)' in low):good=False
        rows.append({'configuration':configuration,'case':name,'expected_valid':valid,'exit':result.returncode,
                     'passed':good,'command':command,'wasm':pin(file),'original_log':pin(log)})
        (output/'rows.json').write_text(json.dumps(rows,indent=2)+'\n')
        if not good:raise RuntimeError(configuration+' '+name+' violates actual typed-stack oracle')
    for configuration,base in configs:
        for name,file,valid in cells:execute(configuration,name,file,valid,base)
    if a.uwvm:
        for disabled in ('memory64','multi-memory'):
            flags=[x for x in features if x!='-WFE-'+disabled]+['-WFD-'+disabled]
            execute('validator-off-'+disabled,'mixed-pages',mixed,False,[str(a.uwvm.resolve()),'-m','validation',*flags,'--run'])
    summary={'source_id':a.source_id,'passed':True,'rows':rows,'tools':tools,'runner':pin(__file__),
             'fixture':pin(fixture),'helpers':{x:pin(repo/'test/0014.llvm_jit'/x) for x in ('run_wasm3_multi_memory.py','run_wasm3_multi_memory_spec.py')},
             'all_core3_same_typed_semantic_core_complete':False,'LLVM_direct_materialization_provenance_qualified':False,
             'performance_qualified':False,'generated_assembly_checked':False,'maximum_memory_pages_per_memory':1}
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({'passed':True,'rows':len(rows)}))
if __name__=='__main__':main()
