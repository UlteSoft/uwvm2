#!/usr/bin/env python3
"""Qualify the self-return_call lowering; cross-function/indirect tail execution is pending.

Runs real Core 3.0 syntax against UWVM full/lazy/tiered and Wasmtime. The optional
spec source selects unchanged fac-acc/count function bodies from upstream; it is
explicitly a subset, not a claim that the whole tail-call suite passes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('uwvm',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--wat2wasm',required=True)
    p.add_argument('--wasmtime',required=True)
    p.add_argument('--backend',choices=['jit','int','tiered'],default='jit')
    p.add_argument('--ros',action='store_true')
    p.add_argument('--spec-source',type=Path)
    a=p.parse_args()
    if a.ros and a.backend=='tiered': p.error('ROS has no tiered backend')
    root=Path(__file__).resolve().parents[2]
    subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    a.uwvm=a.uwvm.resolve(); a.output=a.output.resolve(); a.output.mkdir(parents=True,exist_ok=False)
    binary_hash=hashlib.sha256(a.uwvm.read_bytes()).hexdigest()
    rows=[]
    def run(label,cmd,error=None):
        r=subprocess.run(cmd,capture_output=True,timeout=60)
        log=(r.stdout+r.stderr).decode(errors='replace'); (a.output/(label+'.log')).write_text(log)
        ok=r.returncode==0 if error is None else r.returncode!=0 and error in log
        rows.append(dict(case=label,command=cmd,exit=r.returncode,passed=ok))
        (a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
        if not ok: raise RuntimeError(f'{label}: exit {r.returncode}, expected {error!r}\n{log}')
        return log
    for name,path in [('wabt',a.wat2wasm),('wasmtime',a.wasmtime)]: run(name+'-version',[path,'--version'])
    cases=[('mixed-tuple',r'''(module
      (func $f (param $n i32) (param $a i32) (param $b i32) (param $s i64) (param $v f64)
        (result i32 i32 i64 f64) (local $zero i32)
        local.get $zero i32.eqz if else unreachable end
        i32.const 23 local.set $zero
        local.get $n i32.eqz if local.get $a local.get $b local.get $s local.get $v return end
        i64.const 37
        local.get $n i32.const 1 i32.sub local.get $b local.get $a
        local.get $s i64.const 7 i64.add local.get $v return_call $f)
      (func (export "_start")
        i32.const 1000001 i32.const 7 i32.const 9 i64.const 13 f64.const 0.25 call $f
        f64.const 0.25 f64.ne if unreachable end
        i64.const 7000020 i64.ne if unreachable end
        i32.const 7 i32.ne if unreachable end
        i32.const 9 i32.ne if unreachable end))''',None),
      ('void-large-locals',r'''(module (memory 1)
      (func $f (param $n i32) (local i64 i64 i64 i64 i64 i64 i64 i64)
        (local i64 i64 i64 i64 i64 i64 i64 i64)
        local.get 16 i64.eqz if else unreachable end
        i64.const 99 local.set 16
        local.get $n i32.eqz if return end
        i32.const 0 i32.const 0 i32.load i32.const 1 i32.add i32.store
        local.get $n i32.const 1 i32.sub return_call $f)
      (func $start i32.const 1000000 call $f
        i32.const 0 i32.load i32.const 1000000 i32.ne if unreachable end)
      (start $start) (func (export "_start")))''',None),
      ('tail-trap',r'''(module
      (func $f (param $n i32)
        local.get $n i32.eqz if unreachable end
        local.get $n i32.const 1 i32.sub return_call $f)
      (func (export "_start") i32.const 1000000 call $f))''','unreachable')]
    if a.spec_source:
        source=a.spec_source.read_text()
        # Selected functions contain no string escapes/parentheses in their
        # export names or comments. Scan balanced parentheses, preserve bytes.
        selected=[]
        for name in ['fac-acc','count']:
            start=source.index('(func $'+name+' '); depth=0; end=None
            for i in range(start,len(source)):
                if source[i]=='(': depth+=1
                elif source[i]==')':
                    depth-=1
                    if depth==0: end=i+1; break
            if end is None: raise RuntimeError('unterminated upstream function')
            selected.append(source[start:end])
        checks=[]
        for n,want in [(0,1),(1,1),(5,120),(25,7034535277573963776)]:
            checks.append(f'i64.const {n} i64.const 1 call $fac-acc i64.const {want} i64.ne if unreachable end')
        for n in [0,1000,1000000]: checks.append(f'i64.const {n} call $count i64.eqz if else unreachable end')
        cases.append(('spec-self-subset','(module\n'+'\n'.join(selected)+'\n(func (export "_start")\n'+'\n'.join(checks)+'))',None))
        (a.output/'spec-source.json').write_text(json.dumps(dict(path=str(a.spec_source),
            sha256=hashlib.sha256(a.spec_source.read_bytes()).hexdigest(), functions=['fac-acc','count'],assertions=7),indent=2)+'\n')
    modes=['full'] if a.ros else ['full','lazy','lazy+verification']
    configs=[(mode,[]) for mode in modes]
    if a.backend=='tiered':
        configs=[('all',[]),('no-t0',['-Rtiered-disable-t0']),('no-t2',['-Rtiered-disable-t2']),('no-t0-no-t2',['-Rtiered-disable-t0','-Rtiered-disable-t2'])]
    policies=['instruction'] if a.backend=='int' else ['instruction','unwind']
    for name,wat,trap in cases:
        src=a.output/(name+'.wat'); wasm=src.with_suffix('.wasm'); src.write_text(wat+'\n')
        run(name+'-assemble',[a.wat2wasm,'--enable-tail-call',str(src),'-o',str(wasm)])
        run(name+'-wasmtime',[a.wasmtime,'-C','cache=n','-W','tail-call=y',str(wasm)],trap)
        for mode,extra in configs:
            for policy in policies:
                label=f'{name}-{a.backend}-{mode}-{policy}'; log=a.output/(label+'.compile.log')
                if a.ros: base=['-Raot' if a.backend=='jit' else '-Rint']
                elif a.backend=='tiered': base=['-Rtiered',*extra]
                else: base=['-Rcc',a.backend,'-Rcm',mode]
                if a.backend!='int': base+=['-Rllvm-cache-path','disable','-Rllvm-call-stack',policy]
                run(label,[str(a.uwvm),*base,'-Rclog','file',str(log),'-WFE-tail-call','--run',str(wasm)],trap)
                if trap is None and (not log.is_file() or not any(marker in log.read_text() for marker in ['event=stats.func','compile-end','optimize-start'])):
                    raise RuntimeError(label+': compilation evidence missing')
        run(name+'-disabled',[str(a.uwvm),'-m','validation','-WFD-tail-call','--run',str(wasm)],'--wasm-feature-enable-tail-call')
    assert binary_hash==hashlib.sha256(a.uwvm.read_bytes()).hexdigest()
    subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    (a.output/'summary.json').write_text(json.dumps(dict(passed=True,scope='self-return-call only',fixtures=len(cases),checks=len(rows),
        binary_sha256=binary_hash,backend=a.backend,ros=a.ros,modes=configs,policies=policies),indent=2)+'\n')
    print(f'PASS self return_call: {len(cases)} programs, {len(rows)} CLI/Wasmtime/gate checks')

if __name__=='__main__': main()
