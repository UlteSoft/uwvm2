from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex
D=Path(__file__).parent;E=D.parent.parent;O=D/'products/linux-integrated/uwvm2';S=D/'inputs/uwvm2';sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert sys.argv[1:]==['joint-foreign-probe3-r32','uwvm2']
M=json.loads((D/'inputs.json').read_text());assert all(sha(D/'inputs'/p)==h for p,h in M.items())
r=next(r for r in json.loads((O/'results.json').read_text()) if r['name']=='foreign-observation-build');argv=r['argv'];base=argv[:argv.index('-MD')]
P=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35');sdk=json.loads((P/'sdk-qualified-r35.json').read_text());assert all(sha(p)==h for group in ('archives','source_inputs','generated_headers') for p,h in sdk[group].items())
for label in ('runtime','host-api'):assert sha(O/(label+'.o'))==json.loads((O/(label+'-qualified.json')).read_text())['object_sha256']
src=D/'foreign_probe3.cc';s=(S/'test/0017.runtime/debug_checkpoint_observation_foreign_runtime.cc').read_text();a='    if(argc != 4) { return 2; }';assert s.count(a)==1;s=s.replace('prepare_owned_full_cli_source()','prepare_owned_full_cli_source(true)');s=s.replace('    actual_reentry_module.store(module, ::std::memory_order_release);','    // Owned CLI preparation defers active segments to the start dispatcher.\n    // This embedding fixture owns that cold startup before any guest enters.\n    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"checkpoint-host-effects");\n    actual_reentry_module.store(module, ::std::memory_order_release);');assert hashlib.sha256(s.encode()).hexdigest()=='269fe72502eb6de3623f691338bab51a6b511e312749fe56f81e840b8066173e';src.write_text(s.replace(a,a+'\n    ::uwvm2::uwvm::io::show_verbose=true;'))
first=argv.index(str(O/'runtime.o'));link=argv[first:argv.index('-o')];binary=D/'foreign_probe3';args=[*base,'-MD','-MF',D/'foreign_probe3.d',src,*link,'-o',binary]
with (D/'foreign-probe3-build.log').open('wb') as f:q=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,timeout=300);assert q.returncode==0
rows=[]
for policy in ('instruction','unwind'):
 for entry in range(1,7):
  p=D/('foreign-probe3-'+policy+'-'+str(entry)+'.log')
  with p.open('wb') as f:q=subprocess.run([str(binary),str(O/'foreign-observation.wasm'),policy,str(entry)],stdout=f,stderr=subprocess.STDOUT,timeout=90)
  text=p.read_text(errors='replace');expected='PASS entry='+str(entry)+' policy='+policy+' foreign-capture-declined=1 native-calls=1 whole-restore=0'
  rows.append(dict(policy=policy,entry=entry,exit=q.returncode,passed=q.returncode==0 and expected in text,log_sha256=sha(p)))
  (D/'foreign-probe3.json').write_text(json.dumps(dict(passed=all(r['passed'] for r in rows) and len(rows)==12,diagnostic_only=True,exact_frozen_runtime_dependencies_unchanged=True,source_sha256=sha(src),binary_sha256=sha(binary),rows=rows),indent=2)+'\n')
  print('real callback',policy,entry,q.returncode,expected in text,flush=True)
  assert rows[-1]['passed'],text[-5000:]
