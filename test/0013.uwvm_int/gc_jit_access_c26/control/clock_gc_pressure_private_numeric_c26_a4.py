from pathlib import Path
import sys,json,os,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/typed-store-gc-pressure-c21/cases.json').read_text())['cases'];vs=['typed-array-access-inline-static32','private-numeric-array-lock-numeric-struct','wasmtime-drc'];elves={v:R/('build/deferred-'+v+'/uwvm') for v in vs[:-1]};elves[vs[-1]]=O/'build/wasmtime-bench';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-function-references','-WFE-exceptions','-WFE-tail-call'];rows=[];reps=4
with N.lease():
 for rd in range(3):
  for ci,c in enumerate(cs):
   order=vs[(rd+ci)%3:]+vs[:(rd+ci)%3]
   if rd%2:order=order[::-1]
   for v in order:
    lab='gc-pressure-inline-auth-clock-c26-a3-a1-r'+str(rd)+'-'+c['name']+'-'+v;env=dict(os.environ,WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS=str(reps),LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'));env.pop('WASM_PERF_NATIVE_PROFILE',None);env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None);N.run(lab,[elves[v],*flags,'--run',c['wasm']] if v!=vs[-1] else [elves[v],c['wasm']],cpu=2,timeout=60,env=env);ss=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(ss)==5 and all(x[1]==reps and x[2]>0 for x in ss)
    if v!=vs[-1]:assert all(x[3]==reps*c['expected_allocations'] and x[4]>0 and x[5]>0 and x[6]==0 and x[8]==0 for x in ss),(v,c,ss)
    rows.append(dict(case=c['name'],variant=v,round=rd,reps=reps,samples=ss,ELF_sha=N.sha(elves[v]),WASM_sha=c['wasm_sha'],receipt=lab))
   print('C26a3_GC_PRESSURE_CLOCK',rd,c['name'],flush=True)
 summary=[]
 for c in cs:
  rr=[x for x in rows if x['case']==c['name']];times={v:statistics.median(s[2]/reps for x in rr if x['variant']==v for s in x['samples']) for v in vs}
  for rd in range(3):assert [s[3:] for x in rr if x['variant']==vs[0] and x['round']==rd for s in x['samples']]==[s[3:] for x in rr if x['variant']==vs[1] and x['round']==rd for s in x['samples']]
  summary.append(dict(case=c['name'],ns=times,c26_over_c23=times[vs[1]]/times[vs[0]],c26_over_wasmtime=times[vs[1]]/times[vs[2]],six_work_metrics_equal=True))
 N.save(R/'evidence/gc-pressure-inline-auth-clock-c26-a3-a1.json',dict(cells=len(rows),results=rows,summary=summary,scope='Matched C23/C26a3 identical33flags, sameWasm/reps4,5postJIT POSIXsamples3balanced rounds, allocations+collections+reclaimed+policy metrics verified UWboth. Actual GCpressure+C numericarrayaccess; not wholeGCcycle/parallelcollectorparity. Wasmtime assertoracles; nativecollector counters unavailable forWT.'))
 print('GC_PRESSURE_CLOCK_DONE',summary,flush=True)
