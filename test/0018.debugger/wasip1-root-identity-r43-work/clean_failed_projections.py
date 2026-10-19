from pathlib import Path
import hashlib,json,os,shutil
D=Path(__file__).parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
g=json.loads((D/'guard-matrix-v4.json').read_text());assert g['passed'] and g['ram_disposable_products_retired'] and all(x['pidfd_retired'] for x in g['processes'])
q=json.loads((D/'qualified-v4.json').read_text());assert q['passed'] and q['fresh_native_cases']==36
retired=[]
for version in (1,2):
 root=D/('source-v'+str(version));manifest=json.loads((D/('source-manifest-v'+str(version)+'.json')).read_text())
 guard=json.loads((D/('guard-matrix-v'+str(version)+'.json')).read_text())
 assert not guard['passed'] and all(x['pidfd_retired'] for x in guard['processes']) and guard['ram_disposable_products_retired']
 assert root.is_dir() and not root.is_symlink() and root.parent==D
 assert all(sha(root/k)==h for k,h in manifest.items())
 identity=root.stat();regular=[]
 for p in root.rglob('*'):
  if p.is_symlink():continue
  if p.is_file():
   assert p.stat().st_uid==1000
   regular.append(dict(path=str(p.relative_to(root)),size=p.stat().st_size,allocated=p.stat().st_blocks*512,sha256=sha(p)))
 assert identity.st_uid==1000
 retired.append(dict(path=str(root),device=identity.st_dev,inode=identity.st_ino,uid=identity.st_uid,
  source_manifest_sha256=sha(D/('source-manifest-v'+str(version)+'.json')),regular_files=regular,
  retired_allocated_bytes=sum(p['allocated'] for p in regular),
  complete_original_delta_retained=sha(D/('delta-v'+str(version)+'.tar.gz')),
  all_attempt_processes_physically_dead=True))
 shutil.rmtree(root);assert not root.exists()
assert all(sha(D/'source-v3'/k)==h for k,h in json.loads((D/'source-manifest-v4.json').read_text()).items())
(D/'owned-failed-source-cleanup.json').write_text(json.dumps(dict(passed=True,retired=retired,only_this_round_unused_failed_projections_removed=True,
 total_retired_allocated_bytes=sum(x['retired_allocated_bytes'] for x in retired),
 final_source_projection_retained=True,original_source_delta_and_logs_retained=True),indent=2)+'\n')
print('retired owned redundant source bytes',sum(x['retired_allocated_bytes'] for x in retired))
