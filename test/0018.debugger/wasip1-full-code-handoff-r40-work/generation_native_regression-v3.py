"""Four genuine generation-2 native cases, after the base guarded suite."""
from pathlib import Path
import hashlib, json, os, resource, subprocess, sys, tarfile, time

D=Path(__file__).parent; E=D.parent.parent; S=D/'syntax-overlay-generation-v3'; BASE=D/'syntax-overlay-v1'
OLD=E/'rounds/wasip1-prepared-retirement-20261008-r34'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
base=json.loads((D/'syntax-overlay-v1.json').read_text())
manifest=json.loads((D/'syntax-overlay-generation-v2.json').read_text())
equivalence=json.loads((D/'generation-v2-source-equivalence.json').read_text())
qualification=json.loads((D/'analysis-compile-qualified-v1.json').read_text())
guard=json.loads((D/'guard-analysis-compile-v1.json').read_text())
assert qualification['passed'] and len(qualification['rows'])==16 and guard['passed']
assert guard['parent_events_unchanged'] and all(p['pidfd_retired'] for p in guard['processes'])
assert Path('/proc/self/cgroup').read_text()==guard['cgroup']
assert len(manifest)==44 and len(base)==42 and equivalence['passed']
assert equivalence['old_manifest_sha256']==sha(D/'syntax-overlay-v1.json')==qualification['source_manifest_sha256']
assert equivalence['new_manifest_sha256']==sha(D/'syntax-overlay-generation-v2.json')
assert equivalence['unchanged_inputs']==base and all(manifest[k]==h and sha(BASE/k)==h for k,h in base.items())
added={k:h for k,h in manifest.items() if k not in base}
assert len(added)==2 and added==equivalence['added_inputs']
S.mkdir(exist_ok=True)
for repo in ('uwvm2','uwvm2-ros'):
    root=S/repo; root.mkdir(exist_ok=True)
    (root/'src').symlink_to(BASE/repo/'src')
    test=root/'test/0017.runtime'; test.mkdir(parents=True)
    for p in (BASE/repo/'test/0017.runtime').iterdir():
        (test/p.name).symlink_to(p)
seen=set()
with tarfile.open(D/'syntax-overlay-generation-v2.tar.gz','r:gz') as archive:
    for member in archive:
        assert member.isfile() and member.name in manifest and member.name not in seen
        data=archive.extractfile(member).read(); assert hashlib.sha256(data).hexdigest()==manifest[member.name]
        if member.name in added:
            path=S/member.name
            with path.open('xb') as output:output.write(data)
            os.chmod(path,0o444)
        else:assert sha(S/member.name)==manifest[member.name]
        seen.add(member.name)
assert seen==set(manifest) and all(sha(S/k)==h for k,h in manifest.items())
T=Path('/dev/shm')/('uwvm-checkpoint-r40-generation-v3-'+str(os.getpid())); T.mkdir(mode=0o700)
birth=int(Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()[19])
(D/'generation-native-v3-storage.new').write_text(json.dumps(dict(path=str(T),creator_pid=os.getpid(),creator_birth=birth,
    device=T.stat().st_dev,inode=T.stat().st_ino,uid=os.getuid(),maximum_bytes=768<<20)))
