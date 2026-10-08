#!/usr/bin/env python3
"""Execute the actual Core 3 reference JIT under QEMU with separate stack policies.

This checks full module translation/materialization, host entry, reference payloads,
branch PHIs and null-trap stack diagnostics. It is a focused target profile, not
complete Core 3 or the final all-platform JIT acceptance matrix.
"""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('out',type=Path)
p.add_argument('--llvm-root',type=Path,required=True)
p.add_argument('--llvm-source-root',type=Path,required=True)
p.add_argument('--llvm-build-manifest',type=Path,required=True)
p.add_argument('--target',choices=['aarch64-linux-gnu','arm-linux-gnueabihf'],required=True)
p.add_argument('--suite',choices=['nonnull','branches','ref-func'],action='append')
a=p.parse_args();root=Path(__file__).resolve().parents[2];os.chdir(root)
subprocess.run(['bash','tools/ci/require_wasm3_test_cgroup.sh'],check=True)
import resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
def audit_resources(label):
 subprocess.run(['bash','tools/ci/require_wasm3_test_cgroup.sh'],check=True)
 fields=['memory.max','memory.swap.max','memory.peak','memory.events','cpuset.cpus.effective']
 (a.out/(label+'-cgroup.json')).write_text(json.dumps({f:(Path('/sys/fs/cgroup')/f).read_text().strip() for f in fields},indent=2)+'\n')
