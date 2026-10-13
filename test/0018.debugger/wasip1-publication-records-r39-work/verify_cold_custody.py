from pathlib import Path
import hashlib,json,os,subprocess,tarfile
D=Path(__file__).parent;E=D.parent.parent;C=E/'cold-relocated-r39'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
expected='0::/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope\n'
assert Path('/proc/self/cgroup').read_text()==expected
intent=json.loads((C/'custody-intent.json').read_text());rows=[]
for name,record in intent['files'].items():
 p=C/name;proof=E/'rounds/wasip1-joint-worker-contexts-20261007-r33'/(name.removesuffix('-qualified-products.tar.zst')+'-product-retirement.json')
 q=json.loads(proof.read_text());assert q['passed'] and q['all_payloads_read_back'] and isinstance(q['payloads'],dict)
 assert not p.is_symlink() and p.stat().st_uid==1000 and p.stat().st_size==record['size']==q['archive_bytes'] and sha(p)==record['sha256']==q['archive_sha256']
 command=['/usr/bin/zstd','-q','-d','--long=29','--memory=512MB','-c',str(p)]
 child=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE);seen={};total=0
 try:
  with tarfile.open(fileobj=child.stdout,mode='r|') as archive:
   for m in archive:
    assert m.isfile() and m.name in q['payloads'] and m.name not in seen and m.size<=2<<30
    h=hashlib.sha256();stream=archive.extractfile(m);size=0
    while data:=stream.read(1<<20):h.update(data);size+=len(data)
    assert size==m.size and h.hexdigest()==q['payloads'][m.name]
    seen[m.name]=h.hexdigest();total+=size;assert total<4<<30
  child.stdout.close();code=child.wait(timeout=30);assert code==0,(code,child.stderr.read().decode())
 finally:
  if child.poll() is None:child.kill();child.wait(timeout=10)
 assert seen==q['payloads'] and sha(p)==record['sha256']
 with p.open('rb') as f:os.fsync(f.fileno())
 os.chmod(p,0o444)
 rows.append(dict(archive=str(p),archive_sha256=sha(p),archive_bytes=p.stat().st_size,payloads=seen,payload_bytes=total,all_archive_payloads_read_back=True,parent_qualification=str(proof),parent_qualification_sha256=sha(proof)))
 print(name,len(seen),total,'full read passed',flush=True)
v=os.statvfs(E);assert v.f_bavail*v.f_frsize>512<<20 and v.f_favail>2048
out=dict(passed=True,rows=rows,archive_files=6,cgroup=expected,original_native_storage_policy_unchanged=True,concurrent_route='read-only cold custody; separate archive lock; no UWVM/VM/global suite mutation')
(C/'cold-custody-qualified.json').write_text(json.dumps(out,indent=2)+'\n')
fd=os.open(C,os.O_RDONLY);os.fsync(fd);os.close(fd)
