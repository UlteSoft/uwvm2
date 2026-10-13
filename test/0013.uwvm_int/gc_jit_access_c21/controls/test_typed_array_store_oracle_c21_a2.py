from pathlib import Path
import sys,json,fcntl,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/typed-array-store-c21-a2/cases.json').read_text())['cases'];rows=[]
f=open(N.BASE/'control/native-c21-oracle-correctness.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
N.check()
for i,c in enumerate(cs):
 lab='typed-store-oracle-c21-a2-'+str(i);env=dict(os.environ,LD_LIBRARY_PATH='/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib',WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS='1');env.pop('WASM_PERF_NATIVE_PROFILE',None)
 try:r=N.run(lab,['/tmp/uwvm-gc-perf-20261011-r73/build/wasmtime-bench',c['wasm']],cpu=6,timeout=45,env=env)
 except AssertionError:
  q=R/'evidence'/(lab+'.json')
  if not q.exists():raise
  r=json.loads(q.read_text());assert c['trap'] and r['returncode']!=0 and r['guard'] is None,r
 if c['trap']:
  err=(R/'evidence'/(lab+'.stderr')).read_text().lower();assert ('bounds' if c['trap']=='bounds' else 'null') in err,(c,err)
 else:
  assert r['returncode']==0
 rows.append(dict(name=c['name'],pass_=True,trap=c['trap'],wasm_sha=c['wasm_sha'],receipt=lab))
 if i%20==0:print('C21_ORACLE',i+1,len(cs),flush=True)
N.save(R/'evidence/typed-store-oracle-c21-a2.json',{'cases':len(rows),'rows':rows,'ELF_sha':N.sha('/tmp/uwvm-gc-perf-20261011-r73/build/wasmtime-bench'),'DSO_sha':N.sha('/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib/libwasmtime.so')})
