from pathlib import Path
import sys,json,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
canonical_sources=[Path('/tmp/uwvm-gc-perf-20261011-r73/source/cases-restored-a1.json')]+[R/'source'/s for s in ['array-edge-mutations-native-a1.json','gc-scalar-graph-native-a1.json','gc-raw-nan-native-a1.json','repeated-array-types-native-a1.json','observable-features-native-a1.json','gc-phase-feedback-native-c1.json','uniform-reference-mutations-native-c3.json','local-catch-reference-fixtures-native-c6.json']]
extra={'feature-memory64':['-WFE-memory64'],'feature-table64':['-WFE-table64'],'feature-multi-memory':['-WFE-multi-memory'],'feature-extended-constant-expressions':['-WFE-extended-const'],'feature-threads-atomic-semantics':['-WFE-threads']}
cases=[dict(c,expected_trap=None) for p in canonical_sources for c in json.loads(p.read_text())]
for n in ['typed-array-boundaries-c20','array-length-boundaries-c20']:
 for c in json.loads((R/'source'/n/'cases.json').read_text())['cases']:cases.append(dict(c,expected_trap=c['trap']))
assert len(cases)==279,len(cases)
runflags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd']

cases += [dict(c,expected_trap=c['trap']) for c in json.loads((R/'source/typed-array-store-c21-a2/cases.json').read_text())['cases']]
rows=[];elf=R/'build/deferred-typed-array-access-known-length/uwvm'
with N.lease():
 for i,c in enumerate(cases):
  assert N.sha(c['wasm'])==c['wasm_sha'];lab='inline-auth-correctness-c25-a1-'+str(i);env=dict(os.environ)
  for k in list(env):
   if k.startswith('WASM_PERF_NATIVE') or k=='UWVM_GC_ARRAY_BUDGET_MIB':env.pop(k,None)
  try:r=N.run(lab,[elf,*runflags,*extra.get(c['name'],[]),*c.get('flags',[]),'--run',c['wasm']],cpu=6,timeout=45,env=env)
  except AssertionError:
   p=R/'evidence'/(lab+'.json')
   if not p.exists():raise
   r=json.loads(p.read_text());assert c['expected_trap'] and r['guard'] is None and r['returncode']!=0,r
  if c['expected_trap']:assert c['expected_trap'] in (R/'evidence'/(lab+'.stderr')).read_text().lower(),c
  rows.append(dict(name=c['name'],pass_=True,expected_trap=c['expected_trap'],wasm_sha=c['wasm_sha'],receipt=lab))
  if i%25==0:print('C25_CORRECTNESS',i+1,len(cases),flush=True)
 N.save(R/'evidence/inline-auth-correctness-c25-a1.json',{'cases':len(rows),'rows':rows,'ELF_sha':N.sha(elf),'source_binding_sha':N.sha(R/'source/uwvm2-candidate-c25-a1/source-binding-c25-a1.json'),'scope':'279 existing+100 new native Wasm tests, expected traps/validator rejections included; private C23 inline-auth full JIT ELF.'})
