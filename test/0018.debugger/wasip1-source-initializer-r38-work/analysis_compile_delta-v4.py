from pathlib import Path
import hashlib,json,os,subprocess,tarfile,time,sys
D=Path(__file__).parent;S=D/'syntax-overlay-v4'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
m=json.loads((D/'syntax-overlay-v4.json').read_text());old=json.loads((D/'syntax-overlay-v3.json').read_text())
q=json.loads((D/'analysis-compile-qualified-v3b.json').read_text());g=json.loads((D/'guard-analysis-compile-v3b.json').read_text())
assert q['passed'] and g['passed'] and g['parent_events_unchanged'] and all(x['pidfd_retired'] for x in g['processes'])
assert Path('/proc/self/cgroup').read_text()==g['cgroup'] and set(m)==set(old)
changed={k for k in m if m[k]!=old[k]}
assert changed=={r+'/'+p for r in ('uwvm2','uwvm2-ros') for p in ('test/0017.runtime/debug_checkpoint_complete_preload_runtime.cc','src/uwvm2/uwvm/debugger/wasip1_checkpoint.md')}
S.mkdir()
with tarfile.open(D/'syntax-overlay-v4.tar.gz','r:gz') as archive:
 seen=set()
 for member in archive:
  assert member.isfile() and member.name in m and member.name not in seen
  data=archive.extractfile(member).read();assert hashlib.sha256(data).hexdigest()==m[member.name]
  p=S/member.name;p.parent.mkdir(parents=True,exist_ok=True)
  with p.open('xb') as f:f.write(data)
  os.chmod(p,0o444);seen.add(member.name)
 assert seen==set(m)
for repo in ('uwvm2','uwvm2-ros'):
 lib=S/repo/'src/uwvm2/runtime/lib'
 for p in (D/'syntax-overlay-v3'/repo/'src/uwvm2/runtime/lib').iterdir():
  out=lib/p.name
  if out.exists():continue
  if p.name=='uwvm_runtime.default.cpp':out.write_bytes(p.read_bytes());os.chmod(out,0o444)
  else:out.symlink_to(p.resolve())
rows=[];fresh=0
for row in q['rows']:
 r=dict(row)
 if row['target_os']=='linux' and row['stage']=='preload':
  assert sha(row['argv'][0])==row['compiler_sha256']
  argv=[a.replace('syntax-overlay-v3/','syntax-overlay-v4/') for a in row['argv']]
  log=D/(row['repo']+'-linux-preload-v4.log');start=time.monotonic()
  print(row['repo'],'preload V4 frontend START',flush=True)
  with log.open('xb') as f:result=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT,timeout=240,
   env=dict(os.environ,MALLOC_ARENA_MAX='1',TMPDIR=str(D),LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu'))
  r.update(argv=argv,passed=result.returncode==0,exit=result.returncode,seconds=time.monotonic()-start,
   log_sha256=sha(log),source_manifest_sha256=sha(D/'syntax-overlay-v4.json'),reused_unchanged_compile=False);fresh+=1
  print(row['repo'],'preload V4 frontend',result.returncode,flush=True)
  if result.returncode:print(log.read_text(errors='replace'));sys.exit(1)
 else:r.update(reused_unchanged_compile=True,parent_qualified_sha256=sha(D/'analysis-compile-qualified-v3b.json'))
 rows.append(r)
assert fresh==2 and len(rows)==16 and all(x['passed'] for x in rows) and all(sha(S/k)==h for k,h in m.items())
validation=q['wasm_data_fixture_checks']
assert len(validation)==8 and all(x['passed'] for x in validation)
for r in validation:
 source=Path(r['argv'][2]) if r['stage']=='assemble' else None
 if source:assert sha(source)==r['source_sha256']
 wasm=Path(r['argv'][4]) if r['stage']=='assemble' else Path(r['argv'][2]);assert sha(wasm)==r['wasm_sha256']
out=dict(passed=True,rows=rows,wasm_data_fixture_checks=validation,source_manifest_sha256=sha(D/'syntax-overlay-v4.json'),
 fresh_frontend_launches=2,reused_unchanged_frontend_rows=14,reused_unchanged_wasm_checks=8,changed_paths=sorted(changed),
 parent_qualification_sha256=sha(D/'analysis-compile-qualified-v3b.json'),parent_guard_sha256=sha(D/'guard-analysis-compile-v3b.json'),
 source_equivalence_verified=True,actual_native_runtime_execution=False,controller_sha256=sha(__file__))
(D/'analysis-compile-qualified-v4.json').write_text(json.dumps(out,indent=2)+'\n')
print('final source coverage',len(rows),'fresh',fresh,'verified unchanged',len(rows)-fresh,flush=True)
