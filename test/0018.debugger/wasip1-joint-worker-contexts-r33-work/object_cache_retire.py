from pathlib import Path
import sys,json,hashlib
D=Path(__file__).parent;E=D.parent.parent
assert sys.argv[1]=='joint-object-cache-retire-r33' and sys.argv[2] in ('windows','freebsd') and sys.argv[3] in ('uwvm2','uwvm2-ros')
platform,repo=sys.argv[2:4];O=D/'products'/platform/repo
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
import qualified_archive as qa
manifest=D/('ros-repaired-inputs.json' if repo=='uwvm2-ros' else 'inputs.json')
compiled=json.loads((O/'qualified.json').read_text());guard=E/('guard-joint-cross-r33-'+platform+'-'+repo+'.json');G=json.loads(guard.read_text())
assert compiled['passed'] and compiled['source_manifest_sha256']==sha(manifest) and G['passed'] and G['actual_root_exit']==0 and all(r['pidfd_retired'] for r in G['retirement'])
assert G['limits']['memory.max']=='68719476736' and G['limits']['memory.swap.max']=='0'
members={};rows=[]
for label,field in (('runtime','runtime_sha256'),('host-api','host_api_sha256')):
 p=O/(label+'.o');q=json.loads((O/(label+'-qualified.json')).read_text());h=sha(p)
 assert p.is_file() and not p.is_symlink() and h==compiled[field]==q['object_sha256'] and all(sha(k)==v for k,v in q['dependencies'].items())
 members[p.name]=h;rows.append(dict(path=str(p),bytes=p.stat().st_size,sha256=h,archive_member=p.name))
archive=D/(repo+'-'+platform+'-link-object-cache.tar.zst');assert not archive.exists()
with qa.open_writer(archive) as t:
 for row in rows:t.add(row['path'],arcname=row['archive_member'],recursive=False)
seen={}
with qa.open_reader(archive) as t:
 for m in t:
  assert m.isfile() and m.name in members and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==members
proof=dict(passed=True,all_payloads_read_back=True,archive=str(archive),archive_bytes=archive.stat().st_size,archive_sha256=sha(archive),payloads=members,source_manifest_sha256=sha(manifest),compiled_guard_sha256=sha(guard),retired_raw=rows,native_execution_claimed=False,ordinary_test_limits_unchanged=True,cgroup=Path('/proc/self/cgroup').read_text())
(D/(repo+'-'+platform+'-object-cache-retirement.json')).write_text(json.dumps(proof,indent=2)+'\n');(O/'link-object-cache-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for row in rows:
 assert sha(row['path'])==row['sha256'];Path(row['path']).unlink()
print('Actual qualified linked objects archived and fully read back before retiring raw copies',repo,sum(r['bytes'] for r in rows),archive.stat().st_size,flush=True)
