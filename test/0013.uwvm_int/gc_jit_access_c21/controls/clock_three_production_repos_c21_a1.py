from pathlib import Path
import sys,json,os,re,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/typed-store-integrated-clock-c21/cases.json').read_text())['cases'];vs=['uwvm2','uwvm2-stable','uwvm2-ros','wasmtime-cli49-drc'];elves={r:R/'build/repo-sync-c21-a1'/r/'uwvm' for r in vs[:3]};elves[vs[-1]]=R/'source/wasmtime-codegen-analysis-a1/wasmtime-v49.0.2-x86_64-linux/wasmtime';rows=[]
with N.lease():
 for rd in range(3):
  for ci,c in enumerate(cs):
   order=vs[(rd+ci)%4:]+vs[:(rd+ci)%4]
   if rd%2:order=order[::-1]
   for v in order:
    elf=elves[v];assert elf.exists();lab='three-production-store-clock-c21-a1-r'+str(rd)+'-'+c['name']+'-'+v;env=dict(os.environ)
    for k in list(env):
     if k.startswith('WASM_PERF_NATIVE') or k=='UWVM_GC_ARRAY_BUDGET_MIB':env.pop(k,None)
    if v==vs[-1]:args=[elf,'run','-C','collector=drc,cache=n,parallel-compilation=n,cranelift-debug-verifier=y','-O','opt-level=2',c['wasm']]
    else:args=[elf,*(['-Rcc','jit','-Rcm','full'] if v==vs[0] else ['-Raot','-Rllvm-full-policy','pb-o3']),'-Rct','0','-WFE-gc','-WFE-function-references','-Rllvm-cache-path','disable','--run',c['wasm']]
    N.run(lab,args,cpu=2,timeout=45,env=env);lines=(R/'evidence'/(lab+'.stdout')).read_text().splitlines();assert len(lines)==5 and all(re.fullmatch('[0-9a-f]{16}',l) for l in lines),lines;samples=[int(l,16) for l in lines];assert all(x>0 for x in samples);rows.append(dict(variant=v,case=c['name'],round=rd,reps=32,samples_ns=samples,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab))
   print('C21_PRODUCTION_CLOCK',rd,c['name'],flush=True)
 summary=[]
 for c in cs:
  rr=[x for x in rows if x['case']==c['name']];times={v:statistics.median(s/32 for x in rr if x['variant']==v for s in x['samples_ns']) for v in vs};summary.append(dict(case=c['name'],ns=times,over_wasmtime={v:times[v]/times[vs[-1]] for v in vs[:3]}))
 N.save(R/'evidence/three-production-store-clock-c21-a1.json',dict(cells=len(rows),results=rows,summary=summary,scope='Actual3 current production snapshots,5macro typed-array-access profile. Guest WASI internal MONOTONIC32calls,2warm batches excluded,5samples,3balanced rounds; LLVM caches disabled. Wasmtime official49.0.2 CLI O2DRC. No native benchmark hooks. Different protocol from private same-flagC20/C21matrix; do not use these times as direct before/after optimization ratios. Both include clock/import transitions, exclude JIT and printing.'))
 print('C21_PRODUCTION_CLOCK_DONE',summary,flush=True)
