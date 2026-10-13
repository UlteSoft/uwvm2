"""Actual active generation-2 nested frames, after both independent base suites."""
from pathlib import Path
import hashlib,json,os,resource,subprocess,sys,tarfile,time
D=Path(__file__).parent;E=D.parent.parent;BASE=D/"syntax-overlay-v1";PREVIOUS=D/"syntax-overlay-generation-v3";S=D/"syntax-overlay-active-generation-v1"
OLD=E/"rounds/wasip1-prepared-retirement-20261008-r34"
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
base=json.loads((D/"syntax-overlay-v1.json").read_text())
previous=json.loads((D/"syntax-overlay-generation-v2.json").read_text())
manifest=json.loads((D/"syntax-overlay-active-generation-v1.json").read_text())
equivalence=json.loads((D/"active-generation-v1-source-equivalence.json").read_text())
q=json.loads((D/"ram-native-v2-qualified.json").read_text());g=json.loads((D/"guard-ram-native-v2.json").read_text())
v=json.loads((D/"generation-native-v3-qualified.json").read_text());vg=json.loads((D/"guard-generation-native-v3.json").read_text())
assert q["passed"] and q["fresh_native_runs"]==24 and v["passed"] and v["fresh_native_cases"]==4
for proof in (g,vg):
 assert proof["passed"] and proof["parent_events_unchanged"] and proof["ram_disposable_products_retired"] and all(p["pidfd_retired"] for p in proof["processes"])
assert Path("/proc/self/cgroup").read_text()==g["cgroup"]==vg["cgroup"]
assert len(manifest)==50 and len(base)==42 and len(previous)==44 and equivalence["passed"]
assert equivalence["old_manifest_sha256"]==sha(D/"syntax-overlay-v1.json")==q["source_manifest_sha256"]
assert equivalence["previous_manifest_sha256"]==sha(D/"syntax-overlay-generation-v2.json")==v["source_manifest_sha256"]
assert equivalence["new_manifest_sha256"]==sha(D/"syntax-overlay-active-generation-v1.json")
assert equivalence["unchanged_inputs"]==base and equivalence["previous_inputs_unchanged"]==previous
assert all(manifest[k]==h and sha(BASE/k)==h for k,h in base.items())
assert all(manifest[k]==h and sha(PREVIOUS/k)==h for k,h in previous.items())
added={k:h for k,h in manifest.items() if k not in previous};assert len(added)==6 and added==equivalence["active_witness_added_inputs"]
S.mkdir()
for repo in ("uwvm2","uwvm2-ros"):
 root=S/repo;root.mkdir();(root/"src").symlink_to(BASE/repo/"src")
 test=root/"test/0017.runtime";test.mkdir(parents=True)
 for p in (BASE/repo/"test/0017.runtime").iterdir():
  if p.name!="fixtures":(test/p.name).symlink_to(p)
 fixtures_dir=test/"fixtures";fixtures_dir.mkdir()
 for p in (BASE/repo/"test/0017.runtime/fixtures").iterdir():
  if repo+"/test/0017.runtime/fixtures/"+p.name not in added:(fixtures_dir/p.name).symlink_to(p)
 for k in previous:
  if k.startswith(repo+"/test/") and k not in base:(S/k).symlink_to(PREVIOUS/k)
seen=set()
with tarfile.open(D/"syntax-overlay-active-generation-v1.tar.gz","r:gz") as archive:
 for member in archive:
  assert member.isfile() and member.name in manifest and member.name not in seen
  data=archive.extractfile(member).read();assert hashlib.sha256(data).hexdigest()==manifest[member.name]
  if member.name in added:
   path=S/member.name
   with path.open("xb") as f:f.write(data)
   os.chmod(path,0o444)
  else:assert sha(S/member.name)==manifest[member.name]
  seen.add(member.name)
assert seen==set(manifest) and all(sha(S/k)==h for k,h in manifest.items())

