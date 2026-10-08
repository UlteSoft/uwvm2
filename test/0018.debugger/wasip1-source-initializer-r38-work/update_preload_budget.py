from pathlib import Path
import hashlib,json,os,tarfile,io,difflib
B=Path('/Users/liyinan/Documents/MacroModel/src');W=B/'uwvm2/test/0018.debugger/wasip1-source-initializer-r38-work'
M=json.loads((W/'syntax-overlay-v3.json').read_text());assert all(hashlib.sha256((B/k).read_bytes()).hexdigest()==v for k,v in M.items())
plans=[]
for repo in ('uwvm2','uwvm2-ros'):
 p=B/repo/'test/0017.runtime/debug_checkpoint_complete_preload_runtime.cc';data=p.read_bytes();s=data.decode();old='prepare.maximum_native_payload_bytes=512u*1024u*1024u;';assert s.count(old)==1;s=s.replace(old,'prepare.maximum_native_payload_bytes=768u*1024u*1024u;')
 plans.append((p,data,s.encode()))
 p=B/repo/'src/uwvm2/uwvm/debugger/wasip1_checkpoint.md';data=p.read_bytes();s=data.decode();old='request budget; the complete instance/preload tests use 512 MiB and continue to\nexercise smaller budget refusal.';assert s.count(old)==1;s=s.replace(old,'request budget; the complete instance test uses 512 MiB and the two-module\npreload test uses 768 MiB. Both continue to exercise smaller budget refusal.')
 plans.append((p,data,s.encode()))
back=W/'before-preload-budget-fix';back.mkdir()
for p,data,t in plans:
 q=back/p.relative_to(B);q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(data)
 temp=p.with_name(p.name+'.r38-preload-budget.tmp');assert not temp.exists()
 with temp.open('xb') as f:f.write(t);f.flush();os.fsync(f.fileno())
 os.chmod(temp,p.stat().st_mode&0o777);assert p.read_bytes()==data;os.replace(temp,p)
m={k:hashlib.sha256((B/k).read_bytes()).hexdigest() for k in M};(W/'syntax-overlay-v4.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(W/'syntax-overlay-v4.tar.gz','w:gz') as a:
 for k in m:
  data=(B/k).read_bytes();i=tarfile.TarInfo(k);i.mode=0o444;i.size=len(data);i.mtime=0;a.addfile(i,io.BytesIO(data))
changed={}
for p in (W/'before').rglob('*'):
 if p.is_file():
  k=str(p.relative_to(W/'before'));changed[k]=(p.read_text(),(B/k).read_text())
for repo in ('uwvm2','uwvm2-ros'):
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_source_initializer.h';changed[k]=('',(B/k).read_text())
patch=''.join(''.join(difflib.unified_diff(a.splitlines(True),z.splitlines(True),fromfile='a/'+k,tofile='b/'+k)) for k,(a,z) in sorted(changed.items()))
(W/'owned-source-changes.patch').write_text(patch)
print('test/doc changes',len(plans),'runtime code unchanged','manifest',hashlib.sha256((W/'syntax-overlay-v4.json').read_bytes()).hexdigest())
