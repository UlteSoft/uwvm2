#!/usr/bin/env python3
"""Core3 branch label validation consistency in ROS interpreter/JIT full."""
import argparse,hashlib,json,resource,subprocess,sys,time
from pathlib import Path
P=argparse.ArgumentParser()
P.add_argument('--binary',type=Path,required=True)
P.add_argument('--source',type=Path,required=True)
P.add_argument('--source-id',required=True)
P.add_argument('--wasm-tools',type=Path,required=True)
P.add_argument('--wasmtime',type=Path,required=True)
P.add_argument('--out',type=Path,required=True)
a=P.parse_args()
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
assert Path('/sys/fs/cgroup/memory.max').read_text().strip()==str(64*1024**3)
assert Path('/sys/fs/cgroup/memory.swap.max').read_text().strip()=='0'
assert a.binary.is_file() and not a.out.exists()
a.out.mkdir(parents=True)
T=a.wasm_tools
W=a.wasmtime
assert T.is_file() and W.is_file()
actual_source_id=subprocess.check_output([sys.executable,str(a.source/'tools/ci/wasm3_source_fingerprint.py'),str(a.source),str(a.out/'source-before.json')],text=True).strip()
assert actual_source_id==a.source_id,(actual_source_id,a.source_id)
F=a.source/'test/0014.llvm_jit/fixtures'
CASES={
 'br_if': ('(func (export "_start") call 1 drop)',True),
 'br_on_null': ('(func (export "_start") ref.null func call 1 drop)',True),
 'br_on_non_null': ('(func (export "_start") ref.null func call 1 drop drop)',True),
}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows=[]
def run(label,cmd):
    t=time.monotonic();p=subprocess.run([str(c) for c in cmd],capture_output=True,timeout=120)
    log=a.out/(label+'.log');log.write_bytes(p.stdout+p.stderr)
    row={'label':label,'command':[str(c) for c in cmd],'exit':p.returncode,'log':str(log),'seconds':round(time.monotonic()-t,3),'compiled_marker':b'optimize-start' in p.stdout+p.stderr}
    rows.append(row);(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    return row,p.stdout+p.stderr
for name,(wrapper,_) in CASES.items():
    for validity in ('valid','invalid'):
        stem=f'branch_label_{name}_{validity}'
        wat=F/(stem+'.wat');raw=wat.read_text();assert raw.rstrip().endswith(')')
        generated=a.out/(stem+'-wrapped.wat')
        entry=wrapper if validity=='valid' else '(func (export "_start"))'
        generated.write_text(raw.rstrip()[:-1]+'\n  '+entry+'\n)\n')
        wasm=a.out/(stem+'.wasm')
        row,data=run(stem+'-parse',[T,'parse',generated,'-o',wasm]);assert row['exit']==0,(stem,data[-1000:])
        row,data=run(stem+'-oracle',[T,'validate',wasm]);assert (row['exit']==0)==(validity=='valid'),(stem,data[-1000:])
        if validity=='valid':
            row,data=run(stem+'-wasmtime',[W,'run','-C','cache=n',wasm]);assert row['exit']==0,(stem,data[-1000:])
        for mode,args in (('int-full',['-Rint']),('jit-full',['-Raot','-Rllvm-cache-path','disable'])):
            row,data=run(stem+'-'+mode,[a.binary,*args,'-WFE-function-references','-WFE-reference-types','-Rclog','err','--run',wasm])
            passed=(row['exit']==0)==(validity=='valid') and (validity=='valid' or b'Validation error' in data) and (mode!='jit-full' or validity!='valid' or row['compiled_marker'])
            row['passed']=passed
            (a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
            print(stem,mode,'PASS' if passed else 'FAIL','exit',row['exit'],'compiled',row['compiled_marker'],flush=True)
            assert passed,(stem,mode,data[-3000:])
after_source_id=subprocess.check_output([sys.executable,str(a.source/'tools/ci/wasm3_source_fingerprint.py'),str(a.source),str(a.out/'source-after.json')],text=True).strip()
assert after_source_id==a.source_id
result={'passed':True,'source_id':a.source_id,'binary_sha256':sha(a.binary),'wasm_tools_sha256':sha(T),'wasmtime_sha256':sha(W),'cases':6,'mode_runs':12,
        'fixture_sha256':{p.name:sha(p) for p in sorted(F.glob('branch_label_*.wat'))}}
(a.out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS',result,flush=True)
