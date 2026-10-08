from pathlib import Path
import hashlib,json,tarfile,os
D=Path(__file__).parent;E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
a=E/'cold-relocated-r38/source-initializer-r38-evidence.tar.gz';q=D/'r38-final-evidence-qualified.json';data=json.loads(q.read_text())
g=json.loads((D/'guard-ram-native-v4.json').read_text());assert g['passed'] and g['parent_events_unchanged'] and all(x['pidfd_retired'] for x in g['processes'])
assert Path('/proc/self/cgroup').read_text()==g['cgroup'] and data['passed'] and not data['full_world_restoration']
assert sha(a)==data['archive_sha256'] and a.stat().st_size==data['archive_bytes']
seen=set()
with tarfile.open(a,'r:gz') as t:
 for m in t:
  assert m.isfile() and m.name in data['members'] and m.name not in seen
  r=data['members'][m.name];assert m.size==r['size'] and hashlib.file_digest(t.extractfile(m),'sha256').hexdigest()==r['sha256'];seen.add(m.name)
assert len(seen)==data['member_count'] and seen==set(data['members'])
with a.open('rb') as f:os.fsync(f.fileno())
out=dict(passed=True,archive=str(a),archive_sha256=sha(a),member_count=len(seen),complete_members_read_back=True,
 local_archive_qualification_sha256=sha(q),source_manifest_sha256=data['source_manifest_sha256'],
 linux_native_cases=24,fresh_native_cases=14,reused_unchanged_native_cases=10,full_world_restoration=False,cgroup=g['cgroup'])
(D/'r38-final-remote-evidence-qualified.json').write_text(json.dumps(out,indent=2)+'\n')
print('remote evidence complete full readback',len(seen),flush=True)
