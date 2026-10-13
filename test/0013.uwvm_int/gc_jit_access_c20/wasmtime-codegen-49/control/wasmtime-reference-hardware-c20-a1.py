from pathlib import Path
import sys,json,os,math,runpy
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
import disjoint_pmu_read_lock_c20_v1
R=N.ROOT;O=Path('/tmp/uwvm-gc-perf-20261011-r73');D=R/'source/wasmtime-codegen-analysis-a1';c=json.loads((D/'fixtures/cases.json').read_text())['cases'][0];clock=json.loads((D/'reference-clock-c20-a1.json').read_text());times=next(x for x in clock['summary'] if x['case']==c['name'])['ns_per_start'];F=D/'hardware-specs-a1';F.mkdir(exist_ok=False);probes=[]
elves={'typed-array-load':R/'build/deferred-typed-array-load/uwvm','wasmtime-drc':O/'build/wasmtime-bench'};flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references']
with N.lease():
 for v in ['typed-array-load','wasmtime-drc']:
  for analysis in ['hotspots','uarch-exploration']:
   template=json.loads((R/'source/read-lock-hardware-specs-c20'/('read-lock-hw-c20-f32-immutable-'+v+'-'+analysis+'.json')).read_text());reps=math.ceil(1_200_000_000/times[v]/16)*16
   label='codegen-reference-read-c20-'+v+'-'+analysis+'-a1';pd=R/'profiles'/label;sp=F/(label+'.json');cmd=[elves[v],*flags,'--run',c['wasm']] if v!='wasmtime-drc' else [elves[v],c['wasm']]
   spec=dict(template,label=label,case=c['name'],kind='reference-access-codegen',dir=str(pd),cmd=list(map(str,cmd)),wasm=c['wasm'],wasm_sha=c['wasm_sha'],reps=reps,interval=25,source_clock_sha=N.sha(D/'reference-clock-c20-a1.json'),scope='Same-byteWasmreference-read. Reps calibrated for>500ms ROI separately perVM; not equal total work acrossVM captures. Counters/samples not a matched-work speed ratio; timing uses independent same32reps matrix.')
   N.save(sp,spec)
   lib=O/'source/wasmtime-capi/lib/libwasmtime.so';dep=json.loads((R/'source/wasmtime-dynamic-library-binding-c20.json').read_text());st=lib.stat();assert N.sha(lib)==dep['sha256'] and st.st_ino==dep['inode']
   N.run(label,['python3',R/'control/profile_worker_c20.py',sp],cpu=20,timeout=180,pmu=True)
   if v=='wasmtime-drc':
    assert N.sha(lib)==dep['sha256'] and lib.stat().st_ino==st.st_ino
    for moment in ['before','after']:
     maps=[l.split() for l in (pd/(moment+'-maps.txt')).read_text().splitlines() if l.split()[-1]==str(lib)];assert maps and all(int(l[4])==st.st_ino and l[3]==dep['device'] for l in maps)
    N.save(pd/'dependency-binding-codegen.json',{'dso_sha':N.sha(lib),'inode':st.st_ino,'device':dep['device'],'maps_verified':True})
   sys.argv=[str(R/'control/validate_profile_a3.py'),str(pd)];runpy.run_path(sys.argv[0],run_name='__main__')
   probes.append({'spec':str(sp),'spec_sha':N.sha(sp),'qualified':str(pd/'validation-a3/qualified.json'),'qualified_sha':N.sha(pd/'validation-a3/qualified.json')});print('REFERENCE_HARDWARE',len(probes),4,label,flush=True)
 N.save(D/'reference-hardware-c20-a1.json',{'profiles':len(probes),'probes':probes,'scope':'4hardwareHS/detaileduarch captures with zeroPERFloss,targetPID/TIDCPU2userRAWclockROI>=500ms, RXbeforeafterstable; independentperf eventtype/domain join pending; numeric crossVM ratios only from sameworkclock matrix.'})
