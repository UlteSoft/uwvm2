"""R41 canonical retry/live-slot separation: real Linux + target frontend checks."""
from pathlib import Path
import hashlib,json,os,resource,subprocess,sys,tarfile,time
D=Path(__file__).parent;E=D.parent.parent;OLD=E/"rounds/wasip1-prepared-retirement-20261008-r34"
PREV=E/"rounds/wasip1-full-code-handoff-20261008-r40";BASE=PREV/"syntax-overlay-active-generation-v1";S=D/"source-v2";S0=D/"before-source-v2"
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
manifest=json.loads((D/"source-manifest.json").read_text());eq=json.loads((D/"source-equivalence.json").read_text())
q=json.loads((PREV/"active-generation-native-v1-qualified.json").read_text());guard=json.loads((PREV/"guard-active-generation-native-v1.json").read_text())
assert q["passed"] and q["fresh_native_cases"]==4 and guard["passed"] and guard["parent_events_unchanged"]
assert all(p["pidfd_retired"] for p in guard["processes"]) and Path("/proc/self/cgroup").read_text()==guard["cgroup"]
assert sha(PREV/"syntax-overlay-active-generation-v1.json")==eq["previous_manifest_sha256"]==q["source_manifest_sha256"]
assert len(manifest)==56 and len(eq["unchanged_inputs"])==50 and len(eq["delta"])==6
assert all(manifest[k]==h and sha(BASE/k)==h for k,h in eq["unchanged_inputs"].items())
# Reuse only this round's fully written, hash-qualified source projection.
# The failed v2 admission performed no compile/run and its guard reaped all owners.
if not S.exists():
 for projection in (S,S0):
  projection.mkdir()
  for repo in ("uwvm2","uwvm2-ros"):
   root=projection/repo;lib=root/"src/uwvm2/runtime/lib";lib.mkdir(parents=True)
   for p in (BASE/repo/"src/uwvm2/runtime/lib").iterdir():
    target=lib/p.name;k=repo+"/src/uwvm2/runtime/lib/"+p.name
    if k in eq["delta"]:continue
    if p.name=="uwvm_runtime.default.cpp":target.write_bytes(p.read_bytes());os.chmod(target,0o444)
    else:target.symlink_to(p)
   test=root/"test/0017.runtime";test.mkdir(parents=True)
   for p in (BASE/repo/"test/0017.runtime").iterdir(): (test/p.name).symlink_to(p)
 for projection in (S,S0):
  for k in eq["unchanged_inputs"]:
   target=projection/k
   if not target.exists():
    target.parent.mkdir(parents=True,exist_ok=True);target.symlink_to(BASE/k)
 seen=set()
 with tarfile.open(D/"delta.tar.gz","r:gz") as archive:
  for member in archive:
   assert member.isfile() and member.name not in seen
   data=archive.extractfile(member).read();seen.add(member.name)
   if member.name.startswith("before/"):
    k=member.name.removeprefix("before/");assert k in eq["before"] and hashlib.sha256(data).hexdigest()==eq["before"][k]["sha256"]
    assert sha(BASE/k)==eq["before"][k]["sha256"],"R34 inherited guest registry must match pre-edit local bytes"
    target=S0/k
   else:
    assert member.name in eq["delta"] and hashlib.sha256(data).hexdigest()==eq["delta"][member.name]
    target=S/member.name
   assert not target.exists()
   target.write_bytes(data);os.chmod(target,0o444)
 assert seen==set(eq["delta"])|{"before/"+k for k in eq["before"]}
 for repo in ("uwvm2","uwvm2-ros"):
  k=repo+"/test/0017.runtime/debug_checkpoint_worker_registry_reuse_runtime.cc"
  (S0/k).symlink_to(S/k)
 assert all(sha(S/k)==h for k,h in manifest.items())

assert all(sha(S/k)==h for k,h in manifest.items())
assert all(sha(S0/k)==r["sha256"] for k,r in eq["before"].items())
for repo in ("uwvm2","uwvm2-ros"):
 assert sha(S/repo/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")==sha(BASE/repo/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")
 assert sha(S0/repo/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")==sha(BASE/repo/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")
T=Path("/dev/shm")/("uwvm-checkpoint-r41-v3-"+str(os.getpid()));T.mkdir(mode=0o700)
birth=int(Path("/proc/self/stat").read_text().rsplit(")",1)[1].split()[19])
(D/"native-storage-v3.json").write_text(json.dumps(dict(path=str(T),creator_pid=os.getpid(),creator_birth=birth,device=T.stat().st_dev,inode=T.stat().st_ino,uid=os.getuid(),maximum_bytes=768<<20)))
env=dict(os.environ,MALLOC_ARENA_MAX="1",TMPDIR=str(T),PYTHONDONTWRITEBYTECODE="1",
 LD_LIBRARY_PATH="/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu")
