from pathlib import Path
import sys,json,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/foreign-typed-store-c21/cases.json').read_text())['cases'];rows=[];f=open(N.BASE/'control/native-c21-repo-foreign-feature.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 elf=R/'build/repo-sync-c23-a1'/repo/'uwvm';flags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-WFE-gc','-WFE-function-references','-Rllvm-cache-path','disable']
 for c in cs:
  lab='foreign-typed-store-c23-a1-'+repo+'-'+c['name'];args=[elf,*flags,'-Wpre',c['provider']['path'],'provider','--run',c['importer']['path']]
  try:r=N.run(lab,args,cpu=6,timeout=45)
  except AssertionError:
   p=R/'evidence'/(lab+'.json')
   if not p.exists():raise
   r=json.loads(p.read_text());assert c['trap'] and r['guard'] is None and r['returncode']!=0,r
  if c['trap']:assert c['trap'] in (R/'evidence'/(lab+'.stderr')).read_text().lower()
  rows.append(dict(repo=repo,case=c['name'],pass_=True,trap=c['trap'],ELF_sha=N.sha(elf),provider_sha=c['provider']['sha'],WASM_sha=c['importer']['sha'],receipt=lab))
 print('FOREIGN_STORE_THREE_REPOS',repo,flush=True)
N.save(R/'evidence/foreign-typed-store-three-repos-c23-a1.json',dict(cells=len(rows),rows=rows))
