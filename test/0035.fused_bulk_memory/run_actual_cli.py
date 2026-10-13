#!/usr/bin/env python3
"""Finite Core3 memory32/memory64 bulk memory semantic gate. Native execution is keeper-only.

Use a fresh coherent source/product cut with these private changes applied and
the existing Linux cgroup guardian; this runner never grants native admission.
"""
import argparse, hashlib, json, resource, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--repository',type=Path,required=True)
    parser.add_argument('--source-id',required=True)
    parser.add_argument('--uwvm',type=Path)
    parser.add_argument('--wasmtime',type=Path)
    parser.add_argument('--wast2json',type=Path,required=True)
    parser.add_argument('--wat2wasm',type=Path,required=True)
    parser.add_argument('--ros',action='store_true')
    parser.add_argument('--configuration',default='full',help='full, all, or one exact existing configuration')
    args=parser.parse_args();repo=args.repository.resolve();here=Path(__file__).resolve().parent
    subprocess.run(['bash',str(repo/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(repo/'test/0014.llvm_jit'))
    from run_wasm3_multi_memory import configurations
    from run_wasm3_multi_memory_spec import wrap
    tools=[]
    for role,path in (('wast2json',args.wast2json),('wat2wasm',args.wat2wasm),('uwvm',args.uwvm),('wasmtime',args.wasmtime)):
        if path is None: continue
        actual=path.resolve();run=subprocess.run([str(actual),'--version'],capture_output=True,timeout=30)
        log=output/(role+'-version.log');log.write_bytes(run.stdout+run.stderr)
        if run.returncode: raise RuntimeError(role+' version command failed; preserved original output')
        tools.append({'role':role,'path':str(actual),'sha256':sha(actual),'version_log_sha256':sha(log)})
    commands=output/'commands.json'
    subprocess.run([str(args.wast2json.resolve()),'--enable-memory64','--enable-multi-memory',
                    str(here/'fixtures/bulk_memory32_memory64.wast'),'-o',str(commands)],check=True)
    script=json.loads(commands.read_text())['commands'];module=None;returns=[];invalid=[];traps=[]
    for command in script:
        kind=command['type']
        if kind=='module':
            if module is not None: raise RuntimeError('fixture must contain one valid module')
            module=output/command['filename']
        elif kind=='assert_return': returns.append(command)
        elif kind=='assert_invalid': invalid.append(output/command['filename'])
        elif kind=='assert_trap': traps.append(command)
        else: raise RuntimeError('unexpected actual WAST command '+kind)
    if module is None or len(returns)!=20 or len(invalid)!=10 or len(traps)!=14: raise RuntimeError('actual WAST command oracle mismatch')
    executable=output/'bulk-assertions.wasm';executable.write_bytes(wrap(module.read_bytes(),returns))
    cells=[('20-bulk-assertions',executable,True)]+[('invalid-'+str(i),file,False) for i,file in enumerate(invalid)]
    oracle=json.loads((here/'fixtures/oracle.json').read_text())
    for record in oracle['malformed_binary']:
        file=here/'fixtures'/record['path']
        if file.stat().st_size!=record['size'] or sha(file)!=record['sha256']: raise RuntimeError('malformed fixture pin mismatch')
        cells.append((file.stem,file,False))
    original=output/'unused-original32.wasm'
    subprocess.run([str(args.wat2wasm.resolve()),'--enable-memory64','--enable-multi-memory','--no-check',
                    str(here/'fixtures/bulk_unused_invalid_32.wat'),'-o',str(original)],check=True)
    unused=output/'unused-original32-plus-entry.wasm'
    unused.write_bytes(wrap(original.read_bytes(),[{'type':'action','action':{'type':'invoke','field':'entry','args':[]},'expected':[]}]))
    cells.append(('unused-invalid-bulk-memory64-function31',unused,False))
    trap_files=[]
    for index,command in enumerate(traps):
        action={'type':'action','action':command['action'],'expected':[]}
        file=output/('trap-'+str(index)+'.wasm')
        file.write_bytes(wrap(module.read_bytes(),[action]))
        trap_files.append((index,file))
    configs=[]
    if args.uwvm:
        for name,command in configurations(args.uwvm.resolve(),args.ros):
            if args.configuration=='full' and not any(name.startswith(x) for x in ('int-full-','jit-full-')): continue
            if args.configuration not in ('full','all',name): continue
            command=[x for x in command[:-1] if x!='-WFE-multi-memory']
            configs.append((name,command+['-Rct','0','-WFE-memory64','-WFE-multi-memory','-WFE-bulk-memory','--run']))
        configs.append(('validator',[str(args.uwvm.resolve()),'-m','validation','-WFE-memory64','-WFE-multi-memory','-WFE-bulk-memory','--run']))
    if args.wasmtime: configs.append(('wasmtime',[str(args.wasmtime.resolve()),'-C','cache=n','-W','memory64=y','-W','multi-memory=y','-W','bulk-memory=y']))
    if not configs: raise RuntimeError('no actual products selected')
    rows=[]
    def execute(configuration,name,file,valid,command,trap=False):
        run=subprocess.run(command+[str(file)],capture_output=True,timeout=90)
        log=output/(configuration+'-'+name+'.log');log.write_bytes(run.stdout+run.stderr)
        diagnostics=(run.stdout+run.stderr).lower()
        accepted=run.returncode==0 if valid else run.returncode>0 and any(x in diagnostics for x in (b'validat',b'parsing error',b'type mismatch',b'unknown memory',b'unknown data',b'invalid',b'unknown 0xfc',b'offset'))
        if not valid and (b'there are no errors' in diagnostics or b'(null)' in diagnostics): accepted=False
        if trap: accepted=(run.returncode>0 or run.returncode in (-4,-6)) and any(x in diagnostics for x in (b'out of bounds memory',b'out-of-bounds memory',b'memory access violation',b'memory access out of bounds',b'memory out of bounds',b'memory access out of range',b'memory_oob'))
        rows.append({'configuration':configuration,'cell':name,'expected_valid':valid,'exit':run.returncode,
                     'passed':accepted,'expected_runtime_trap':trap,'command':command+[str(file)],'wasm_sha256':sha(file),'log_sha256':sha(log)})
        (output/'rows.json').write_text(json.dumps(rows,indent=2)+'\n')
        if not accepted: raise RuntimeError(configuration+' '+name+' violated actual semantic oracle')
    for configuration,command in configs:
        for name,file,valid in cells: execute(configuration,name,file,valid,command)
        for index,file in trap_files:
            # A trapping module is valid: validator must accept without executing;
            # runtime configurations must actually report the memory trap.
            if configuration not in ('validator','wasmtime'):
                # The exact same fresh trap module must first validate. A JIT
                # emission/rejection error cannot qualify as a runtime trap.
                execute(configuration+'-trap-validation','memory-trap-'+str(index),file,True,
                        [str(args.uwvm.resolve()),'-m','validation','-WFE-memory64','-WFE-multi-memory','-WFE-bulk-memory','--run'])
            execute(configuration,'memory-trap-'+str(index),file,configuration=='validator',command,trap=configuration!='validator')
    if args.uwvm:
        for disabled in ('memory64','multi-memory','bulk-memory'):
            flags=['-WFE-memory64','-WFE-multi-memory','-WFE-bulk-memory'];flags.remove('-WFE-'+disabled);flags.append('-WFD-'+disabled)
            execute('validator-off-'+disabled,'bulk-mixed',executable,False,[str(args.uwvm.resolve()),'-m','validation',*flags,'--run'])
    summary={'source_id':args.source_id,'source_only':False,'tools':tools,'runner_sha256':sha(__file__),
        'fixture_sha256':sha(here/'fixtures/bulk_memory32_memory64.wast'),'rows':rows,'passed':True,
        'low_memory_max_pages':[1,1], 'trap_after_state_unchanged_qualified':False,'performance_qualified':False,'generated_assembly_checked':False,
        'all_core3_same_typed_semantic_core_complete':False,
        'helpers':{x:sha(repo/'test/0014.llvm_jit'/x) for x in ('run_wasm3_multi_memory.py','run_wasm3_multi_memory_spec.py')}}
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({'passed':True,'rows':len(rows),'performance':False}))
if __name__=='__main__': main()
