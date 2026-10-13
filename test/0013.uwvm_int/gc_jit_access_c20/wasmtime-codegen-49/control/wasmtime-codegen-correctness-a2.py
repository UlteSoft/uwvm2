import sys,pathlib,json,os
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
D=N.ROOT/'source/wasmtime-codegen-analysis-a1';CLI=D/'wasmtime-v49.0.2-x86_64-linux/wasmtime';new=json.loads((D/'fixtures/cases.json').read_text())['cases'];old=json.loads((N.ROOT/'source/read-lock-isolation-c20/cases.json').read_text())['cases'];rows=[]
env=os.environ.copy();env.update(WASM_PERF_NATIVE_CLI='1',WASM_PERF_REPS='1',LD_LIBRARY_PATH='/tmp/uwvm-gc-perf-20261011-r73/source/wasmtime-capi/lib');env.pop('WASM_PERF_PROFILE',None)
variants=[('primary',N.ROOT/'build/repo-sync-c20-v4/uwvm2/uwvm',['-Rcc','jit','-Rcm','full']),('stable',N.ROOT/'build/repo-sync-c20-v4/uwvm2-stable/uwvm',['-Raot','-Rllvm-full-policy','pb-o3']),('ros',N.ROOT/'build/repo-sync-c20-v4/uwvm2-ros/uwvm',['-Raot','-Rllvm-full-policy','pb-o3'])]
def run_or_reuse(label,argv,**kw):
 p=N.ROOT/'evidence'/(label+'.json')
 if p.exists():
  r=json.loads(p.read_text());assert r['returncode']==0 and r['guard'] is None;return r
 return N.run(label,argv,**kw)
for _,elf,_ in variants:assert N.sha(elf)==json.loads((elf.parent/'build-binding.json').read_text())['ELF_sha']
with N.lease():
 for c in old+new:
  label='wasmtime49-cli-oracle-'+c['name']+'-a1';argv=[str(CLI),'run','-C','collector=drc,cache=n,parallel-compilation=n',c['wasm']];r=run_or_reuse(label,argv,cpu=6,timeout=30);rows.append({'runtime':'official-cli49-drc','name':c['name'],'wasm_sha':c['wasm_sha'],'elf_sha':N.sha(CLI),'receipt':label,'pass':r['returncode']==0 and not r['guard']});print(label,'PASS',flush=True)
 for c in new:
  label='wasmtime49-capi-oracle-'+c['name']+'-a1';argv=['/tmp/uwvm-gc-perf-20261011-r73/build/wasmtime-bench',c['wasm']];r=run_or_reuse(label,argv,cpu=6,timeout=30,env=env);rows.append({'runtime':'measured-capi49-drc','name':c['name'],'wasm_sha':c['wasm_sha'],'runner_sha':N.sha(pathlib.Path(argv[0])),'lib_sha':N.sha(pathlib.Path(env['LD_LIBRARY_PATH'])/'libwasmtime.so'),'receipt':label,'pass':r['returncode']==0 and not r['guard']});print(label,'PASS',flush=True)
  for name,elf,flags in variants:
   label='uwvm-c20-'+name+'-codegen-oracle-'+c['name']+'-a2';argv=[str(elf)]+flags+['-Rct','0','-WFE-gc','-WFE-function-references','-WFE-tail-call','-WFE-exceptions','-WFE-simd','-WFE-relaxed-simd','--',c['wasm']];r=run_or_reuse(label,argv,cpu=6,timeout=30,env=env);rows.append({'runtime':'integrated-c20-'+name,'name':c['name'],'wasm_sha':c['wasm_sha'],'elf_sha':N.sha(elf),'receipt':label,'pass':r['returncode']==0 and not r['guard']});print(label,'PASS',flush=True)
N.save(D/'codegen-correctness-a2.json',{'rows':rows,'passed':sum(r['pass'] for r in rows),'total':len(rows),'scope':'12fixtureCLI49oracles,4newfixtures on measuredCAPI49 and3integratedC20fullJIT builds. Whole checksum/final slots assertions; CAPI benchmark stdout used only as oracle execution, not timings. Not forced-GC/cycle/concurrent GC qualification.'})
