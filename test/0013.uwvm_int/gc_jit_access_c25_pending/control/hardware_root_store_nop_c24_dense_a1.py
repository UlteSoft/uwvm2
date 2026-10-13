from pathlib import Path
import sys,json,math,runpy
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_read_lock_c20_v1
cs={c['name']:c for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases']};fixed=R/'evidence/root-store-nop-fixed-c24-a1.json';summary=json.loads(fixed.read_text())['summary'];vs=['typed-array-access-inline-static32','root-store-nop-probe','wasmtime-drc'];F=R/'source/hardware-specs-root-store-nop-c24-dense-a1';F.mkdir(exist_ok=True);rows=[];worker=R/'control/profile_worker_root_nop_c24_a1.py';recipe=R/'source/root-nop-probe-c24-a1/recipes.json';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references']
with N.lease():
 for name in ['i32-write-read']:
  c=cs[name];times=next(x for x in summary if x['case']==name)['medians']
  for analysis in ['uarch-exploration']:
   for v in vs:
    elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/'build/deferred-typed-array-access-inline-static32/uwvm'
    template=json.loads((R/'source/read-lock-hardware-specs-c20'/('read-lock-hw-c20-f32-immutable-'+('wasmtime-drc' if v=='wasmtime-drc' else 'typed-array-load')+'-'+analysis+'.json')).read_text());reps=math.ceil(8_000_000_000/times[v]['ns']/16)*16;label='root-store-nop-c24-dense-a1-'+name+'-'+v+'-'+analysis;pd=R/'profiles'/label;sp=F/(label+'.json');cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,*flags,'--run',c['wasm']]
    rp=R/'evidence'/(label+'.json')
    if rp.exists() and (json.loads(rp.read_text())['returncode']!=0 or json.loads(rp.read_text())['guard'] is not None):label+='-retry-a2';pd=R/'profiles'/label;sp=F/(label+'.json')
    s=dict(template,label=label,case=name,kind='typed-array-causal-root-store',variant=v,elf=str(elf),elf_sha=N.sha(elf),dir=str(pd),cmd=list(map(str,cmd)),wasm=c['wasm'],wasm_sha=c['wasm_sha'],reps=reps,interval=1,source_clock_sha=N.sha(fixed),root_nop_recipe_sha=N.sha(recipe),scope='Store-only causal experiment;2identical Wasm cases, originalC23 own-child three3equal-length root-store NOPs orWasmtime49. Reps independentlycalibrated>=8s; no rawtotal crossVMcounter ratio claim, no compiledC24 qualification.',worker_sha_expected=N.sha(worker))
    if not sp.exists():N.save(sp,s)
    else:assert json.loads(sp.read_text())==s
    if not (pd/'capture.json').exists():N.run(label,['python3',worker,sp],cpu=20,timeout=180,pmu=True)
    else:
     r=json.loads((R/'evidence'/(label+'.json')).read_text());assert r['returncode']==0 and r['guard'] is None
    if v=='wasmtime-drc' and not (pd/'dependency-binding-c21.json').exists():
     dep=json.loads((R/'source/wasmtime-dynamic-library-binding-c20.json').read_text());lib=Path(dep['path']);assert N.sha(lib)==dep['sha256'] and lib.stat().st_ino==dep['inode']
     for moment in ['before','after']:
      ms=[l.split() for l in (pd/(moment+'-maps.txt')).read_text().splitlines() if l.split()[-1]==str(lib)];assert ms and all(int(l[4])==dep['inode'] and l[3]==dep['device'] for l in ms)
     N.save(pd/'dependency-binding-c21.json',dict(dep,maps_verified=True))
    if not (pd/'validation-a3/qualified.json').exists():sys.argv=[str(R/'control/validate_profile_a3.py'),str(pd)];runpy.run_path(sys.argv[0],run_name='__main__')
    rows.append(dict(spec=str(sp),spec_sha=N.sha(sp),qualified=str(pd/'validation-a3/qualified.json'),qualified_sha=N.sha(pd/'validation-a3/qualified.json')));print('NOP_HARDWARE',len(rows),3,label,flush=True)
 N.save(R/'evidence/root-store-nop-hardware-c24-dense-a1.json',dict(profiles=len(rows),rows=rows,controller_sha=N.sha(__file__),scope='HardwareHS+ detailedMicroarch, postJIT/warm paused READY/GO/DONE, stableactualRX andzeroPERFloss. PMUcoretype/independentcrosscheck pending; no compiledC24 code claim.'))
