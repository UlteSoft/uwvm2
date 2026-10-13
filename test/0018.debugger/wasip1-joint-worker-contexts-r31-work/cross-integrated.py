from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex,time,tarfile,shutil
D=Path(__file__).parent;E=D.parent.parent;R=E/'rounds/wasip1-cross-jit-20261007-r26'
platform,repo=sys.argv[2:4];assert sys.argv[1]=='joint-cross-r31' and platform in ('windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
old_platform={'windows':'windows-readonly','freebsd':'freebsd','macos':'macos-platform'}[platform]
old=R/'products'/old_platform/repo;S=D/'inputs'/repo;B=S/'test/0017.runtime';O=D/'products'/platform/repo;O.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads((D/'inputs.json').read_text());assert all(sha(D/'inputs'/p)==h for p,h in M.items())
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
  with tarfile.open(recovery['archive'],'r|gz') as archive:
   for member in archive:
    if member.name!=original.name:continue
    with archive.extractfile(member) as inp,target.open('xb') as out:shutil.copyfileobj(inp,out,1048576)
    break
  assert sha(target)==expected;link=[str(target) if v==str(original) else v for v in link];extra.append(dict(path=str(target),sha256=expected,archive=recovery['archive'],archive_sha256=recovery['archive_sha256']))
T=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm');env=dict(os.environ,MALLOC_ARENA_MAX='1',MALLOC_TRIM_THRESHOLD_='131072',MALLOC_MMAP_THRESHOLD_='131072');rows=[]
def run(name,args,timeout=1200):
 p=O/(name+'.log');assert not p.exists(),name;print('starting',platform,repo,name,flush=True);start=time.monotonic()
 with p.open('wb') as f:q=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,env=env,timeout=timeout)
 row=dict(name=name,argv=list(map(str,args)),exit=q.returncode,seconds=time.monotonic()-start,log_sha256=sha(p),passed=q.returncode==0);rows.append(row);(O/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print('completed',platform,repo,name,q.returncode,flush=True)
 if q.returncode:print(p.read_text(errors='replace')[-10000:],flush=True);raise SystemExit(1)
def deps(p):return {str(Path(os.path.normpath(v))):sha(Path(os.path.normpath(v))) for v in shlex.split(p.read_text().replace('\\\n',' ').split(':',1)[1])}
for label,source in [('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),('host-api','src/uwvm2/uwvm/host_api.default.cpp')]:
 run(label+'-compile',[*base,'-MD','-MF',O/(label+'.d'),'-c',S/source,'-o',O/(label+'.o')])
 (O/(label+'-qualified.json')).write_text(json.dumps(dict(argv=rows[-1]['argv'],object_sha256=sha(O/(label+'.o')),dependencies=deps(O/(label+'.d')),compiler_sha256=sha(T/'bin/clang++'),sdk_proof_sha256=sha(R/('llvm-'+platform)/'qualified.json')),indent=2)+'\n')
W=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools');assert sha(W)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
for name,source in [('checkpoint',S/'test/0017.runtime/fixtures/debug_wasip1_checkpoint.wat'),('aliases',B/'fixtures/debug_wasip1_checkpoint_builtin_aliases.wat')]:
 run(name+'-assemble',[W,'parse',source,'-o',O/(name+'.wasm')],60);run(name+'-validate',[W,'validate',O/(name+'.wasm'),'--features','all'],60)
run('fixture-build',[*base,'-MD','-MF',O/'fixture.d',B/'debug_wasip1_checkpoint_runtime.cc',*link,'-o',O/'fixture.exe'])
for name,source in [('group-main',B/'fixtures/debug_wasip1_environment_group.wat'),('group-provider',B/'fixtures/debug_wasip1_checkpoint.wat')]:
 run(name+'-assemble',[W,'parse',source,'-o',O/(name+'.wasm')],60);run(name+'-validate',[W,'validate',O/(name+'.wasm'),'--features','all'],60)
for name in ('main','provider'):
 source=B/'fixtures'/('debug_wasip1_worker_environment_'+name+'.wat')
 run('worker-chain-'+name+'-assemble',[W,'parse',source,'-o',O/('worker-chain-'+name+'.wasm')],60)
 run('worker-chain-'+name+'-validate',[W,'validate',O/('worker-chain-'+name+'.wasm'),'--features','all'],60)
run('environment-group-build',[*base,'-MD','-MF',O/'environment-group.d',B/'debug_wasip1_environment_group_runtime.cc',*link,'-o',O/'environment-group.exe'])
(O/'environment-group-qualified.json').write_text(json.dumps(dict(passed=True,binary_sha256=sha(O/'environment-group.exe'),source_manifest_sha256=sha(D/'inputs.json'),dependencies=deps(O/'environment-group.d'),actual_native_execution=False),indent=2)+'\n')
output=dict(passed=True,platform=platform,repo=repo,source_manifest_sha256=sha(D/'inputs.json'),test_source_sha256=sha(B/'debug_wasip1_checkpoint_runtime.cc'),alias_source_sha256=sha(B/'fixtures/debug_wasip1_checkpoint_builtin_aliases.wat'),binary_sha256=sha(O/'fixture.exe'),wasm={n:sha(O/(n+'.wasm')) for n in ('checkpoint','aliases')},dependencies=deps(O/'fixture.d'),runtime_sha256=sha(O/'runtime.o'),host_api_sha256=sha(O/'host-api.o'),llvm_sdk_sha256=sha(R/('llvm-'+platform)/'qualified.json'),extra_objects=extra,actual_native_execution=False,group_binary_sha256=sha(O/'environment-group.exe'),group_wasm={n:sha(O/(n+'.wasm')) for n in ('group-main','group-provider','worker-chain-main','worker-chain-provider')})
(O/'qualified.json').write_text(json.dumps(output,indent=2)+'\n')
# Only this invocation's restored caches are retired after their exact hashes
# were authenticated. Qualified archives and generated/public headers remain.
for p in restored:assert sha(p)==SDK['archives'][str(p)]
(O/'sdk-cache-retirement.json').write_text(json.dumps(dict(passed=True,archive=str(retirement),archive_sha256=sha(retirement),restored_then_retired=[dict(path=str(p),sha256=SDK['archives'][str(p)],bytes=p.stat().st_size) for p in restored]),indent=2)+'\n')
for p in restored:p.unlink()
print('linked fresh actual target joint-preparation fixture',platform,repo,flush=True)
