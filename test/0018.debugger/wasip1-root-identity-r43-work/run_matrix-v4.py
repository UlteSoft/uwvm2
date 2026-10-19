"""R43 authentic compiled root identity: cold/live roots, hot generation2 fallback and private unsealed generation2 preparation."""
from pathlib import Path
import hashlib,json,os,resource,subprocess,sys,tarfile,time
D=Path(__file__).parent;E=D.parent.parent;OLD=E/"rounds/wasip1-prepared-retirement-20261008-r34"
PREV=E/"rounds/wasip1-closed-worker-20261008-r42";BASE=PREV/"source-v2";S=D/"source-v3"
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
manifest=json.loads((D/"source-manifest-v4.json").read_text());eq=json.loads((D/"source-equivalence-v4.json").read_text())
q=json.loads((PREV/"qualified-v2.json").read_text());guard=json.loads((PREV/"guard-matrix-v2.json").read_text())
assert q["passed"] and q["fresh_native_cases"]==24 and guard["passed"] and guard["parent_events_unchanged"]
assert all(p["pidfd_retired"] for p in guard["processes"]) and Path("/proc/self/cgroup").read_text()==guard["cgroup"]
assert sha(PREV/"source-manifest-v2.json")==eq["previous_manifest_sha256"]==q["source_manifest_sha256"]
assert len(manifest)==80 and len(eq["unchanged_inputs"])==50 and len(eq["delta"])==30
assert all(manifest[k]==h and sha(BASE/k)==h for k,h in eq["unchanged_inputs"].items())
# Exact v3 immutable source and all successful frontend rows are retained.
# The later Python registry tuple error was before native execution.
prior_guard=json.loads((D/"guard-matrix-v3.json").read_text())
prior_results=json.loads((D/"results-v3.json").read_text())
assert not prior_guard["passed"] and prior_guard["parent_events_unchanged"] and prior_guard["ram_disposable_products_retired"]
assert all(p["pidfd_retired"] for p in prior_guard["processes"])
assert sum(r["actual_native_runtime_execution"] for r in prior_results["rows"])==0
assert prior_results["source_manifest_sha256"]==sha(D/"source-manifest-v3.json")==sha(D/"source-manifest-v4.json")
assert all(sha(S/k)==h for k,h in manifest.items())
prior_frontends=[dict(row) for row in prior_results["rows"] if row["compiler_frontend_only"]]
assert len(prior_frontends)==24 and all(row["passed"] and row["exit"]==0 for row in prior_frontends)
for row in prior_frontends:
 log=D/(row["repo"]+"-"+row["stage"]+"-v3.log")
 assert sha(log)==row["log_sha256"]
 row["log_file"]=str(log);row["reused_exact_source_frontend"]=True
T=Path("/dev/shm")/("uwvm-checkpoint-r43-v4-"+str(os.getpid()));T.mkdir(mode=0o700)
birth=int(Path("/proc/self/stat").read_text().rsplit(")",1)[1].split()[19])
(D/"native-storage-v4.json").write_text(json.dumps(dict(path=str(T),creator_pid=os.getpid(),creator_birth=birth,device=T.stat().st_dev,inode=T.stat().st_ino,uid=os.getuid(),maximum_bytes=768<<20)))
env=dict(os.environ,MALLOC_ARENA_MAX="1",TMPDIR=str(T),PYTHONDONTWRITEBYTECODE="1",
 LD_LIBRARY_PATH="/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu")
validator=Path("/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools")
assert sha(validator)=="23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453"
rows=prior_frontends;deps={};products={};fixtures={}
def save():
 (D/"results-v4.json").write_text(json.dumps(dict(rows=rows,dependencies=deps,products=products,fixtures=fixtures,source_manifest_sha256=sha(D/"source-manifest-v4.json"),controller_sha256=sha(__file__)),indent=2)+"\n")
