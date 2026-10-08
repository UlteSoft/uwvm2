#!/usr/bin/env python3
"""Replay frozen positive Wasm GC/SIMD/exception oracles in a bounded Linux cgroup.

Negative fixtures are assembled and checked by wasm-tools. Native trap and
validation-PC evidence is recorded separately by the GDB qualification suite.
This runner records correctness, not post-JIT timing or GC pause latency.
"""
import argparse,hashlib,json,os,pathlib,re,resource,subprocess,sys

def sha(path):
    with pathlib.Path(path).open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm',type=pathlib.Path,required=True)
    parser.add_argument('--wasm-tools',type=pathlib.Path,required=True)
    parser.add_argument('--out',type=pathlib.Path,required=True)
    parser.add_argument('--cgroup',type=pathlib.Path,required=True)
    parser.add_argument('--cpu',type=int,required=True)
    parser.add_argument('--ros',action='store_true')
    parser.add_argument('--only',help='Regular expression over case names')
    args=parser.parse_args()
    cg=args.cgroup.resolve();group=pathlib.Path('/proc/self/cgroup').read_text().strip()
    assert str(os.getpid()) in (cg/'cgroup.procs').read_text().splitlines(), 'Actual process must belong to --cgroup'
    assert 0<int((cg/'memory.max').read_text())<=64*1024**3 and (cg/'memory.swap.max').read_text().strip()=='0'
    permitted=set()
    for component in (cg/'cpuset.cpus.effective').read_text().strip().split(','):
        first,separator,last=component.partition('-');permitted.update(range(int(first),int(last if separator else first)+1))
    assert args.cpu in permitted,'Select a permitted, independently verified native P-core'
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.sched_setaffinity(0,{args.cpu})
    out=args.out.resolve();assert out.is_relative_to(pathlib.Path('/tmp')),'Generated code/results belong under Linux /tmp'
    assert __import__('shutil').disk_usage(out.parent).free>2*1024**3
    out.mkdir(parents=True,exist_ok=False)
    root=pathlib.Path(__file__).resolve().parent
    manifest=json.loads((root/'manifest.json').read_text());rows=manifest['cases']
    if args.only:rows=[x for x in rows if re.search(args.only,x['name'])]
    assert rows and len(rows)<=512
    binary=args.uwvm.resolve();tool=args.wasm_tools.resolve()
    prefix=[str(binary)] if args.ros else [str(binary),'-Rcc','jit','-Rcm','full']
    prefix+=['-Rct','0','-Rllvm-cache-path','disable','-Rclog','err']
    prefix+=['-WFE-'+f for f in ['gc','function-references','exceptions','tail-call','simd','relaxed-simd','threads','memory64','multi-memory','table64','extended-const','table-initializer']]
    env=os.environ.copy()
    for key in ['WASM_PERF_NATIVE_CLI','WASM_PERF_NATIVE_REPS','WASM_PERF_NATIVE_PROFILE']:env.pop(key,None)
    records=[]
    for row in rows:
        name=row['name'];assert re.fullmatch(r'[A-Za-z0-9_-]+',name)
        source=root/(name+'.wat');assert sha(source)==row['wat_sha256']
        module=out/(name+'.wasm')
        subprocess.run([str(tool),'parse',str(source),'-o',str(module)],check=True,capture_output=True,timeout=30)
        validated=subprocess.run([str(tool),'validate','--features','all',str(module)],capture_output=True,timeout=30)
        assert (validated.returncode==0)==(row['expect']!='validation'),(name,validated.stderr)
        module_sha256=sha(module)
        if row.get('frozen_wasm_sha256'):
            assert module_sha256==row['frozen_wasm_sha256'], ('Frozen Wasm byte hash mismatch',name)
        record=dict(case=name,wat_sha256=sha(source),wasm_sha256=module_sha256,expect=row['expect'])
        if row['expect']=='return':
            argv=[*prefix,'--run',str(module)]
            result=subprocess.run(argv,capture_output=True,env=env,timeout=90)
            log=(result.stdout+result.stderr).decode(errors='replace')
            (out/(name+'.log')).write_text(log)
            metrics=[{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)} for line in log.splitlines() if '[gc-managed]' in line]
            assert result.returncode==0,(name,result.returncode,log[-2200:])
            if row.get('require_gc'):
                assert metrics and all(m['collections']>0 and m['roots_requested']==1 and m['disabled']==0 and m['reason']==0 for m in metrics),(name,metrics)
                if row.get('category')=='gc-exception-root':assert all(m['registered_exception_collections']>0 for m in metrics)
            record.update(native_execution_qualified=True,argv=argv,gc_metrics=metrics)
        else:record.update(native_execution_qualified=None,scope='Assembly/validation only; consult separate exact-PC native GDB evidence')
        records.append(record);(out/'progress.json').write_text(json.dumps(records,indent=2))
        print('PASS',name,record['expect'],flush=True)
    assert pathlib.Path('/proc/self/cgroup').read_text().strip()==group
    (out/'summary.json').write_text(json.dumps(dict(qualified=True,cgroup=group,cpu=args.cpu,binary_sha256=sha(binary),tool_sha256=sha(tool),records=records,
        executed_cases=sum(x['native_execution_qualified'] is True for x in records),scope=__doc__),indent=2))

if __name__=='__main__':main()
