from pathlib import Path
import sys,json,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_c20_fixed_v2
cs={c['name']:c for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases']}
for f in [O/'source/cases-restored-a1.json',R/'source/repeated-array-types-native-a1.json',R/'source/observable-features-native-a1.json']:
 for c in json.loads(f.read_text()):cs[c['name']]=c
vs=['typed-array-access','typed-array-access-inline','wasmtime-drc'];elves={v:R/('build/deferred-'+v+'/uwvm') for v in vs[:-1]};elves[vs[-1]]=O/'build/wasmtime-bench';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references','-WFE-simd','-WFE-relaxed-simd'];rows=[];F=R/'source/fixed-inline-auth-c22-a2';F.mkdir(exist_ok=False);template=json.loads(next((R/'source/fixed-counter-specs-c20-v2').glob('*.json')).read_text())
with N.lease():
 for rd in range(2):
  for ci,(name,reps) in enumerate([('i8-write',1024),('i32-write',1024),('i32-write-read',1024),('observable-tail-call',65536),('repeat-init-func',128)]):
   order=vs if name in ('i8-write','i32-write','i32-write-read') else vs[:2]
   if rd%2:order=order[::-1]
   for v in order:
    c=cs[name];elf=elves[v];lab='fixed-inline-auth-c22-a2-r'+str(rd)+'-'+name+'-'+v;sp=F/(lab+'.json');pd=R/'profiles'/lab;cmd=[elf,c['wasm']] if v==vs[-1] else [elf,*flags,'--run',c['wasm']];s=dict(template,label=lab,case=name,variant=v,reps=reps,elf=str(elf),elf_sha=N.sha(elf),wasm=c['wasm'],wasm_sha=c['wasm_sha'],dir=str(pd),cmd=list(map(str,cmd)),worker_sha_expected=N.sha(R/'control/fixed_counter_worker_c20_v2.py'),round=rd,scope='Same repetitions percase acrosscomparedVMs; grouped unscaled core2user counters, postJIT READY/GO/DONE. Fixed counters include tiny protocol user serialization. Fastpath retention checks also capture same instructions/cycles.')
    N.save(sp,s);N.run(lab,['python3',R/'control/fixed_counter_worker_c20_v2.py',sp],cpu=20,timeout=180,pmu=True);capture=json.loads((pd/'fixed-counter-capture.json').read_text());rows.append(capture);print('C22_FIXED',rd,name,v,flush=True)
 summaries=[]
 for name,reps in [('i8-write',1024),('i32-write',1024),('i32-write-read',1024),('observable-tail-call',65536),('repeat-init-func',128)]:
  rr=[x for x in rows if x['case']==name];values={v:{k:statistics.median(x['counts'][k]['per_rep'] for x in rr if x['variant']==v) for k in ['cycles','instructions','ref_cycles']} for v in (vs if name in ('i8-write','i32-write','i32-write-read') else vs[:2])};summaries.append(dict(case=name,counts=values,c22_over_c21={k:values[vs[1]][k]/values[vs[0]][k] for k in values[vs[0]]}))
 N.save(R/'evidence/inline-auth-fixed-c22-a2.json',dict(cells=len(rows),rows=rows,summary=summaries));print('C22_FIXED_DONE',summaries,flush=True)
