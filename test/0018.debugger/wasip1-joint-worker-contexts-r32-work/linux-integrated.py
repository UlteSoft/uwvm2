from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex,time
D=Path(__file__).parent;E=D.parent.parent;R=E/'rounds/wasip1-cross-jit-20261007-r26'
repo=sys.argv[2];assert sys.argv[1] in ('joint-linux-integrated-r32','joint-unit-r32') and repo in ('uwvm2','uwvm2-ros')
S=D/'inputs'/repo;B=S/'test/0017.runtime';O=D/'products/linux-integrated'/repo;O.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads((D/'inputs.json').read_text());assert all(sha(D/'inputs'/p)==h for p,h in M.items())
P=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35')
SDK=json.loads((P/'sdk-qualified-r35.json').read_text())
assert all(sha(p)==h for g in ('archives','source_inputs','generated_headers') for p,h in SDK[g].items())
old=json.loads((R/'products/linux'/repo/'runtime-compiled.json').read_text())['argv']
base=old[:old.index('-MD')];base=[v.replace(str(R/'inputs'/repo),str(S)) for v in base]
T=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
link=['--ld-path='+str(T/'bin/ld.lld'),'-L'+str(T/'lib/x86_64-unknown-linux-gnu'),'-Wl,-rpath,'+str(T/'lib/x86_64-unknown-linux-gnu'),'-rtlib=compiler-rt','-unwindlib=libunwind','-Wl,--start-group',*SDK['archives'].keys(),'-Wl,--end-group','-lrt','-ldl','-lm','-lssl','-lcrypto']
env=dict(os.environ,MALLOC_ARENA_MAX='1',MALLOC_TRIM_THRESHOLD_='131072',MALLOC_MMAP_THRESHOLD_='131072')
rows=json.loads((O/'results.json').read_text()) if (O/'results.json').exists() else []
def run(name,args,timeout=1200):
 p=O/(name+'.log')
 previous=next((r for r in rows if r['name']==name),None)
 if previous is not None:
  assert sha(p)==previous['log_sha256']
  if previous['passed']:
   assert previous['argv']==list(map(str,args));print('qualified completed step retained',repo,name,flush=True);return previous
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
run('wasip1-memory-binding-build',[*base,'-MD','-MF',O/'wasip1-memory-binding.d',B/'wasip1_memory_binding_reset.cc',*link,'-o',O/'wasip1-memory-binding'])
unit=run('wasip1-memory-binding',[O/'wasip1-memory-binding'],60)
assert 'wasip1_memory_binding_reset PASS checks=10' in (O/'wasip1-memory-binding.log').read_text()
(O/'wasip1-memory-binding-qualified.json').write_text(json.dumps(dict(passed=True,binary_sha256=sha(O/'wasip1-memory-binding'),checks=10,log_sha256=unit['log_sha256'],dependencies=deps(O/'wasip1-memory-binding.d'),source_manifest_sha256=sha(D/'inputs.json')),indent=2)+'\n')
if sys.argv[1]=='joint-unit-r32':raise SystemExit(0)
for label,source in [('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),('host-api','src/uwvm2/uwvm/host_api.default.cpp')]:
 compiled=run(label+'-compile',[*base,'-MD','-MF',O/(label+'.d'),'-c',S/source,'-o',O/(label+'.o')])
 (O/(label+'-qualified.json')).write_text(json.dumps(dict(argv=compiled['argv'],object_sha256=sha(O/(label+'.o')),dependencies=deps(O/(label+'.d')),compiler_sha256=sha(T/'bin/clang++'),sdk_proof_sha256=sha(P/'sdk-qualified-r35.json')),indent=2)+'\n')
W=P.parent/'assets/wasm-tools/wasm-tools'
tool=json.loads((P.parent/'assets/wasm-tools/package-manifest41.json').read_text());assert sha(W)==tool['tool_sha256']
name='debug_wasip1_checkpoint'
run('assemble',[W,'parse',S/'test/0017.runtime/fixtures'/(name+'.wat'),'-o',O/'fixture.wasm'],60)
run('validate',[W,'validate',O/'fixture.wasm','--features','all'],60)
run('fixture-build',[*base,'-MD','-MF',O/'fixture.d',B/(name+'_runtime.cc'),O/'runtime.o',O/'host-api.o',*link,'-o',O/'fixture'])
proof=dict(binary_sha256=sha(O/'fixture'),wasm_sha256=sha(O/'fixture.wasm'),runtime_sha256=sha(O/'runtime.o'),host_api_sha256=sha(O/'host-api.o'),dependencies=deps(O/'fixture.d'),source_manifest_sha256=sha(D/'inputs.json'),compiler_sha256=sha(T/'bin/clang++'),sdk_proof_sha256=sha(P/'sdk-qualified-r35.json'),wasm_tools=tool)
(O/'fixture-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
for policy in ('instruction','unwind'):
 root=O/policy;root.mkdir(exist_ok=True)
 run('native-'+policy,[O/'fixture',O/'fixture.wasm',policy,root],300)
 text=(O/('native-'+policy+'.log')).read_text();assert 'debug_wasip1_checkpoint_runtime PASS checks=' in text
 assert 'JOINT_PREPARATION status=0 ' in text
print('PASS actual joint cold preparation',repo,flush=True)

W=P.parent/'assets/wasm-tools/wasm-tools'
alias=B/'fixtures/debug_wasip1_checkpoint_builtin_aliases.wat'
run('aliases-assemble',[W,'parse',alias,'-o',O/'aliases.wasm'],60)
run('aliases-validate',[W,'validate',O/'aliases.wasm','--features','all'],60)
for policy in ('instruction','unwind'):
 root=O/('aliases-'+policy);root.mkdir(exist_ok=True)
 run('aliases-'+policy,[O/'fixture',O/'aliases.wasm',policy,root],300)
for name in ('debug_wasip1_environment_group','debug_wasip1_checkpoint'):
 run(name+'-group-assemble',[W,'parse',B/'fixtures'/(name+'.wat'),'-o',O/(name+'-group.wasm')],60)
 run(name+'-group-validate',[W,'validate',O/(name+'-group.wasm'),'--features','all'],60)
run('environment-group-build',[*base,'-MD','-MF',O/'environment-group.d',B/'debug_wasip1_environment_group_runtime.cc',O/'runtime.o',O/'host-api.o',*link,'-o',O/'environment-group'])
for policy in ('instruction','unwind'):
 root=O/('environment-group-'+policy);root.mkdir(exist_ok=True)
 run('environment-group-'+policy,[O/'environment-group',O/'debug_wasip1_environment_group-group.wasm',O/'debug_wasip1_checkpoint-group.wasm',policy,root],300)
for name in ('main','provider'):
 run('worker-chain-'+name+'-assemble',[W,'parse',B/'fixtures'/('debug_wasip1_worker_environment_'+name+'.wat'),'-o',O/('worker-chain-'+name+'.wasm')],60)
 run('worker-chain-'+name+'-validate',[W,'validate',O/('worker-chain-'+name+'.wasm'),'--features','all'],60)
for policy in ('instruction','unwind'):
 root=O/('worker-chain-'+policy);root.mkdir(exist_ok=True)
 run('worker-chain-'+policy,[O/'environment-group',O/'worker-chain-main.wasm',O/'worker-chain-provider.wasm',policy,root,'worker-chain'],300)
for fixture in ('debug_checkpoint_complete_instance','debug_checkpoint_complete_preload'):
 names=[fixture] if fixture.endswith('instance') else ['debug_checkpoint_complete_preload_main','debug_checkpoint_complete_preload_provider']
 for name in names:
  run(name+'-assemble',[W,'parse',S/'test/0017.runtime/fixtures'/(name+'.wat'),'-o',O/(name+'.wasm')],60)
  run(name+'-validate',[W,'validate',O/(name+'.wasm'),'--features','all'],60)
 run(fixture+'-build',[*base,'-MD','-MF',O/(fixture+'.d'),S/'test/0017.runtime'/(fixture+'_runtime.cc'),O/'runtime.o',O/'host-api.o',*link,'-o',O/fixture])
 for policy in ('instruction','unwind'):
  run(fixture+'-'+policy,[O/fixture,*[O/(name+'.wasm') for name in names],policy],300)
print('PASS qualified joint preparation, builtin aliases, complete-instance and preload regression',repo,flush=True)

name='debug_checkpoint_host_effects'
run('foreign-observation-assemble',[W,'parse',B/'fixtures'/(name+'.wat'),'-o',O/'foreign-observation.wasm'],60)
run('foreign-observation-validate',[W,'validate',O/'foreign-observation.wasm','--features','all'],60)
run('foreign-observation-build',[*base,'-MD','-MF',O/'foreign-observation.d',B/'debug_checkpoint_observation_foreign_runtime.cc',O/'runtime.o',O/'host-api.o',*link,'-o',O/'foreign-observation'])
for policy in ('instruction','unwind'):
 for entry in range(1,7):
  run('foreign-observation-'+policy+'-'+str(entry),[O/'foreign-observation',O/'foreign-observation.wasm',policy,str(entry)],300)
print('PASS six genuine foreign callback call forms retain observation capture refusal',repo,flush=True)
