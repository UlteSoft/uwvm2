from pathlib import Path
import sys,json,hashlib,tarfile,importlib.util
D=Path(__file__).parent;E=D.parent.parent;platform,repo=sys.argv[2:4]
assert sys.argv[1]=='joint-archive-r33' and platform in ('linux','linux-integrated','windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
O=D/'products'/platform/repo;assert O.is_dir()
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
import sys as _sys
_sys.path.insert(0,str(Path(__file__).parent))
import qualified_archive as qa
spec=importlib.util.spec_from_file_location('archive_budget',E/'rounds/wasip1-cross-jit-20261007-r26/archive_budget.py');budget=importlib.util.module_from_spec(spec);spec.loader.exec_module(budget)
if platform=='linux-integrated':
 import re,shlex,os
 logs={}
 for policy in ('instruction','unwind'):
  p=O/('environment-group-'+policy+'.log');text=p.read_text();found=re.search(r'^debug_wasip1_environment_group_runtime PASS checks=([0-9]+) policy='+policy+r'$',text,re.M)
  assert found and 'PRIVATE_WASIP1_WORLD status=0 environments=2 modules=3 memories=3 shared=1' in text
  logs[policy]=dict(checks=int(found[1]),log_sha256=sha(p))
 assert logs['instruction']['checks']==logs['unwind']['checks']
 dependencies={str(Path(os.path.normpath(v))):sha(Path(os.path.normpath(v))) for v in shlex.split((O/'environment-group.d').read_text().replace('\\\n',' ').split(':',1)[1])}
 (O/'environment-group-qualified.json').write_text(json.dumps(dict(passed=True,binary_sha256=sha(O/'environment-group'),source_manifest_sha256=sha(D/'inputs.json'),dependencies=dependencies,actual_native_execution=True,native_cases=logs),indent=2)+'\n')
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
 assert native['passed'] and native['binary_sha256']==q['binary_sha256'] and native['group_binary_sha256']==q['group_binary_sha256'] and len(native['rows'])==8
 assert native['source_manifest_sha256']==sha(D/'inputs.json') and all(r['exit']==0 and r['passed'] and r['aggregate_peak_upper_bytes']<2<<30 for r in native['rows'])
 (O/'native-evidence-import-qualified.json').write_text(json.dumps(dict(passed=True,transport_sha256=sha(transport),all_payloads_read_back=True,receipt_sha256=sha(O/'receipt.json')),indent=2)+'\n')
members={str(p.relative_to(O)):sha(p) for p in O.rglob('*') if p.is_file() and not p.is_symlink()}
assert members
raw=[p for p in O.rglob('*') if p.is_file() and (p.suffix in ('.o','.exe') or p.name in ('fixture','qualified-fixture','debug_checkpoint_complete_instance','debug_checkpoint_complete_preload','environment-group','wasip1-memory-binding','foreign-observation'))]
assert raw
target=D/(repo+'-'+platform+'-qualified-products.tar.zst')
with qa.open_writer(target) as t:
 for name in members:
  p=O/name;assert sha(p)==members[name];t.add(p,arcname=name,recursive=False)
observed={}
with qa.open_reader(target) as t:
 for member in t:
  assert member.isfile() and member.name in members
  with t.extractfile(member) as source:h=hashlib.file_digest(source,'sha256').hexdigest()
  assert h==members[member.name];observed[member.name]=h
assert observed==members
rows=[]
for p in raw:
 key=str(p.relative_to(O));assert sha(p)==members[key];rows.append(dict(path=str(p),bytes=p.stat().st_size,sha256=members[key],archive_member=key))
receipt=dict(passed=True,all_payloads_read_back=True,archive=str(target),archive_bytes=target.stat().st_size,archive_sha256=sha(target),payloads=members,retired_raw=rows,encoder=qa.decoder_identity(),archive_format='tar.zst-long-window-bounded')
(D/(repo+'-'+platform+'-product-retirement.json')).write_text(json.dumps(receipt,indent=2)+'\n')
for row in rows:Path(row['path']).unlink()
print('qualified archive read back and own raw products retired',platform,repo,sum(r['bytes'] for r in rows),flush=True)

