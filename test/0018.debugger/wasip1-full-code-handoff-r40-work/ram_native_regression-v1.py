"""Bounded Linux LLVM-full native regression with all products in owned tmpfs."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,time,resource
D=Path(__file__).parent;E=D.parent.parent;S=D/'syntax-overlay-v1'
OLD=E/'rounds/wasip1-prepared-retirement-20261008-r34'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
q=json.loads((D/'analysis-compile-qualified-v1.json').read_text());assert q['passed'] and len(q['rows'])==16
g=json.loads((D/'guard-analysis-compile-v1.json').read_text());assert g['passed'] and all(x['pidfd_retired'] for x in g['processes'])
assert Path('/proc/self/cgroup').read_text()==g['cgroup']
m=json.loads((D/'syntax-overlay-v1.json').read_text());assert all(sha(S/k)==v for k,v in m.items())
T=Path('/dev/shm')/('uwvm-checkpoint-r40-'+str(os.getpid()));T.mkdir(mode=0o700)
birth=int(Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19])
(D/'ram-native-v1-storage.new').write_text(json.dumps(dict(path=str(T),creator_pid=os.getpid(),creator_birth=birth,
 device=T.stat().st_dev,inode=T.stat().st_ino,uid=os.getuid(),maximum_bytes=768<<20)))
os.replace(D/'ram-native-v1-storage.new',D/'ram-native-v1-storage.json')
env=dict(os.environ,MALLOC_ARENA_MAX='1',TMPDIR=str(T),PYTHONDONTWRITEBYTECODE='1',
 LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu')
validator=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools')
assert sha(validator)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
rows=[]
deps={};fixtures={};products={}
def save():
 (D/'ram-native-v1-results.json').write_text(json.dumps(dict(rows=rows,dependencies=deps,fixtures=fixtures,
  products=products,source_manifest_sha256=sha(D/'syntax-overlay-v1.json'),controller_sha256=sha(__file__),
  llvm_only=True,all_native_products_in_tmpfs=True),indent=2)+'\n')
def run(repo,stage,argv,native=False):
 print(repo,stage,'START',flush=True);log=D/(repo+'-'+stage+'-ram-native-v1.log');assert not log.exists()
 started=time.monotonic()
 with log.open('xb') as output:
  def address_limit():
   cap=64<<30 if native else 8<<30
   resource.setrlimit(resource.RLIMIT_AS,(cap,cap))
  r=subprocess.run(argv,preexec_fn=address_limit,stdout=output,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,cwd=T,env=env,timeout=600)
 rows.append(dict(repo=repo,stage=stage,argv=argv,exit=r.returncode,passed=r.returncode==0,log_sha256=sha(log),
  seconds=time.monotonic()-started,actual_native_runtime_execution=native,reused_unchanged_native_execution=False));save()
 print(repo,stage,r.returncode,flush=True)
 if r.returncode:print(log.read_text(errors='replace')[-12000:],flush=True);sys.exit(1)
for repo in ('uwvm2','uwvm2-ros'):
 parent=OLD/'repaired-inputs-v8'/repo;P=OLD/'products-v8/linux-integrated'/repo;root=S/repo
 proof=json.loads((P/'runtime-qualified.json').read_text());assert all(sha(p)==v for p,v in proof['dependencies'].items())
 hostproof=json.loads((P/'host-api-qualified.json').read_text());assert all(sha(p)==v for p,v in hostproof['dependencies'].items())
 historical=json.loads((P/'results.json').read_text())
 base=next(r['argv'] for r in historical if r['name']=='runtime-compile')
 base=base[:base.index('-MD')];base=[a for a in base if a!='-DUWVM_USE_UWVM_INT']
 base[1:1]=['-DUWVM_DISABLE_INT','-fdelayed-template-parsing','-I'+str(root/'src')]
 assert sha(base[0])==proof['compiler_sha256'];deps[base[0]]=sha(base[0])
 link=next(r['argv'] for r in historical if r['name']=='debug_checkpoint_prepared_retirement-build')
 tail=link[link.index('--ld-path='+str(Path(base[0]).parent/'ld.lld')):link.index('-o')]
 for arg in tail:
  path=arg.removeprefix('--ld-path=')
  if Path(path).is_file():deps[path]=sha(path)
 runtime=T/(repo+'-runtime.o');host=T/(repo+'-host-api.o')
 run(repo,'runtime-compile',[*base,'-c',str(root/'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),'-o',str(runtime)])
 products[str(runtime)]=dict(sha256=sha(runtime),size=runtime.stat().st_size);save()
 run(repo,'host-api-compile',[*base,'-c',str(parent/'src/uwvm2/uwvm/host_api.default.cpp'),'-o',str(host)])
 products[str(host)]=dict(sha256=sha(host),size=host.stat().st_size);save()
 names=['debug_checkpoint_retirement_cohort','debug_wasip1_checkpoint','debug_checkpoint_complete_instance',
 'debug_checkpoint_complete_preload_main','debug_checkpoint_complete_preload_provider',
 'debug_checkpoint_indirect_retirement_cohort','debug_wasip1_indirect_prepared_retirement']
 wasm={}
 for name in names:
  wat=root/'test/0017.runtime/fixtures'/(name+'.wat')
  if not wat.exists():wat=parent/'test/0017.runtime/fixtures'/(name+'.wat')
  fixtures[str(wat)]=sha(wat);out=T/(repo+'-'+name+'.wasm');wasm[name]=str(out)
  run(repo,name+'-assemble',[str(validator),'parse',str(wat),'-o',str(out)])
  run(repo,name+'-validate',[str(validator),'validate',str(out),'--features','all'])
 specs=[
 ('core','debug_checkpoint_prepared_retirement_runtime.cc',[
  ('normal',[wasm['debug_checkpoint_retirement_cohort']]),
  ('indirect',[wasm['debug_checkpoint_indirect_retirement_cohort']])]),
 ('wasip1','debug_wasip1_prepared_retirement_runtime.cc',[
  ('normal',[wasm['debug_wasip1_checkpoint']]),
  ('indirect',[wasm['debug_wasip1_indirect_prepared_retirement']])]),
 ('instance','debug_checkpoint_complete_instance_runtime.cc',[('normal',[wasm['debug_checkpoint_complete_instance']])]),
 ('preload','debug_checkpoint_complete_preload_runtime.cc',[('normal',[wasm['debug_checkpoint_complete_preload_main'],wasm['debug_checkpoint_complete_preload_provider']])])]
 for kind,name,cases in specs:
  exe=T/(repo+'-'+kind);src=root/'test/0017.runtime'/name
  run(repo,kind+'-build',[*base,str(src),str(runtime),str(host),*tail,'-o',str(exe)])
  products[str(exe)]=dict(sha256=sha(exe),size=exe.stat().st_size);save()
  for case,args in cases:
   for policy in ('instruction','unwind'):
    extra=['indirect'] if case=='indirect' else []
    run(repo,kind+'-'+case+'-'+policy,[str(exe),*args,policy,*extra],True)
    assert sha(exe)==products[str(exe)]['sha256'] and all(sha(S/k)==v for k,v in m.items())
  exe.unlink()
 for name in wasm.values():Path(name).unlink()
 runtime.unlink();host.unlink()
 assert all(sha(p)==v for p,v in proof['dependencies'].items()) and all(sha(p)==v for p,v in hostproof['dependencies'].items())
assert all(sha(p)==v for p,v in deps.items()) and all(sha(p)==v for p,v in fixtures.items())
assert len([r for r in rows if r['actual_native_runtime_execution'] and r['passed']])==24
save();(D/'ram-native-v1-qualified.json').write_text(json.dumps(dict(passed=True,native_runs=24,
 source_manifest_sha256=sha(D/'syntax-overlay-v1.json'),results_sha256=sha(D/'ram-native-v1-results.json'),
 controller_sha256=sha(__file__),fresh_native_runs=24,reused_unchanged_native_runs=0,all_native_products_in_tmpfs=True,llvm_only=True,full_world_publication=False),indent=2)+'\n')
