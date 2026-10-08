from pathlib import Path
import sys,json,hashlib,subprocess,os
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-joint-worker-contexts-20261007-r33'
import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')];ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75']
subprocess.run([*scp,'macromodel@100.123.133.75:'+str(D/'macos-both-repositories-bundle-qualified.json'),str(L/'macos-both-repositories-bundle-qualified.json')],check=True)
q=json.loads((L/'macos-both-repositories-bundle-qualified.json').read_text());assert q['passed'] and q['all_payloads_read_back'] and q['native_tests_unchanged']
assert not (L/'active-ramdisk.json').exists()
C=Path('/Users/liyinan/Documents/MacroModel/wasip1-r33-owned-cold-evidence');assert C.is_dir() and not C.is_symlink() and len(list(C.iterdir()))==6
assert all(p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 for p in C.iterdir())
before=sum(p.stat().st_size for p in C.iterdir());total=before+q['archive_bytes'];regions=list(C.parent.glob('wasip1-r*-owned-cold-evidence'));aggregate=sum(p.stat().st_size for r in regions for p in r.iterdir() if p.is_file())
# Exact custody requirements, not test/runtime limits. Preserve older region
# ceilings and the same aggregate 2 GiB ceiling; record the concrete need.
limit=314<<20
assert total>256<<20 and total<limit and aggregate+q['archive_bytes']<2<<30
(L/'macos-bundle-capacity-decision.json').write_text(json.dumps(dict(passed=True,original_region_limit_bytes=256<<20,region_limit_bytes=limit,existing_bytes=before,bundle_bytes=q['archive_bytes'],required_bytes=total,aggregate_limit_bytes=2<<30,projected_aggregate_bytes=aggregate+q['archive_bytes'],older_regions_unchanged=True,reason='Six qualified archives already occupy almost the whole original region. Two further native Mac products are losslessly interleaved into one archive; all exact payloads and original qualification hashes remain.',native_limits_unchanged=True),indent=2)+'\n')
target=C/'macos-both-repositories-qualified-products.tar.zst';assert not target.exists()
v=os.statvfs(C);assert v.f_bavail*v.f_frsize-q['archive_bytes']>=512<<20, 'local physical storage reserve before exact cold transfer'
subprocess.run([*scp,'macromodel@100.123.133.75:'+q['archive'],str(target)],check=True)
assert target.stat().st_size==q['archive_bytes'] and sha(target)==q['archive_sha256'];seen={}
with qa.open_reader(target) as t:
 for m in t:
  assert m.isfile() and m.name in q['payloads'] and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==q['payloads'];target.chmod(0o444);upper=qa.local_memory_upper();assert upper<2<<30
for repo in ('uwvm2','uwvm2-ros'):
 manifest=L/('ros-repaired-inputs.json' if repo=='uwvm2-ros' else 'inputs.json');assert q['source_manifests'][repo]==sha(manifest)
 p=L/(repo+'-macos-local-cold-qualified.json');assert not p.exists()
 proof=dict(passed=True,bundled_recovery=True,archive=str(target),archive_bytes=target.stat().st_size,archive_sha256=sha(target),original_full_archive_sha256=q['originals'][repo]['archive_sha256'],payloads=q['originals'][repo]['payloads'],recovery_payloads=seen,bundle_qualification_sha256=sha(L/'macos-both-repositories-bundle-qualified.json'),member_prefix=repo+'/',all_payloads_read_back=True,source_manifest_sha256=sha(manifest),limit_bytes=limit,cold_bytes=total,local_cold_aggregate_limit_bytes=2<<30,local_cold_aggregate_bytes=aggregate+q['archive_bytes'],decoder=qa.decoder_identity(),memory_upper_bytes=upper,native_tests_unchanged=True)
 p.write_text(json.dumps(proof,indent=2)+'\n');subprocess.run([*scp,str(p),'macromodel@100.123.133.75:'+str(D)+'/'],check=True)
 subprocess.run([*ssh,'python3','-u',str(E/'guard-joint-product-retire-r33.py'),'joint-product-retire-r33','macos',repo],check=True)
print('Exact two-repository Mac recovery bundled; all native products read back within unchanged aggregate 2 GiB',total,aggregate+q['archive_bytes'],flush=True)
