"""Syntax-only checks of the new delta over the authenticated R34 snapshot.

Four target frontends run on Linux; target syntax is not native OS execution.
"""
from pathlib import Path
import hashlib, json, os, subprocess, sys, tarfile, time
D=Path(__file__).parent; E=D.parent.parent; OLD=E/'rounds/wasip1-prepared-retirement-20261008-r34'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert Path('/proc/self/cgroup').read_text()=='0::/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope\n'
inputs=json.loads((D/'syntax-overlay-v1.json').read_text()); S=D/'syntax-overlay-v1'; S.mkdir(exist_ok=True)
with tarfile.open(D/'syntax-overlay-v1.tar.gz','r:gz') as stream:
    seen=set()
    for member in stream:
        assert member.isfile() and member.name in inputs and member.name not in seen
        data=stream.extractfile(member).read(); assert hashlib.sha256(data).hexdigest()==inputs[member.name]
        p=S/member.name; p.parent.mkdir(parents=True,exist_ok=True)
        if p.exists(): assert sha(p)==inputs[member.name]
        else:
            with p.open('xb') as output: output.write(data)
            os.chmod(p,0o444)
        seen.add(member.name)
    assert seen==set(inputs)
validator=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools')
assert sha(validator)=='23a32d99b55eb6623665aa0258e98de24cd760a3e9efaf2ec6739193b5976453'
validation=[]
for repo in ('uwvm2','uwvm2-ros'):
 for name in ('debug_checkpoint_indirect_retirement_cohort','debug_wasip1_indirect_prepared_retirement'):
  wat=S/repo/'test/0017.runtime/fixtures'/(name+'.wat');wasm=D/(repo+'-'+name+'-v1.wasm')
  for stage,command in [('assemble',[str(validator),'parse',str(wat),'-o',str(wasm)]),('validate',[str(validator),'validate',str(wasm),'--features','all'])]:
   started=time.monotonic();log=D/(repo+'-'+name+'-'+stage+'-v1.log')
   with log.open('xb') as output:
    result=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT,timeout=60)
   validation.append(dict(repo=repo,fixture=name,stage=stage,argv=command,passed=result.returncode==0,exit=result.returncode,
      source_sha256=sha(wat),wasm_sha256=sha(wasm) if wasm.exists() else None,log_sha256=sha(log),
      seconds=time.monotonic()-started,actual_native_runtime_execution=False))
   (D/'wasm-fixture-validation-v1.json').write_text(json.dumps(validation,indent=2)+'\n')
   print(repo,name,stage,result.returncode,flush=True)
   if result.returncode:print(log.read_text(errors='replace'));sys.exit(1)
rows=[]
for repo in ('uwvm2','uwvm2-ros'):
    parent=OLD/'repaired-inputs-v8'/repo; root=S/repo; library=root/'src/uwvm2/runtime/lib'
    for p in (parent/'src/uwvm2/runtime/lib').iterdir():
        if not p.is_file(): continue
        target=library/p.name
        if target.exists(): continue
        if p.name=='uwvm_runtime.default.cpp':
            with target.open('xb') as output: output.write(p.read_bytes())
            os.chmod(target,0o444)
        else: target.symlink_to(p)
    for target_os in ('linux','windows','freebsd','macos'):
        products=OLD/('products-v8/linux-integrated' if target_os=='linux' else 'products-v8/'+target_os)/repo
        proof_path=products/'runtime-qualified.json'; proof=json.loads(proof_path.read_text())
        assert all(sha(p)==h for p,h in proof['dependencies'].items()),'parent input changed'
        results_path=products/'results.json'; completed=json.loads(results_path.read_text())
        matching=[r for r in completed if r['name']=='runtime-compile' and r['passed'] and r['exit']==0]
        assert len(matching)==1; actual=matching[0]; base=actual['argv'][:actual['argv'].index('-MD')]
        assert base[0].endswith('/clang++') and sha(base[0])==proof['compiler_sha256']
        # LLVM-only is an explicit restriction of analysis. The full native
        # interpreter/JIT matrix remains pending under its original disk guards.
        base=[a for a in base if a!='-DUWVM_USE_UWVM_INT']
        base[1:1]=['-DUWVM_DISABLE_INT','-fdelayed-template-parsing','-I'+str(root/'src')]
        stages=[('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp')]
        if target_os=='linux':
            stages=[('core','test/0017.runtime/debug_checkpoint_prepared_retirement_runtime.cc'),
                    ('wasip1','test/0017.runtime/debug_wasip1_prepared_retirement_runtime.cc'),('instance','test/0017.runtime/debug_checkpoint_complete_instance_runtime.cc'),('preload','test/0017.runtime/debug_checkpoint_complete_preload_runtime.cc'),*stages]
        for stage,relative in stages:
            label=repo+'-'+target_os+'-'+stage+'-v1'; log=D/(label+'.log'); assert not log.exists()
            command=[*base,'-fsyntax-only',str(root/relative)]; started=time.monotonic()
            with log.open('xb') as output:
                result=subprocess.run(command,stdout=output,stderr=subprocess.STDOUT,timeout=240,
                    env=dict(os.environ,MALLOC_ARENA_MAX='1',TMPDIR=str(D),
                    LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu'))
            assert all(sha(S/p)==h for p,h in inputs.items()),'frozen new inputs changed'
            assert all(sha(p)==h for p,h in proof['dependencies'].items()),'parent input changed'
            row=dict(repo=repo,target_os=target_os,stage=stage,argv=command,exit=result.returncode,
                passed=result.returncode==0,log_sha256=sha(log),seconds=time.monotonic()-started,
                source_manifest_sha256=sha(D/'syntax-overlay-v1.json'),parent_compile_proof_sha256=sha(proof_path),
                parent_actual_compile_results_sha256=sha(results_path),compiler_sha256=sha(base[0]),
                llvm_only=True,delayed_template_parsing=True,actual_native_runtime_execution=False)
            rows.append(row); (D/'analysis-compile-results-v1.json').write_text(json.dumps(rows,indent=2)+'\n')
            print(repo,target_os,stage,result.returncode,flush=True)
            if result.returncode: print(log.read_text(errors='replace')[-12000:],flush=True); sys.exit(1)
assert len(rows)==16
(D/'analysis-compile-qualified-v1.json').write_text(json.dumps(dict(passed=True,rows=rows,
    compiler_frontend_checks=16,targets=['linux','windows','freebsd','macos'],
    source_manifest_sha256=sha(D/'syntax-overlay-v1.json'),controller_sha256=sha(__file__),wasm_data_fixture_checks=validation,
    llvm_only=True,delayed_template_parsing=True,actual_native_runtime_execution=False),indent=2)+'\n')
