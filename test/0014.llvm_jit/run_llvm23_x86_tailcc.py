#!/usr/bin/env python3
"""Qualify a linked X86 LLVM provider before enabling its guaranteed tail ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--llc',type=Path,required=True)
p.add_argument('--clang',type=Path,required=True)
p.add_argument('--expect-broken',action='store_true')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
source=Path(__file__).parent/'fixtures/llvm23-x86-tailcc-stack.ll'
rows=[]
for model in ['small','large']:
 obj=a.output/(model+'.o');exe=a.output/(model+'.exe')
 command=[str(a.llc),'-O2','-mtriple=x86_64-unknown-linux-gnu','-relocation-model=pic','-code-model='+model,'-filetype=obj',str(source),'-o',str(obj)]
 subprocess.run(command,check=True)
 subprocess.run([str(a.llc),'-O2','-mtriple=x86_64-unknown-linux-gnu','-relocation-model=pic','-code-model='+model,str(source),'-o',str(a.output/(model+'.s'))],check=True)
 subprocess.run([str(a.clang),'-fuse-ld=lld','-rtlib=compiler-rt',str(obj),'-o',str(exe)],check=True)
 run=subprocess.run([str(exe)],capture_output=True,timeout=30)
 rows.append(dict(model=model,command=command,exit=run.returncode,passed=run.returncode==0,object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest()))
summary=dict(provider_version=subprocess.check_output([str(a.llc),'--version'],text=True),
             source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),runs=rows,
             qualified=all(r['passed'] for r in rows),expected_broken=a.expect_broken)
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
assert summary['qualified'] != a.expect_broken, summary
print('Reproduced broken provider' if a.expect_broken else 'PASS native tailcc: two code models, 1000001 stack-argument transfers each')
