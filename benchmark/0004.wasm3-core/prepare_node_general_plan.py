#!/usr/bin/env python3
"""Bind actual Node artifacts and source-only scalar argv; does not launch any tool."""
from pathlib import Path
import argparse, hashlib, json, os, re, tempfile
from generate_general_gc import FAMILIES, PHASES, oracle
NODE_SHA='2967db94f10bcc211d5b0bf1448b44a7fa765be299560b5fba672ab8913102c2'
VERSION='v26.10.0'
FLAGS=['--max-old-space-size=1024','--max-semi-space-size=16']
VERSION_QUERY="console.log(JSON.stringify({node:process.version,v8:process.versions.v8,platform:process.platform,arch:process.arch}))"
ENV={'NODE_OPTIONS':'','NODE_DISABLE_COMPILE_CACHE':'1','PYTHONDONTWRITEBYTECODE':'1'}
def require(ok,reason):
    if not ok: raise RuntimeError(reason)
def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(item):
    require(type(item) is dict and set(item)=={'path','sha256'},'Exact actual path/sha pin required')
    p=Path(item['path']).resolve(strict=True)
    require(str(p)==item['path'] and p.is_file() and sha(p)==item['sha256'],'Actual canonical artifact changed')
    return {'path':str(p),'bytes':p.stat().st_size,'sha256':item['sha256']}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bindings',required=True,type=Path);p.add_argument('--out',required=True,type=Path)
    p.add_argument('--mode',required=True,choices=('cold','warm'));p.add_argument('--gc-telemetry',action='store_true')
    a=p.parse_args();raw=json.loads(a.bindings.read_bytes())
    require(set(raw)=={'schema','node','version_receipt','syntax_receipt'} and raw['schema']=='uwvm-node-general-artifacts-v1','Actual binding shape')
    tool=pin(raw['node']);version_receipt=pin(raw['version_receipt']);syntax_receipt=pin(raw['syntax_receipt'])
    here=Path(__file__).parent;source=here/'managed_general/node/GeneralGc.mjs'
    require(sha(source)==NODE_SHA,'Frozen JS source changed')
    version=json.loads(Path(version_receipt['path']).read_bytes())
    require(set(version)=={'argv','returncode','version'} and version['argv']==[tool['path'],'--eval',VERSION_QUERY] and version['returncode']==0,'Actual version command receipt changed')
    v=version['version']
    require(set(v)=={'node','v8','platform','arch'} and v['node']==VERSION and v['platform']=='linux' and v['arch']=='x64' and type(v['v8']) is str and re.fullmatch(r'14\.6\.202\.34(?:-.+)?',v['v8']) is not None,'Actual Node/V8/platform version mismatch')
    syntax=json.loads(Path(syntax_receipt['path']).read_bytes())
    require(set(syntax)=={'argv','returncode','node_sha256','source_sha256'} and syntax['argv']==[tool['path'],'--check',str(source.resolve())] and syntax['returncode']==0 and syntax['node_sha256']==tool['sha256'] and syntax['source_sha256']==NODE_SHA,'Actual syntax/tool/source receipt mismatch')
    if a.mode=='cold':cells=[(f,p,65536) for f in FAMILIES for p in PHASES];rounds=0
    else:cells=[(f,p,n) for f,p in [('reference-array','allocate'),('mutable-struct','mutate')] for n in (1000000,2000000)];rounds=8
    rows=[]
    for family,phase,n in cells:
        expected=oracle(family,phase,n);warm=oracle(family,phase,250000)
        extra=[family,phase,str(n),'250000',str(rounds)]
        for values in (expected,warm):extra.extend(str(values[k]) for k in ('step_checksum_u32','root_checksum_u32','last_lcg_u32'))
        if a.gc_telemetry:extra.append('--gc-telemetry')
        rows.append({'engine':'node-v8','family':family,'phase':phase,'iterations':n,'warmup_rounds':rounds,
                     'argv':[tool['path'],*FLAGS,str(source.resolve()),*extra],'environment_delta':ENV,
                     'expected':expected,'warm_expected':warm,'actual_version':v,'whole_process_multitid':True,
                     'collector_qualified':False,'single_tid_hw_counter_qualified':False})
    sources={n:{'bytes':(here/n).stat().st_size,'sha256':sha(here/n)} for n in ('managed_general/node/GeneralGc.mjs','prepare_node_general_plan.py','run_node_general_measurement.py','generate_general_gc.py')}
    plan={'schema':'uwvm-node-general-argv-plan-v1','mode':a.mode,'rows':rows,'node':tool,
          'version_receipt':version_receipt,'syntax_receipt':syntax_receipt,'actual_version':v,'source_pins':sources,
          'artifact_binding_path':str(a.bindings.resolve()),'artifact_binding_sha256':sha(a.bindings),
          'current_native_tls_product_binding':None,'formal_acceptance':False,'actual_semantics_passed':False,
          'temperature_policy':'observation_only','pending':'actual current CG admission/SDK/DSOs/semantic/escape/GC/frequency evidence',
          'ROI':'Run internal setup/loop/final roots; warmup/output excluded; parent wait4 allTIDs/process distinct'}
    a.out.parent.mkdir(parents=True,exist_ok=True);fd,t=tempfile.mkstemp(prefix='.node-plan-',dir=a.out.parent)
    try:
        with os.fdopen(fd,'wb') as f:f.write((json.dumps(plan,indent=2)+'\n').encode());f.flush();os.fsync(f.fileno())
        os.link(t,a.out)
    finally:os.unlink(t)
    print(json.dumps({'plan':str(a.out),'sha256':sha(a.out),'rows':len(rows),'native_executed':False}))
if __name__=='__main__':main()
