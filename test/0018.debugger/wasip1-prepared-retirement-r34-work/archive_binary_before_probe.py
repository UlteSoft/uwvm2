from pathlib import Path
import json,hashlib
import qualified_archive as qa
D=Path(__file__).parent;E=D.parent.parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();O=D/'products/linux-integrated/uwvm2';p=O/'debug_checkpoint_prepared_retirement';A=D/'second-runtime-failed-binary.tar.zst';assert p.is_file() and not p.is_symlink();Q=json.loads((O/(p.name+'-qualified.json')).read_text());h=sha(p);assert h==Q['binary_sha256'];G=E/'guard-joint-linux-r34-uwvm2.json';g=json.loads(G.read_text());assert not g['passed'] and g['actual_root_exit']==1
with qa.open_writer(A) as t:t.add(p,arcname=p.name,recursive=False)
seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name==p.name and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen=={p.name:h}
(D/'second-runtime-failed-binary-qualified.json').write_text(json.dumps(dict(passed=True,native_test_passed=False,archive=str(A),archive_sha256=sha(A),all_payloads_read_back=True,payloads=seen,original_qualification=Q,original_guard_sha256=sha(G),retired_path=str(p),bytes=p.stat().st_size),indent=2)+'\n');assert sha(p)==h;p.unlink();print('Negative native binary fully read back and own raw copy retired',flush=True)
