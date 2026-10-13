from pathlib import Path
import sys,json,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_c20_fixed_v2
vs=['typed-array-access-inline-static32','root-store-nop-probe','wasmtime-drc']
cs={c['name']:c for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases']}
UW=R/'build/deferred-typed-array-access-inline-static32/uwvm';WT=O/'build/wasmtime-bench';worker=R/'control/fixed_counter_root_nop_c24_a1.py';recipe=R/'source/root-nop-probe-c24-a1/recipes.json'
F=R/'source/fixed-root-store-nop-c24-a1';F.mkdir(exist_ok=True);rows=[]
flags=['-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references']
with N.lease():
 for rd in range(3):
  for name,reps in [('i32-write',6144),('i32-write-read',4096)]:
   for v in vs[rd:]+vs[:rd]:
    c=cs[name];elf=WT if v=='wasmtime-drc' else UW;label='fixed-root-store-nop-c24-a1-r'+str(rd)+'-'+name+'-'+v
    base_label=label
    for attempt in range(8):
     prior=R/'evidence'/(label+'.json')
     if not prior.exists() or (json.loads(prior.read_text())['returncode']==0 and json.loads(prior.read_text())['guard'] is None):break
     label=base_label+'-retry-a'+str(attempt+2)
    else:raise RuntimeError('retry budget exhausted')
    sp=F/(label+'.json');pd=R/'profiles'/label
    s=dict(label=label,case=name,variant=v,reps=reps,elf=str(elf),elf_sha=N.sha(elf),wasm=c['wasm'],wasm_sha=c['wasm_sha'],dir=str(pd),cmd=list(map(str,[elf,c['wasm']] if v=='wasmtime-drc' else [elf,*flags,'--run',c['wasm']])),worker_sha_expected=N.sha(worker),root_nop_recipe_sha=N.sha(recipe),round=rd,scope='Causal memory-store-only experiment: original C23 vs own-child three exact equal-length root-store NOPs vs Wasmtime49. Same case and reps, rotated order3rounds, no JIT time. NOP is not compiled C24 and no GC algorithm qualification.')
    if not sp.exists():N.save(sp,s)
    else:assert json.loads(sp.read_text())==s
    cap=pd/'fixed-counter-capture.json';rp=R/'evidence'/(label+'.json')
    if cap.exists():
     r=json.loads(rp.read_text());assert r['returncode']==0 and r['guard'] is None
    else:N.run(label,['python3',worker,sp],cpu=20,timeout=180,pmu=True)
    x=json.loads(cap.read_text());assert x['worker_sha_expected']==N.sha(worker);rows.append(x);print('NOP_FIXED',len(rows),18,name,v,x['ns_per_rep'],{k:x['counts'][k]['per_rep'] for k in ['cycles','instructions','ref_cycles']},flush=True)
 summaries=[]
 for name in ['i32-write','i32-write-read']:
  rr=[x for x in rows if x['case']==name];val={v:{**{k:statistics.median(x['counts'][k]['per_rep'] for x in rr if x['variant']==v) for k in ['cycles','instructions','ref_cycles']},'ns':statistics.median(x['ns_per_rep'] for x in rr if x['variant']==v)} for v in vs};summaries.append(dict(case=name,medians=val,nop_over_original={k:val[vs[1]][k]/val[vs[0]][k] for k in val[vs[0]]},original_over_wasmtime={k:val[vs[0]][k]/val[vs[2]][k] for k in val[vs[0]]},nop_over_wasmtime={k:val[vs[1]][k]/val[vs[2]][k] for k in val[vs[0]]}))
 N.save(R/'evidence/root-store-nop-fixed-c24-a1.json',dict(cells=len(rows),rows=rows,summary=summaries,controller_sha=N.sha(__file__),recipe_sha=N.sha(recipe),scope='Store-only causal experiment;3NOPs preserve instruction count but remove3root stores perhotloop iteration. Not compiledC24, not GC pressure/collector correctness, no numerical projection to C24. All Wasm result assertions preserved every invocation.'))
 print('NOP_FIXED_DONE',json.dumps(summaries),flush=True)
