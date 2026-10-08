from pathlib import Path
import hashlib,json,sys,subprocess,tarfile
D=Path(__file__).parent;sys.path.insert(0,str(D));import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
paths=[D/'foreign_probe',D/'foreign_probe2'];assert all(p.is_file() and not p.is_symlink() for p in paths)
q=json.loads((D/'foreign-probe.json').read_text());assert q['diagnostic_only'] and q['exit']==-4
q=json.loads((D/'foreign-probe2.json').read_text());assert q['diagnostic_only'] and not q['passed'] and q['rows'][0]['passed'] and q['rows'][1]['exit']==-4
M={p.name:sha(p) for p in paths};A=D/'foreign-probes-recoverable.tar.zst';assert not A.exists()
with A.open('xb') as f:
 proc=subprocess.Popen([str(qa.tool()),'-q','-5','--long=29','-T1','-c'],stdin=subprocess.PIPE,stdout=f)
 try:
  with tarfile.open(fileobj=proc.stdin,mode='w|') as t:
   for p in paths:t.add(p,arcname=p.name,recursive=False)
  proc.stdin.close();assert proc.wait(timeout=300)==0
 finally:
  if proc.poll() is None:proc.kill();proc.wait()
assert A.stat().st_size<256<<20;seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name in M and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==M
proof=dict(passed=True,archive=str(A),archive_sha256=sha(A),payloads=M,all_payloads_read_back=True,bytes_retired=sum(p.stat().st_size for p in paths),archive_bytes=A.stat().st_size,failed_diagnostics_unchanged=True,diagnostic_only=True)
(D/'foreign-probes-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for p in paths:assert sha(p)==M[p.name];p.unlink()
print('Only exact failed probe binaries recoverably retired',proof['bytes_retired'],proof['archive_bytes'],flush=True)
