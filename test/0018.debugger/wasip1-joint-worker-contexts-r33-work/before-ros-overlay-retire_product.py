from pathlib import Path
import sys,json,hashlib,tarfile
D=Path(__file__).parent;platform,repo=sys.argv[2:4]
assert sys.argv[1]=='joint-product-retire-r33' and platform in ('linux-integrated','windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
import sys as _sys
_sys.path.insert(0,str(Path(__file__).parent))
import qualified_archive as qa
A=json.loads((D/(repo+'-'+platform+'-product-retirement.json')).read_text())
receipt=D/(repo+'-'+platform+'-local-cold-qualified.json');Q=json.loads(receipt.read_text())
original=D/(repo+'-'+platform+'-qualified-products.tar.zst');assert str(original)==A['archive']
assert A['passed'] and Q['passed'] and Q['all_payloads_read_back'] and Q['payloads']==A['payloads'] and Q['archive_sha256']==A['archive_sha256']==sha(original)
assert Q['archive_bytes']==original.stat().st_size and Q['limit_bytes']==256<<20 and Q['cold_bytes']<Q['limit_bytes'] and Q['memory_upper_bytes']<2<<30
assert Q['local_cold_aggregate_limit_bytes']==2<<30 and Q['local_cold_aggregate_bytes']<2<<30
assert Q['source_manifest_sha256']==sha(D/'inputs.json') and Q['native_tests_unchanged']
assert str(Q['archive']).startswith('/Users/liyinan/Documents/MacroModel/wasip1-r33-owned-cold-evidence/')
seen={}
with qa.open_reader(original) as archive:
 for m in archive:
  assert m.isfile() and m.name in A['payloads'] and m.name not in seen
  with archive.extractfile(m) as inp:seen[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
assert seen==A['payloads']
proof=dict(passed=True,archive_sha256=sha(original),bytes=original.stat().st_size,qualification_sha256=sha(receipt),all_payloads_read_back=True,local_recovery_archive=Q['archive'],source_manifest_sha256=sha(D/'inputs.json'),cgroup=Path('/proc/self/cgroup').read_text())
(D/(repo+'-'+platform+'-local-cold-retirement.json')).write_text(json.dumps(proof,indent=2)+'\n')
original.unlink()
print('R33 bounded local cold recovery verified; retired only own compressed product',platform,repo,proof['bytes'],flush=True)
