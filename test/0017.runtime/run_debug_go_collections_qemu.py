#!/usr/bin/env python3
"""Run the finite Go collection/array/descriptor DATA components on a real target ELF under QEMU.

Use the existing birth/PIDFD bounded component supervisor. This is not a
language producer, VM stop or full debugger architecture parity check.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess,sys
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description=__doc__)
for name in ['root','deps-root','qemu-root','qemu-manifest','out']:ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('--repository',choices=['uwvm2','uwvm2-ros'],required=True);ap.add_argument('--profile',required=True)
a=ap.parse_args();assert sys.platform=='linux';S=a.root/a.repository
subprocess.run(['bash',str(S/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
module=S/'test/0017.runtime/run_debug_linux_qemu_components.py'
spec=importlib.util.spec_from_file_location('go_component_registry',module);recipe=importlib.util.module_from_spec(spec);spec.loader.exec_module(recipe)
profile,triple,emulator,machine,bits,order,flags=next(v for v in recipe.PROFILES if v[0]==a.profile)
qm=json.loads(a.qemu_manifest.read_text());q=a.qemu_root/'usr/bin'/('qemu-'+emulator);assert sha(q)==qm['binaries']['qemu-'+emulator]['sha256']
pins={str(Path(__file__).resolve()):sha(__file__),str(module):sha(module),str(a.qemu_manifest):sha(a.qemu_manifest),str(q.resolve()):sha(q),str(Path(sys.executable).resolve()):sha(sys.executable)}
m=json.loads((a.root/(a.repository+'-current-manifest.json')).read_text())
for r in m['files']:pins[str((S/r['path']).resolve())]=r['sha256']
for d in [a.deps_root/'usr'/triple,a.deps_root/'usr/lib/gcc-cross'/triple,a.deps_root/'usr/lib'/triple,a.deps_root/'usr/include',a.deps_root/'usr/lib/x86_64-linux-gnu',Path('/usr/include'),Path('/usr/lib/gcc/x86_64-linux-gnu'),Path('/usr/lib/llvm-22/lib/clang')]:
 if d.is_dir():
  for p in d.rglob('*'):
   if p.is_file():pins[str(p.resolve())]=sha(p)
for p in [Path('/usr/lib/llvm-22/bin/clang++'),Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/ld.lld')]:pins[str(p.resolve())]=sha(p)
if a.profile in ('ppc64','ppc32','sparc64'):
 p=a.deps_root/'usr/bin'/(triple+'-ld');pins[str(p.resolve())]=sha(p)
if a.profile=='x86_64':
 for p in Path('/usr/lib/x86_64-linux-gnu').iterdir():
  if p.is_file() and p.name.startswith(('libstdc++','libgcc','libatomic','libc.','libm.','libpthread','ld-linux','ld.so','crt')):pins[str(p.resolve())]=sha(p)
for p,h in pins.items():assert sha(p)==h,('before pin mismatch',p)
# The registered path is fixed below; no arbitrary executable case is admitted.
recipe.CASES=['debug_source_go_collections','debug_source_go_arrays','debug_source_go_builtins']
sys.argv=[str(module),'--root',str(a.root),'--out',str(a.out),'--profiles',a.profile,'--repositories',a.repository,'--deps-root',str(a.deps_root),'--qemu-root',str(a.qemu_root),'--cases','debug_source_go_collections,debug_source_go_arrays,debug_source_go_builtins']
assert recipe.main()==0
p=a.out/'results.json';r=json.loads(p.read_text());assert r['passed'] and r['inputs_after_unchanged'] and len(r['rows'])==3
for row in r['rows']:
 assert row['passed']
 for dep,h in row['dependency_hashes'].items():assert pins.get(dep)==h,('actual dependency missing before pin',dep)
for case,assertions in [('debug_source_go_collections',1402),('debug_source_go_arrays',450),('debug_source_go_builtins',145)]:
 log=a.out/a.profile/a.repository/case/'qemu.log';assert log.read_text()==case+': PASS '+str(assertions)+' assertions\n'
for name,h in pins.items():assert sha(name)==h,('after pin mismatch',name)
r.update(wrapper_inputs=pins,inputs_before_equals_after=True,collection_assertions=1402,array_assertions=450,descriptor_assertions=145,same_semantic_output_as_x86_64_component=True,scope=__doc__)
p.write_text(json.dumps(r,indent=2)+'\n');print(a.repository,a.profile,'PASS 1402 collection / 450 array / 145 descriptor DATA assertions; actual target ELF/QEMU, no full VM parity',flush=True)
