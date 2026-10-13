from pathlib import Path
import sys,json,os,fcntl,time
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
class L:
 def __enter__(self):
  self.f=open(N.BASE/'control/native-c23-repo-build.lock','a+');fcntl.flock(self.f,fcntl.LOCK_EX|fcntl.LOCK_NB);N.check(build=True);return self
 def __exit__(self,*a):fcntl.flock(self.f,fcntl.LOCK_UN);self.f.close()
N.lease=L
plan=json.loads((R/'source/build-plan-typed-array-load-c20.json').read_text());newflags=['-DUWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS=1','-DUWVM_EXPERIMENTAL_SCALAR_ARRAY_LENGTH=1','-DUWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES=1','-DUWVM_EXPERIMENTAL_TYPED_ARRAY_LOAD=1','-DUWVM_EXPERIMENTAL_TYPED_ARRAY_STORE=1','-DUWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH=1']
canonical_sources=[Path('/tmp/uwvm-gc-perf-20261011-r73/source/cases-restored-a1.json')]+[R/'source'/s for s in ['array-edge-mutations-native-a1.json','gc-scalar-graph-native-a1.json','gc-raw-nan-native-a1.json','repeated-array-types-native-a1.json','observable-features-native-a1.json','gc-phase-feedback-native-c1.json','uniform-reference-mutations-native-c3.json','local-catch-reference-fixtures-native-c6.json']]
extra={'feature-memory64':['-WFE-memory64'],'feature-table64':['-WFE-table64'],'feature-multi-memory':['-WFE-multi-memory'],'feature-extended-constant-expressions':['-WFE-extended-const'],'feature-threads-atomic-semantics':['-WFE-threads']}
cases=[dict(c,expected_trap=None) for p in canonical_sources for c in json.loads(p.read_text())]
for n in ['typed-array-boundaries-c20','array-length-boundaries-c20']:
 for c in json.loads((R/'source'/n/'cases.json').read_text())['cases']:cases.append(dict(c,expected_trap=c['trap']))
