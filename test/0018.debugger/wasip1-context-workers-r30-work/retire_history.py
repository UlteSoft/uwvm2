from pathlib import Path
import sys,json,hashlib,os,time
D=Path(__file__).parent;E=D.parent.parent;old=D.parent/'wasip1-dispatch-20261007-r29'
assert sys.argv[1:] == ['joint-rehome-r29-r30']
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
Q=json.loads((D/'historical-r29-local-qualified.json').read_text());assert Q['passed'] and Q['all_payloads_read_back'] and Q['original_qualification_unchanged']
assert Q['region_bytes']<Q['limit_bytes']==1<<30 and Q['aggregate_bytes']<Q['aggregate_limit_bytes']==2<<30 and Q['memory_upper_bytes']<Q['macos_limit_bytes']==2<<30
assert len(Q['archives'])==2;rows=[];identities=set()
for repo in ('uwvm2','uwvm2-ros'):
 p=old/(repo+'-linux-integrated-qualified-products.tar.gz');name='r29-'+p.name;row=Q['archives'][name]
 assert p.is_file() and not p.is_symlink() and str(p)==row['original_path'] and sha(p)==row['sha256'] and p.stat().st_size==row['bytes']
 receipt=old/(repo+'-linux-integrated-product-retirement.json');R=json.loads(receipt.read_text());assert sha(receipt)==row['prior_retirement_sha256'] and R['passed'] and R['archive_sha256']==row['sha256'] and R['payloads']==row['payloads']
 identities.add((p.stat().st_dev,p.stat().st_ino));rows.append(row)
cg=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope');assert Path('/proc/self/cgroup').read_text().strip()=='0::/'+str(cg).removeprefix('/sys/fs/cgroup/')
for pid in (cg/'cgroup.procs').read_text().split():
 try:
  for fd in (Path('/proc')/pid/'fd').iterdir():
   try:s=fd.stat()
   except FileNotFoundError:continue
   assert (s.st_dev,s.st_ino) not in identities,('owned archive in use; retain',pid,str(fd))
 except (FileNotFoundError,ProcessLookupError):continue
proof=dict(passed=True,timestamp_ns=time.time_ns(),retired=rows,bytes=sum(r['bytes'] for r in rows),local_qualification_sha256=sha(D/'historical-r29-local-qualified.json'),cgroup=Path('/proc/self/cgroup').read_text(),foreign_processes_signalled=False,scope='Two own fully recovered R29 Linux compiled-product archives only; no test/VM/compiler admission')
(D/'historical-r29-hot-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for row in rows:Path(row['original_path']).unlink()
A=E.parent/'wasip1-active-environment.json';state=json.loads(A.read_text());state['r30_recovered_r29_linux_products']=dict(proof=str(D/'historical-r29-hot-retirement.json'),proof_sha256=sha(D/'historical-r29-hot-retirement.json'),qualification=str(D/'historical-r29-local-qualified.json'),qualification_sha256=sha(D/'historical-r29-local-qualified.json'),local_directory=Q['local_directory'],checked_region_limit_bytes=1<<30,checked_local_aggregate_limit_bytes=2<<30)
tmp=A.with_name(A.name+'.r30-history.tmp');assert not tmp.exists();tmp.write_text(json.dumps(state,indent=2)+'\n');os.replace(tmp,A)
print('retired two authenticated own R29 Linux compressed copies; preserved recovery',proof['bytes'],flush=True)
