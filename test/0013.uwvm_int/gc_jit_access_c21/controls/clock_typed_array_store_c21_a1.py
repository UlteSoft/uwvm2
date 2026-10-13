from pathlib import Path
import sys,json,os,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
assert json.loads((R/'evidence/typed-store-correctness-c21-a1.json').read_text())['cases']==379
old=json.loads((R/'source/alternating-long-clock-design-c20.json').read_text());jobs=dict(old['jobs'])
cases={x['name']:x for p in [O/'source/cases-restored-a1.json']+[R/'source'/n for n in ['repeated-array-types-native-a1.json','gc-scalar-graph-native-a1.json','observable-features-native-a1.json','gc-phase-feedback-native-c1.json']] for x in json.loads(p.read_text())}
w=R/'source/array-length-hot-reads-c20/array-length-hot-reads.wasm';cases['array-length-hot-reads']=dict(name='array-length-hot-reads',wasm=str(w),wasm_sha=N.sha(w))
for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases']+json.loads((R/'source/wasmtime-codegen-analysis-a1/fixtures/cases.json').read_text())['cases'][:3]:cases[c['name']]=c;jobs[c['name']]=32
vs=['typed-array-load','typed-array-access','wasmtime-drc'];elves={v:R/('build/deferred-'+v+'/uwvm') for v in vs[:-1]};elves[vs[-1]]=O/'build/wasmtime-bench';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references','-WFE-simd','-WFE-relaxed-simd'];rows=[]
with N.lease():
 for rd in range(3):
  for ci,(name,reps) in enumerate(jobs.items()):
   c=cases[name];assert N.sha(c['wasm'])==c['wasm_sha'];order=vs[(rd+ci)%3:]+vs[:(rd+ci)%3]
   if rd%2:order=order[::-1]
   for v in order:
    lab='typed-store-clock-c21-a1-r'+str(rd)+'-'+name+'-'+v;env=dict(os.environ,WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS=str(reps),LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'));env.pop('WASM_PERF_NATIVE_PROFILE',None);env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None)
    N.run(lab,[elves[v],*flags,*c.get('flags',[]),'--run',c['wasm']] if v!=vs[-1] else [elves[v],c['wasm']],cpu=2,timeout=60,env=env)
    ss=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(ss)==5 and all(x[1]==reps and x[2]>0 for x in ss);rows.append(dict(case=name,variant=v,round=rd,reps=reps,samples=ss,ELF_sha=N.sha(elves[v]),WASM_sha=c['wasm_sha'],receipt=lab));N.latest(R/'evidence/typed-store-clock-progress-c21-a1.json',dict(completed=len(rows),planned=len(jobs)*9,last=rows[-1]))
   print('C21_CLOCK',rd,name,flush=True)
 summary=[]
 for name,reps in jobs.items():
  rr=[x for x in rows if x['case']==name];times={v:statistics.median(s[2]/reps for x in rr if x['variant']==v for s in x['samples']) for v in vs}
  for rd in range(3):assert [s[3:] for x in rr if x['variant']==vs[0] and x['round']==rd for s in x['samples']]==[s[3:] for x in rr if x['variant']==vs[1] and x['round']==rd for s in x['samples']],(name,rd)
  summary.append(dict(case=name,ns=times,c21_over_c20=times[vs[1]]/times[vs[0]],c21_over_wasmtime=times[vs[1]]/times[vs[2]],six_work_metrics_equal=True))
 N.save(R/'evidence/typed-store-clock-c21-a1.json',dict(cells=len(rows),rounds=3,results=rows,summary=summary,scope='Private measured C20 and C21 differ only typed numeric stores. Same byte Wasm, same reps acrossVMs, 5 postJIT internalPOSIXsamples percell, first+2warm batches excluded. VM transitions/constructors included. 8writeaccess+3reference+11priorfast/slower retained workloads. No forcedGC/cycle/parallelGCaccess ratio claim, no CPU idle gate. Package/background contention possible.'))
 print('C21_CLOCK_DONE',json.dumps(summary),flush=True)
