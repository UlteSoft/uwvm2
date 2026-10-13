from pathlib import Path
import sys,json,hashlib,tarfile,importlib.util
D=Path(__file__).parent;E=D.parent.parent;platform,repo=sys.argv[2:4]
assert sys.argv[1]=='joint-archive-r34' and platform in ('linux','linux-integrated','windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
input_manifest=D/'repaired-inputs-v6.json'
O=D/'products'/platform/repo;assert O.is_dir()
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
import sys as _sys
_sys.path.insert(0,str(Path(__file__).parent))
import qualified_archive as qa
spec=importlib.util.spec_from_file_location('archive_budget',E/'rounds/wasip1-cross-jit-20261007-r26/archive_budget.py');budget=importlib.util.module_from_spec(spec);spec.loader.exec_module(budget)
if platform=='linux-integrated':
 import re
 results=json.loads((O/'results.json').read_text());assert len(results)==15 and all(r['passed'] and r['exit']==0 and sha(O/(r['name']+'.log'))==r['log_sha256'] for r in results)
 native=[]
 for fixture in ('debug_checkpoint_prepared_retirement','debug_checkpoint_native_cohort_retirement','debug_wasip1_prepared_retirement'):
  for policy in ('instruction','unwind'):
   p=O/(fixture+'-'+policy+'.log');text=p.read_text();assert 'whole_restore=0 PASS' in text
   found=re.search(r'checks=([0-9]+)',text);native.append(dict(fixture=fixture,policy=policy,passed=True,checks=int(found[1]) if found else None,log_sha256=sha(p)))
 (O/'native-execution.json').write_text(json.dumps(dict(passed=True,rows=native,counted_assertions=sum(r['checks'] or 0 for r in native)),indent=2)+'\n')
if platform=='macos':
 from pathlib import PurePosixPath
 imported=D/(repo+'-macos-native-evidence.json');transport=D/(repo+'-macos-native-evidence.tar.gz');payload=json.loads(imported.read_text())
 assert payload['passed'] and sha(transport)==payload['archive_sha256']
 seen={}
 with tarfile.open(str(transport),'r|gz') as t:
  for m in t:
   n=PurePosixPath(m.name);assert m.isfile() and not n.is_absolute() and '..' not in n.parts and str(n)==m.name and m.name in payload['payloads'] and m.size<8<<20
   with t.extractfile(m) as f:data=f.read()
   h=hashlib.sha256(data).hexdigest();assert h==payload['payloads'][m.name]
   path=O/m.name;path.parent.mkdir(parents=True,exist_ok=True)
   if path.exists():assert path.is_file() and not path.is_symlink() and sha(path)==h
   else:
    with path.open('xb') as out:out.write(data)
   seen[m.name]=h
 assert seen==payload['payloads']
 native=json.loads((O/'receipt.json').read_text());q=json.loads((O/'qualified.json').read_text())
 assert native['passed'] and native['input_sha256']=={**{i['binary']:i['binary_sha256'] for i in q['fixtures'].values()},**{i['wasm']:i['wasm_sha256'] for i in q['fixtures'].values()}} and len(native['rows'])==6
 assert native['source_manifest_sha256']==sha(input_manifest) and all(r['exit']==0 and r['passed'] and r['aggregate_peak_upper_bytes']<2<<30 for r in native['rows'])
 (O/'native-evidence-import-qualified.json').write_text(json.dumps(dict(passed=True,transport_sha256=sha(transport),all_payloads_read_back=True,receipt_sha256=sha(O/'receipt.json')),indent=2)+'\n')
members={str(p.relative_to(O)):sha(p) for p in O.rglob('*') if p.is_file() and not p.is_symlink()}
assert members
caches=[]
for receipt_name in ('link-object-cache-retirement.json','link-executable-cache-retirement.json'):
 p=O/receipt_name
 if not p.exists():continue
 cache=json.loads(p.read_text());assert platform in ('windows','freebsd') and cache['passed'] and cache['all_payloads_read_back'] and cache['source_manifest_sha256']==sha(input_manifest) and sha(cache['archive'])==cache['archive_sha256']
 for name,h in cache['payloads'].items():
  assert not (O/name).exists() and name not in members;members[name]=h
 caches.append((p,cache))
raw=[p for p in O.rglob('*') if p.is_file() and (p.suffix in ('.o','.exe') or p.name in ('debug_checkpoint_prepared_retirement','debug_checkpoint_native_cohort_retirement','debug_wasip1_prepared_retirement'))]
assert raw or caches
target=D/(repo+'-'+platform+'-qualified-products.tar.zst')
with qa.open_writer(target) as t:
 for name in members:
  p=O/name
  if any(name in c['payloads'] for _,c in caches):continue
  assert sha(p)==members[name];t.add(p,arcname=name,recursive=False)
 for cache_receipt,cache in caches:
  restored={}
  with qa.open_reader(cache['archive']) as archived:
   for member in archived:
    assert member.isfile() and member.name in cache['payloads'] and member.name not in restored
    with archived.extractfile(member) as source:t.addfile(member,source)
    restored[member.name]=cache['payloads'][member.name]
  assert restored==cache['payloads']
observed={}
with qa.open_reader(target) as t:
 for member in t:
  assert member.isfile() and member.name in members and member.name not in observed
  with t.extractfile(member) as source:h=hashlib.file_digest(source,'sha256').hexdigest()
  assert h==members[member.name];observed[member.name]=h
assert observed==members
rows=[]
for p in raw:
 key=str(p.relative_to(O));assert sha(p)==members[key];rows.append(dict(path=str(p),bytes=p.stat().st_size,sha256=members[key],archive_member=key))
receipt=dict(passed=True,all_payloads_read_back=True,archive=str(target),archive_bytes=target.stat().st_size,archive_sha256=sha(target),payloads=members,retired_raw=rows,encoder=qa.decoder_identity(),archive_format='tar.zst-long-window-bounded',source_manifest_sha256=sha(input_manifest),recovery_custody='retained in bounded original Linux task volume; no new local cold capacity allocated')
(D/(repo+'-'+platform+'-product-retirement.json')).write_text(json.dumps(receipt,indent=2)+'\n')
for row in rows:Path(row['path']).unlink()
for cache_receipt,cache in caches:
 assert sha(cache['archive'])==cache['archive_sha256']
 (D/(repo+'-'+platform+'-'+cache_receipt.stem+'-final-retirement.json')).write_text(json.dumps(dict(passed=True,final_archive_sha256=sha(target),all_payloads_read_back=True,payloads=cache['payloads'],cache_archive_sha256=cache['archive_sha256'],cache_receipt_sha256=sha(cache_receipt),cache_archive_bytes=cache['archive_bytes']),indent=2)+'\n');Path(cache['archive']).unlink()
print('qualified archive read back and own raw products retired',platform,repo,sum(r['bytes'] for r in rows),flush=True)