T=Path('/dev/shm')/('uwvm-checkpoint-r40-active-generation-v1-'+str(os.getpid())); T.mkdir(mode=0o700)
birth=int(Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19])
(D/'active-generation-native-v1-storage.new').write_text(json.dumps(dict(path=str(T),creator_pid=os.getpid(),creator_birth=birth,
    device=T.stat().st_dev,inode=T.stat().st_ino,uid=os.getuid(),maximum_bytes=768<<20)))
os.replace(D/'active-generation-native-v1-storage.new',D/'active-generation-native-v1-storage.json')
env=dict(os.environ,MALLOC_ARENA_MAX='1',TMPDIR=str(T),PYTHONDONTWRITEBYTECODE='1',
    LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu')
validator=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools')
assert sha(validator)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
rows=[]; dependencies={}; products={}; fixtures={}
def save():
    (D/'active-generation-native-v1-results.json').write_text(json.dumps(dict(rows=rows,dependencies=dependencies,products=products,
        fixtures=fixtures,source_manifest_sha256=sha(D/'syntax-overlay-generation-v2.json'),controller_sha256=sha(__file__)),indent=2)+'\n')
def run(repo,stage,argv,native=False,frontend=False):
    print(repo,stage,'START',flush=True); log=D/(repo+'-'+stage+'-active-generation-v1.log'); started=time.monotonic()
    with log.open('xb') as output:
        def limits():
            cap=64<<30 if native else 8<<30
            resource.setrlimit(resource.RLIMIT_AS,(cap,cap))
        result=subprocess.run(argv,preexec_fn=limits,stdout=output,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,
            cwd=T,env=env,timeout=600)
    row=dict(repo=repo,stage=stage,argv=argv,exit=result.returncode,passed=result.returncode==0,log_sha256=sha(log),
        seconds=time.monotonic()-started,actual_native_runtime_execution=native,compiler_frontend_only=frontend)
    if native and result.returncode==0:
        evidence=log.read_text()
        row['actual_active_generation2_body_handoffs']=evidence.count('ACTIVE_GENERATION2 private_effective_body_owners=1 active_native_frames=2')==2
        row['original_retained_world_continuation_1190']='checkpoint_actual_generation2_nested_continuation: PASS' in evidence and 'complete_instance_restore=false' in evidence
        row['passed']=row['passed'] and row['actual_active_generation2_body_handoffs'] and row['original_retained_world_continuation_1190']
    rows.append(row); save(); print(repo,stage,result.returncode,flush=True)
    if not row['passed']:print(log.read_text(errors='replace')[-12000:],flush=True); sys.exit(1)
    assert all(sha(S/k)==h for k,h in manifest.items())

for repo in ("uwvm2","uwvm2-ros"):
 parent=OLD/"repaired-inputs-v8"/repo;P=OLD/"products-v8/linux-integrated"/repo;root=S/repo
 proof=json.loads((P/"runtime-qualified.json").read_text());hostproof=json.loads((P/"host-api-qualified.json").read_text())
 assert all(sha(p)==h for p,h in proof["dependencies"].items()) and all(sha(p)==h for p,h in hostproof["dependencies"].items())
 historical=json.loads((P/"results.json").read_text())
 argv=next(r["argv"] for r in historical if r["name"]=="runtime-compile");compile=argv[:argv.index("-MD")]
 compile=[arg for arg in compile if arg!="-DUWVM_USE_UWVM_INT"]
 compile[1:1]=["-DUWVM_DISABLE_INT","-fdelayed-template-parsing","-I"+str(root/"src")]
 assert sha(compile[0])==proof["compiler_sha256"];dependencies[compile[0]]=sha(compile[0])
 link=next(r["argv"] for r in historical if r["name"]=="debug_checkpoint_prepared_retirement-build")
 flags=link[link.index("--ld-path="+str(Path(compile[0]).parent/"ld.lld")):link.index("-o")]
 for arg in flags:
  path=arg.removeprefix("--ld-path=")
  if Path(path).is_file():dependencies[path]=sha(path)
 source=root/"test/0017.runtime/debug_checkpoint_active_code_handoff_generation_runtime.cc"
 run(repo,"active-generation-test-frontend",[*compile,"-fsyntax-only",str(source)],frontend=True)
 runtime=T/(repo+"-runtime.o");host=T/(repo+"-host-api.o")
 run(repo,"runtime-compile",[*compile,"-c",str(root/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"),"-o",str(runtime)])
 run(repo,"host-api-compile",[*compile,"-c",str(parent/"src/uwvm2/uwvm/host_api.default.cpp"),"-o",str(host)])
 wasm={}
 for name in ("checkpoint_actual_generation2_nested_continuation","checkpoint_actual_generation2_nested_replacement"):
  wat=root/"test/0017.runtime/fixtures"/(name+".wat");fixtures[str(wat)]=sha(wat);out=T/(repo+"-"+name+".wasm")
  run(repo,name+"-assemble",[str(validator),"parse",str(wat),"-o",str(out)])
  run(repo,name+"-validate",[str(validator),"validate",str(out),"--features","all"]);wasm[name]=out
 replacement=wasm["checkpoint_actual_generation2_nested_replacement"].read_bytes()
 assert replacement[:8]==b"\x00asm\x01\x00\x00\x00"
 def uleb(data,offset):
  value=0
  for step in range(5):
   assert offset<len(data);byte=data[offset];offset+=1;value|=(byte&127)<<(step*7)
   if not byte&128:assert value<=0xffffffff;return value,offset
  raise AssertionError("unbounded official Wasm length")
 offset=8;body=None
 while offset<len(replacement):
  kind=replacement[offset];offset+=1;size,start=uleb(replacement,offset);end=start+size;assert end<=len(replacement)
  if kind==10:
   assert body is None;count,index=uleb(replacement,start);assert count==1
   length,index=uleb(replacement,index);assert 0<length<=1048576 and index+length==end
   body=replacement[index:end]
  offset=end
 assert offset==len(replacement) and body is not None and body[-1]==11
 body_path=T/(repo+"-official-generation2-body.bin")
 with body_path.open("xb") as f:f.write(body)
 products[str(body_path)]=dict(sha256=sha(body_path),size=len(body),official_assembled_validator_input=True)
 exe=T/(repo+"-active-generation2")
 run(repo,"active-generation-test-build",[*compile,str(source),str(runtime),str(host),*flags,"-o",str(exe)])
 for path in (runtime,host,exe):products[str(path)]=dict(sha256=sha(path),size=path.stat().st_size)
 for policy in ("instruction","unwind"):
  run(repo,"active-generation2-"+policy,[str(exe),str(wasm["checkpoint_actual_generation2_nested_continuation"]),policy,str(body_path)],native=True)
  assert all(sha(path)==products[str(path)]["sha256"] for path in (runtime,host,exe,body_path))
 for path in (runtime,host,exe,body_path,*wasm.values()):path.unlink()
 assert all(sha(p)==h for p,h in proof["dependencies"].items()) and all(sha(p)==h for p,h in hostproof["dependencies"].items())
assert all(sha(p)==h for p,h in dependencies.items()) and all(sha(p)==h for p,h in fixtures.items())
assert sum(r["actual_native_runtime_execution"] and r["passed"] for r in rows)==4
save()
(D/"active-generation-native-v1-qualified.json").write_text(json.dumps(dict(passed=True,fresh_native_cases=4,frontend_checks=2,
 genuine_active_generation_2_cases=4,active_nested_body_handoff_episodes=8,
 source_manifest_sha256=sha(D/"syntax-overlay-active-generation-v1.json"),
 base_native_qualification_sha256=sha(D/"ram-native-v2-qualified.json"),
 dormant_generation_qualification_sha256=sha(D/"generation-native-v3-qualified.json"),
 source_equivalence_sha256=sha(D/"active-generation-v1-source-equivalence.json"),
 results_sha256=sha(D/"active-generation-native-v1-results.json"),controller_sha256=sha(__file__),
 original_retained_world_continuation_tested=True,original_retained_world_continuation_result=1190,
 world_publication=False,restored_worker_startup=False,guest_replay=False),indent=2)+"\n")
