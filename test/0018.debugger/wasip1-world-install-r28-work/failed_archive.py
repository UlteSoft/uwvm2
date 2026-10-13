from pathlib import Path
import tarfile,json,hashlib,importlib.util
D=Path(__file__).parent;E=D.parent.parent;O=D/'products/linux-integrated/uwvm2'
paths=list(O.glob('environment-group-before-*.exe'));assert len(paths)==2
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
rows={p.name:sha(p) for p in paths}
spec=importlib.util.spec_from_file_location('archive_budget',E/'rounds/wasip1-cross-jit-20261007-r26/archive_budget.py');b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
target=D/'failed-environment-group-products.tar.gz'
with b.open_archive(target) as t:
 for p in paths:t.add(p,arcname=p.name,recursive=False)
seen={}
with tarfile.open(target,'r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in rows
  with t.extractfile(m) as f:h=hashlib.file_digest(f,'sha256').hexdigest()
  assert h==rows[m.name];seen[m.name]=h
assert seen==rows
proof=dict(passed=True,archive=str(target),archive_sha256=sha(target),archive_bytes=target.stat().st_size,all_payloads_read_back=True,retired_raw=[dict(path=str(p),bytes=p.stat().st_size,sha256=rows[p.name]) for p in paths])
(D/'failed-environment-group-products-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for p in paths:assert sha(p)==rows[p.name];p.unlink()
print('own failed group products archived, read back, raw duplicates retired',flush=True)