validator=Path("/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools")
assert sha(validator)=="23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453"
rows=[];deps={};products={};fixtures={}
def save():
 (D/"results-v3.json").write_text(json.dumps(dict(rows=rows,dependencies=deps,products=products,fixtures=fixtures,source_manifest_sha256=sha(D/"source-manifest.json"),controller_sha256=sha(__file__)),indent=2)+"\n")
def run(repo,stage,argv,native=False,frontend=False,expected_failure=False):
 print(repo,stage,"START",flush=True);log=D/(repo+"-"+stage+"-v3.log");started=time.monotonic()
 with log.open("xb") as output:
  def limits():
   cap=64<<30 if native else 8<<30;resource.setrlimit(resource.RLIMIT_AS,(cap,cap))
  result=subprocess.run(argv,preexec_fn=limits,stdout=output,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,cwd=T,env=env,timeout=600)
 evidence=log.read_text(errors="replace")
 passed=result.returncode==0
 if expected_failure:passed=result.returncode!=0 and "REGISTRY first_actual_join_slot_reuse_status=4 retained_old_handles=256" in evidence
 if native and not expected_failure and "registry-" in stage:
  passed=passed and "actual_workers=585 actual_guest_returns_42=329 weak_holes_reused=320 PASS" in evidence
 row=dict(repo=repo,stage=stage,argv=argv,exit=result.returncode,passed=passed,log_sha256=sha(log),seconds=time.monotonic()-started,
  actual_native_runtime_execution=native,compiler_frontend_only=frontend,expected_pre_fix_regression=expected_failure)
 rows.append(row);save();print(repo,stage,result.returncode,flush=True)
 if not passed:print(evidence[-12000:],flush=True);sys.exit(1)
 assert all(sha(S/k)==h for k,h in manifest.items())
def flags(repo,target,root):
 P=OLD/("products-v8/linux-integrated" if target=="linux" else "products-v8/"+target)/repo
 proof=json.loads((P/"runtime-qualified.json").read_text());assert all(sha(p)==h for p,h in proof["dependencies"].items())
 records=json.loads((P/"results.json").read_text())
 args=next(r["argv"] for r in records if r["name"]=="runtime-compile");args=args[:args.index("-MD")]
 args=[a for a in args if a!="-DUWVM_USE_UWVM_INT"];args[1:1]=["-DUWVM_DISABLE_INT","-fdelayed-template-parsing","-I"+str(root/"src")]
 assert sha(args[0])==proof["compiler_sha256"];deps[args[0]]=sha(args[0])
 return args,proof,records
for repo in ("uwvm2","uwvm2-ros"):
 for target in ("linux","windows","freebsd","macos"):
  args,proof,records=flags(repo,target,S/repo)
  for kind,relative in (("registry","test/0017.runtime/debug_checkpoint_worker_registry_reuse_runtime.cc"),("runtime","src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")):
   run(repo,target+"-"+kind+"-frontend",[*args,"-fsyntax-only",str(S/repo/relative)],frontend=True)
