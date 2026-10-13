from pathlib import Path
import sys,json,math,runpy,statistics
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
import disjoint_pmu_read_lock_c20_v1
cs={c['name']:c for dataset in ['typed-array-store-bench-c21','typed-store-gc-pressure-c21'] for c in json.loads((R/'source'/dataset/'cases.json').read_text())['cases']}
fresh=R/'evidence/gc-pressure-inline-auth-fixed-c26-a3-complete.json';old=R/'evidence/inline-auth-fixed-c25-a1-final.json';freshj=json.loads(fresh.read_text());oldj=json.loads(old.read_text());times={}
for j in [oldj,freshj]:
 for x in j['rows']:times.setdefault(x['case'],{}).setdefault(x['variant'],[]).append(x['ns_per_rep'])
vs=['typed-array-access-inline-static32','typed-array-access-known-length','private-numeric-array-lock-numeric-struct','wasmtime-drc'];worker=R/'control/profile_worker_known_length_c26_a3.py';F=R/'source/hardware-specs-private-numeric-c26-a3';F.mkdir(exist_ok=False);rows=[]
flags=['-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-exceptions','-WFE-tail-call','-WFE-function-references']
with N.lease():
 for name in ['i32-write','i32-write-read','i32-write-read-gc-pressure']:
  c=cs[name]
  for analysis in ['hotspots','uarch-exploration']:
   for v in vs:
    if name.endswith('-pressure') and v=='typed-array-access-known-length':continue
    elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/('build/deferred-'+v+'/uwvm')
    tf=R/('source/hardware-specs-root-store-nop-c24-a1/root-store-nop-c24-a1-i32-write-typed-array-access-inline-static32-hotspots.json' if analysis=='hotspots' else 'source/read-lock-hardware-specs-c20/read-lock-hw-c20-f32-immutable-typed-array-load-uarch-exploration.json');template=json.loads(tf.read_text())
    target_ns=2_000_000_000 if analysis=='hotspots' else 8_000_000_000
    ns=statistics.median(times[name][v]);reps=math.ceil(target_ns/ns/16)*16;label='private-numeric-c26-a3-'+name+'-'+v+'-'+analysis;D=R/'profiles'/label;sp=F/(label+'.json')
    cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,*flags,'--run',c['wasm']]
    s=dict(template,label=label,case=name,kind='private-numeric-array-lock',analysis=analysis,variant=v,elf=str(elf),elf_sha=N.sha(elf),dir=str(D),cmd=list(map(str,cmd)),wasm=c['wasm'],wasm_sha=c['wasm_sha'],reps=reps,interval=5 if analysis=='hotspots' else 1,boot=N.BOOT,source_clock_sha=N.sha(fresh) if v!='typed-array-access-known-length' else N.sha(old),calibrated_ns_per_rep=ns,worker_sha_expected=N.sha(worker),scope='Actual compiled original C23/C25/C26a3 and Wasmtime49, sameWasm, postJIT and warmup excluded. Separately calibrated reps; no raw sample-total crossVM speed ratio. Hardware HS and detailed uarch; exact core2 target PID/TID ROI validation; no CPU-idle gate.')
    s.pop('root_nop_recipe_sha',None);s.pop('parent_spec_sha',None);N.save(sp,s)
    N.run(label,['python3',worker,sp],cpu=20,timeout=180,pmu=True)
    if v=='wasmtime-drc':
     dep=json.loads((R/'source/wasmtime-dynamic-library-binding-post-reboot-c25-a1.json').read_text());lib=Path(dep['path']);assert N.sha(lib)==dep['sha256'] and lib.stat().st_ino==dep['inode']
     for moment in ['before','after']:
      ms=[l.split() for l in (D/(moment+'-maps.txt')).read_text().splitlines() if l.split()[-1]==str(lib)];assert ms and all(int(l[4])==dep['inode'] and l[3]==dep['device'] for l in ms)
     N.save(D/'dependency-binding-c26.json',dict(dep,maps_verified=True))
    sys.argv=[str(R/'control/validate_profile_a3.py'),str(D)];runpy.run_path(sys.argv[0],run_name='__main__')
    q=D/'validation-a3/qualified.json';rows.append(dict(spec=str(sp),spec_sha=N.sha(sp),qualified=str(q),qualified_sha=N.sha(q)));N.latest(R/'evidence/hardware-private-numeric-c26-a3-progress.json',dict(completed=len(rows),planned=22,last=rows[-1]));print('PRIVATE_HW_QUALIFIED',len(rows),22,label,flush=True)
 N.save(R/'evidence/hardware-private-numeric-c26-a3.json',dict(profiles=len(rows),rows=rows,controller_sha=N.sha(__file__),scope='HardwareHS+detailedmicroarch, actual stableRX; raw zeroPERFloss and correctPMUdomain independentstats/assembly audit follow. Explicit fixed counters carry crossVM ratios; samples locate hotspots only.'))
