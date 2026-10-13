from pathlib import Path
import sys,json,os,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
Q=R/'source/repo-sync-c20-fixtures-export-v2';m=json.loads((Q/'cases.json').read_text());rows=[];elf=R/'build/deferred-typed-array-access/uwvm';flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd'];f=open(N.BASE/'control/native-c21-private-extras.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);N.check()
for kind in ['foreign','features']:
 for i,c in enumerate(m[kind]):
  w=Q/(c['importer'] if kind=='foreign' else c['wasm']);lab='typed-store-private-extra-c21-a1-'+kind+'-'+str(i);args=flags+c.get('flags',[])
  if kind=='foreign':args+=['-Wpre',Q/c['provider'],c['provider_module']]
  try:rec=N.run(lab,[elf,*args,'--run',w],cpu=6,timeout=45)
  except AssertionError:
   p=R/'evidence'/(lab+'.json')
   if not p.exists():raise
   rec=json.loads(p.read_text());assert c['trap'] and rec['guard'] is None and rec['returncode']!=0
  if c['trap']:assert c['trap'] in (R/'evidence'/(lab+'.stderr')).read_text().lower()
  rows.append(dict(kind=kind,name=c['name'],wasm_sha=N.sha(w),receipt=lab,pass_=True))
 print('C21_EXTRA_PASS',kind,len(m[kind]),flush=True)
N.save(R/'evidence/typed-store-private-extra-c21-a1.json',dict(cells=len(rows),rows=rows,ELF_sha=N.sha(elf)))
