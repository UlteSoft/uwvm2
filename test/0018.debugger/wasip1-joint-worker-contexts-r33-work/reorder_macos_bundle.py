from pathlib import Path,PurePosixPath
import json,sys,hashlib,os
D=Path(__file__).parent;E=D.parent.parent
assert sys.argv[1:]==['joint-reorder-macos-r33']
import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
p=D/'macos-both-repositories-bundle-qualified.json';original=json.loads(p.read_text());assert original['passed'] and original['all_payloads_read_back'] and sha(original['archive'])==original['archive_sha256']
backup=D/'macos-first-bundle-readback-too-large.json';assert not backup.exists();backup.write_bytes(p.read_bytes())
S=D/'macos-bundle-ordered-source';assert not S.exists();S.mkdir(mode=0o700);seen={}
with qa.open_reader(original['archive']) as t:
 for m in t:
  name=PurePosixPath(m.name);assert m.isfile() and m.name in original['payloads'] and not name.is_absolute() and '..' not in name.parts and m.name not in seen
  target=S/m.name;target.parent.mkdir(parents=True,exist_ok=True)
  with t.extractfile(m) as inp,target.open('xb') as out:
   while b:=inp.read(1048576):out.write(b)
  assert sha(target)==original['payloads'][m.name];seen[m.name]=sha(target)
assert seen==original['payloads']
# Exact raw copies and both original per-repository archives remain recoverable.
# Retire only this already-read-back unsorted duplicate before new compression.
for r,a in original['originals'].items():assert sha(a['archive'])==a['archive_sha256']
assert sha(original['archive'])==original['archive_sha256'];Path(original['archive']).unlink()
archive=D/'macos-both-repositories-sorted-qualified-products.tar.zst';assert not archive.exists()
with qa.open_writer(archive) as t:
 for name in sorted(seen,key=lambda n:(n.split('/',1)[1],n.split('/',1)[0])):
  target=S/name;assert sha(target)==seen[name];t.add(target,arcname=name,recursive=False)
observed={}
with qa.open_reader(archive) as t:
 for m in t:
  assert m.isfile() and m.name in seen and m.name not in observed
  with t.extractfile(m) as inp:observed[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
assert observed==seen
proof=dict(original);proof.update(archive=str(archive),archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,deterministic_corresponding_member_order=True,initial_readback_qualification_sha256=sha(backup),initial_unsorted_archive_sha256=original['archive_sha256'],initial_unsorted_archive_bytes=original['archive_bytes'],temporary_raw_bytes=sum(x.stat().st_size for x in S.rglob('*') if x.is_file()),all_payloads_read_back=True)
p.write_text(json.dumps(proof,indent=2)+'\n')
for name,h in seen.items():target=S/name;assert sha(target)==h;target.unlink()
for directory in sorted((x for x in S.rglob('*') if x.is_dir()),key=lambda p:len(p.parts),reverse=True):directory.rmdir()
S.rmdir()
print('Exact original native payloads reordered, recompressed and fully read back; own staging retired',archive.stat().st_size,flush=True)
