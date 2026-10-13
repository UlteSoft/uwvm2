#!/usr/bin/env python3
"""Build/run actual mixed-width memory64 LLVM bulk calls with the bundled ROS LLVM provider.

This emits and executes native typed bridge calls, saving IR and object
code. A fixture-owned terminal trap sink checks diagnostic arguments. It is
not whole-VM memory64, stack-unwind or frontend qualification.
"""
import argparse,hashlib,json,os,re,resource,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path)
p.add_argument('--backend',choices=['mmap','allocator','threaded-allocator'],default='mmap')
p.add_argument('--objdump',type=Path,default=Path('/toolchain/bin/llvm-objdump'))
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
fingerprint=['python3',str(root/'tools/ci/wasm3_source_fingerprint.py'),str(root)]
before=subprocess.check_output(fingerprint+[str(a.output/'source-before.json')],text=True).strip()
build=Path('/work/artifacts/uwvm2-ros-jit/llvm');source=Path('/work/uwvm2-ros/third-parties/llvm/llvm')
compile=['/toolchain/bin/clang++','-std=c++26','-O2','-fno-rtti','-ffp-model=precise','-Wno-deprecated-declarations',
 '-Wno-undefined-inline','-DUWVM=2','-DUWVM_TEST=2','-DUWVM_USE_UWVM_INT','-DUWVM_USE_LLVM_JIT','-DUWVM_USE_THREAD_LOCAL',
 '-include','uwvm2/uwvm/io/impl.h','-Isrc','-Ithird-parties/fast_io/include','-Ithird-parties/bizwen/include',
 '-Ithird-parties/boost_unordered/include','-I'+str(build/'include'),'-I'+str(source/'include'),'-stdlib=libc++']
if a.backend!='mmap':compile+=['-DUWVM_FORCE_DISABLE_MMAP']
if a.backend=='threaded-allocator':compile+=['-DUWVM_USE_MULTITHREAD_ALLOCATOR']
rows=[]
def run(label,cmd,timeout=300):
 rows.append(dict(label=label,command=[str(x) for x in cmd]));(a.output/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
 with (a.output/(label+'.log')).open('wb') as f:r=subprocess.run(cmd,cwd=root,stdout=f,stderr=f,timeout=timeout)
 rows[-1]['exit']=r.returncode;(a.output/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
 if r.returncode:raise RuntimeError((label,r.returncode,a.output))
 return (a.output/(label+'.log')).read_text()
run('compile',[*compile,'-c','test/0014.llvm_jit/llvm_memory64_bulk.cc','-o',str(a.output/'test.o')])
run('link',['/toolchain/bin/clang++',str(a.output/'test.o'),'@'+str(build/'consumer-link.rsp'),
 '-stdlib=libc++','-fuse-ld=lld','-rtlib=compiler-rt','-unwindlib=libunwind','-pthread','-o',str(a.output/'test')])
for mode,flags in [('baseline',[]),('O3',['--optimize-ir'])]:
 out=a.output/mode;out.mkdir()
 log=run(mode,[str(a.output/'test'),str(out),*flags]);assert 'PASS memory64 actual LLVM bulk: 6 configurations' in log,log
 print(log,end='',flush=True)
 assembly=run(mode+'-assembly',[str(a.objdump),'-dr',str(out/'memory64-bulk.o')])
 assert len(re.findall(r'<bulk_(?:copy|fill)_[^>]+>:',assembly))==6

symbols=subprocess.check_output([str(a.objdump.with_name('llvm-nm')),'--defined-only','-j',str(a.output/'test')],text=True).splitlines()
symbols=[s for s in symbols if 'llvm_jit_memory64_copy_bridge' in s or 'llvm_jit_memory64_fill_bridge' in s]
assert len(symbols)>=6
run('native-bridge-assembly',[str(a.objdump),'-d','--demangle','--disassemble-symbols='+','.join(symbols),str(a.output/'test')])
summary=dict(passed=True,objdump=str(a.objdump),scope='actual x86-64 LLVM bulk bridge calls; no frontend or unwind qualification',
 source_id=before,inputs={str(x):hashlib.file_digest(x.open('rb'),'sha256').hexdigest() for x in
  [root/'test/0014.llvm_jit/llvm_memory64_bulk.cc',build/'include/llvm/Config/llvm-config.h',build/'consumer-link.rsp']})
summary['provider_version']=(build/'include/llvm/Config/llvm-config.h').read_text().split('#define LLVM_VERSION_STRING ')[1].splitlines()[0]
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
after=subprocess.check_output(fingerprint+[str(a.output/'source-after.json')],text=True).strip();assert before==after
