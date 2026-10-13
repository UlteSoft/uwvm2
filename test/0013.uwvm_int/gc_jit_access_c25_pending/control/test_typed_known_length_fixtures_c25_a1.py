from pathlib import Path
import sys,json,os,argparse
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
ap=argparse.ArgumentParser();ap.add_argument('--candidate',action='store_true');a=ap.parse_args()
cs=json.loads((R/'source/typed-known-length-fixtures-c25-a1/cases.json').read_text())['cases'];vs=['typed-array-access-known-length'] if a.candidate else ['typed-array-access-inline-static32','wasmtime-drc'];rows=[]
flags=['-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd']
with N.lease():
 for v in vs:
  elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/('build/deferred-'+v+'/uwvm')
  for c in cs:
   assert N.sha(c['wasm'])==c['wasm_sha'] and N.sha(c['wat'])==c['wat_sha'];lab='length-facts-c25-a1-'+v+'-'+c['name'];out=R/'evidence'/(lab+'.qualified.json');cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,*flags,'--run',c['wasm']]
   if out.exists():
    row=json.loads(out.read_text());assert row['ELF_sha']==N.sha(elf) and row['WASM_sha']==c['wasm_sha'];rows.append(row);continue
   env=dict(os.environ,LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'),WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS='1');env.pop('WASM_PERF_NATIVE_PROFILE',None);env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None)
   N.run(lab,cmd,cpu=6,timeout=60,env=env);ss=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(ss)==5 and all(x[1]==1 and x[2]>0 for x in ss),(lab,ss)
   if v!='wasmtime-drc':assert all(x[3]>0 and x[6]==0 and x[8]==0 for x in ss),(lab,ss)
   er=(R/'evidence'/(lab+'.stderr')).read_text().splitlines();assert not er if v=='wasmtime-drc' else all(l.startswith('UWVM-GC-FUNC-PUB ') for l in er),er
   row=dict(case=c['name'],variant=v,samples=ss,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],WAT_sha=c['wat_sha'],receipt=lab,assertions_pass=True,scope='Five post-warm self-checking invocations; this is correctness only, not performance comparison or collector pressure.');N.save(out,row);rows.append(row);print('C25_LENGTH_PASS',len(rows),len(vs)*len(cs),v,c['name'],flush=True)
 N.save(R/'evidence'/('length-facts-c25-candidate-a1.json' if a.candidate else 'length-facts-c25-baseline-a1.json'),dict(cells=len(rows),rows=rows,controller_sha=N.sha(__file__),scope='20 independent array-length-fact correctness fixtures; baselineC23/WT passes cannot qualify uncompiledC25. Candidate flag only becomes executable after32GiB build admission and signed ELF binding.'))
