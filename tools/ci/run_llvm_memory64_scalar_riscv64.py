#!/usr/bin/env python3
"""Execute real LLVM23 memory64 scalar fallback code under RISC-V64 QEMU."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
fingerprint=['python3',str(root/'tools/ci/wasm3_source_fingerprint.py'),str(root)]
before=subprocess.check_output(fingerprint+[str(a.output/'source-before.json')],text=True).strip()
llvm=Path('/work/artifacts/uwvm2-ros-riscv64-unwind-abi-r5/llvm');llvm_source=Path('/work/uwvm2-ros/third-parties/llvm/llvm')
# Verify the existing target libraries against their earlier qualified toolchain manifest.
qualified=json.loads(Path('/work/artifacts/shared-r63/uwvm2-ros/riscv64-drain/toolchain.json').read_text())
for file,expected in qualified.items():
 assert hashlib.file_digest(Path(file).open('rb'),'sha256').hexdigest()==expected,file
(a.output/'toolchain.json').write_text(json.dumps(qualified,indent=2)+'\n')
assert '23.1.1-uwvm-ros.9' in (llvm/'include/llvm/Config/llvm-config.h').read_text()
deps=Path('/work/deps');triple='riscv64-linux-gnu';sysroot=deps/'usr'/triple
flags=['/toolchain/bin/clang++',f'--target={triple}',f'--sysroot={deps}',f'--gcc-install-dir={deps}/usr/lib/gcc-cross/{triple}/15',
 '-idirafter',str(sysroot/'include'),'-stdlib=libstdc++','-std=c++26','-O2','-g0','-fno-rtti','-ffp-model=precise',
 '-Wno-undefined-inline','-DUWVM=2','-DUWVM_TEST=2','-DUWVM_USE_UWVM_INT','-DUWVM_USE_LLVM_JIT','-DUWVM_USE_THREAD_LOCAL',
 '-DUWVM_FORCE_DISABLE_MMAP','-DUWVM_USE_MULTITHREAD_ALLOCATOR','-include','uwvm2/uwvm/io/impl.h']
for path in [llvm/'include',llvm_source/'include',root/'src',root/'third-parties/fast_io/include',root/'third-parties/bizwen/include',root/'third-parties/boost_unordered/include']:
 flags+=['-I',str(path)]
rows=[]
def run(label,cmd,timeout=300):
 row=dict(label=label,command=list(map(str,cmd)));rows.append(row)
 (a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 with (a.output/(label+'.log')).open('wb') as log:result=subprocess.run(cmd,cwd=root,stdout=log,stderr=log,timeout=timeout)
 row['exit']=result.returncode;(a.output/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 if result.returncode:raise RuntimeError((label,result.returncode))
 return (a.output/(label+'.log')).read_text()
run('compile',[*flags,'-c','test/0014.llvm_jit/llvm_memory64_scalar_bridge.cc','-o',str(a.output/'test.o')])
run('link',[*flags,'-fuse-ld=lld',str(a.output/'test.o'),'@'+str(llvm/'consumer-link.rsp'),'-L'+str(sysroot/'lib'),'-latomic','-ldl','-pthread','-o',str(a.output/'test')])
for mode,extra in [('baseline',[]),('O3',['--optimize-ir'])]:
 out=a.output/mode;out.mkdir()
 log=run(mode,[str(deps/'usr/bin/qemu-riscv64'),'-U','LD_LIBRARY_PATH','-E',
  'LD_LIBRARY_PATH='+str(deps/'usr/lib'/triple)+':'+str(sysroot/'lib'),'-L',str(sysroot),str(a.output/'test'),str(out),*extra])
 assert 'PASS memory64 actual LLVM scalar bridges: 46 configurations, 322 checks' in log,log
 print(log,end='',flush=True)
 run(mode+'-assembly',['/toolchain/bin/llvm-objdump','-dr',str(out/'memory64-scalar-bridge.o')])
after=subprocess.check_output(fingerprint+[str(a.output/'source-after.json')],text=True).strip();assert before==after
(a.output/'summary.json').write_text(json.dumps(dict(passed=True,source_id=before,target=triple,llvm='23.1.1-uwvm-ros.9',
 scope='actual QEMU LLVM scalar bridge execution with threaded allocator; no whole-VM or unwind claim',
 fixture_sha256=hashlib.file_digest((root/'test/0014.llvm_jit/llvm_memory64_scalar_bridge.cc').open('rb'),'sha256').hexdigest(),runs=rows),indent=2)+'\n')
