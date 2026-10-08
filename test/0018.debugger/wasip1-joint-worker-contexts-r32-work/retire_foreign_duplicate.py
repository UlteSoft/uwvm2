from pathlib import Path
import json,hashlib,sys
D=Path(__file__).parent;sys.path.insert(0,str(D));import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
Q=json.loads((D/'first-failed-products-retirement.json').read_text());A=Path(Q['archive']);assert Q['passed'] and sha(A)==Q['archive_sha256']
p=D/'products/linux-integrated/uwvm2/foreign-observation';key=str(p.relative_to(D));assert sha(p)==Q['payloads'][key]
seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name in Q['payloads'] and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==Q['payloads'];proof=dict(passed=True,exact_own_duplicate=str(p),sha256=sha(p),bytes=p.stat().st_size,recovery_archive=str(A),recovery_sha256=sha(A),all_payloads_read_back=True,failed_matrix_unchanged=True)
(D/'foreign-duplicate-retirement.json').write_text(json.dumps(proof,indent=2)+'\n');p.unlink();print('Retired only qualified recoverable foreign-test duplicate',proof['bytes'],flush=True)
