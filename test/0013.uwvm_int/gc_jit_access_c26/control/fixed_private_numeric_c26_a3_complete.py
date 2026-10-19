from pathlib import Path
import sys,json,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_c20_fixed_v2
cs={c['name']:c for p in ['typed-store-gc-pressure-c21','typed-array-store-bench-c21'] for c in json.loads((R/'source'/p/'cases.json').read_text())['cases']}
vs=['typed-array-access-inline-static32','private-numeric-array-lock-numeric-struct','wasmtime-drc'];elves={v:R/('build/deferred-'+v+'/uwvm') for v in vs[:-1]};elves[vs[-1]]=O/'build/wasmtime-bench';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references','-WFE-simd','-WFE-relaxed-simd'];rows=[];F=R/'source/fixed-gc-pressure-inline-auth-c26-a3-complete';F.mkdir(exist_ok=True);template=json.loads(next((R/'source/fixed-counter-specs-c20-v2').glob('*.json')).read_text())
with N.lease():
 for rd in range(3):
  for ci,(name,reps) in enumerate([('i32-write',6144),('i32-write-read',6144)]+[(ty+'-write-read-gc-pressure',256) for ty in ['i8','i16','i32','f32']]):
   order=vs
   if rd%2:order=order[::-1]
   for v in order:
    c=cs[name];elf=elves[v];lab='fixed-gc-pressure-inline-auth-c26-a3-complete-r'+str(rd)+'-'+name+'-'+v;sp=F/(lab+'.json');pd=R/'profiles'/lab;cmd=[elf,c['wasm']] if v==vs[-1] else [elf,*flags,'--run',c['wasm']];s=dict(template,label=lab,case=name,variant=v,reps=reps,elf=str(elf),elf_sha=N.sha(elf),wasm=c['wasm'],wasm_sha=c['wasm_sha'],dir=str(pd),cmd=list(map(str,cmd)),worker_sha_expected=N.sha(R/'control/fixed_counter_worker_c25_a2.py'),round=rd,scope='Same repetitions percase acrosscomparedVMs; grouped unscaled core2user counters, postJIT READY/GO/DONE. Fixed counters include tiny protocol user serialization. Fastpath retention checks also capture same instructions/cycles.')
    if not sp.exists():N.save(sp,s)
    else:assert json.loads(sp.read_text())==s
    receipt=R/'evidence'/(lab+'.json')
    if receipt.exists():
     j=json.loads(receipt.read_text());assert j['returncode']==0 and j['guard'] is None and (pd/'fixed-counter-capture.json').exists()
    else:N.run(lab,['python3',R/'control/fixed_counter_worker_c25_a2.py',sp],cpu=20,timeout=180,pmu=True)
    capture=json.loads((pd/'fixed-counter-capture.json').read_text());assert capture['elf_sha']==s['elf_sha'] and capture['wasm_sha']==s['wasm_sha'];rows.append(capture);print('C26_PRESSURE_FIXED_RETAINED_OR_COMPLETED',rd,name,v,flush=True)
 summaries=[]
 for name,reps in [('i32-write',6144),('i32-write-read',6144)]+[(ty+'-write-read-gc-pressure',256) for ty in ['i8','i16','i32','f32']]:
  rr=[x for x in rows if x['case']==name];values={v:{k:statistics.median(x['counts'][k]['per_rep'] for x in rr if x['variant']==v) for k in ['cycles','instructions','ref_cycles']} for v in vs};summaries.append(dict(case=name,counts=values,c26_over_c23={k:values[vs[1]][k]/values[vs[0]][k] for k in values[vs[0]]}))
 N.save(R/'evidence/gc-pressure-inline-auth-fixed-c26-a3-complete.json',dict(cells=len(rows),rows=rows,summary=summaries));print('C26_FIXED_DONE',summaries,flush=True)
