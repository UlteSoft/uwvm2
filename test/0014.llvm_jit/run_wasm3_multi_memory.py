#!/usr/bin/env python3
"""Execute NEW Core 3 multi-memory encodings across the requested runtime modes.

Each positive fixture has a guest _start assertion, so successful process exit
checks the returned bit pattern rather than merely accepting compilation. Negative
fixtures exercise full standalone validation and materialization in every mode.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def configurations(binary, ros):
    rows=[]
    for backend in ('int','jit','tiered'):
        if ros and backend=='tiered':continue
        modes=('full',) if ros else (('lazy','lazy+verification') if backend=='tiered' else ('full','lazy','lazy+verification'))
        for mode in modes:
            policies=('instruction','unwind') if backend in ('jit','tiered') else ('instruction',)
            variants=((),('-Rtiered-disable-t0',),('-Rtiered-disable-t2',),('-Rtiered-disable-t0','-Rtiered-disable-t2')) if backend=='tiered' else ((),)
            for policy in policies:
                for variant in variants:
                    name=f'{backend}-{mode}-{policy}'+''.join(variant)
                    command=[str(binary)]+(['-Raot' if backend=='jit' else '-Rint'] if ros else ['-Rcc',backend,'-Rcm',mode])
                    command+=list(variant)
                    if backend!='int':command+=['-Rllvm-cache-path','disable','-Rllvm-call-stack',policy]
                    rows.append((name,command+['-WFE-multi-memory','--run']))
    return rows


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('fixtures',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--uwvm',type=Path);p.add_argument('--wasmtime',type=Path);p.add_argument('--ros',action='store_true')
    p.add_argument('--configuration',help='run a single named backend configuration')
    p.add_argument('--combine-matrix',action='store_true',help='run all interpreter combine/delay variants')
    p.add_argument('--source-id',help='fingerprint of the product source snapshot')
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    cases=json.loads((a.fixtures/'multi-memory-cases.json').read_text());configs=[]
    if a.wasmtime:configs.append(('wasmtime',[str(a.wasmtime),'-C','cache=n','-W','multi-memory=y']))
    if a.uwvm:
        configs+=configurations(a.uwvm,a.ros)
        configs.append(('validator',[str(a.uwvm),'-m','validation','-WFE-multi-memory','--run']))
    if a.combine_matrix:
        if a.ros or not a.uwvm:raise RuntimeError('combine matrix requires ordinary interpreter product')
        configs=[config for config in configs if config[0]=='wasmtime']
        for mode in ('full','lazy','lazy+verification'):
            for level in ('disable','soft','heavy','extra'):
                for no_delay in (False,True):
                    label=f'int-{mode}-{level}-'+('no-delay' if no_delay else 'delay')
                    command=[str(a.uwvm),'-Rcc','int','-Rcm',mode,'-Rint-op-conbine-level',level]
                    if no_delay:command.append('-Rint-no-delay-local')
                    configs.append((label,command+['-WFE-multi-memory','--run']))
    if a.configuration:configs=[c for c in configs if c[0]==a.configuration]
    if not configs:raise RuntimeError('no selected runtime configurations')
    results=[]; reference_limits=[]
    for label,command in configs:
        for case in cases:
            fixture=a.fixtures/(case['name']+'-exec.wasm')
            run=subprocess.run(command+[str(fixture)],capture_output=True,timeout=60)
            output=run.stdout+run.stderr;name=label+'-'+case['name'];(a.output/(name+'.log')).write_bytes(output)
            if label=='wasmtime' and case.get('memories',0)>100 and b'memories count exceeds limit of 100' in output:
                reference_limits.append(dict(case=case['name'],reason='Wasmtime validator limit: 100 memories',exit=run.returncode)); continue
            if (run.returncode==0)!=case['valid']:
                raise RuntimeError(f"{name}: expected valid={case['valid']}, exit={run.returncode}: {output.decode(errors='replace')}")
            # Lazy grouped compilation can fail one local function while a waiter
            # observes another member's error slot. A generic validation header
            # alone must not mask the empty "There are no errors" payload.
            if not case['valid'] and label!='wasmtime' and (b'there are no errors' in output.lower() or b'(null)' in output.lower() or not any(t in output.lower() for t in (b'validat',b'parsing error'))):
                raise RuntimeError(f'{name}: failure lacks a validation diagnostic: {output!r}')
            results.append(dict(configuration=label,case=case['name'],valid=case['valid'],exit=run.returncode))
        count=sum(r['configuration']==label for r in results)
        print(f'PASS {label}: {count} checked new-encoding cases',flush=True)
    if a.uwvm and not a.configuration:
        fixture=a.fixtures/'load-memory1-exec.wasm'
        for flags in ([],['-WFD-multi-memory']):
            run=subprocess.run([str(a.uwvm),'-m','validation',*flags,'--run',str(fixture)],capture_output=True,timeout=30)
            output=run.stdout+run.stderr
            if run.returncode==0 or b'--wasm-feature-enable-multi-memory' not in output:
                raise RuntimeError(f'missing independent multi-memory feature rejection: {output!r}')
        run=subprocess.run([str(a.uwvm),'-WFE-multi-memory','-WFD-multi-memory','--run',str(fixture)],capture_output=True,timeout=30)
        if run.returncode==0 or b'conflicts' not in run.stdout+run.stderr:raise RuntimeError('multi-memory feature ownership conflict accepted')
    def digest(path):
        with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
    summary=dict(feature='multi-memory',new_syntax_cases=len(cases),positive=sum(c['valid'] for c in cases),negative=sum(not c['valid'] for c in cases),configurations=len(configs),runs=len(results),reference_limits=reference_limits,cases=results,
                 source_id=a.source_id,binary_sha256=digest(a.uwvm) if a.uwvm else None,
                 wasmtime_sha256=digest(a.wasmtime) if a.wasmtime else None,
                 fixture_manifest_sha256=digest(a.fixtures/'multi-memory-cases.json'),
                 runner_sha256=digest(Path(__file__)),
                 cgroup={name:Path('/sys/fs/cgroup',name).read_text().strip() for name in ('memory.max','memory.swap.max','cpuset.cpus.effective')})
    (a.output/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='cases'}))
if __name__=='__main__':main()
