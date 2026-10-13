from pathlib import Path
import json,hashlib,sys
import qualified_archive as qa
D=Path(__file__).parent;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert sys.argv[1]=='joint-negative-bundle-r34'
items=[('first','first-runtime-failed-products-qualified.json'),('second','second-runtime-failed-binary-qualified.json'),('third','third-runtime-failed-binary-qualified.json')];payloads={};originals={}
for prefix,name in items:
 q=json.loads((D/name).read_text());assert q['passed'] and not q['native_test_passed'] and q['all_payloads_read_back'] and sha(q['archive'])==q['archive_sha256'];originals[prefix]=dict(archive=q['archive'],archive_sha256=q['archive_sha256'],payloads=q['payloads'],qualification_sha256=sha(D/name))
 for n,h in q['payloads'].items():payloads[prefix+'/'+n]=h
A=D/'negative-native-products-recovery.tar.zst';assert not A.exists()
with qa.open_writer(A) as t:
 for prefix,name in items:
  q=json.loads((D/name).read_text());seen={}
  with qa.open_reader(q['archive']) as archived:
   for m in archived:
    assert m.isfile() and m.name in q['payloads'] and m.name not in seen;original=m.name;m.name=prefix+'/'+original
    with archived.extractfile(m) as inp:t.addfile(m,inp)
    seen[original]=q['payloads'][original]
  assert seen==q['payloads']
seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name in payloads and m.name not in seen
  with t.extractfile(m) as inp:seen[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
assert seen==payloads
q=dict(passed=True,all_payloads_read_back=True,negative_tests_counted_as_passed=False,archive=str(A),archive_sha256=sha(A),archive_bytes=A.stat().st_size,payloads=payloads,originals=originals,cgroup=Path('/proc/self/cgroup').read_text());(D/'negative-native-products-bundle-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
for prefix,item in originals.items():assert sha(item['archive'])==item['archive_sha256'];Path(item['archive']).unlink()
print('Actual failed native products recovered losslessly in one bounded archive',q['archive_bytes'],flush=True)
