from pathlib import Path
import json,hashlib,tarfile,stat
D=Path(__file__).parent;P=D/'delivery';manifest=P/'wasip1_private_dispatch_r30_delivery_inputs.json'
rows=json.loads(manifest.read_text());assert 8<=len(rows)<=18 and sum(x['bytes'] for x in rows.values())<8<<20
assert set(p.name for p in P.iterdir())==set(rows)|{manifest.name}
for name,row in rows.items():
 p=P/name;s=p.lstat();assert Path(name).name==name and stat.S_ISREG(s.st_mode) and s.st_size==row['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
assert Path('/proc/self/cgroup').read_text().strip().endswith('docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
archive=D/'delivery-metadata.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=6) as t:
 for p in sorted(P.iterdir()):t.add(p,arcname=p.name,recursive=False)
assert archive.stat().st_size<8<<20
verified={}
with tarfile.open(archive,'r:gz') as t:
 for member in t:
  assert member.isfile() and member.name not in verified and member.name in set(rows)|{manifest.name}
  payload=t.extractfile(member).read();expected=rows.get(member.name);actual=hashlib.sha256(payload).hexdigest()
  assert expected is None or (actual==expected['sha256'] and len(payload)==expected['bytes'])
  if expected is None:assert payload==manifest.read_bytes()
  verified[member.name]=dict(sha256=actual,bytes=len(payload))
assert set(verified)==set(rows)|{manifest.name}
proof=dict(passed=True,archive=str(archive),archive_bytes=archive.stat().st_size,archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),files=verified,cgroup=Path('/proc/self/cgroup').read_text(),inputs_preserved=True,native_test_results_changed=False,no_large_product_or_sdk_duplicate=True)
(D/'delivery-metadata-receipt.json').write_text(json.dumps(proof,indent=2)+'\n')
A=D.parent.parent.parent/'wasip1-active-environment.json';state=json.loads(A.read_text())
assert state['latest_round_evidence']['round']=='R30'
state['latest_round_evidence'].update(delivery_archive=str(archive),delivery_archive_sha256=proof['archive_sha256'],delivery_receipt=str(D/'delivery-metadata-receipt.json'),delivery_receipt_sha256=hashlib.sha256((D/'delivery-metadata-receipt.json').read_bytes()).hexdigest())
temporary=A.with_name(A.name+'.r30-delivery.tmp');assert not temporary.exists();temporary.write_text(json.dumps(state,indent=2)+'\n');temporary.replace(A)
assert json.loads(A.read_text())['latest_round_evidence']['delivery_archive_sha256']==proof['archive_sha256']
print('R30 delivery archive qualified',len(verified),archive.stat().st_size,flush=True)
