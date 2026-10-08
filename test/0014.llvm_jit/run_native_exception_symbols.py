#!/usr/bin/env python3
"""Build a focused native LLVM EH-symbol probe inside the remote Linux cgroup.

The fixture alone discovers C++ typeinfo with an Itanium runtime API. Production
symbol import code receives a qualified host binding and does not call that API.
"""
import argparse, hashlib, json, os, resource, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--llvm-config',type=Path,required=True)
p.add_argument('--llvm-tools',type=Path,help='Directory with llvm-readobj, llvm-dwarfdump and llvm-objdump (defaults beside llvm-config)')
a=p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False)
out=a.out.resolve()
def config(*args):return shlex.split(subprocess.check_output([str(a.llvm_config),*args],text=True))
flags=[x for x in config('--cxxflags') if not x.startswith('-std=') and x!='-fno-exceptions']
source=root/'test/0014.llvm_jit/native_exception_symbols.cc'
header=root/'src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h'
module=header.with_suffix('.cppm')
cross=root/'test/0014.llvm_jit/native_exception_symbols_cross.cc'
sources=(source,header,module,cross)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={str(x.relative_to(root)):sha(x) for x in sources}
command=[os.environ.get('CXX','clang++'),*flags,'-std=c++26','-fexceptions','-fno-rtti','-O1','-g0','-fuse-ld=lld','-rtlib=compiler-rt','-unwindlib=libunwind','-I'+str(root/'src'),str(source),'-o',str(out/'probe'),*config('--ldflags','--libs','core','executionengine','mcjit','nativecodegen','native','--system-libs'),'-pthread']
(out/'build.command').write_text(shlex.join(command)+'\n')
with (out/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
command=[str(out/'probe'),str(out)]
with (out/'run.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=60,check=True)
assert 'PASS native EH symbol imports:' in (out/'run.log').read_text()
assert before=={str(x.relative_to(root)):sha(x) for x in sources}
assert (out/'first.ll').read_bytes()==(out/'second.ll').read_bytes(),'IR must not contain process/engine-specific addresses'
# Module exports must compile independently of the textual header route.
command=[os.environ.get('CXX','clang++'),*flags,'-std=c++26','-fexceptions','-fno-rtti','-DUWVM_DISABLE_INT','-DUWVM_USE_LLVM_JIT','-I'+str(root/'src'),'--precompile',str(module),'-o',str(out/'native_exception_symbols.pcm')]
(out/'module.command').write_text(shlex.join(command)+'\n')
with (out/'module.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
# Foreign objects qualify metadata lowering, not execution of a foreign MCJIT.
command=[os.environ.get('CXX','clang++'),*flags,'-std=c++26','-fexceptions','-fno-rtti','-O1','-g0','-fuse-ld=lld','-rtlib=compiler-rt','-unwindlib=libunwind','-I'+str(root/'src'),str(cross),'-o',str(out/'cross-probe'),*config('--ldflags','--libs','core','executionengine','mcjit','x86codegen','aarch64codegen','riscvcodegen','--system-libs'),'-pthread']
(out/'cross-build.command').write_text(shlex.join(command)+'\n')
with (out/'cross-build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
with (out/'cross-run.log').open('w') as log:subprocess.run([str(out/'cross-probe'),str(out)],stdout=log,stderr=subprocess.STDOUT,timeout=60,check=True)
tool_dir=a.llvm_tools or a.llvm_config.parent
reader=tool_dir/'llvm-readobj'
dwarf=tool_dir/'llvm-dwarfdump'
objdump=tool_dir/'llvm-objdump'
objects=[out/'native-eh.o',*[out/(triple+'-'+mode+'.o') for mode in ('jit-default','pic') for triple in ('aarch64-unknown-linux-gnu','riscv64-unknown-linux-gnu','i386-unknown-linux-gnu')]]
for obj in objects:
 metadata=subprocess.check_output([str(reader),'--sections','--relocations','--symbols',str(obj)],text=True)
 obj.with_suffix('.metadata.log').write_text(metadata)
 for required in ('.gcc_except_table','.eh_frame','uwvm_guest_exception_typeinfo_v1','__gxx_personality_v0'):
  assert required in metadata,(obj,required)
 with obj.with_suffix('.frames.log').open('w') as log:subprocess.run([str(dwarf),'--eh-frame',str(obj)],stdout=log,stderr=subprocess.STDOUT,check=True)
 with obj.with_suffix('.disassembly.log').open('w') as log:subprocess.run([str(objdump),'-dr',str(obj)],stdout=log,stderr=subprocess.STDOUT,check=True)
assert before=={str(x.relative_to(root)):sha(x) for x in sources}
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
(out/'summary.json').write_text(json.dumps(dict(passed=True,source=before,binary_sha256=sha(out/'probe'),objects={x.name:sha(x) for x in objects},module_sha256=sha(out/'native_exception_symbols.pcm'),fixture_scope='LLVM EH symbol imports only; guest EH remains disabled'),indent=2)+'\n')
print((out/'run.log').read_text()+(out/'cross-run.log').read_text(),end='')
