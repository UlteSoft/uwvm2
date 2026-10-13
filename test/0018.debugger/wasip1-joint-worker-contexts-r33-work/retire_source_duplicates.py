from pathlib import Path
import json,hashlib,tarfile
D=Path(__file__).parent;E=D.parent.parent;Q=json.loads((D/'historical-source-duplicate-qualified.json').read_text())
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert Q['passed'] and Q['memory_upper_bytes']<2<<30 and Q['no_new_local_archive_copy'] and [r['round'] for r in Q['rows']]==[31,32]
for r in Q['rows']:
 R=E/'rounds'/('wasip1-joint-worker-contexts-20261007-r'+str(r['round']));p=R/'inputs.tar.gz';assert str(p)==r['remote'] and p.is_file() and not p.is_symlink() and sha(p)==r['sha256'] and p.stat().st_size==r['bytes']
 assert r['local']=='/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0018.debugger/wasip1-joint-worker-contexts-r'+str(r['round'])+'-work/inputs.tar.gz'
 assert sha(R/'inputs.json')==r['manifest_sha256'];M=json.loads((R/'inputs.json').read_text());seen={}
 with tarfile.open(p,'r|gz') as t:
  for m in t:
   assert m.isfile() and m.name in M and m.name not in seen
   with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
 assert seen==M
proof=dict(passed=True,rows=Q['rows'],source_bytes_recoverable=True,all_payloads_read_back=True,retired_bytes=sum(r['bytes'] for r in Q['rows']),qualification_sha256=sha(D/'historical-source-duplicate-qualified.json'),cold_product_limit_unchanged=True)
(D/'historical-source-duplicate-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for r in Q['rows']:p=Path(r['remote']);assert sha(p)==r['sha256'];p.unlink()
print('Only exact duplicate historical source archives retired; readonly originals retained locally',proof['retired_bytes'],flush=True)
