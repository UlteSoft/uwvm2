from pathlib import Path
import sys,json,hashlib,os
import qualified_archive as qa
D=Path(__file__).parent
assert sys.argv[1:]==['joint-input-history-retire-r34','final','all']
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
active=D/'repaired-inputs-v6';base=json.loads((D/'repaired-inputs-v6.json').read_text())
snapshots={};delta={};counts={}
for version in ('v2','v3'):
 root=D/('repaired-inputs-'+version);manifest=D/('repaired-inputs-'+version+'.json')
 assert root.is_dir() and not root.is_symlink()
 M=json.loads(manifest.read_text());assert len(M)==8787
 for n,h in M.items():
  p=root/n;assert p.is_file() and not p.is_symlink() and sha(p)==h
  if base.get(n)==h:assert sha(active/n)==h
  else:delta[version+'/'+n]=h
 snapshots[version]=dict(root=str(root),manifest=str(manifest),manifest_sha256=sha(manifest),files=len(M),unmatched=sum(base.get(n)!=h for n,h in M.items()))
 counts[version]=M
A=D/'historical-v2-v3-source-unmatched.tar.zst';assert not A.exists()
with qa.open_writer(A) as t:
 t.dereference=True
 for name,h in delta.items():
  version,n=name.split('/',1);t.add(D/('repaired-inputs-'+version)/n,arcname=name,recursive=False)
seen={}
with qa.open_reader(A) as a:
 for m in a:
  assert m.isfile() and m.name in delta and m.name not in seen
  with a.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==delta
before=os.statvfs(D).f_favail
q=dict(passed=True,native_execution_claimed=False,all_source_payloads_read_back=True,
 snapshots=snapshots,archive=str(A),archive_sha256=sha(A),unmatched_members=delta,
 reconstruction='For each original manifest member, use unchanged readonly v6 bytes when its SHA equals the v6 manifest; otherwise use <version>/<member> from the fully verified archive',
 retained_fallback_manifest=str(D/'repaired-inputs-v6.json'),retained_fallback_manifest_sha256=sha(D/'repaired-inputs-v6.json'),retained_fallback_directory=str(active),source_contents_unchanged=True,inodes_before=before)
(D/'historical-v2-v3-source-retirement-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
for version,info in snapshots.items():
 root=Path(info['root']);directories=[];retired={}
 for parent,dirs,names in os.walk(root,followlinks=False):
  directory=Path(parent);assert not directory.is_symlink();directories.append(directory);directory.chmod(directory.stat().st_mode|0o700)
  assert all(not (directory/n).is_symlink() for n in dirs)
  for n in names:
   p=directory/n;key=str(p.relative_to(root));assert key in counts[version] and sha(p)==counts[version][key]
   retired[key]=counts[version][key];p.unlink()
 assert retired==counts[version]
 for p in reversed(directories):p.rmdir()
q['inodes_after']=os.statvfs(D).f_favail
(D/'historical-v2-v3-source-retirement-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
print('Recoverable old v2/v3 namespaces retired; unchanged v6/v7 retained, free inodes',q['inodes_after'],flush=True)
