from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex,time,tarfile,shutil
D=Path(__file__).parent;E=D.parent.parent;R=E/'rounds/wasip1-cross-jit-20261007-r26'
platform,repo=sys.argv[2:4];assert sys.argv[1]=='joint-cross-r34' and platform in ('windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
old_platform={'windows':'windows-readonly','freebsd':'freebsd','macos':'macos-platform'}[platform]
input_manifest=D/'repaired-inputs-v8.json'
old=R/'products'/old_platform/repo;S=D/'repaired-inputs-v8'/repo;B=S/'test/0017.runtime';O=D/'products-v8'/platform/repo;O.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads(input_manifest.read_text());overlay=json.loads((D/'repair-v8-input-freeze-qualified.json').read_text());assert overlay['passed'] and overlay['overlay_manifest_sha256']==sha(input_manifest) and all(sha(D/'repaired-inputs-v8'/p)==h for p,h in M.items())

early=O/'interrupted-object-cache-retirement.json'
if early.exists():
 assert platform=='macos' and repo=='uwvm2'
 import qualified_archive as qa
 cache=json.loads(early.read_text());guard=json.loads((E/'guard-joint-macos-recovery-cache-r34-macos-uwvm2.json').read_text())
 assert cache['passed'] and cache['all_payloads_read_back'] and cache['source_manifest_sha256']==sha(input_manifest)
 assert guard['passed'] and guard['actual_root_exit']==0 and all(r['pidfd_retired'] for r in guard['retirement'])
 assert sha(cache['archive'])==cache['archive_sha256']
 seen={}
 with qa.open_reader(cache['archive']) as a:
  for m in a:
   assert m.isfile() and m.name in cache['payloads'] and m.name not in seen
   target=O/m.name
   with a.extractfile(m) as f:
    if target.exists():
     seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest();assert sha(target)==seen[m.name]
    else:
     with target.open('xb') as out:shutil.copyfileobj(f,out,1048576)
     seen[m.name]=sha(target);os.chmod(target,m.mode)
 assert seen==cache['payloads']
 (O/'interrupted-compilation-recovered.json').write_text(json.dumps(dict(passed=True,cache_receipt_sha256=sha(early),archive_sha256=sha(cache['archive']),payloads=seen,source_manifest_sha256=sha(input_manifest),native_execution_claimed=False),indent=2)+'\n')

SDK=json.loads((R/('llvm-'+platform)/'qualified.json').read_text());assert SDK['passed'] and SDK['version']=='23.1.1-uwvm-ros.11'
for group in ('source_inputs','generated_headers'):assert all(sha(p)==h for p,h in SDK[group].items())
crypto=R/'openssl-windows/qualified.json' if platform=='windows' else old/'crypto-inputs.json'
CRYPTO=json.loads(crypto.read_text());assert CRYPTO['passed']
for group in ('source_inputs','archives'):assert all(sha(p)==h for p,h in CRYPTO[group].items())
retirement=R/('llvm-'+platform)/('readonly-qualified-libraries-retirement.json' if platform=='windows' else 'library-retirement.json')
recovery=json.loads(retirement.read_text());assert recovery['passed'] and recovery['all_payloads_read_back'] and sha(recovery['archive'])==recovery['archive_sha256']
needed={Path(p).name:(Path(p),h) for p,h in SDK['archives'].items() if not Path(p).exists()};assert len(needed)==sum(not Path(p).exists() for p in SDK['archives'])
restored=[]
with tarfile.open(recovery['archive'],'r|gz') as archive:
 for member in archive:
  if Path(member.name).name not in needed:continue
  target,expected=needed[Path(member.name).name];assert member.isfile() and not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  with archive.extractfile(member) as inp,target.open('xb') as out:shutil.copyfileobj(inp,out,1048576)
  assert sha(target)==expected;restored.append(target)
assert all(sha(p)==h for p,h in SDK['archives'].items())
proof=json.loads((old/'runtime-compiled.json').read_text());argv=proof['argv'];base=argv[:argv.index('-MD')]
for original_root in (R/'inputs'/repo,R/'readonly-fixed-inputs'/repo):base=[v.replace(str(original_root),str(S)) for v in base]
links=[]
for path in old.glob('attempt-*.json'):
 q=json.loads(path.read_text())
 if q.get('passed') and q.get('name')=='checkpoint-link':links.append(q)
assert links
argv=links[-1]['argv'];first=next(i for i,v in enumerate(argv) if v.endswith('/runtime.o'));link=argv[first:argv.index('-o')]
link=[v.replace(str(old/'runtime.o'),str(O/'runtime.o')).replace(str(old/'host-api.o'),str(O/'host-api.o')) for v in link]
extra=[]
if platform=='macos':
 recovery=json.loads((R/(repo+'-macos-product-retirement.json')).read_text())['delivery_archive']
 assert sha(recovery['archive'])==recovery['archive_sha256']
 for original in [Path(v) for v in link if v.endswith('.o') and not v.endswith(('/runtime.o','/host-api.o'))]:
  assert original.parent==old;target=O/original.name;expected=recovery['payload_sha256'][original.name]
  if not target.exists():
   with tarfile.open(recovery['archive'],'r|gz') as archive:
    for member in archive:
     if member.name!=original.name:continue
     with archive.extractfile(member) as inp,target.open('xb') as out:shutil.copyfileobj(inp,out,1048576)
     break
  assert sha(target)==expected;link=[str(target) if v==str(original) else v for v in link];extra.append(dict(path=str(target),sha256=expected,archive=recovery['archive'],archive_sha256=recovery['archive_sha256']))
T=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm');env=dict(os.environ,MALLOC_ARENA_MAX='1',MALLOC_TRIM_THRESHOLD_='131072',MALLOC_MMAP_THRESHOLD_='131072');rows=json.loads((O/'results.json').read_text()) if platform=='macos' and (O/'results.json').exists() else []
if rows:
 assert early.exists()
 reference=json.loads((D/'products-v8/freebsd'/repo/'qualified.json').read_text())
 assert reference['passed'] and reference['source_manifest_sha256']==sha(input_manifest)
 for item in reference['fixtures'].values():assert sha(O/item['wasm'])==item['wasm_sha256']
def run(name,args,timeout=1200):
 p=O/(name+'.log');args=list(map(str,args));old=next((r for r in rows if r['name']==name and r['passed'] and r['exit']==0),None)
 if old:
  assert platform=='macos' and old['argv']==args and sha(p)==old['log_sha256']
  print('retained individually verified completed stage',platform,repo,name,flush=True);return
 if p.exists():
  assert platform=='macos' and early.exists()
  interrupted=O/'interrupted-attempts';interrupted.mkdir(exist_ok=True);p.rename(interrupted/(str(time.time_ns())+'-'+p.name))
 print('starting',platform,repo,name,flush=True);start=time.monotonic()
 with p.open('wb') as f:q=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,env=env,timeout=timeout)
 row=dict(name=name,argv=list(map(str,args)),exit=q.returncode,seconds=time.monotonic()-start,log_sha256=sha(p),passed=q.returncode==0);rows.append(row);(O/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print('completed',platform,repo,name,q.returncode,flush=True)
 if q.returncode:print(p.read_text(errors='replace')[-10000:],flush=True);raise SystemExit(1)
def deps(p):return {str(Path(os.path.normpath(v))):sha(Path(os.path.normpath(v))) for v in shlex.split(p.read_text().replace('\\\n',' ').split(':',1)[1])}
for label,source in [('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),('host-api','src/uwvm2/uwvm/host_api.default.cpp')]:
 run(label+'-compile',[*base,'-MD','-MF',O/(label+'.d'),'-c',S/source,'-o',O/(label+'.o')])
 (O/(label+'-qualified.json')).write_text(json.dumps(dict(argv=rows[-1]['argv'],object_sha256=sha(O/(label+'.o')),dependencies=deps(O/(label+'.d')),compiler_sha256=sha(T/'bin/clang++'),sdk_proof_sha256=sha(R/('llvm-'+platform)/'qualified.json')),indent=2)+'\n')
W=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools');assert sha(W)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
for name in ('debug_checkpoint_retirement_cohort','debug_wasip1_checkpoint'):
 run(name+'-assemble',[W,'parse',B/'fixtures'/(name+'.wat'),'-o',O/(name+'.wasm')],60);run(name+'-validate',[W,'validate',O/(name+'.wasm'),'--features','all'],60)
fixtures={}
for fixture,wasm in [('debug_checkpoint_prepared_retirement','debug_checkpoint_retirement_cohort'),('debug_checkpoint_native_cohort_retirement','debug_checkpoint_retirement_cohort'),('debug_wasip1_prepared_retirement','debug_wasip1_checkpoint')]:
 run(fixture+'-build',[*base,'-MD','-MF',O/(fixture+'.d'),B/(fixture+'_runtime.cc'),*link,'-o',O/(fixture+'.exe')])
 q=dict(passed=True,binary_sha256=sha(O/(fixture+'.exe')),source_manifest_sha256=sha(input_manifest),dependencies=deps(O/(fixture+'.d')),actual_native_execution=False)
 (O/(fixture+'-qualified.json')).write_text(json.dumps(q,indent=2)+'\n');fixtures[fixture]=dict(binary=fixture+'.exe',binary_sha256=q['binary_sha256'],wasm=wasm+'.wasm',wasm_sha256=sha(O/(wasm+'.wasm')),qualification=fixture+'-qualified.json')
output=dict(passed=True,platform=platform,repo=repo,source_manifest_sha256=sha(input_manifest),fixtures=fixtures,dependencies=deps(O/'runtime.d'),runtime_sha256=sha(O/'runtime.o'),host_api_sha256=sha(O/'host-api.o'),llvm_sdk_sha256=sha(R/('llvm-'+platform)/'qualified.json'),extra_objects=extra,actual_native_execution=False)
(O/'qualified.json').write_text(json.dumps(output,indent=2)+'\n')
# Only this invocation's restored caches are retired after their exact hashes
# were authenticated. Qualified archives and generated/public headers remain.
for p in restored:assert sha(p)==SDK['archives'][str(p)]
(O/'sdk-cache-retirement.json').write_text(json.dumps(dict(passed=True,archive=str(retirement),archive_sha256=sha(retirement),restored_then_retired=[dict(path=str(p),sha256=SDK['archives'][str(p)],bytes=p.stat().st_size) for p in restored]),indent=2)+'\n')
for p in restored:p.unlink()
print('linked fresh actual target prepared-retirement fixtures',platform,repo,flush=True)
