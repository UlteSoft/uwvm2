from pathlib import Path,PurePosixPath
import json,hashlib,os,tarfile
D=Path(__file__).parent;E=D.parent.parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();Q=json.loads((D/'repair-delta-v2-qualified.json').read_text());base=json.loads((D/'repaired-inputs.json').read_text());M=json.loads((D/'repaired-inputs-v2.json').read_text())
assert Q['passed'] and sha(D/'repaired-inputs.json')==Q['base_manifest_sha256'] and sha(D/'repaired-inputs-v2.json')==Q['overlay_manifest_sha256'] and sha(D/'repair-delta-v2.tar.gz')==Q['delta_archive_sha256'];assert set(M)==set(base) and {n:h for n,h in M.items() if base[n]!=h}==Q['changed_files']
S=D/'repaired-inputs-v2';assert not S.exists();S.mkdir();changed={}
with tarfile.open(D/'repair-delta-v2.tar.gz','r|gz') as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and not n.is_absolute() and '..' not in n.parts and m.name in Q['changed_files'] and m.name not in changed
  with t.extractfile(m) as f:data=f.read()
  assert hashlib.sha256(data).hexdigest()==M[m.name];p=S/m.name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);p.chmod(0o444);changed[m.name]=M[m.name]
assert changed==Q['changed_files'];linked=0
for n,h in M.items():
 if n in changed:continue
 p=S/n;p.parent.mkdir(parents=True,exist_ok=True);original=D/'repaired-inputs'/n;assert original.is_file() and not original.is_symlink() and sha(original)==h;os.link(original,p);linked+=1
assert all(sha(S/n)==h for n,h in M.items())
old=E/'guard-joint-linux-r34-uwvm2.json';g=json.loads(old.read_text());assert not g['passed'] and g['actual_root_exit']==1
O=D/'products/linux-integrated/uwvm2';failed=D/'products/linux-first-runtime-failed/uwvm2';failed.parent.mkdir();assert not failed.exists();O.rename(failed);(D/'first-runtime-failed-guard.json').write_bytes(old.read_bytes())
proof=dict(passed=True,base_manifest_sha256=sha(D/'repaired-inputs.json'),overlay_manifest_sha256=sha(D/'repaired-inputs-v2.json'),delta_archive_sha256=sha(D/'repair-delta-v2.tar.gz'),all_payloads_read_back=True,changed_files=changed,hardlinked_immutable_files=linked,old_inputs_untouched=True,failed_original_guard_sha256=sha(old),failed_products_directory=str(failed),cgroup=Path('/proc/self/cgroup').read_text());(D/'repair-v2-input-freeze-qualified.json').write_text(json.dumps(proof,indent=2)+'\n');print('R34 limited friend repair immutable overlay qualified',linked,flush=True)

import qualified_archive as qa
members={str(p.relative_to(failed)):sha(p) for p in failed.rglob('*') if p.is_file() and not p.is_symlink()};archive=D/'first-runtime-failed-products.tar.zst'
with qa.open_writer(archive) as t:
 for n,h in members.items():assert sha(failed/n)==h;t.add(failed/n,arcname=n,recursive=False)
seen={}
with qa.open_reader(archive) as t:
 for m in t:
  assert m.isfile() and m.name in members and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==members
rows=[]
for n,h in members.items():
 p=failed/n
 if p.suffix=='.o' or p.name=='debug_checkpoint_prepared_retirement':rows.append(dict(path=str(p),sha256=h,bytes=p.stat().st_size,member=n))
(D/'first-runtime-failed-products-qualified.json').write_text(json.dumps(dict(passed=True,negative_run_preserved=True,native_test_passed=False,archive=str(archive),archive_sha256=sha(archive),all_payloads_read_back=True,payloads=members,retired_raw=rows),indent=2)+'\n')
for row in rows:assert sha(row['path'])==row['sha256'];Path(row['path']).unlink()
print('Actual failed native test products recovered losslessly; own raw copies retired',flush=True)
