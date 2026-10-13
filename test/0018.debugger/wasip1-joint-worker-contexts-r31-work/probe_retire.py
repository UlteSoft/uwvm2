from pathlib import Path
import json,sys,hashlib
D=Path(__file__).parent;sys.path.insert(0,str(D));import qualified_archive as qa
p=D/'chain_probe';q=json.loads((D/'chain-probe.json').read_text());assert q['diagnostic_only'] and q['exit']==-4
sha=lambda p:hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
h=sha(p);target=D/'chain-probe-recoverable.tar.zst';assert not target.exists()
with qa.open_writer(target) as t:t.add(p,arcname='chain_probe',recursive=False)
with qa.open_reader(target) as t:
 members=[]
 for m in t:
  assert m.isfile() and m.name=='chain_probe';members.append(m.name)
  with t.extractfile(m) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==h
assert members==['chain_probe']
proof=dict(passed=True,archive=str(target),archive_sha256=sha(target),payload_sha256=h,bytes=p.stat().st_size,diagnostic_only=True,failed_test_result_unchanged=True)
(D/'chain-probe-retirement.json').write_text(json.dumps(proof,indent=2)+'\n');assert sha(p)==h;p.unlink()
print('own diagnostic executable recoverably retired',proof['bytes'],flush=True)
