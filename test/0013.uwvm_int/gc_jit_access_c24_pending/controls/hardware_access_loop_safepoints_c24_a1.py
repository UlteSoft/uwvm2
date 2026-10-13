from pathlib import Path
import sys,json,math,runpy
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_read_lock_c20_v1
c=next(c for c in json.loads((R/'source/typed-array-store-bench-c21/cases.json').read_text())['cases'] if c['name']=='i32-write-read');clock=json.loads((R/'evidence/inline-auth-clock-c24-a1.json').read_text());times=next(x for x in clock['summary'] if x['case']==c['name'])['ns'];vs=['typed-array-access-inline-static32','typed-array-access-inline-safepoint','wasmtime-drc'];F=R/'source/hardware-specs-inline-auth-c24-a1';F.mkdir(exist_ok=False);rows=[];flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references']
with N.lease():
 for v in vs:
  elf=O/'build/wasmtime-bench' if v==vs[-1] else R/('build/deferred-'+v+'/uwvm')
  for analysis in ['hotspots','uarch-exploration']:
   template=json.loads((R/'source/read-lock-hardware-specs-c20'/('read-lock-hw-c20-f32-immutable-'+('wasmtime-drc' if v==vs[-1] else 'typed-array-load')+'-'+analysis+'.json')).read_text());reps=math.ceil(1_200_000_000/times[v]/16)*16;label='loop-safepoints-c24-i32-write-read-'+v+'-'+analysis+'-a2';pd=R/'profiles'/label;sp=F/(label+'.json');cmd=[elf,c['wasm']] if v==vs[-1] else [elf,*flags,'--run',c['wasm']]
   s=dict(template,label=label,case=c['name'],kind='typed-numeric-store',variant=v,elf=str(elf),elf_sha=N.sha(elf),dir=str(pd),cmd=list(map(str,cmd)),wasm=c['wasm'],wasm_sha=c['wasm_sha'],reps=reps,interval=25,source_clock_sha=N.sha(R/'evidence/inline-auth-clock-c24-a1.json'),scope='Same Wasm numeric store; reps separately calibrated>=1.2s. No raw total crossVMcounter ratio claim.')
   s['worker_sha_expected']=N.sha(R/'control/profile_worker_c23_a3.py');N.save(sp,s);N.run(label,['python3',R/'control/profile_worker_c23_a3.py',sp],cpu=20,timeout=180,pmu=True)
   if v==vs[-1]:
    dep=json.loads((R/'source/wasmtime-dynamic-library-binding-c20.json').read_text());lib=Path(dep['path']);assert N.sha(lib)==dep['sha256'] and lib.stat().st_ino==dep['inode']
    for moment in ['before','after']:
     ms=[l.split() for l in (pd/(moment+'-maps.txt')).read_text().splitlines() if l.split()[-1]==str(lib)];assert ms and all(int(l[4])==dep['inode'] and l[3]==dep['device'] for l in ms)
    N.save(pd/'dependency-binding-c21.json',dict(dep,maps_verified=True))
   sys.argv=[str(R/'control/validate_profile_a3.py'),str(pd)];runpy.run_path(sys.argv[0],run_name='__main__');rows.append(dict(spec=str(sp),spec_sha=N.sha(sp),qualified=str(pd/'validation-a3/qualified.json'),qualified_sha=N.sha(pd/'validation-a3/qualified.json')));print('C21_HARDWARE',len(rows),6,label,flush=True)
 N.save(R/'evidence/inline-auth-hardware-c24-a1.json',dict(profiles=len(rows),rows=rows,scope='HardwareHS+ detailedMicroarch, postJIT/warm paused READY/GO/DONE protocol, stable actual RX capture and zeroPERFloss validator. PMUtype/core/independentcrosscheck pending.'))
