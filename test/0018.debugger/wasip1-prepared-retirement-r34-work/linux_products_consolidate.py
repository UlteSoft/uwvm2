from pathlib import Path
from contextlib import ExitStack
from itertools import zip_longest
import copy,sys,json,hashlib
import qualified_archive as qa
D=Path(__file__).parent
assert sys.argv[1:]==['joint-linux-products-consolidate-r34','final','all']
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
groups=[[
 ('v4-uwvm2',D/'uwvm2-linux-integrated-v4-baseline-product-retirement.json'),
 ('v6-uwvm2',D/'historical-v6-products/uwvm2-linux-integrated-product-retirement.json'),
 ('v7-uwvm2',D/'uwvm2-linux-integrated-product-retirement.json')],[
 ('v6-ros',D/'historical-v6-products/uwvm2-ros-linux-integrated-product-retirement.json'),
 ('v7-ros',D/'uwvm2-ros-linux-integrated-product-retirement.json')]]
inputs=[];expected={}
for group in groups:
 for label,p in group:
  q=json.loads(p.read_text());a=D/'historical-v6-products'/Path(q['archive']).name if label.startswith('v6-') else Path(q['archive'])
  assert q['passed'] and q['all_payloads_read_back'] and sha(a)==q['archive_sha256']
  info=dict(label=label,receipt=str(p),receipt_sha256=sha(p),original_archive=str(a),original_archive_sha256=sha(a),original_archive_bytes=a.stat().st_size,payloads=q['payloads'])
  inputs.append(info)
  for n,h in q['payloads'].items():expected[label+'/'+n]=h
A=D/'linux-v4-v6-v7-qualified-products-consolidated.tar.zst';assert not A.exists()
with qa.open_writer(A) as target:
 target.dereference=True
 for group in groups:
  infos=[next(i for i in inputs if i['label']==label) for label,p in group]
  with ExitStack() as stack:
   readers=[stack.enter_context(qa.open_reader(i['original_archive'])) for i in infos]
   for members in zip_longest(*(iter(a) for a in readers)):
    for member,reader,info in zip(members,readers,infos):
     if member is None:continue
     assert member.isfile() and member.name in info['payloads']
     copied=copy.copy(member);copied.name=info['label']+'/'+member.name;copied.pax_headers=dict(member.pax_headers)
     copied.pax_headers.pop('path',None)
     with reader.extractfile(member) as f:target.addfile(copied,f)
seen={}
with qa.open_reader(A) as a:
 for m in a:
  assert m.isfile() and m.name in expected and m.name not in seen
  with a.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==expected
q=dict(passed=True,all_payloads_read_back=True,archive=str(A),archive_sha256=sha(A),archive_bytes=A.stat().st_size,payloads=expected,inputs=inputs,native_execution_claimed=False,original_test_limits_unchanged=True)
(D/'linux-products-consolidation-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
for info in inputs:
 p=Path(info['original_archive']);assert sha(p)==info['original_archive_sha256'];p.unlink()
print('All original object/executable/qualification bytes read back before retiring five separate containers',sum(i['original_archive_bytes'] for i in inputs),q['archive_bytes'],flush=True)
