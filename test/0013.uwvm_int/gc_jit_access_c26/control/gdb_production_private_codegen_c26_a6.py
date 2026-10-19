from pathlib import Path
import sys,json,re
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
D=R/'source/production-private-codegen-c26-a5';D.mkdir(exist_ok=False);rows=[]
cs=json.loads((R/'source/private-numeric-publication-fixtures-c26-a3/cases.json').read_text())['cases'];cs=[c for c in cs if c['name'] in ['fresh-no-publication','global-publication-after-loop']]
symbol='_ZN5uwvm27runtime8compiler8llvm_jit21compile_all_from_uwvm7details45llvm_jit_gc_numeric_array_index_domain_bridgeEm'
llvm=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin')
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 elf=R/'build/repo-sync-c26-a5'/repo/'uwvm';b=json.loads((elf.parent/'build-binding.json').read_text());assert b['ELF_sha']==N.sha(elf) and b['verified_source_id']==b['input_sha']
 for c in cs:
  Q=D/(repo+'-'+c['name']);Q.mkdir();capture=Q/'capture.json';script=Q/'capture.gdb'
  py="import gdb,json,pathlib,hashlib,os\nD=pathlib.Path("+repr(str(Q))+" )\nclass Capture(gdb.Breakpoint):\n def __init__(self):\n  super().__init__("+repr(symbol)+",internal=True);self.done=False\n def stop(self):\n  if self.done:return False\n  self.done=True;inf=gdb.selected_inferior();pid=inf.pid;P=pathlib.Path('/proc')/str(pid);maps=(P/'maps').read_text();(D/'maps.txt').write_text(maps);rows=[]\n  for l in maps.splitlines():\n   z=l.split();a,e=[int(v,16)for v in z[0].split('-')]\n   if 'x' in z[1] and len(z)==5 and 0<e-a<=33554432:\n    raw=bytes(inf.read_memory(a,e-a));p=D/('rx-'+hex(a)+'.bin');p.write_bytes(raw);rows.append(dict(start=a,end=e,sha=hashlib.sha256(raw).hexdigest(),file=str(p)))\n  assert rows\n  (D/'capture.json').write_text(json.dumps(dict(pid=pid,tid=gdb.selected_thread().ptid[1],boot=pathlib.Path('/proc/sys/kernel/random/boot_id').read_text().strip(),cgroup=(P/'cgroup').read_text().strip(),affinity=sorted(os.sched_getaffinity(pid)),ELF_sha="+repr(N.sha(elf))+",WASM_sha="+repr(c['wasm_sha'])+",rx=rows),indent=2)+'\\n');return False\nCapture()\n"
  script.write_text('set pagination off\nset confirm off\nset disable-randomization off\npython\n'+py+'end\nrun\n');N.persist(script)
  cache=R/'tmp/cache-reload-c26-a7'/repo/c['name'];assert cache.exists()
  flags=(['-Rcc','jit','-Rcm','full'] if repo=='uwvm2' else ['-Raot','-Rllvm-full-policy','pb-o3'])+['-Rct','0','-Rclog','err','-Rllvm-cache-path','path',str(cache),'-WFE-gc','-WFE-function-references','-WFE-tail-call','--run',c['wasm']]
  lab='production-private-codegen-c26-a5-'+repo+'-'+c['name'];N.run(lab,['gdb','-q','-nx','-batch','-x',script,'--args',elf,*flags],cpu=6,timeout=90)
  out=(R/'evidence'/(lab+'.stdout')).read_text();err=(R/'evidence'/(lab+'.stderr')).read_text();assert 'exited normally' in out and 'object-cache-hit module=' in err and 'signature_verified=1' in err,(out[-1200:],err[-1500:])
  j=json.loads(capture.read_text());assert j['cgroup']=='0::'+N.CG and j['affinity']==[6] and j['boot']==N.BOOT,j
  locks=[]
  for i,x in enumerate(j['rx']):
   p=Path(x['file']);assert N.sha(p)==x['sha'];o=Q/(str(i)+'.o');N.run(lab+'-objcopy-'+str(i),[llvm/'llvm-objcopy','-I','binary','-O','elf64-x86-64','-B','i386:x86-64','--rename-section','.data=.text,alloc,load,readonly,code,contents',p,o],cpu=20,timeout=30)
   a=lab+'-objdump-'+str(i);N.run(a,[llvm/'llvm-objdump','-d','--no-show-raw-insn','--x86-asm-syntax=intel','--adjust-vma='+str(x['start']),o],cpu=20,timeout=30);text=(R/'evidence'/(a+'.stdout')).read_text();(Q/(str(i)+'.asm')).write_text(text);o.unlink();locks += [l for l in text.splitlines() if re.search(r'\bxchg\s+byte ptr \[[^\]]*\+ 0x20\]',l)]
  assert (len(locks)>0)==c['requires_lock'],(repo,c['name'],locks)
  for p in Q.rglob('*'):
   if p.is_file():N.persist(p)
  rows.append(dict(repo=repo,case=c['name'],requires_lock=c['requires_lock'],lock_sites=locks,capture_sha=N.sha(capture),ELF_sha=N.sha(elf),WASM_sha=c['wasm_sha'],signed_cache_hit=True,receipt=lab));print('PRODUCTION_CODEGEN_PASS',repo,c['name'],len(locks),flush=True)
N.save(D/'result.json',dict(cells=len(rows),rows=rows,scope='Normal seven-macro production CLI, fresh verified signed-cache-hit processes, external GDB reads actual anonymous executable mappings without enabling UWVM debug mode. Private constructor-only case has no object flag XCHG; late global publication retains locks. Correctness Wasm assertions passed. No performance claims from debugger execution.'))
