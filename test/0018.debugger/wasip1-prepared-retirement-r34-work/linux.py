from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex,time
D=Path(__file__).parent;E=D.parent.parent;R=E/'rounds/wasip1-cross-jit-20261007-r26'
repo=sys.argv[2];assert sys.argv[1]=='joint-linux-r34' and repo in ('uwvm2','uwvm2-ros')
input_manifest=D/'repaired-inputs-v8.json'
S=D/'repaired-inputs-v8'/repo;B=D/'repaired-inputs-v8'/repo/'test/0017.runtime';O=D/'products-v8/linux-integrated'/repo;O.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads(input_manifest.read_text());Q=json.loads((D/'repair-v8-input-freeze-qualified.json').read_text());assert Q['passed'] and Q['overlay_manifest_sha256']==sha(input_manifest) and all(sha(D/'repaired-inputs-v8'/p)==h for p,h in M.items())

P=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35')
SDK=json.loads((P/'sdk-qualified-r35.json').read_text())
assert all(sha(p)==h for g in ('archives','source_inputs','generated_headers') for p,h in SDK[g].items())
previous=E/'rounds/wasip1-joint-worker-contexts-20261007-r33';old=json.loads((previous/'products/linux-integrated'/repo/'runtime-qualified.json').read_text())['argv']
base=old[:old.index('-MD')];base=[v.replace(str(previous/('ros-repaired-inputs' if repo=='uwvm2-ros' else 'inputs')/repo),str(S)) for v in base]
T=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
link=['--ld-path='+str(T/'bin/ld.lld'),'-L'+str(T/'lib/x86_64-unknown-linux-gnu'),'-Wl,-rpath,'+str(T/'lib/x86_64-unknown-linux-gnu'),'-rtlib=compiler-rt','-unwindlib=libunwind','-Wl,--start-group',*SDK['archives'].keys(),'-Wl,--end-group','-lrt','-ldl','-lm','-lssl','-lcrypto']
env=dict(os.environ,MALLOC_ARENA_MAX='1',MALLOC_TRIM_THRESHOLD_='131072',MALLOC_MMAP_THRESHOLD_='131072')
rows=json.loads((O/'results.json').read_text()) if (O/'results.json').exists() else []
def run(name,args,timeout=1200):
 p=O/(name+'.log')
 previous=next((r for r in rows if r['name']==name),None)
 if previous is not None:
  assert sha(p)==previous['log_sha256']
  if previous['passed'] and previous['argv']==list(map(str,args)):
   print('qualified completed step retained',repo,name,flush=True);return previous
  (O/('failed-'+name+'-'+str(time.time_ns())+'.json')).write_text(json.dumps(previous,indent=2)+'\n')
  p.rename(O/('failed-'+name+'-'+str(time.time_ns())+'.log'));rows.remove(previous)
 assert not p.exists(),name
 print('starting',repo,name,flush=True);start=time.monotonic()
 with p.open('wb') as f:q=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,env=env,timeout=timeout)
 row=dict(name=name,argv=list(map(str,args)),exit=q.returncode,seconds=time.monotonic()-start,log_sha256=sha(p),passed=q.returncode==0)
 rows.append(row);(O/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
 print('completed',repo,name,q.returncode,flush=True)
 if q.returncode:print(p.read_text(errors='replace')[-10000:],flush=True);raise SystemExit(1)
 return row
def deps(p):
 return {str(Path(os.path.normpath(v))):sha(Path(os.path.normpath(v))) for v in shlex.split(p.read_text().replace('\\\n',' ').split(':',1)[1])}
for label,source in [('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),('host-api','src/uwvm2/uwvm/host_api.default.cpp')]:
 compiled=run(label+'-compile',[*base,'-MD','-MF',O/(label+'.d'),'-c',S/source,'-o',O/(label+'.o')])
 (O/(label+'-qualified.json')).write_text(json.dumps(dict(argv=compiled['argv'],object_sha256=sha(O/(label+'.o')),dependencies=deps(O/(label+'.d')),compiler_sha256=sha(T/'bin/clang++'),sdk_proof_sha256=sha(P/'sdk-qualified-r35.json'),source_manifest_sha256=sha(input_manifest)),indent=2)+'\n')
W=P.parent/'assets/wasm-tools/wasm-tools';tool=json.loads((P.parent/'assets/wasm-tools/package-manifest41.json').read_text());assert sha(W)==tool['tool_sha256']
for name in ('debug_checkpoint_retirement_cohort','debug_wasip1_checkpoint'):
 run(name+'-assemble',[W,'parse',B/'fixtures'/(name+'.wat'),'-o',O/(name+'.wasm')],60)
 run(name+'-validate',[W,'validate',O/(name+'.wasm'),'--features','all'],60)
for fixture,wasm in [('debug_checkpoint_prepared_retirement','debug_checkpoint_retirement_cohort'),('debug_checkpoint_native_cohort_retirement','debug_checkpoint_retirement_cohort'),('debug_wasip1_prepared_retirement','debug_wasip1_checkpoint')]:
 build=run(fixture+'-build',[*base,'-MD','-MF',O/(fixture+'.d'),B/(fixture+'_runtime.cc'),O/'runtime.o',O/'host-api.o',*link,'-o',O/fixture])
 (O/(fixture+'-qualified.json')).write_text(json.dumps(dict(passed=True,binary_sha256=sha(O/fixture),dependencies=deps(O/(fixture+'.d')),source_manifest_sha256=sha(input_manifest),runtime_object_sha256=sha(O/'runtime.o')),indent=2)+'\n')
 for policy in ('instruction','unwind'):run(fixture+'-'+policy,[O/fixture,O/(wasm+'.wasm'),policy],300)
print('R34 genuine preparation/old-worker retirement/closed-ready/discard and legacy retirement passed',repo,flush=True)
