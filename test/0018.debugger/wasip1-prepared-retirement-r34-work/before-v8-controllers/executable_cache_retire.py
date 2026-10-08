from pathlib import Path
import sys,json,hashlib
import qualified_archive as qa
D=Path(__file__).parent;E=D.parent.parent;assert sys.argv[1]=='joint-executable-cache-retire-r34';platform,repo=sys.argv[2:4];assert platform in ('windows','freebsd') and repo in ('uwvm2','uwvm2-ros')
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();O=D/'products-v7'/platform/repo;M=D/'repaired-inputs-v7.json';Q=json.loads((O/'qualified.json').read_text());G=E/('guard-joint-cross-r34-'+platform+'-'+repo+'.json');g=json.loads(G.read_text());assert Q['passed'] and Q['source_manifest_sha256']==sha(M) and g['passed']
rows=[];members={}
for item in Q['fixtures'].values():
 p=O/item['binary'];h=sha(p);assert h==item['binary_sha256'] and p.stat().st_size<256<<20;members[p.name]=h;rows.append(dict(path=str(p),bytes=p.stat().st_size,sha256=h,archive_member=p.name))
assert sum(r['bytes'] for r in rows)<1<<30
A=D/(repo+'-'+platform+'-link-executable-cache.tar.zst')
with qa.open_writer(A) as t:
 for r in rows:t.add(r['path'],arcname=r['archive_member'],recursive=False)
seen={}
with qa.open_reader(A) as t:
 for m in t:
  assert m.isfile() and m.name in members and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==members
q=dict(passed=True,all_payloads_read_back=True,archive=str(A),archive_sha256=sha(A),archive_bytes=A.stat().st_size,payloads=members,retired_raw=rows,source_manifest_sha256=sha(M),compiled_guard_sha256=sha(G),native_execution_claimed=False,original_resource_limits_unchanged=True,cgroup=Path('/proc/self/cgroup').read_text());(O/'link-executable-cache-retirement.json').write_text(json.dumps(q,indent=2)+'\n');(D/(repo+'-'+platform+'-executable-cache-retirement.json')).write_text(json.dumps(q,indent=2)+'\n')
for r in rows:assert sha(r['path'])==r['sha256'];Path(r['path']).unlink()
print('All actual target executables fully read back before retiring raw disk copies',platform,repo,sum(r['bytes'] for r in rows),q['archive_bytes'],flush=True)
