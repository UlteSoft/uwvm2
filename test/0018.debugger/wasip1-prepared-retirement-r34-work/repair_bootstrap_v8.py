from pathlib import Path
import os, sys, json, hashlib, tarfile
D=Path(__file__).parent
assert sys.argv[1:]==['joint-repair-v8-inputs-r34','final','all']
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
request=json.loads((D/'repair-v8-request.json').read_text())
parent=json.loads((D/'repaired-inputs-v7.json').read_text())
manifest=json.loads((D/'repaired-inputs-v8.json').read_text())
assert sha(D/'repaired-inputs-v7.json')==request['parent_manifest_sha256']
assert sha(D/'repaired-inputs-v8.json')==request['overlay_manifest_sha256']
assert sha(D/'repair-v8-delta.tar.gz')==request['delta_sha256']
assert len(manifest)==8791 and set(manifest)-set(parent)==set(request['new_paths'])
assert set(request['changed_paths']) == set(request['new_paths']) | {n for n,h in parent.items() if manifest[n]!=h}
assert all(n.endswith(('debug_checkpoint_prepared_retirement_runtime.cc','debug_checkpoint_native_cohort_retirement_runtime.cc')) for n in set(request['changed_paths'])-set(request['new_paths']))
S=D/'repaired-inputs-v8'
if S.exists():
 assert S.is_dir() and not any(S.iterdir());S.rmdir()
S.mkdir();permissions={}
for n,h in parent.items():
 if n in request['changed_paths']:continue
 p=D/'repaired-inputs-v7'/n;assert p.is_file() and not p.is_symlink() and sha(p)==h
 if p.stat().st_mode&0o222:
  permissions[n]=oct(p.stat().st_mode);p.chmod(p.stat().st_mode&~0o222)
 dest=S/n;dest.parent.mkdir(parents=True,exist_ok=True);os.link(p,dest)
seen={}
with tarfile.open(D/'repair-v8-delta.tar.gz','r|gz') as a:
 for m in a:
  assert m.isfile() and m.name in request['changed_paths'] and m.name not in seen and m.size<1<<20
  with a.extractfile(m) as f:data=f.read()
  h=hashlib.sha256(data).hexdigest();assert h==manifest[m.name]
  p=S/m.name;p.parent.mkdir(parents=True,exist_ok=True)
  with p.open('xb') as f:f.write(data)
  p.chmod(0o444);seen[m.name]=h
assert set(seen)==set(request['changed_paths'])
assert all(sha(S/n)==h for n,h in manifest.items())
for p in sorted(S.rglob('*'),key=lambda p:len(p.parts),reverse=True):
 if p.is_dir():p.chmod(0o555)
S.chmod(0o555)
assert all((S/n).stat().st_mode&0o222==0 for n in manifest)
q=dict(passed=True,overlay_manifest_sha256=request['overlay_manifest_sha256'],parent_manifest_sha256=request['parent_manifest_sha256'],source_files=len(manifest),new_paths=request['new_paths'],all_source_payloads_read_back=True,library_source_unchanged=True,native_execution_claimed=False,owned_unchanged_source_inode_write_bits_removed=permissions,unix_readonly_all=True)
(D/'repair-v8-input-freeze-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
print('Read-only V8 8791-file input: same V7 production library, serial TLS cleanup with partial-exit refusal',flush=True)