def run(repo,stage,argv,native=False,frontend=False,expected_failure=False):
 print(repo,stage,"START",flush=True);log=D/(repo+"-"+stage+"-v4.log");started=time.monotonic()
 with log.open("xb") as output:
  def limits():
   cap=64<<30 if native else 8<<30;resource.setrlimit(resource.RLIMIT_AS,(cap,cap))
  result=subprocess.run(argv,preexec_fn=limits,stdout=output,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL,cwd=T,env=env,timeout=600)
 evidence=log.read_text(errors="replace")
 passed=result.returncode==0
 if expected_failure:passed=result.returncode!=0 and "REGISTRY first_actual_join_slot_reuse_status=4 retained_old_handles=256" in evidence
 if native and not expected_failure and "registry-" in stage:
  passed=passed and "actual_workers=585 actual_guest_returns_42=329 weak_holes_reused=320 PASS" in evidence
 row=dict(repo=repo,stage=stage,argv=argv,exit=result.returncode,passed=passed,log_sha256=sha(log),log_file=str(log),seconds=time.monotonic()-started,
  actual_native_runtime_execution=native,compiler_frontend_only=frontend,expected_pre_fix_regression=expected_failure)
 rows.append(row);save();print(repo,stage,result.returncode,flush=True)
 if native and stage.startswith("observation-"):assert "checkpoint_actual_observation: PASS" in evidence
 if native and stage.startswith("native-code-"):assert "debug_native_code_runtime: PASS" in evidence
 if native and stage.startswith("active-generation2-"):assert "ROOT_IDENTITY private_generation2=1 live_seal=0 PASS" in evidence
 if native and stage.startswith(("core-","wasip1-")):
  passed=passed and "cancel_actual_os_tls_join_before_open=1 guest_execution=0 PASS" in evidence
  row["passed"]=passed;save()
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
  args,proof,records=flags(repo,target,S/repo) # recheck every exact target dependency closure
assert all(sha(S/k)==h for k,h in manifest.items())
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
        "checkpoint_actual_generation2_nested_continuation","checkpoint_actual_generation2_nested_replacement","debug_checkpoint_worker_registry_reuse","checkpoint_actual_continuation","debug_activation_runtime")
 wasm={}
 for name in names:
  wat=root/"test/0017.runtime/fixtures"/(name+".wat")
  if name=="debug_activation_runtime":wat=root/"test/0017.runtime/debug_activation_runtime.wat"
  if not wat.exists():wat=parent/"test/0017.runtime/fixtures"/(name+".wat")
  fixtures[str(wat)]=sha(wat);out=T/(repo+"-"+name+".wasm");wasm[name]=out
  run(repo,name+"-assemble",[str(validator),"parse",str(wat),"-o",str(out)])
  run(repo,name+"-validate",[str(validator),"validate",str(out),"--features","all"])
 run(repo,"runtime-compile",[*args,"-c",str(root/"src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"),"-o",str(runtime)])
 products[str(runtime)]=dict(sha256=sha(runtime),size=runtime.stat().st_size);save()
 specs=(("registry","debug_checkpoint_worker_registry_reuse_runtime.cc",(("normal","debug_checkpoint_worker_registry_reuse"),)),
        ("core","debug_checkpoint_prepared_retirement_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),("indirect","debug_checkpoint_indirect_retirement_cohort"))),
        ("wasip1","debug_wasip1_prepared_retirement_runtime.cc",(("normal","debug_wasip1_checkpoint"),("indirect","debug_wasip1_indirect_prepared_retirement"))),
        ("private-generation2","debug_checkpoint_private_code_handoff_generation_runtime.cc",(("normal","debug_checkpoint_retirement_cohort"),)),
        ("observation","debug_checkpoint_actual_observation_runtime.cc",(("normal","checkpoint_actual_continuation"),)),
        ("native-code","debug_native_code_runtime.cc",(("normal","debug_activation_runtime"),)))
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
assert sum(r["compiler_frontend_only"] and r["passed"] for r in rows)==24
assert sum(r["actual_native_runtime_execution"] and r["passed"] for r in rows)==36
save()
(D/"qualified-v4.json").write_text(json.dumps(dict(passed=True,fresh_native_cases=36,no_profile_native_code_cases=4,observation_only_checkpoint_cases=4,closed_cohort_native_cases=16,frontend_checks=24,reused_exact_source_frontend_checks=24,fresh_frontend_checks=0,private_generation2_native_cases=4,
 frontend_source_manifest_sha256=sha(D/"source-manifest-v3.json"),frontend_prior_results_sha256=sha(D/"results-v3.json"),frontend_prior_guard_sha256=sha(D/"guard-matrix-v3.json"),
 current_cold_root_identity_sealed_by_actual_materializer=True,committed_hot_generation2_fallback_validated=True,private_candidate_root_identity_unsealed=True,
 fresh_restored_generation2_root_publication=False,
 source_manifest_sha256=sha(D/"source-manifest-v4.json"),source_equivalence_sha256=sha(D/"source-equivalence-v4.json"),results_sha256=sha(D/"results-v4.json"),controller_sha256=sha(__file__),
 held_before_actual_old_retirement=True,rechecked_after_actual_old_os_tls_join=True,cancelled_and_joined_before_old_admission_reopens=True,
 world_publication=False,restored_worker_activation=False,new_world_replay=False,other_three_os_native_delta_validation=False),indent=2)+"\n")
