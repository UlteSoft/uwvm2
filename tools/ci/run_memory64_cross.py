#!/usr/bin/env python3
"""Execute internal memory64 load/store opfuncs on native QEMU target ABIs.

Integrated suites additionally execute complete functions through the translator
using binary memory64 declarations. This remains focused qualification, not a
claim of whole-VM conformance.
Clang compiles the unchanged production headers; bundled LLVM emits the objects.
Both byref and musttail register-ring handlers run with both memory backends.
"""
import argparse,gzip,hashlib,json,resource,shutil,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'test/0014.llvm_jit'))
from run_wasm3_int_cross import PROFILES
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path);p.add_argument('--only',default='armhf-neon,aarch64-neon')
p.add_argument('--suite',default='integer,values,pages')
p.add_argument('--backend',choices=['allocator','threaded-allocator','mmap','both'],default='both')
a=p.parse_args();selected=set(a.only.split(','));suites=a.suite.split(',')
assert set(suites)<={'integer','values','pages','bulk','atomic','rmw','wait','init','metadata','initializer','simd','integrated-scalar','integrated-bulk','integrated-simd','integrated-atomic','integrated-table','recursive-binary','recursive-validation','reference-validation','gc-immediate'},suites
assert selected<={x[0] for x in PROFILES},selected
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.output.mkdir(parents=True,exist_ok=False)
fingerprint=['python3',str(root/'tools/ci/wasm3_source_fingerprint.py'),str(root)]
before=subprocess.check_output(fingerprint+[str(a.output/'source-before.json')],text=True).strip()
deps=Path('/work/deps');llvm=Path('/work/artifacts/uwvm2-ros-jit/llvm/bin');rows=[]
for name,triple,cpu,features,emulator,_,flags in PROFILES:
 if name not in selected:continue
 gcc=deps/'usr/lib/gcc-cross'/triple/'15';sysroot=deps/'usr'/triple
 compiler=['/toolchain/bin/clang++',f'--target={triple}',f'--sysroot={deps}',f'--gcc-install-dir={gcc}',
  '-idirafter',str(sysroot/'include'),'-stdlib=libstdc++','-fuse-ld=lld','-O2',*flags]
 target=['-Xclang','-target-cpu','-Xclang',cpu]
 for f in features.split(','):target+=['-Xclang','-target-feature','-Xclang',f]
 includes=sum((['-I',str(root/entry)] for entry in ('src','third-parties/fast_io/include','third-parties/bizwen/include','third-parties/boost_unordered/include')),[])
 link_flags=['--ld-path='+str(deps/'usr/bin'/(triple+'-ld'))] if triple in {'powerpc64-linux-gnu','powerpc-linux-gnu','sparc64-linux-gnu'} else []
 codegen=[str(llvm/'llc'),'-O2','-verify-machineinstrs','-relocation-model=pic',f'-mcpu={cpu}',f'-mattr={features}']
 if triple.startswith('mips'):codegen+=['-mips-tail-calls']
 for backend in (['threaded-allocator' if 'integrated-atomic' in suites else 'allocator','mmap'] if a.backend=='both' else [a.backend]):
  for suite in suites:
   out=a.output/name/backend/suite;out.mkdir(parents=True)
   source_suite='integer' if suite=='atomic' else suite
   definitions=(['-DUWVM_TEST_MEMORY64_ATOMIC'] if suite=='atomic' else [])+['-DUWVM=2','-DUWVM_USE_UWVM_INT','-DUWVM_DISABLE_JIT']+(['-DUWVM_FORCE_DISABLE_MMAP'] if backend!='mmap' else [])+(['-DUWVM_USE_MULTITHREAD_ALLOCATOR'] if backend=='threaded-allocator' else [])
   source=root/(f'test/0011.initializer/memory64_{suite}.cc' if suite in {'metadata','initializer'} else f'test/0017.runtime/memory64_{source_suite}.cc')
   if suite in {'recursive-binary','recursive-validation','reference-validation','gc-immediate'}:
    source=root/'test/0012.validator/wasm3'/{'recursive-binary':'recursive_type_binary.cc','recursive-validation':'recursive_type_validation.cc','reference-validation':'reference_validation.cc','gc-immediate':'gc_immediate.cc'}[suite]
    definitions=[x for x in definitions if x!='-DUWVM_USE_UWVM_INT']+['-DUWVM_DISABLE_INT']
   if suite.startswith('integrated-'):
    fixture={'integrated-scalar':'memory64_scalar.cc','integrated-bulk':'memory64_bulk_translation.cc','integrated-simd':'memory64_simd_translation.cc','integrated-atomic':'memory64_atomic_translation.cc','integrated-table':'table64_translation.cc'}[suite]
    source=root/'test/0013.uwvm_int/wasm3'/fixture
    definitions+=['-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1']
   if suite in {'metadata','initializer'}:definitions=[x for x in definitions if x!='-DUWVM_USE_UWVM_INT']+['-DUWVM_DISABLE_INT','-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1']
   if suite=='initializer' and backend=='mmap':definitions+=['-DUWVM_FORCE_USE_MMAP','-DUWVM_TEST_EXPECT_MMAP']
   phases=[
    ('clang',[*compiler,*target,'-std=c++26','-g0','-ferror-limit=2','-Wno-undefined-inline',*includes,*definitions,
      '-S','-emit-llvm',str(source),'-o',str(out/'test.ll')]),
    ('object',[*codegen,'-filetype=obj',str(out/'test.ll'),'-o',str(out/'test.o')]),
    ('assembly',[*codegen,'-filetype=asm',str(out/'test.ll'),'-o',str(out/'test.s')]),
    ('link',[*compiler,*link_flags,str(out/'test.o'),'-L'+str(sysroot/'lib'),'-latomic','-pthread','-o',str(out/'test')]),
    ('run',[str(deps/'usr/bin'/('qemu-'+emulator)),*(['-cpu','power10'] if name=='ppc64le-pcrel' else []),
      '-U','LD_LIBRARY_PATH','-L',str(sysroot),str(out/'test')])]
   row=dict(profile=name,backend=backend,suite=suite,phases=[])
   for phase,cmd in phases:
    try:r=subprocess.run(cmd,capture_output=True,timeout=300);status,log=r.returncode,r.stdout+r.stderr
    except subprocess.TimeoutExpired as e:status,log='timeout',(e.stdout or b'')+(e.stderr or b'')
    (out/(phase+'.log')).write_bytes(log);row['phases'].append(dict(phase=phase,command=cmd,exit=status))
    if status!=0:break
   row['passed']=phase=='run' and status==0
   for suffix in ['.ll','.s']:
    artifact=out/('test'+suffix)
    if artifact.is_file():
     row['test'+suffix+'.sha256']=hashlib.file_digest(artifact.open('rb'),'sha256').hexdigest()
     with artifact.open('rb') as inp,gzip.open(str(artifact)+'.gz','wb',compresslevel=3) as dst:shutil.copyfileobj(inp,dst)
     artifact.unlink()
   (out/'result.json').write_text(json.dumps(row,indent=2)+'\n');rows.append(row)
   print(name,backend,suite,'PASS' if row['passed'] else 'FAIL',flush=True)
   if not row['passed']:print(log.decode(errors='replace')[-2200:],flush=True)
   (a.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
after=subprocess.check_output(fingerprint+[str(a.output/'source-after.json')],text=True).strip();assert before==after
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
raise SystemExit(not all(row['passed'] for row in rows))
