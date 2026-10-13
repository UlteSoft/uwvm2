from pathlib import Path
import sys,json,os,statistics
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
R=N.ROOT;O=Path('/tmp/uwvm-gc-perf-20261011-r73');D=R/'source/wasmtime-codegen-analysis-a1';cases=json.loads((D/'fixtures/cases.json').read_text())['cases'];variants=['immutable-array-length-values','typed-array-load','wasmtime-drc'];elves={v:R/('build/deferred-'+v+'/uwvm') for v in variants[:-1]};elves[variants[-1]]=O/'build/wasmtime-bench';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references'];rows=[];reps=32
with N.lease():
 for rd in range(3):
  for ci,c in enumerate(cases[rd:]+cases[:rd]):
   order=variants[(rd+ci)%3:]+variants[:(rd+ci)%3]
   if rd%2:order=order[::-1]
   for v in order:
    lab='codegen-reference-clock-c20-r'+str(rd)+'-'+c['name']+'-'+v;env=dict(os.environ,WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS=str(reps),LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'));env.pop('WASM_PERF_NATIVE_PROFILE',None);env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None);argv=[elves[v],*flags,'--run',c['wasm']] if v!=variants[-1] else [elves[v],c['wasm']]
    N.run(lab,argv,cpu=2,timeout=45,env=env)
    samples=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(samples)==5 and all(x[1]==reps and x[2]>0 for x in samples);rows.append({'case':c['name'],'variant':v,'round':rd,'samples':samples,'ELF_sha':N.sha(elves[v]),'WASM_sha':c['wasm_sha'],'receipt':lab})
   print('REFERENCE_CLOCK',rd,c['name'],flush=True)
 summary=[]
 for c in cases:
  rr=[x for x in rows if x['case']==c['name']];times={v:statistics.median(s[2]/reps for x in rr if x['variant']==v for s in x['samples']) for v in variants}
  for rd in range(3):assert [s[3:] for x in rr if x['round']==rd and x['variant']==variants[0] for s in x['samples']]==[s[3:] for x in rr if x['round']==rd and x['variant']==variants[1] for s in x['samples']]
  summary.append({'case':c['name'],'ns_per_start':times,'c20_over_c19':times[variants[1]]/times[variants[0]],'c20_over_wasmtime':times[variants[1]]/times[variants[2]],'all_six_UW_work_metrics_equal':True})
 N.save(D/'reference-clock-c20-a1.json',{'cells':len(rows),'rounds':3,'rows':rows,'summary':summary,'scope':'Private measured C19/C20 experiment ELFs, not final3 integrated4macroELFs. Post-JIT internal POSIX timing,first+2warm batches excluded,5samples each32starts,CPU2. Same exact Wasm and checksum/finalslot oracle. VMtransitions/constructors included. Accesses, not forced-GC/cycle/parallelGC benchmark. No CPU utilization gate; package/background contention possible.'});print('REFERENCE_CLOCK_DONE',json.dumps(summary),flush=True)
