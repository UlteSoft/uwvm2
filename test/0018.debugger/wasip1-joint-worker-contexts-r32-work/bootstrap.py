from pathlib import Path,PurePosixPath
import json,hashlib,tarfile,os
D=Path(__file__).parent;target=D/'inputs';previous=D.parent/'wasip1-joint-worker-contexts-20261007-r31/inputs'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
M=json.loads((D/'inputs.json').read_text());cut=json.loads((D/'local-source-cut-qualified.json').read_text())
assert cut['passed'] and cut['manifest_sha256']==sha(D/'inputs.json') and cut['source_archive_sha256']==sha(D/'inputs.tar.gz') and cut['files']==len(M)
assert not target.exists();target.mkdir()
seen={};linked=0;written=0
with tarfile.open(D/'inputs.tar.gz','r|gz') as archive:
 for member in archive:
  name=PurePosixPath(member.name)
  assert member.isfile() and not name.is_absolute() and '..' not in name.parts and member.name in M and member.name not in seen and member.size<8<<20
  with archive.extractfile(member) as inp:payload=inp.read()
  digest=hashlib.sha256(payload).hexdigest();assert digest==M[member.name]
  p=target/member.name;p.parent.mkdir(parents=True,exist_ok=True)
  original=previous/member.name
  if original.is_file() and not original.is_symlink() and sha(original)==digest:
   os.link(original,p);linked+=1
  else:
   with p.open('xb') as out:out.write(payload)
   p.chmod(0o444);written+=1
  seen[member.name]=digest
assert seen==M and all(sha(target/n)==h for n,h in M.items())
proof=dict(passed=True,files=len(M),hardlinked_immutable=linked,new_files=written,manifest_sha256=sha(D/'inputs.json'),source_archive_sha256=sha(D/'inputs.tar.gz'),all_payloads_read_back=True,cgroup=Path('/proc/self/cgroup').read_text())
(D/'input-freeze-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
print('R32 immutable source qualified',json.dumps(proof),flush=True)