assert len(cases)==279,len(cases)
cases += [dict(c,expected_trap=c['trap']) for c in json.loads((R/'source/typed-array-store-c21-a2/cases.json').read_text())['cases']]
cases += [dict(c,expected_trap=c['trap']) for c in json.loads((R/'source/KEEP-C23-fixtures/cases.json').read_text())['cases']]
cases += [dict(c,expected_trap=c['trap']) for c in json.loads((R/'source/static-layout-fixtures-c23-a2/cases.json').read_text())['cases']]
assert len(cases)==491
runflags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd','-Rllvm-cache-path','disable']
with N.lease():
 for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
  S=R/'source/repo-sync-c23-current'/repo;assert (S/'current-source-binding.json').exists();binding=json.loads((S/'current-source-binding.json').read_text());assert all(N.sha(S/f)==h for f,h in binding['files'].items());BD=R/'build/repo-sync-c23-a1'/repo;BD.mkdir(parents=True,exist_ok=True)
  if (R/'evidence'/('repo-sync-c23-a1-'+repo+'-qualification.json')).exists():
   print('REPO_SYNC_EXISTING_QUALIFICATION',repo,flush=True);continue
  runflags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd','-Rllvm-cache-path','disable']
  source_id=N.sha(S/'current-source-binding.json');flags=plan['common']+newflags+['-DUWVM2_BUILD_SOURCE_ID=u8"'+source_id+'"']+['-I'+str(S/'src')]+['-I'+str(S/'third-parties'/x/'include') for x in ['fast_io','bizwen','boost_unordered']];env=dict(os.environ,TMPDIR=str(R/'tmp'))
  if not (BD/'build-binding.json').exists():
   for i,tu in enumerate(plan['tus']):
    N.run('repo-sync-c23-a1-'+repo+'-compile-'+str(i),[plan['compiler'],*flags,'-c',S/tu,'-o',BD/(str(i)+'.o')],cpu=4,timeout=600,build=True,env=env);print('REPO_SYNC_COMPILED',repo,i,flush=True)
   elf=BD/'uwvm';N.run('repo-sync-c23-a1-'+repo+'-link',[plan['compiler'],*[BD/(str(i)+'.o') for i in range(3)],'@'+plan['link_rsp'],'-stdlib=libc++','-pthread','-ldl','-lz','-l:libzstd.so.1','-lm','-lcrypto','-lssl','-Wl,--gc-sections','-Wl,--strip-debug','-o',elf],cpu=4,timeout=120,build=True,env=env);N.persist(elf)
   N.save(BD/'build-binding.json',{'ELF_sha':N.sha(elf),'input_sha':N.sha(S/'current-source-binding.json'),'common':plan['common'],'optimization_flags':newflags,'verified_source_id':source_id,'compiler_flags':flags,'translation_units':{tu:N.sha(S/tu) for tu in plan['tus']},'scope':'Full3TUs current merged Mac source compiled with native LLVM verifier enabled; benchmark-only driver/publication instrumentation absent. No performance claims from these correctness binaries.'})
  else:
   elf=BD/'uwvm';binding=json.loads((BD/'build-binding.json').read_text());assert binding['ELF_sha']==N.sha(elf) and binding['input_sha']==N.sha(S/'current-source-binding.json')
  rows=[]
  for ci,c in enumerate(cases):
   assert N.sha(c['wasm'])==c['wasm_sha'];label='repo-sync-c23-a1-'+repo+'-case-'+str(ci);env=dict(os.environ)
   for k in list(env):
    if k.startswith('WASM_PERF_NATIVE') or k=='UWVM_GC_ARRAY_BUDGET_MIB':env.pop(k,None)
   try:rec=N.run(label,[elf,*runflags,*extra.get(c['name'],[]),*c.get('flags',[]),'--run',c['wasm']],cpu=6,timeout=45,env=env)
   except AssertionError:
    p=R/'evidence'/(label+'.json')
    if not p.exists():raise
    rec=json.loads(p.read_text());assert c['expected_trap'] and rec['guard'] is None and rec['returncode']!=0,rec
   if c['expected_trap']:assert c['expected_trap'] in (R/'evidence'/(label+'.stderr')).read_text().lower(),(repo,c)
   else:assert rec['returncode']==0
   rows.append({'name':c['name'],'expected_trap':c['expected_trap'],'pass':True,'WASM_sha':c['wasm_sha'],'receipt_sha':N.sha(R/'evidence'/(label+'.json'))})
   if ci%25==0:print('REPO_SYNC_WASM_PASS',repo,ci+1,len(cases),flush=True)
  N.save(R/'evidence'/('repo-sync-c23-a1-'+repo+'-qualification.json'),{'repo':repo,'ELF_sha':N.sha(elf),'input_sha':N.sha(S/'current-source-binding.json'),'cases':len(rows),'passed':len(rows),'expected_invalid_Wasm_validation_cases':2,'rows':rows,'scope':'Normal CLI native execution in cgroup;411 independent Wasm assertions/traps incl32 new authentication/domain fixtures. Private benchmark hooks absent; no clock/performance/work-counter claims.'})
  removed=[]
  for i in range(3):
   p=BD/(str(i)+'.o');removed.append({'file':str(p),'sha':N.sha(p),'bytes':p.stat().st_size});p.unlink()
  N.save(BD/'owned-intermediate-cleanup.json',{'removed':removed,'scope':'Only this build3 owned link inputs removed after ELF persisted; archive source retained.'});print('REPO_SYNC_QUALIFIED',repo,len(rows),N.sha(elf),flush=True)
 N.save(R/'evidence/repo-sync-c23-a1-all-three-qualified.json',{'repos':{r:N.sha(next(p for p in [R/'evidence'/('repo-sync-c23-a1-'+r+'-qualification.json'),R/'evidence'/('repo-sync-c23-a1-'+r+'-qualification.json')] if p.exists())) for r in ['uwvm2','uwvm2-stable','uwvm2-ros']},'cases':1473,'post':N.check()})
