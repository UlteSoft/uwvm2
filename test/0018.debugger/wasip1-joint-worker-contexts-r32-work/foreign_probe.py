from pathlib import Path
import os,sys,json,hashlib,subprocess,shlex
D=Path(__file__).parent;E=D.parent.parent;O=D/'products/linux-integrated/uwvm2';S=D/'inputs/uwvm2';sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert sys.argv[1:]==['joint-foreign-probe-r32','uwvm2']
M=json.loads((D/'inputs.json').read_text());assert all(sha(D/'inputs'/p)==h for p,h in M.items())
r=next(r for r in json.loads((O/'results.json').read_text()) if r['name']=='foreign-observation-build');argv=r['argv'];base=argv[:argv.index('-MD')]
P=Path('/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35');sdk=json.loads((P/'sdk-qualified-r35.json').read_text());assert all(sha(p)==h for group in ('archives','source_inputs','generated_headers') for p,h in sdk[group].items())
for label in ('runtime','host-api'):assert sha(O/(label+'.o'))==json.loads((O/(label+'-qualified.json')).read_text())['object_sha256']
src=D/'foreign_probe.cc';s=(S/'test/0017.runtime/debug_checkpoint_observation_foreign_runtime.cc').read_text();a='    if(argc != 4) { return 2; }';assert s.count(a)==1;src.write_text(s.replace(a,a+'\n    ::uwvm2::uwvm::io::show_verbose=true;'))
first=argv.index(str(O/'runtime.o'));link=argv[first:argv.index('-o')];binary=D/'foreign_probe';args=[*base,'-MD','-MF',D/'foreign_probe.d',src,*link,'-o',binary]
with (D/'foreign-probe-build.log').open('wb') as f:q=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,timeout=300);assert q.returncode==0
with (D/'foreign-probe.log').open('wb') as f:q=subprocess.run([str(binary),str(O/'foreign-observation.wasm'),'instruction','1'],stdout=f,stderr=subprocess.STDOUT,timeout=90)
proof=dict(exit=q.returncode,diagnostic_only=True,source_manifest_sha256=sha(D/'inputs.json'),source_sha256=sha(src),binary_sha256=sha(binary),argv=list(map(str,args)),log_sha256=sha(D/'foreign-probe.log'))
(D/'foreign-probe.json').write_text(json.dumps(proof,indent=2)+'\n');print((D/'foreign-probe.log').read_text(errors='replace')[-10000:],flush=True)
