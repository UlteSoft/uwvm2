from pathlib import Path
import sys,json,os,fcntl
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
flags=['-Rcc','jit','-Rcm','full','-Rct','0','-WFE-gc','-WFE-function-references','-WFE-exceptions','-WFE-tail-call'];rows=[];cs=json.loads((R/'source/typed-store-live-collection-c21-a2/cases.json').read_text())['cases'];f=open(N.BASE/'control/native-c21-repo-foreign-feature.lock','a+');fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
for c in cs:
 pair=[]
 for v in ['typed-array-access','typed-array-access-inline']:
  elf=R/('build/deferred-'+v+'/uwvm');lab='inline-auth-live-collection-c23-a1-'+c['name']+'-'+v;env=dict(os.environ,WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS='2');env.pop('WASM_PERF_NATIVE_PROFILE',None)
  N.run(lab,[elf,*flags,'--run',c['wasm']],cpu=6,timeout=45,env=env);ss=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(ss)==5 and all(x[1]==2 and x[6]==0 and x[8]==0 for x in ss) and sum(x[4] for x in ss)>0 and sum(x[5] for x in ss)>0,(c,v,ss);pair.append(ss);rows.append(dict(case=c['name'],variant=v,samples=ss,collections=sum(x[4] for x in ss),reclaimed=sum(x[5] for x in ss),wasm_sha=c['wasm_sha'],ELF_sha=N.sha(elf),receipt=lab))
 assert [s[3:] for s in pair[0]]==[s[3:] for s in pair[1]]
 print('LIVE_GC_STORE_ROOT_PASS',c['name'],rows[-1]['collections'],rows[-1]['reclaimed'],flush=True)
N.save(R/'evidence/inline-auth-live-collection-c23-a1.json',dict(cells=len(rows),rows=rows,scope='Old/new private paired WASM correctness, actual automaticcollection and reclamation counts>0, rejection0 anddisabled0, same6workmetrics. Four original churn fixtures stripped only unused exception tag.'))
