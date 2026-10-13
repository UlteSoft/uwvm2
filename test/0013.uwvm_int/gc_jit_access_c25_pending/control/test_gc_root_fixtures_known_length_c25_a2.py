from pathlib import Path
import sys,json,os,argparse
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
p=argparse.ArgumentParser();p.add_argument('--candidate',action='store_true');a=p.parse_args();cs=json.loads((R/'source/loop-root-fixtures-c24-a2/cases.json').read_text())['cases'];vs=['typed-array-access-inline-static32','wasmtime-drc'] if not a.candidate else ['typed-array-access-known-length'];rows=[]
flags=['-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd']
with N.lease():
 for v in vs:
  elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/('build/deferred-'+v+'/uwvm')
  for c in cs:
   assert N.sha(c['wasm'])==c['wasm_sha'];lab='loop-roots-c25-a2-'+v+'-'+c['name'];out=R/'evidence'/(lab+'.qualified.json');cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,*flags,'--run',c['wasm']]
   if out.exists():
    row=json.loads(out.read_text());assert row['ELF_sha']==N.sha(elf) and row['WASM_sha']==c['wasm_sha'];rows.append(row);continue
   env=dict(os.environ,LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'),WASM_PERF_NATIVE_CLI='1',WASM_PERF_NATIVE_REPS='1');env.pop('WASM_PERF_NATIVE_PROFILE',None);env.pop('UWVM_GC_ARRAY_BUDGET_MIB',None)
   N.run(lab,cmd,cpu=6,timeout=60,env=env);ss=[list(map(int,l.split())) for l in (R/'evidence'/(lab+'.stdout')).read_text().splitlines() if len(l.split())==9 and all(x.lstrip('-').isdigit() for x in l.split())];assert len(ss)==5 and all(x[1]==1 and x[2]>0 for x in ss),(lab,ss)
   if v!='wasmtime-drc' and c['actual_gc']:assert all(x[3]>=32768 and x[4]>0 and x[5]>0 and x[6]==0 and x[8]==0 for x in ss),(lab,ss)
   if v!='wasmtime-drc' and not c['actual_gc']:assert all(x[4]==0 and x[8]!=0 for x in ss),(lab,ss)
   row=dict(case=c['name'],variant=v,actual_gc=c['actual_gc'],samples=ss,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab,pass_=True);N.save(out,row);rows.append(row);print('C25_ROOT_PASS',v,c['name'],ss[0][3:],flush=True)
 N.save(R/'evidence'/('loop-roots-c25-candidate-a2.json' if a.candidate else 'loop-roots-c25-baseline-a2.json'),dict(cells=len(rows),rows=rows,scope='Self-checking loop/local/operand-prefix/call/tail-call/graph/ref-array/EH modules. No-tag UWmodules must actually collect+reclaim with policy rejection0 anddisabled0 in every sample; EH gate disablement explicit, WT counters unsupported.'))
