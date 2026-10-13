import sys,pathlib,json,os
sys.path.insert(0,'/tmp/uwvm-gc-perf-20261011-r74/control');import native as N
D=N.ROOT/'source/wasmtime-codegen-analysis-a1';CLI=D/'wasmtime-v49.0.2-x86_64-linux/wasmtime'
cases=json.loads((N.ROOT/'source/read-lock-isolation-c20/cases.json').read_text())['cases']+json.loads((D/'fixtures/cases.json').read_text())['cases']
OUT=D/'matrix-codegen-a1';OUT.mkdir(exist_ok=False);rows=[]
obj='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llvm-objdump'
with N.lease():
 for c,opt in [(c,2) for c in cases]+[(c,0) for c in cases if c['name'].startswith('f32-')]:
  name=c['name']+'-O'+str(opt);clif=OUT/(name+'-clif');clif.mkdir();serial=OUT/(name+'.cwasm');assert N.sha(pathlib.Path(c['wasm']))==c['wasm_sha']
  argv=[str(CLI),'compile','-C','collector=drc,cache=n,parallel-compilation=n,cranelift-debug-verifier=y','-O','opt-level='+str(opt),'--emit-clif',str(clif),'-o',str(serial),c['wasm']]
  label='wasmtime-codegen-'+name+'-a1';r=N.run(label,argv,cpu=2,timeout=60,build=True);assert r['returncode']==0 and not r['guard'],r
  alabel='wasmtime-codegen-asm-'+name+'-a1';a=N.run(alabel,[obj,'--disassemble','--no-show-raw-insn','--x86-asm-syntax=intel',str(serial)],cpu=20,timeout=30);assert a['returncode']==0 and not a['guard']
  asm=OUT/(name+'.asm');asm.write_bytes((N.ROOT/'evidence'/(alabel+'.stdout')).read_bytes())
  files=[]
  for p in list(clif.iterdir())+[serial,asm]:N.persist(p);files.append({'path':str(p),'sha':N.sha(p),'bytes':p.stat().st_size})
  pattern=(D/'immutable-f32-hotloop-pattern.bin').read_bytes();blob=serial.read_bytes();count=blob.count(pattern)
  rows.append({'name':c['name'],'opt_level':opt,'wasm':c['wasm'],'wasm_sha':c['wasm_sha'],'files':files,'receipt':label,'assembly_receipt':alabel,'exact_measured_immutable_f32_hotloop_occurrences':count,'pattern_file_offset':blob.find(pattern) if count else None})
  print(name,'PASS',count,flush=True)
N.save(D/'codegen-matrix-a1.json',{'cli_sha':N.sha(CLI),'source_commit':'3c8a3e79aa3188a1a12b3aebbaa097abccceac92','rows':rows,'scope':'12same-byteWasmfixtures O2 plus2f32 O0. emit_clif emits compiler IR after compilation; O0 is a different optimization setting, not guaranteed pre-pass IR. Compilation measurements not benchmarked.'})
