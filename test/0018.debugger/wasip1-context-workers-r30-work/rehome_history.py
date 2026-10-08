from pathlib import Path
import json,hashlib,subprocess,tarfile,resource
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-context-workers-20261007-r30';old=E/'rounds/wasip1-dispatch-20261007-r29'
C=Path('/Users/liyinan/Documents/MacroModel/wasip1-r29-owned-cold-evidence');new=C.with_name('wasip1-r30-owned-cold-evidence')
scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')];sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert C.stat().st_mode&0o222==0 and all(p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 for p in C.iterdir())
initial=sha(C/'qualification.json');previous=json.loads((C/'qualification.json').read_text());assert previous['passed'] and len(previous['archives'])==6
assert all(sha(C/name)==row['sha256'] for name,row in previous['archives'].items())
rows={}
for repo in ('uwvm2','uwvm2-ros'):
 p=L/('r29-'+repo+'-linux-retirement.json');subprocess.run([*scp,'macromodel@100.123.133.75:'+str(old/(repo+'-linux-integrated-product-retirement.json')),str(p)],check=True)
 q=json.loads(p.read_text());assert q['passed'] and q['all_payloads_read_back']
 name='r29-'+repo+'-linux-integrated-qualified-products.tar.gz';target=C/name
 assert not target.exists() and sum(p.stat().st_size for p in C.iterdir())+q['archive_bytes']<1<<30
 assert sum(p.stat().st_size for region in C.parent.glob('wasip1-r*-owned-cold-evidence') for p in region.iterdir() if p.is_file())+q['archive_bytes']<2<<30
 C.chmod(0o755)
 try:subprocess.run([*scp,'macromodel@100.123.133.75:'+q['archive'],str(target)],check=True)
 finally:C.chmod(0o555)
 assert sha(target)==q['archive_sha256'] and target.stat().st_size==q['archive_bytes']
 seen={}
 with tarfile.open(target,'r|gz') as archive:
  for m in archive:
   assert m.isfile() and m.name in q['payloads'] and m.name not in seen
   with archive.extractfile(m) as inp:seen[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
 assert seen==q['payloads'];target.chmod(0o444)
 rows[name]=dict(original_path=q['archive'],local_recovery_path=str(target),sha256=sha(target),bytes=target.stat().st_size,all_payloads_read_back=True,prior_retirement=str(old/(repo+'-linux-integrated-product-retirement.json')),prior_retirement_sha256=sha(p),payloads=q['payloads'])
assert sha(C/'qualification.json')==initial and all(sha(C/name)==row['sha256'] for name,row in previous['archives'].items())
upper=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20);assert upper<2<<30
proof=dict(passed=True,archives=rows,local_directory=str(C),limit_bytes=1<<30,region_bytes=sum(p.stat().st_size for p in C.iterdir()),aggregate_limit_bytes=2<<30,aggregate_bytes=sum(p.stat().st_size for region in C.parent.glob('wasip1-r*-owned-cold-evidence') for p in region.iterdir() if p.is_file()),memory_upper_bytes=upper,macos_limit_bytes=2<<30,original_six_archives_preserved=True,original_qualification_sha256=initial,original_qualification_unchanged=True,all_payloads_read_back=True)
(L/'historical-r29-local-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
subprocess.run([*scp,str(L/'historical-r29-local-qualified.json'),'macromodel@100.123.133.75:'+str(D)+'/'],check=True)
print('R29 Linux products recovered and fully read back inside existing 1 GiB region',proof['region_bytes'],flush=True)
