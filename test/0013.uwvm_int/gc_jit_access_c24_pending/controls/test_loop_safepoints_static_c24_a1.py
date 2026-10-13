from pathlib import Path
import sys,json,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/static-layout-fixtures-c23-a2/cases.json').read_text())['cases'];rows=[]
with N.lease():
 for v in ['typed-array-access-inline-static32','typed-array-access-inline-safepoint','wasmtime-drc']:
  elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/('build/deferred-'+v+'/uwvm')
  for c in cs:
   env=dict(os.environ,LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'))
   for k in list(env):
    if k.startswith('WASM_PERF_NATIVE'):env.pop(k,None)
   lab='static-layout-new-c24-a1-'+v+'-'+c['name'];cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,'-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','--run',c['wasm']]
   try:r=N.run(lab,cmd,cpu=6,timeout=30,env=env)
   except AssertionError:
    p=R/'evidence'/(lab+'.json');assert p.exists();r=json.loads(p.read_text());assert c['trap'] and r['guard'] is None and r['returncode']!=0,r
   if c['trap']:assert c['trap'] in (R/'evidence'/(lab+'.stderr')).read_text().lower(),(lab,c['trap'])
   rows.append(dict(variant=v,case=c['name'],pass_=True,trap=c['trap'],ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab))
  print('C24_NEW_PASS',v,len(cs),flush=True)
 N.save(R/'evidence/static-layout-new-fixtures-c24-a1.json',dict(cells=len(rows),rows=rows))
