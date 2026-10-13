from pathlib import Path,PurePosixPath
import json,hashlib,os,tarfile
D=Path(__file__).parent;E=D.parent.parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();Q=json.loads((D/'repair-delta-v3-qualified.json').read_text());base=json.loads((D/'repaired-inputs-v2.json').read_text());M=json.loads((D/'repaired-inputs-v3.json').read_text())
assert Q['passed'] and sha(D/'repaired-inputs-v2.json')==Q['base_manifest_sha256'] and sha(D/'repaired-inputs-v3.json')==Q['overlay_manifest_sha256'] and sha(D/'repair-delta-v3.tar.gz')==Q['delta_archive_sha256'];assert set(M)==set(base) and {n:h for n,h in M.items() if base[n]!=h}==Q['changed_files']
S=D/'repaired-inputs-v3';assert not S.exists();S.mkdir();changed={}
with tarfile.open(D/'repair-delta-v3.tar.gz','r|gz') as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and not n.is_absolute() and '..' not in n.parts and m.name in Q['changed_files'] and m.name not in changed
  with t.extractfile(m) as f:data=f.read()
  assert hashlib.sha256(data).hexdigest()==M[m.name];p=S/m.name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);p.chmod(0o444);changed[m.name]=M[m.name]
assert changed==Q['changed_files'];linked=0
for n,h in M.items():
 if n in changed:continue
 p=S/n;p.parent.mkdir(parents=True,exist_ok=True);original=D/'repaired-inputs-v2'/n;assert original.is_file() and not original.is_symlink() and sha(original)==h;os.link(original,p);linked+=1
assert all(sha(S/n)==h for n,h in M.items())
proof=dict(passed=True,base_manifest_sha256=sha(D/'repaired-inputs-v2.json'),overlay_manifest_sha256=sha(D/'repaired-inputs-v3.json'),changed_files=changed,hardlinked_immutable_files=linked,all_payloads_read_back=True,cgroup=Path('/proc/self/cgroup').read_text());(D/'repair-v3-input-freeze-qualified.json').write_text(json.dumps(proof,indent=2)+'\n');print('Immutable fixture diagnostic overlay qualified',flush=True)
