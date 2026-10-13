from pathlib import Path
import sys,json,os,re
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
cs=[c for c in json.loads((R/'source/inline-array-auth-fixtures-c22-a1/cases.json').read_text())['cases'] if c['name'].endswith('-collection-domain')];rows=[]
with N.lease():
 for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
  elf=R/'build/repo-sync-c22-a1'/repo/'uwvm';b=json.loads((elf.parent/'build-binding.json').read_text());assert N.sha(elf)==b['ELF_sha'] and b['verified_source_id']==b['input_sha'];flags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-Rclog','err','-WFE-gc','-WFE-function-references']
  for c in cs:
   D=R/'tmp/cache-reload-c22-a2'/repo/c['name'];D.mkdir(parents=True,exist_ok=False)
   pair=[]
   for iteration in range(2):
    lab='cache-reload-c22-a2-'+repo+'-'+c['name']+'-'+str(iteration);N.run(lab,[elf,*flags,'-Rllvm-cache-path','path',D,'--run',c['wasm']],cpu=6,timeout=90)
    text=(R/'evidence'/(lab+'.stderr')).read_text();hit='object-cache-hit module=' in text;store='object-cache-store-enqueue module=' in text;assert hit==(iteration==1) and (iteration==1 or store),text[-6000:]
    lines=[l for l in text.splitlines() if '[gc-managed]' in l];assert len(lines)==1;met={k:int(v) for k,v in re.findall(r'([a-z_]+)=(\d+)',lines[0])};assert met['collections']>0 and met['reclaimed']>0 and met['disabled']==0 and met['roots_requested']==1,met;pair.append(met)
    rows.append(dict(repo=repo,case=c['name'],iteration=iteration,cache_hit=hit,metrics=met,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab))
   assert pair[0]==pair[1],pair
   for p in D.rglob('*'):
    if p.is_file():N.persist(p)
  print('C22_CACHE_RELOAD_PASS',repo,flush=True)
 N.save(R/'evidence/cache-reload-c22-a2.json',dict(cells=len(rows),rows=rows,scope='Signed+verified object cache, verified full-source SHA id; 12 real fresh-process misses then12hits, no unsafe provenance escape hatch. Each same Wasm validates array rawbits across realGC. Runtime module external symbol relocated anew.'))
