#!/usr/bin/env python3
"""Link isolated actual-VM observer tests against a frozen runtime object."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--runtime-build',type=Path,required=True)
p.add_argument('--wasm-tools',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
a.source_root=a.source_root.resolve();a.runtime_build=a.runtime_build.resolve();a.out=a.out.resolve()
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
a.out.mkdir(parents=True,exist_ok=False)
sha=lambda x:hashlib.file_digest(x.open('rb'),'sha256').hexdigest()
source=Path(__file__).with_name('llvm_debug_observer.cc')
(a.out/'fixture.cc').write_bytes(source.read_bytes());(a.out/'runner.py').write_bytes(Path(__file__).read_bytes())
runtime=a.runtime_build/'runtime.o';before=sha(runtime)
command=shlex.split((a.runtime_build/'test.command').read_text())
old=next(x for x in command if x.startswith('test/') and x.endswith('.cc'))
command[command.index(old)]=str(source)
command[command.index('-o')+1]=str(a.out/'probe')
command+=['-iquote',str(a.source_root/'test/0017.runtime')]
(a.out/'build.command').write_text(shlex.join(command)+'\n')
with (a.out/'build.log').open('w') as log:
    subprocess.run(command,cwd=a.source_root,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
wat='(module\n (import "observer-host" "marker" (func $marker (result i32)))\n (tag $e (param i32))\n (func $warm call $marker drop)\n (func $leaf (result i32) i32.const 73 throw $e)\n (func $work (result i32)\n  (block $caught (result i32)\n   (try_table (catch $e $caught) call $leaf drop) unreachable)\n  i32.const 1 i32.add))\n'
(a.out/'fixture.wat').write_text(wat)
subprocess.run([a.wasm_tools,'parse',a.out/'fixture.wat','-o',a.out/'fixture.wasm'],check=True)
subprocess.run([a.wasm_tools,'validate',a.out/'fixture.wasm'],check=True)
rows=[]
for policy in ('instruction','unwind','none'):
    cmd=[str(a.out/'probe'),str(a.out/'fixture.wasm'),policy]
    with (a.out/(policy+'.log')).open('w') as log:
        r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60)
    text=(a.out/(policy+'.log')).read_text()
    rows.append(dict(command=cmd,exit=r.returncode,passed=r.returncode==0 and 'PASS ' in text))
    (a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(policy,text,flush=True)
    assert rows[-1]['passed']
assert sha(runtime)==before
runtime_manifest=json.loads((a.runtime_build/'source-before.json').read_text())
(a.out/'runtime-source-before.json').write_text(json.dumps(runtime_manifest,indent=2)+'\n')
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,executions=len(rows),host_optimization='O1',
    runtime_path=str(runtime),runtime_sha256=before,fixture_sha256=sha(source),wasm_sha256=sha(a.out/'fixture.wasm'),binary_sha256=sha(a.out/'probe'),
    cgroup=Path('/proc/self/cgroup').read_text(),scope='Actual LLVM full instruction observer/prepare, guest-thread owning trace, before-park cold capture, native throw/catch stepping and reset ownership.'),indent=2)+'\n')
