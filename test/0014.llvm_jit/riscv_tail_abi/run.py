#!/usr/bin/env python3
"""Qualify the exact target-host LLVM under QEMU; never compile on the local host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--llvm-root',type=Path,required=True)
p.add_argument('--llvm-source-root',type=Path,required=True)
p.add_argument('--reuse-probe',type=Path,help='Reuse only a driver built from identical source and exact same provider inputs')
p.add_argument('--mode',choices=['baseline','stack-fix','patched'],required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[3]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False);a.out=a.out.resolve()
source=Path(__file__).resolve().parent;sha=lambda x:hashlib.file_digest(x.open('rb'),'sha256').hexdigest()
inputs=[source/'probe.cc',source/'fixtures.py',Path(__file__).resolve(),a.llvm_root/'consumer-link.rsp',*sorted((a.llvm_root/'lib').glob('*.a')),*sorted((a.llvm_root/'include/llvm/Config').glob('*'))]
inputs=[x for x in inputs if x.is_file()];before={str(x):sha(x) for x in inputs};(a.out/'inputs.json').write_text(json.dumps(before,indent=2)+'\n')
commands=[]
def run(name,command,expected=0,contains=None,timeout=300):
    (a.out/(name+'.command')).write_text(shlex.join([str(x) for x in command])+'\n')
    r=subprocess.run([str(x) for x in command],capture_output=True,timeout=timeout)
    text=(r.stdout+r.stderr).decode(errors='replace');(a.out/(name+'.log')).write_text(text)
    passed=(r.returncode!=0 if expected=='nonzero' else r.returncode==expected) and (contains is None or contains in text)
    commands.append(dict(name=name,exit=r.returncode,expected=expected,contains=contains,passed=passed,command=[str(x) for x in command]))
    (a.out/'runs.json').write_text(json.dumps(commands,indent=2)+'\n')
    if not passed:raise RuntimeError((name,r.returncode,text[-6000:]))
    print('OBSERVED FAILURE' if name.startswith('run-') and expected!=0 else 'PASS',name,flush=True)
    return text
run('generate',[sys.executable,source/'fixtures.py',a.out/'fixtures'])
flags=[os.environ.get('CXX','/toolchain/bin/clang++'),'--target=riscv64-linux-gnu','--sysroot=/work/deps','--gcc-install-dir=/work/deps/usr/lib/gcc-cross/riscv64-linux-gnu/15','-idirafter','/work/deps/usr/riscv64-linux-gnu/include','-std=c++26','-stdlib=libstdc++','-fno-rtti','-fexceptions','-fasynchronous-unwind-tables','-O1','-g0','-I'+str(a.llvm_root/'include'),'-I'+str(a.llvm_source_root/'include')]
if a.reuse_probe:
    original=a.reuse_probe.resolve();old_inputs=json.loads((original.parent/'inputs.json').read_text())
    previous_sources=[v for n,v in old_inputs.items() if n.endswith('/probe.cc')]
    assert previous_sources==[sha(source/'probe.cc')], 'probe source changed'
    for path,digest in before.items():
        if path.startswith(str(a.llvm_root)):
            assert old_inputs.get(path)==digest, 'linked LLVM provider changed'
    (a.out/'probe').symlink_to(original)
    (a.out/'reused-probe.json').write_text(json.dumps({'original':str(original),'sha256':sha(original),'source_and_provider_unchanged':True},indent=2)+'\n')
else:
    run('build',flags+[source/'probe.cc','@'+str(a.llvm_root/'consumer-link.rsp'),'-O1','-latomic','-ldl','-pthread','-o',a.out/'probe'])
qemu=['/work/deps/usr/bin/qemu-riscv64','-U','LD_LIBRARY_PATH','-E','LD_LIBRARY_PATH=/work/deps/usr/lib/riscv64-linux-gnu:/work/deps/usr/riscv64-linux-gnu/lib','-L','/work/deps/usr/riscv64-linux-gnu',str(a.out/'probe')]
cases=json.loads((a.out/'fixtures/cases.json').read_text())
for case in cases:
    name=case['name'];path=a.out/'fixtures'/(name+'.ll')
    run('verify-'+name,qemu+[path,'verify','0','return','0','0'],expected=0 if case['valid'] else 4,contains='PASS verifier' if case['valid'] else case['diagnostic'])
for case in cases:
    if not case['valid']:continue
    name=case['name'];path=a.out/'fixtures'/(name+'.ll');obj=a.out/(name+'.o')
    command=qemu+[path,obj,str(case['expected']),case['kind'],str(case['maximum_stack_span']),str(case['minimum_observations'])]
    if a.mode!='patched' and not case['baseline']:
        if name=='tail-same12-permute':run('run-'+name,command,expected='nonzero',contains='Unsupported calling convention')
        continue
    known_failure=case.get('baseline_failure') if a.mode=='baseline' else None
    run('run-'+name,command,expected='nonzero' if known_failure else 0,contains=None if known_failure else 'PASS LLVM')
    run('assembly-'+name,['/toolchain/bin/llvm-objdump','-dr','-M','no-aliases',obj])
    run('frames-'+name,['/toolchain/bin/llvm-dwarfdump','--eh-frame',obj],contains='.eh_frame')
assert before=={str(x):sha(x) for x in inputs},'exact provider inputs changed during qualification'
state={n:(Path('/sys/fs/cgroup')/n).read_text().strip() for n in ['memory.max','memory.swap.max','memory.events','cpuset.cpus.effective']}
summary=dict(passed=True,mode=a.mode,verified_cases=len(cases),executed_cases=sum(row['name'].startswith('run-') and row['expected']==0 for row in commands),binary_sha256=sha(a.out/'probe'),cgroup=state,scope='Exact target-provider LLVM IR/MCJIT qualification, separate from complete VM behavior; baseline mode explicitly demonstrates absent Tail ABI and the existing C musttail stack-slot failure; these expected failures are not implementation successes.')
(a.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary),flush=True)
