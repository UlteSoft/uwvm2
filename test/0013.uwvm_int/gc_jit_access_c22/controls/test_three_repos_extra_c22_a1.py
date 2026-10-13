from pathlib import Path
import sys,json,os,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
Q=R/'source/repo-sync-c20-fixtures-export-v2';manifest=json.loads((Q/'cases.json').read_text());lock=open(N.BASE/'control/native-c21-repo-foreign-feature.lock','a+');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);N.check();assert (R/'evidence/repo-sync-c22-a1-all-three-qualified.json').exists();rows=[]
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 elf=R/'build/repo-sync-c22-a1'/repo/'uwvm';b=json.loads((elf.parent/'build-binding.json').read_text());assert N.sha(elf)==b['ELF_sha'];flags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd','-Rllvm-cache-path','disable']
 for kind in ['foreign','features']:
  for c in manifest[kind]:
   label='repo-sync-c22-a1-'+repo+'-'+kind+'-'+c['name'];w=Q/(c['importer'] if kind=='foreign' else c['wasm']);assert N.sha(w)==c[('importer_sha' if kind=='foreign' else 'wasm_sha')];args=flags+c.get('flags',[])
   if kind=='foreign':p=Q/c['provider'];assert N.sha(p)==c['provider_sha'];args+=['-Wpre',p,c['provider_module']]
   try:rec=N.run(label,[elf,*args,'--run',w],cpu=6,timeout=45)
   except AssertionError:
    f=R/'evidence'/(label+'.json')
    if not f.exists():raise
    rec=json.loads(f.read_text());assert c['trap'] and rec['guard'] is None and rec['returncode']!=0,rec
   if c['trap']:assert c['trap'] in (R/'evidence'/(label+'.stderr')).read_text().lower()
   else:assert rec['returncode']==0
   rows.append({'repo':repo,'kind':kind,'name':c['name'],'ELF_sha':b['ELF_sha'],'WASM_sha':N.sha(w),'receipt_sha':N.sha(R/'evidence'/(label+'.json')),'pass':True})
  print('REPO_SYNC_EXTRA_PASS',repo,kind,len(manifest[kind]),flush=True)
N.save(R/'evidence/repo-sync-c22-a1-foreign-isolated-features-qualification.json',{'cells':len(rows),'pass':True,'rows':rows,'manifest_sha':N.sha(Q/'cases.json'),'scope':'All3 current merged sources:16 foreign-provider/consumer numeric array imports,30k allocation churn and alias mutation/null/bounds;20 separate primary-feature assertion modules. Normal CLI, nativefull backend, no benchmarkhooks and no assertion that churn triggered collection. Threads module checks atomics semantics, not parallel GC. Custom annotation/extendedconst are conformance-only.'})
