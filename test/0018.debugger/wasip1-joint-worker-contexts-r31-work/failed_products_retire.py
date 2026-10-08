from pathlib import Path
import hashlib,json,sys
D=Path(__file__).parent;sys.path.insert(0,str(D));import qualified_archive as qa
E=D.parent.parent;O=D/'products/linux-integrated/uwvm2'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
Q=json.loads((E/'guard-joint-linux-integrated-r31-uwvm2.json').read_text());assert not Q['passed'] and Q['actual_root_exit']==1
q=json.loads((D/'chain-probe2.json').read_text());assert q['diagnostic_only'] and q['exit']==-4
paths=[D/'chain_probe2',*[O/n for n in ('runtime.o','host-api.o','fixture','environment-group','wasip1-memory-binding')]]
assert all(p.is_file() and not p.is_symlink() for p in paths)
manifest={str(p.relative_to(D)):sha(p) for p in paths}
A=D/'first-failed-products-recoverable.tar.zst';assert not A.exists()
with qa.open_writer(A) as t:
 for p in paths:t.add(p,arcname=str(p.relative_to(D)),recursive=False)
seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name in manifest and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==manifest
proof=dict(passed=True,archive=str(A),archive_sha256=sha(A),payloads=manifest,all_payloads_read_back=True,failed_test_result_unchanged=True,diagnostic_only=True,source_manifest_sha256=sha(D/'inputs.json'),bytes_retired=sum(p.stat().st_size for p in paths),archive_bytes=A.stat().st_size,cgroup=Path('/proc/self/cgroup').read_text())
(D/'first-failed-products-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for p in paths:assert sha(p)==manifest[str(p.relative_to(D))];p.unlink()
print('Failed trial remains failed; exact owned products recoverably retired',proof['bytes_retired'],proof['archive_bytes'],flush=True)
