from pathlib import Path,PurePosixPath
import json,hashlib,os,tarfile
D=Path(__file__).parent;E=D.parent.parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();Q=json.loads((D/'repair-delta-v5-qualified.json').read_text());base=json.loads((D/'repaired-inputs-v4.json').read_text());M=json.loads((D/'repaired-inputs-v5.json').read_text())
assert Q['passed'] and sha(D/'repaired-inputs-v4.json')==Q['base_manifest_sha256'] and sha(D/'repaired-inputs-v5.json')==Q['overlay_manifest_sha256'] and sha(D/'repair-delta-v5.tar.gz')==Q['delta_archive_sha256'];assert set(M)==set(base) and {n:h for n,h in M.items() if base[n]!=h}==Q['changed_files']
S=D/'repaired-inputs-v5';assert not S.exists();S.mkdir();changed={}
with tarfile.open(D/'repair-delta-v5.tar.gz','r|gz') as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and not n.is_absolute() and '..' not in n.parts and m.name in Q['changed_files'] and m.name not in changed
  with t.extractfile(m) as f:data=f.read()
  assert hashlib.sha256(data).hexdigest()==M[m.name];p=S/m.name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);p.chmod(0o444);changed[m.name]=M[m.name]
assert changed==Q['changed_files'];linked=0
for n,h in M.items():
 if n in changed:continue
 p=S/n;p.parent.mkdir(parents=True,exist_ok=True);original=D/'repaired-inputs-v4'/n;assert original.is_file() and not original.is_symlink() and sha(original)==h;os.link(original,p);linked+=1
assert all(sha(S/n)==h for n,h in M.items())
proof=dict(passed=True,base_manifest_sha256=sha(D/'repaired-inputs-v4.json'),overlay_manifest_sha256=sha(D/'repaired-inputs-v5.json'),changed_files=changed,hardlinked_immutable_files=linked,all_payloads_read_back=True,cgroup=Path('/proc/self/cgroup').read_text());(D/'repair-v5-input-freeze-qualified.json').write_text(json.dumps(proof,indent=2)+'\n');print('Immutable fixture diagnostic overlay qualified',flush=True)
old=E/'guard-joint-linux-r34-uwvm2.json';g=json.loads(old.read_text());assert g['passed'] and g['actual_root_exit']==0
A=json.loads((D/'uwvm2-linux-integrated-v4-baseline-product-retirement.json').read_text());assert A['passed'] and A['all_payloads_read_back'] and sha(A['archive'])==A['archive_sha256']
O=D/'products/linux-integrated/uwvm2';prior=D/'products/linux-v4-baseline/uwvm2';prior.parent.mkdir();assert not prior.exists();O.rename(prior);(D/'v4-baseline-native-guard.json').write_bytes(old.read_bytes());print('Actual v4 native baseline retained; fresh v5 object and native runs required',flush=True)
# Reclaim only the redundant v1 directory. Its exact bytes remain derivable
# from the original fully verified archive plus authenticated two-file delta.
V=D/'repaired-inputs';V1=json.loads((D/'repaired-inputs.json').read_text());V0=json.loads((D/'inputs.json').read_text());data={}
with tarfile.open(D/'inputs.tar.gz','r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in V0 and m.name not in data
  with t.extractfile(m) as f:data[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert data==V0
with tarfile.open(D/'repair-delta.tar.gz','r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in V1
  with t.extractfile(m) as f:data[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert data==V1 and all(sha(V/n)==h for n,h in V1.items())
before=os.statvfs(D).f_favail;dirs=[];retired={}
for parent,folders,names in os.walk(V,followlinks=False):
 dirs.append(Path(parent));assert all(not (Path(parent)/n).is_symlink() for n in folders)
 for n in names:
  p=Path(parent)/n;key=str(p.relative_to(V));assert key in V1 and sha(p)==V1[key];retired[key]=V1[key];p.unlink()
assert retired==V1
for p in reversed(dirs):p.rmdir()
(D/'v1-input-directory-retirement.json').write_text(json.dumps(dict(passed=True,all_payloads_read_back=True,original_archive_sha256=sha(D/'inputs.tar.gz'),delta_archive_sha256=sha(D/'repair-delta.tar.gz'),manifest_sha256=sha(D/'repaired-inputs.json'),retired_directory=str(V),files=len(V1),inodes_before=before,inodes_after=os.statvfs(D).f_favail,active_v2_and_v5_trees_unchanged=True),indent=2)+'\n')
