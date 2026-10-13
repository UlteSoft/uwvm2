#!/usr/bin/env python3
"""Run the actual typed-EH MCJIT probe on an existing target-host LLVM under QEMU.

No LLVM libraries or VM runtime are rebuilt. Cross-object lowering is not a
substitute for this test: this binary executes its own native MCJIT code.
"""
import argparse,hashlib,json,os,resource,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--llvm-root',type=Path,required=True)
p.add_argument('--llvm-source-root',type=Path,required=True)
p.add_argument('--probe',choices=['symbols','landingpad','host'],default='symbols')
p.add_argument('--target',choices=['aarch64-linux-gnu','riscv64-linux-gnu','i686-linux-gnu'],required=True)
p.add_argument('--deps',type=Path,default=Path('/work/deps'))
p.add_argument('--qemu',type=Path,required=True)
p.add_argument('--trace',action='store_true',help='Print probe check numbers to locate target failures')
p.add_argument('--large-code-model',action='store_true',help='Qualify full-range EH calls explicitly; does not change product target configuration')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False);out=a.out.resolve()
fields=['memory.max','memory.swap.max','memory.peak','memory.events','cpuset.cpus.effective']
def resources(label):
 (out/(label+'-cgroup.json')).write_text(json.dumps({f:(Path('/sys/fs/cgroup')/f).read_text().strip() for f in fields},indent=2)+'\n')
resources('before')
def sha(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
source=root/('test/0014.llvm_jit/native_exception_'+a.probe+'.cc')
header=root/'src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h'
rsp=a.llvm_root/'consumer-link.rsp';libs=shlex.split(rsp.read_text())
inputs=[source,header,Path(__file__).resolve(),rsp,*sorted((a.llvm_root/'include').rglob('*.h')),*[Path(x) for x in libs if x.endswith('.a')]]
if a.probe in ('landingpad','host'):inputs.append(root/'src/uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h')
if a.probe=='host':inputs += [root/'src/uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h',root/'src/uwvm2/runtime/exception/value.h']
before={str(x):sha(x) for x in inputs}
(out/'input-sha256.json').write_text(json.dumps(before,indent=2)+'\n')
triple=a.target;sysroot=a.deps/'usr'/triple
multiarch='i386-linux-gnu' if triple=='i686-linux-gnu' else triple
flags=[os.environ.get('CXX','/toolchain/bin/clang++'),f'--target={triple}',f'--sysroot={a.deps}',f'--gcc-install-dir={a.deps}/usr/lib/gcc-cross/{triple}/15',
 '-idirafter',str(sysroot/'include'),'-stdlib=libstdc++','-std=c++26','-O1','-g0','-fno-rtti','-fexceptions','-fasynchronous-unwind-tables','-DUWVM2TEST_EH_JIT_DEFAULT_MODEL',
 '-I'+str(a.llvm_root/'include'),'-I'+str(a.llvm_source_root/'include'),'-I'+str(root/'src')]
if a.trace:flags.append('-DUWVM2TEST_TRACE_EH_IMPORT')
if a.large_code_model:flags.append('-DUWVM2TEST_EH_LARGE_CODE_MODEL')
rows=[]
def run(label,command,timeout=300):
 (out/(label+'.command')).write_text(shlex.join(command)+'\n')
 with (out/(label+'.log')).open('w') as log:result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
 rows.append(dict(label=label,exit=result.returncode,command=command));(out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 if result.returncode:raise RuntimeError((label,result.returncode))
run('compile',[*flags,'-c',str(source),'-o',str(out/'probe.o')])
run('link',[*flags,'-fuse-ld=lld',str(out/'probe.o'),'@'+str(rsp),'-O1','-L'+str(sysroot/'lib'),'-latomic','-ldl','-pthread','-o',str(out/'probe')])
run('qemu',[str(a.qemu),'-U','LD_LIBRARY_PATH','-E','LD_LIBRARY_PATH='+str(a.deps/'usr/lib'/multiarch)+':'+str(sysroot/'lib'),'-L',str(sysroot),str(out/'probe'),str(out)],timeout=120)
if a.probe=='symbols':
 assert 'PASS native EH symbol imports: 53 checks' in (out/'qemu.log').read_text()
 assert (out/'first.ll').read_bytes()==(out/'second.ll').read_bytes()
 objects=[out/'native-eh.o']
else:
 assert ('PASS real guest EH host bindings:' if a.probe=='host' else 'PASS product EH landingpads:') in (out/'qemu.log').read_text()
 objects=[out/(policy+'.o') for policy in ('instruction','unwind')]
 for policy in ('instruction','unwind'):
  assert (out/(policy+'-cold.ll')).read_bytes()==(out/(policy+'-warm.ll')).read_bytes()
  ir=(out/(policy+'-cold.ll')).read_text()
  assert ('probe_trace_push' in ir)==(policy=='instruction')
  assert ('probe_trace_pop' in ir)==(policy=='instruction')
for obj in objects:
 run(obj.stem+'-sections',['/toolchain/bin/llvm-readobj','--sections','--relocations','--symbols',str(obj)])
 run(obj.stem+'-frames',['/toolchain/bin/llvm-dwarfdump','--eh-frame',str(obj)])
 run(obj.stem+'-disassembly',['/toolchain/bin/llvm-objdump','-dr',str(obj)])
 metadata=(out/(obj.stem+'-sections.log')).read_text()
 assert 'uwvm_guest_exception_typeinfo_v1' in metadata and '__gxx_personality_v0' in metadata
 if a.probe in ('landingpad','host'):
  assert '__cxa_rethrow' in metadata and '__cxa_end_catch' in metadata
  assert ('probe_trace_push' in metadata)==(obj.stem=='instruction')
  assert ('probe_trace_pop' in metadata)==(obj.stem=='instruction')
assert before=={str(x):sha(x) for x in inputs},'input changed during run'
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resources('after')
(out/'summary.json').write_text(json.dumps(dict(passed=True,probe=a.probe,target=triple,code_model='large' if a.large_code_model else 'jit-default',runs=rows,binary_sha256=sha(out/'probe'),objects={x.name:sha(x) for x in objects},
 scope='actual target-host MCJIT typed catch, foreign resume, engine isolation and cached relocations under QEMU; guest VM EH remains disabled'),indent=2)+'\n')
print((out/'qemu.log').read_text(),end='')
