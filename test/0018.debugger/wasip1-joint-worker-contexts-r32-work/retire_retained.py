from pathlib import Path
import json,hashlib,sys,subprocess,tarfile
D=Path(__file__).parent;sys.path.insert(0,str(D));import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
Q=json.loads((D/'foreign-probe3.json').read_text());assert Q['passed'] and Q['diagnostic_only'] and len(Q['rows'])==12
p=D/'foreign_probe3';assert sha(p)==Q['binary_sha256'];A=D/'foreign-probe3-recoverable.tar.zst';assert not A.exists()
with A.open('xb') as f:
 proc=subprocess.Popen([str(qa.tool()),'-q','-5','--long=29','-T1','-c'],stdin=subprocess.PIPE,stdout=f)
 try:
  with tarfile.open(fileobj=proc.stdin,mode='w|') as t:t.add(p,arcname=p.name,recursive=False)
  proc.stdin.close();assert proc.wait(timeout=300)==0
 finally:
  if proc.poll() is None:proc.kill();proc.wait()
with qa.open_reader(A) as t:
 members=[]
 for m in t:
  assert m.isfile() and m.name==p.name;members.append(m.name)
  with t.extractfile(m) as f:assert hashlib.file_digest(f,'sha256').hexdigest()==Q['binary_sha256']
assert members==[p.name]
old=json.loads((D/'first-failed-products-retirement.json').read_text());archive=Path(old['archive']);assert sha(archive)==old['archive_sha256'];seen={}
with qa.open_reader(archive) as t:
 for m in t:
  assert m.isfile() and m.name in old['payloads'] and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==old['payloads'];paths=[D/'products/linux-integrated/uwvm2/runtime.o',D/'products/linux-integrated/uwvm2/host-api.o']
for v in paths:assert sha(v)==old['payloads'][str(v.relative_to(D))]
proof=dict(passed=True,archive=str(A),archive_sha256=sha(A),all_payloads_read_back=True,payload_sha256=Q['binary_sha256'],retired_bytes=p.stat().st_size+sum(v.stat().st_size for v in paths),diagnostic_only=True,successful_diagnostics_unchanged=True,previous_objects_recovery=str(archive),previous_objects_recovery_sha256=sha(archive))
(D/'retained-products-retirement.json').write_text(json.dumps(proof,indent=2)+'\n');p.unlink()
for v in paths:v.unlink()
print('Owned old objects and passing diagnostic executable recoverably retired',proof['retired_bytes'],flush=True)
