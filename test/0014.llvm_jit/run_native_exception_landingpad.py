#!/usr/bin/env python3
"""Qualify product LLVM EH landingpad helpers in the remote Linux cgroup.

This is a focused native ABI test, not enabling guest exceptions in the VM.
Test-owned exception values avoid depending on the unfinished Core 3 value graph.
"""
import argparse, hashlib, json, os, resource, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--llvm-config',type=Path,required=True)
p.add_argument('--llvm-tools',type=Path,default=Path('/toolchain/bin'))
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
a.out.mkdir(parents=True,exist_ok=False);out=a.out.resolve()
def config(*args):return shlex.split(subprocess.check_output([str(a.llvm_config),*args],text=True))
flags=[x for x in config('--cxxflags') if not x.startswith('-std=') and x!='-fno-exceptions']
paths=[root/'test/0014.llvm_jit/native_exception_landingpad.cc',root/'test/0014.llvm_jit/run_native_exception_landingpad.py']+[root/('src/uwvm2/runtime/compiler/llvm_jit/'+f) for f in ('native_exception_landingpad.h','native_exception_landingpad.cppm','native_exception_symbols.h','native_exception_symbols.cppm')]
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={str(x.relative_to(root)):sha(x) for x in paths}
(out/'source-before.json').write_text(json.dumps(before,indent=2)+'\n')
command=[os.environ.get('CXX','clang++'),*flags,'-std=c++26','-stdlib=libc++','-fexceptions','-fno-rtti','-O1','-g0','-fuse-ld=lld','-rtlib=compiler-rt','-unwindlib=libunwind','-I'+str(root/'src'),str(paths[0]),'-o',str(out/'probe'),*config('--ldflags','--libs','core','executionengine','mcjit','nativecodegen','native','--system-libs'),'-pthread']
(out/'build.command').write_text(shlex.join(command)+'\n')
with (out/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
command=[str(out/'probe'),str(out)]
(out/'run.command').write_text(shlex.join(command)+'\n')
with (out/'run.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120,check=True)
assert 'PASS product EH landingpads:' in (out/'run.log').read_text()
assert before=={str(x.relative_to(root)):sha(x) for x in paths}
rows=[]
for policy in ('instruction','unwind'):
 assert (out/(policy+'-cold.ll')).read_bytes()==(out/(policy+'-warm.ll')).read_bytes(),'cached IR must not embed engine/process addresses'
 ir=(out/(policy+'-cold.ll')).read_text()
 assert ('probe_trace_push' in ir)==(policy=='instruction')
 assert ('probe_trace_pop' in ir)==(policy=='instruction')
 for suffix,args in [('metadata.log',['--sections','--relocations','--symbols','--unwind']),('assembly.log',['-dr'])]:
  tool='llvm-readobj' if suffix=='metadata.log' else 'llvm-objdump'
  with (out/(policy+'-'+suffix)).open('w') as log:subprocess.run([str(a.llvm_tools/tool),*args,str(out/(policy+'.o'))],stdout=log,stderr=subprocess.STDOUT,check=True)
 metadata=(out/(policy+'-metadata.log')).read_text()
 assert 'uwvm_guest_exception_typeinfo_v1' in metadata and '__gxx_personality_v0' in metadata
 assert '__cxa_rethrow' in metadata and '__cxa_end_catch' in metadata
 assert ('probe_trace_push' in metadata)==(policy=='instruction')
 assert ('probe_trace_pop' in metadata)==(policy=='instruction')
 rows.append(dict(policy=policy,object_sha256=sha(out/(policy+'.o')),cold_warm_identical_ir=True))
(out/'source-after.json').write_text(json.dumps({str(x.relative_to(root)):sha(x) for x in paths},indent=2)+'\n')
(out/'summary.json').write_text(json.dumps(dict(passed=True,inputs=before,binary_sha256=sha(out/'probe'),objects=rows,scope='product EH emit helpers; test-owned guest values; VM guest exception entry remains disabled'),indent=2)+'\n')
print((out/'run.log').read_text(),end='')