for repo in ("uwvm2","uwvm2-ros"):
 root=S/repo;parent=OLD/"repaired-inputs-v8"/repo;args,proof,records=flags(repo,"linux",root)
 P=OLD/"products-v8/linux-integrated"/repo;hostproof=json.loads((P/"host-api-qualified.json").read_text());assert all(sha(p)==h for p,h in hostproof["dependencies"].items())
 link=next(r["argv"] for r in records if r["name"]=="debug_checkpoint_prepared_retirement-build")
 tail=link[link.index("--ld-path="+str(Path(args[0]).parent/"ld.lld")):link.index("-o")]
 for arg in tail:
  path=arg.removeprefix("--ld-path=")
  if Path(path).is_file():deps[path]=sha(path)
 runtime=T/(repo+"-runtime.o");host=T/(repo+"-host.o")
 run(repo,"host-api-compile",[*args,"-c",str(parent/"src/uwvm2/uwvm/host_api.default.cpp"),"-o",str(host)])
 products[str(host)]=dict(sha256=sha(host),size=host.stat().st_size);save()
 names=("debug_checkpoint_retirement_cohort","debug_checkpoint_indirect_retirement_cohort","debug_wasip1_checkpoint","debug_wasip1_indirect_prepared_retirement",
        "checkpoint_actual_generation2_nested_continuation","checkpoint_actual_generation2_nested_replacement")
 wasm={}
 for name in names:
  wat=root/"test/0017.runtime/fixtures"/(name+".wat")
  if not wat.exists():wat=parent/"test/0017.runtime/fixtures"/(name+".wat")
  fixtures[str(wat)]=sha(wat);out=T/(repo+"-"+name+".wasm");wasm[name]=out
  run(repo,name+"-assemble",[str(validator),"parse",str(wat),"-o",str(out)])
  run(repo,name+"-validate",[str(validator),"validate",str(out),"--features","all"])
 if repo=="uwvm2":
  before_args,before_proof,_=flags(repo,"linux",S0/repo)
  run(repo,"pre-fix-runtime-compile",[*before_args,"-c",str(S0/repo/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"),"-o",str(runtime)])
  exe=T/"before-registry"
  run(repo,"pre-fix-registry-build",[*before_args,str(root/"test/0017.runtime/debug_checkpoint_worker_registry_reuse_runtime.cc"),str(runtime),str(host),*tail,"-o",str(exe)])
  products[str(exe)]=dict(sha256=sha(exe),size=exe.stat().st_size)
  run(repo,"pre-fix-registry-reproduction",[str(exe),str(wasm["debug_checkpoint_retirement_cohort"]),"instruction"],native=True,expected_failure=True)
  exe.unlink();runtime.unlink()
 run(repo,"runtime-compile",[*args,"-c",str(root/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"),"-o",str(runtime)])
 products[str(runtime)]=dict(sha256=sha(runtime),size=runtime.stat().st_size);save()
 specs=(("registry","debug_checkpoint_worker_registry_reuse_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),)),
        ("core","debug_checkpoint_prepared_retirement_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),("indirect","debug_checkpoint_indirect_retirement_cohort"))),
        ("wasip1","debug_wasip1_prepared_retirement_runtime.cc",(("normal","debug_wasip1_checkpoint"),("indirect","debug_wasip1_indirect_prepared_retirement"))))
 for kind,name,cases in specs:
  exe=T/(repo+"-"+kind);source=root/"test/0017.runtime"/name
  run(repo,kind+"-build",[*args,str(source),str(runtime),str(host),*tail,"-o",str(exe)])
  products[str(exe)]=dict(sha256=sha(exe),size=exe.stat().st_size);save()
  for case,fixture in cases:
   for policy in ("instruction","unwind"):
    run(repo,kind+"-"+case+"-"+policy,[str(exe),str(wasm[fixture]),policy,*(["indirect"] if case=="indirect" else [])],native=True)
    assert sha(exe)==products[str(exe)]["sha256"]
  exe.unlink()
 replacement=wasm["checkpoint_actual_generation2_nested_replacement"].read_bytes()
 assert replacement[:8]==b"\0asm\1\0\0\0"
 def uleb(data,offset):
  value=0
  for step in range(5):
   assert offset<len(data);byte=data[offset];offset+=1;value|=(byte&127)<<(step*7)
   if not byte&128:assert value<=0xffffffff;return value,offset
  raise AssertionError("unbounded Wasm size")
 offset=8;body=None
 while offset<len(replacement):
  kind=replacement[offset];offset+=1;size,start=uleb(replacement,offset);end=start+size;assert end<=len(replacement)
  if kind==10:
   assert body is None;count,index=uleb(replacement,start);assert count==1
   length,index=uleb(replacement,index);assert 0<length<=1048576 and index+length==end;body=replacement[index:end]
  offset=end
 assert offset==len(replacement) and body is not None and body[-1]==11
 body_path=T/(repo+"-generation2-body.bin");body_path.write_bytes(body)
 exe=T/(repo+"-active");source=root/"test/0017.runtime/debug_checkpoint_active_code_handoff_generation_runtime.cc"
 run(repo,"active-build",[*args,str(source),str(runtime),str(host),*tail,"-o",str(exe)])
 products[str(exe)]=dict(sha256=sha(exe),size=exe.stat().st_size);save()
 for policy in ("instruction","unwind"):
  run(repo,"active-generation2-"+policy,[str(exe),str(wasm["checkpoint_actual_generation2_nested_continuation"]),policy,str(body_path)],native=True)
 for path in (exe,body_path,runtime,host,*wasm.values()):path.unlink()
 assert all(sha(p)==h for p,h in proof["dependencies"].items()) and all(sha(p)==h for p,h in hostproof["dependencies"].items())
assert all(sha(p)==h for p,h in deps.items()) and all(sha(p)==h for p,h in fixtures.items())
assert sum(r["compiler_frontend_only"] and r["passed"] for r in rows)==16
assert sum(r["actual_native_runtime_execution"] and r["passed"] and not r["expected_pre_fix_regression"] for r in rows)==24
assert sum(r["expected_pre_fix_regression"] and r["passed"] for r in rows)==1
save()
(D/"qualified-v3.json").write_text(json.dumps(dict(passed=True,fresh_native_cases=24,expected_pre_fix_native_failure_cases=1,frontend_checks=16,
 source_manifest_sha256=sha(D/"source-manifest.json"),source_equivalence_sha256=sha(D/"source-equivalence.json"),results_sha256=sha(D/"results-v3.json"),controller_sha256=sha(__file__),
 original_live_native_limit=256,live_capacity_reused_only_after_actual_os_tls_join=True,canonical_completed_retry_preserved=True,
 stress_native_workers=2340,stress_actual_guest_returns_42=1316,world_publication=False,restored_worker_startup=False,new_world_replay=False),indent=2)+"\n")
