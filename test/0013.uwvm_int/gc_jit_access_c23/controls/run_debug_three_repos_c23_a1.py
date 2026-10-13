from pathlib import Path
import sys,fcntl,json
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
f=open(N.BASE/'control/native-c21-repo-foreign-feature.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);rows=[]
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 D=R/'evidence'/('debug-inline-auth-three-repos-c23-a1-'+repo);lab=D.name;elf=R/'build/repo-sync-c23-a1'/repo/'uwvm';N.run(lab,['python3',R/'control/debug_typed_array_store_three_repos_c21_a1.py','--uwvm',elf,'--out',D],cpu=6,timeout=240);j=json.loads((D/'result.json').read_text());assert j['cells']==12 and all(x['pass_'] for x in j['results'])
 for p in D.rglob('*'):
  if p.is_file():N.persist(p)
 rows.append(dict(repo=repo,ELF_sha=N.sha(elf),result_sha=N.sha(D/'result.json'),cells=j['cells']));print('THREE_REPO_DEBUG_STORE_PASS',repo,flush=True)
N.save(R/'evidence/debug-inline-auth-three-repos-c23-a1.json',dict(cells=36,rows=rows))
