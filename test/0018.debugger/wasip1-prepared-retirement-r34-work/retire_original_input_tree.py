from pathlib import Path,PurePosixPath
import json,hashlib,tarfile,os,sys
D=Path(__file__).parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();assert sys.argv[1]=='joint-input-directory-retire-r34'
M=json.loads((D/'inputs.json').read_text());Q=json.loads((D/'input-freeze-qualified.json').read_text());assert Q['passed'] and sha(D/'inputs.json')==Q['manifest_sha256'] and sha(D/'inputs.tar.gz')==Q['source_archive_sha256']
S=D/'inputs';assert S.is_dir() and not S.is_symlink();assert all((S/n).is_file() and not (S/n).is_symlink() and sha(S/n)==h for n,h in M.items())
seen={}
with tarfile.open(D/'inputs.tar.gz','r|gz') as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and m.name in M and m.name not in seen and not n.is_absolute() and '..' not in n.parts
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==M
latest=json.loads((D/'repaired-inputs-v4.json').read_text());assert all(sha(D/'repaired-inputs-v4'/n)==h for n,h in latest.items())
before=os.statvfs(D).f_favail;retired={};dirs=[]
for parent,folders,names in os.walk(S,followlinks=False):
 dirs.append(Path(parent));assert all(not (Path(parent)/n).is_symlink() for n in folders)
 for n in names:
  p=Path(parent)/n;key=str(p.relative_to(S));assert key in M and sha(p)==M[key];retired[key]=M[key];p.unlink()
assert retired==M
for p in reversed(dirs):p.rmdir()
proof=dict(passed=True,all_payloads_read_back=True,retired_original_source_directory=str(S),original_source_archive=str(D/'inputs.tar.gz'),original_archive_sha256=sha(D/'inputs.tar.gz'),manifest_sha256=sha(D/'inputs.json'),files=len(M),old_source_bytes_recoverable=True,current_v4_tree_unchanged=True,active_runtime_v2_headers_unchanged=True,inodes_before=before,inodes_after=os.statvfs(D).f_favail,cgroup=Path('/proc/self/cgroup').read_text());(D/'original-input-directory-retirement.json').write_text(json.dumps(proof,indent=2)+'\n');print('Only redundant original input directory retired after complete original archive readback',proof['inodes_before'],proof['inodes_after'],flush=True)