audit_resources('before')
def digest(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
manifest=a.llvm_source_root/'sources.sha256'
assert digest(manifest)==a.llvm_build_manifest.read_text().split()[0],'bundled LLVM source identity changed'
assert '23.1.1-uwvm-ros.9' in (a.llvm_root/'include/llvm/Config/llvm-config.h').read_text()
libs=shlex.split((a.llvm_root/'consumer-link.rsp').read_text())
inputs=[a.llvm_root/'consumer-link.rsp',manifest,*sorted((a.llvm_root/'include').rglob('*.h'))]
inputs += [Path(x) for x in libs if x.endswith('.a')]
for path in inputs:assert path.is_file(),path
toolchain={str(path):digest(path) for path in inputs}
(a.out/'toolchain.json').write_text(json.dumps(toolchain,indent=2)+'\n')
source_id=subprocess.check_output(['python3','tools/ci/wasm3_source_fingerprint.py','.',str(a.out/'source-before.json')],text=True).strip()
triple=a.target;deps=Path('/work/deps');sysroot=deps/'usr'/triple
flags=['/toolchain/bin/clang++',f'--target={triple}',f'--sysroot={deps}',f'--gcc-install-dir={deps}/usr/lib/gcc-cross/{triple}/15',
 '-idirafter',str(sysroot/'include'),'-stdlib=libstdc++','-std=c++26','-O1','-g0','-fno-rtti','-fasynchronous-unwind-tables','-Wno-undefined-inline',
 '-DUWVM=2','-DUWVM_USE_LLVM_JIT','-DUWVM_DISABLE_INT','-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1',
 '-DUWVM_FORCE_DISABLE_MMAP','-DUWVM_USE_MULTITHREAD_ALLOCATOR','-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519',
 '-DUWVM2_BUILD_SOURCE_ID=u8"'+source_id+'"']
for path in [a.llvm_root/'include',a.llvm_source_root/'llvm/include',deps/'usr/include'/triple,deps/'usr/include',
 Path('src'),Path('third-parties/bizwen/include'),Path('third-parties/fast_io/include'),Path('third-parties/boost_unordered/include')]:
 flags+=['-I',str(path)]
rows=[]
def run(label,command,timeout=900,env=None):
 (a.out/(label+'.command')).write_text(shlex.join(command)+'\n')
 with (a.out/(label+'.log')).open('w') as log:
  result=subprocess.run(command,stdout=log,stderr=log,timeout=timeout,env=env)
 rows.append(dict(label=label,command=command,exit=result.returncode))
 (a.out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 if result.returncode:raise RuntimeError(f'{label} failed; inspect {a.out}')
 return (a.out/(label+'.log')).read_text()
run('runtime-build',[*flags,'-c','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp','-o',str(a.out/'runtime.o')])
sources={'nonnull':'test/0013.uwvm_int/wasm3/ref_as_non_null.cc',
 'branches':'test/0013.uwvm_int/wasm3/ref_branches.cc',
 'ref-func':'test/0013.uwvm_int/wasm3/ref_func_identity.cc'}
for suite in a.suite or sources:
 directory=a.out/suite;directory.mkdir();binary=directory/'fixture';source=Path(sources[suite])
 run(suite+'-build',[*flags,'-DUWVM2TEST_RUNNER_USE_LLVM_JIT','-DUWVM2TEST_STRICT_NO_INTERPRETER','-fuse-ld=lld',str(source),
  'src/uwvm2/uwvm/host_api.default.cpp',str(a.out/'runtime.o'),'@'+str(a.llvm_root/'consumer-link.rsp'),'-O1',
  '-L'+str(deps/'usr/lib'/triple),'-L'+str(sysroot/'lib'),'-lssl','-lcrypto','-latomic','-ldl','-pthread','-o',str(binary)])
 # QEMU -L redirects opening '/' into its sysroot; relative dirfd traversal stays in the host-mounted test tree.
 env=os.environ.copy();(directory/'ir').mkdir();(directory/'cache').mkdir();env['UWVM_SHARED_IR_DIR']=str(directory/'ir');env['UWVM_MEMORY64_CACHE_DIR']=os.path.relpath(directory/'cache',root)
 for policy in ['instruction','unwind']:
  arguments=policy.split('-')
  run(suite+'-'+policy,[str(deps/('usr/bin/qemu-aarch64' if triple.startswith('aarch64') else 'usr/bin/qemu-arm')),'-U','LD_LIBRARY_PATH','-E',
   'LD_LIBRARY_PATH='+str(deps/'usr/lib'/triple)+':'+str(sysroot/'lib'),'-L',str(sysroot),str(binary),*arguments],300,env)
  log=(a.out/(suite+'-'+policy+'.log')).read_text();assert {'nonnull':'PASS 1538 ref.as_non_null executions','branches':'PASS 4096 reference branch executions','ref-func':'PASS 3072 native ref.func identities'}[suite] in log,log
  assert list((directory/'cache'/policy).rglob('*.uwvm-ljc')), 'missing actual target JIT object'
  assert 'Failed to create LLVM JIT cache directory' not in log,log
  if triple=='aarch64-linux-gnu':
   # Decode the runtime's signed cache object, not the separately saved translator IR.
   import sys,re
   sys.path.insert(0,str(root/'test/0014.llvm_jit'))
   from check_wasm3_native_frame_codegen import decode_object
   cached=list((directory/'cache'/policy).rglob('*.uwvm-ljc'));assert cached
   if suite!='ref-func':assert len(cached)==1
   checked=[]
   for object_index,cache_object in enumerate(cached):
    obj=directory/(policy+'-'+str(object_index)+'.o');obj.write_bytes(decode_object(cache_object.read_bytes(),not (root/'src/uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator').is_dir()))
    label=suite+'-'+policy+'-'+str(object_index)
    assembly=run(label+'-assembly',['/toolchain/bin/llvm-objdump','-dr',str(obj)])
    sections=run(label+'-sections',['/toolchain/bin/llvm-objdump','-h',str(obj)]);assert '.eh_frame' in sections
    chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly)
    for index in range(1,len(chunks),3):
     name,body=chunks[index+1:index+3]
     if not re.fullmatch(r'uwvm_m_[0-9a-f]+_func_'+('[345]' if suite=='ref-func' else '[0-9]+'),name):continue
     calls=len(re.findall(r'\bblr?\s',body));expected=(2 if policy=='instruction' else 0)+(1 if suite=='nonnull' else 0)
     assert calls==expected,(name,body);checked.append(dict(function=name,calls=calls))
   assert len(checked)=={'nonnull':6,'branches':8,'ref-func':3}[suite]
   (directory/(policy+'-assembly.json')).write_text(json.dumps(checked,indent=2)+'\n')
  print(log,flush=True)
 (directory/'inputs.json').write_text(json.dumps(dict(test_sha256=digest(source),binary_sha256=digest(binary)),indent=2)+'\n')
after=subprocess.check_output(['python3','tools/ci/wasm3_source_fingerprint.py','.',str(a.out/'source-after.json')],text=True).strip()
assert source_id==after and all(digest(Path(path))==value for path,value in toolchain.items())
audit_resources('after')
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,source_id=source_id,llvm='23.1.1-uwvm-ros.9',
 target=triple,memory_backend='concurrent-allocator',suites=a.suite or list(sources),rows=rows),indent=2)+'\n')
