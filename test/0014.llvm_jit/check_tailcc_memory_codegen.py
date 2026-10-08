#!/usr/bin/env python3
"""Compare real full-JIT memory kernels before/after a private typed ABI change.

Only function return cleanup and nop alignment may differ. Memory operations,
address calculations, guards, and loop control must remain identical. Timing
noise does not relax these assembly assertions.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
from check_wasm3_native_frame_codegen import decode_object
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('build',type=Path)
p.add_argument('fixtures',type=Path)
p.add_argument('output',type=Path)
p.add_argument('--llvm',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
def normalized(dis):
    parsed=[]
    for line in dis.splitlines():
        m=re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9]*)\s*(.*?)\s*$',line)
        if m and not m[2].startswith('nop'):
            parsed.append((int(m[1],16),m[2],m[3].split('#')[0].strip()))
    indices={addr:i for i,(addr,_,_) in enumerate(parsed)}
    out=[]
    for addr,op,operand in parsed:
        if op.startswith('ret'):out.append(('ret',''));continue
        if op.startswith('j'):
            m=re.fullmatch(r'0x([0-9a-f]+) <[^>]+>',operand)
            assert m and int(m[1],16) in indices, operand
            operand='instruction:'+str(indices[int(m[1],16)])
        out.append((op,operand))
    assert out and any(op.startswith('ret') for op,_ in out)
    assert not any(op.startswith('call') for op,_ in out), 'unwind memory kernel must not add bridge calls'
    return out
rows=[]
for kernel in ['scalar-aligned','scalar-unaligned','simd-aligned','load-scalar-aligned','load-scalar-unaligned','load-simd-aligned']:
    versions={}
    for version in ['baseline','current']:
        out=a.output/(kernel+'-'+version);out.mkdir()
        cache=out/'cache';cache.mkdir()
        env=dict(os.environ,UWVM_TEST_JIT_OBJECT_CACHE_DIR=str(cache.resolve()),UWVM_TEST_JIT_CALL_STACK='unwind')
        command=[str((a.build/'full-pbo3'/version).resolve()),str((a.fixtures/(kernel+'-legacy.wasm')).resolve()),'100003','1']
        run=subprocess.run(command,env=env,capture_output=True,timeout=60)
        (out/'run.log').write_bytes(run.stdout+run.stderr);run.check_returncode()
        files=list(cache.rglob('*.uwvm-ljc'));assert len(files)==1,files
        obj=out/'native.o';obj.write_bytes(decode_object(files[0].read_bytes(),False))
        symbols=subprocess.check_output([str(a.llvm/'llvm-nm'),'--defined-only',str(obj)],text=True)
        names=re.findall(r'\b(uwvm_m_[0-9a-f]+_func_0)$',symbols,re.M);assert len(names)==1,names
        dis=subprocess.check_output([str(a.llvm/'llvm-objdump'),'-dr','--disassemble-symbols='+names[0],str(obj)],text=True)
        (out/'assembly.txt').write_text(dis);versions[version]=normalized(dis)
        (out/'instructions.json').write_text(json.dumps(versions[version],indent=2)+'\n')
    assert versions['baseline']==versions['current'],kernel
    rows.append(dict(kernel=kernel,instructions=len(versions['current']),passed=True))
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,scope='x86-64 actual full-JIT mmap kernels, unwind',cases=rows,
    binaries={v:hashlib.sha256((a.build/'full-pbo3'/v).read_bytes()).hexdigest() for v in ['baseline','current']}),indent=2)+'\n')
print('PASS six actual JIT memory kernels: identical bodies; only return cleanup/nop alignment excluded')