os.replace(D/'generation-native-v3-storage.new',D/'generation-native-v3-storage.json')
env=dict(os.environ,MALLOC_ARENA_MAX='1',TMPDIR=str(T),PYTHONDONTWRITEBYTECODE='1',
    LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu')
validator=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools')
assert sha(validator)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
rows=[]; dependencies={}; products={}; fixtures={}
def save():
    (D/'generation-native-v3-results.json').write_text(json.dumps(dict(rows=rows,dependencies=dependencies,products=products,
        fixtures=fixtures,source_manifest_sha256=sha(D/'syntax-overlay-generation-v2.json'),controller_sha256=sha(__file__)),indent=2)+'\n')
def run(repo,stage,argv,native=False,frontend=False):
    print(repo,stage,'START',flush=True); log=D/(repo+'-'+stage+'-generation-v2.log'); started=time.monotonic()
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
        row['actual_hot_replacement_generation_2']='GENERATION2 actual_dormant_callback_commit=2' in evidence
        row['actual_private_effective_body_owner']='GENERATION2 private_effective_body_owners=1' in evidence
        row['original_committed_callback_after_discard']='GENERATION2 original_committed_callback_after_candidate_discard=17' in evidence
        row['passed']=row['passed'] and row['actual_hot_replacement_generation_2'] and row['actual_private_effective_body_owner'] and row['original_committed_callback_after_discard']
    rows.append(row); save(); print(repo,stage,result.returncode,flush=True)
    if not row['passed']:print(log.read_text(errors='replace')[-12000:],flush=True); sys.exit(1)
    assert all(sha(S/k)==h for k,h in manifest.items())
for repo in ('uwvm2','uwvm2-ros'):
    parent=OLD/'repaired-inputs-v8'/repo; P=OLD/'products-v8/linux-integrated'/repo; root=S/repo
    proof=json.loads((P/'runtime-qualified.json').read_text()); hostproof=json.loads((P/'host-api-qualified.json').read_text())
    assert all(sha(p)==h for p,h in proof['dependencies'].items()) and all(sha(p)==h for p,h in hostproof['dependencies'].items())
    historical=json.loads((P/'results.json').read_text())
    argv=next(r['argv'] for r in historical if r['name']=='runtime-compile'); compile=argv[:argv.index('-MD')]
    compile=[arg for arg in compile if arg!='-DUWVM_USE_UWVM_INT']
    compile[1:1]=['-DUWVM_DISABLE_INT','-fdelayed-template-parsing','-I'+str(root/'src')]
    assert sha(compile[0])==proof['compiler_sha256']; dependencies[compile[0]]=sha(compile[0])
    link=next(r['argv'] for r in historical if r['name']=='debug_checkpoint_prepared_retirement-build')
    tail=link[link.index('--ld-path='+str(Path(compile[0]).parent/'ld.lld')):link.index('-o')]
    for arg in tail:
        path=arg.removeprefix('--ld-path=')
        if Path(path).is_file():dependencies[path]=sha(path)
    source=root/'test/0017.runtime/debug_checkpoint_private_code_handoff_generation_runtime.cc'
    run(repo,'generation-test-frontend',[*compile,'-fsyntax-only',str(source)],frontend=True)
    runtime=T/(repo+'-runtime.o'); host=T/(repo+'-host-api.o')
    run(repo,'runtime-compile',[*compile,'-c',str(root/'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),'-o',str(runtime)])
    run(repo,'host-api-compile',[*compile,'-c',str(parent/'src/uwvm2/uwvm/host_api.default.cpp'),'-o',str(host)])
    fixture=parent/'test/0017.runtime/fixtures/debug_checkpoint_retirement_cohort.wat'; fixtures[str(fixture)]=sha(fixture)
    wasm=T/(repo+'-retirement-cohort.wasm')
    run(repo,'fixture-assemble',[str(validator),'parse',str(fixture),'-o',str(wasm)])
    run(repo,'fixture-validate',[str(validator),'validate',str(wasm),'--features','all'])
    exe=T/(repo+'-generation2')
    run(repo,'generation-test-build',[*compile,str(source),str(runtime),str(host),*tail,'-o',str(exe)])
    for path in (runtime,host,exe):products[str(path)]=dict(sha256=sha(path),size=path.stat().st_size)
    for policy in ('instruction','unwind'):
        run(repo,'generation2-'+policy,[str(exe),str(wasm),policy],native=True)
        assert all(sha(path)==products[str(path)]['sha256'] for path in (runtime,host,exe))
    for path in (runtime,host,exe,wasm):path.unlink()
    assert all(sha(p)==h for p,h in proof['dependencies'].items()) and all(sha(p)==h for p,h in hostproof['dependencies'].items())
assert all(sha(p)==h for p,h in dependencies.items()) and all(sha(p)==h for p,h in fixtures.items())
assert sum(r['actual_native_runtime_execution'] and r['passed'] for r in rows)==4
save()
(D/'generation-native-v3-qualified.json').write_text(json.dumps(dict(passed=True,fresh_native_cases=4,
    frontend_checks=2,genuine_generation_2_cases=4,source_manifest_sha256=sha(D/'syntax-overlay-generation-v2.json'),
    base_frontend_qualification_sha256=sha(D/'analysis-compile-qualified-v1.json'),source_equivalence_sha256=sha(D/'generation-v2-source-equivalence.json'),
    results_sha256=sha(D/'generation-native-v3-results.json'),controller_sha256=sha(__file__),
    world_publication=False,restored_worker_startup=False,guest_replay=False),indent=2)+'\n')
