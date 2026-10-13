from pathlib import Path
import sys,json,os,re
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
cs=json.loads((R/'source/inline-array-auth-fixtures-c22-a1/cases.json').read_text())['cases'];rows=[]
with N.lease():
 for v in ['typed-array-access','typed-array-access-inline','wasmtime-drc']:
  elf=O/'build/wasmtime-bench' if v=='wasmtime-drc' else R/('build/deferred-'+v+'/uwvm')
  for c in cs:
   assert N.sha(c['wasm'])==c['wasm_sha'];lab='inline-auth-new-fixtures-c22-a1-'+v+'-'+c['name'];env=dict(os.environ,LD_LIBRARY_PATH=str(O/'source/wasmtime-capi/lib'))
   for k in list(env):
    if k.startswith('WASM_PERF_NATIVE'):env.pop(k,None)
   cmd=[elf,c['wasm']] if v=='wasmtime-drc' else [elf,'-Rcc','jit','-Rcm','full','-Rct','0','-Rllvm-cache-path','disable','-Rclog','err','-WFE-gc','-WFE-exceptions','-WFE-function-references','--run',c['wasm']]
   N.run(lab,cmd,cpu=6,timeout=90,env=env);met=None
   if c['actual_gc'] and v!='wasmtime-drc':
    text=(R/'evidence'/(lab+'.stderr')).read_text();lines=[l for l in text.splitlines() if '[gc-managed]' in l];assert len(lines)==1,lines;met={k:int(val) for k,val in re.findall(r'([a-z_]+)=(\d+)',lines[0])};assert met['collections']>0 and met['reclaimed']>0 and met['disabled']==0 and met['roots_requested']==1,met
   rows.append(dict(case=c['name'],variant=v,pass_=True,actual_gc_metrics=met,ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],receipt=lab))
  print('C22_NEW_FIXTURES_PASS',v,len(cs),flush=True)
 N.save(R/'evidence/inline-auth-new-fixtures-c22-a1.json',dict(cells=len(rows),rows=rows,scope='32 new independent assertion modules, old/new/Wasmtime same WASM; runtime collection metrics proved only for UWVM no-tag collection-domain.'))
