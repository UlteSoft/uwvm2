from pathlib import Path
import sys,json,re,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
f=open(N.BASE/'control/native-c21-repo-foreign-feature.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);cs=json.loads((R/'source/typed-store-live-collection-c21-a2/cases.json').read_text())['cases'];rows=[]
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 elf=R/'build/repo-sync-c22-a1'/repo/'uwvm';flags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-WFE-gc','-WFE-function-references','-Rllvm-cache-path','disable','-Rclog','err']
 for c in cs:
  lab='production-live-gc-c22-a1-'+repo+'-'+c['name'];N.run(lab,[elf,*flags,'--run',c['wasm']],cpu=6,timeout=45);text=(R/'evidence'/(lab+'.stderr')).read_text();lines=[l for l in text.splitlines() if '[gc-managed]' in l];assert len(lines)==1,lines;met={k:int(v) for k,v in re.findall(r'([a-z_]+)=(\d+)',lines[0])};assert met['collections']>0 and met['reclaimed']>0 and met['disabled']==0 and met['reason']==0 and met['roots_requested']==1,met;rows.append(dict(repo=repo,case=c['name'],metrics=met,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab))
 print('PRODUCTION_LIVE_GC',repo,[x['metrics']['collections'] for x in rows if x['repo']==repo],flush=True)
N.save(R/'evidence/production-live-gc-c22-a1.json',dict(cells=len(rows),rows=rows,scope='Actual3 current production6macroELFs, no benchmark hooks. Guest verifies live numeric array rawbits before/after50000allocationchurn. Native built-in runtime-compiler log asserts actualcollections/reclaimed>0 andprecise roots1 disabled0 reason0. LLVM cache disabled.'))
