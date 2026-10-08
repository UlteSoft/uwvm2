from pathlib import Path
import hashlib,json,os,subprocess,tarfile
D=Path(__file__).parent;E=D.parent.parent;C=E/'cold-relocated-r38'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
expected='0::/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope\n'
assert Path('/proc/self/cgroup').read_text()==expected
proof=E/'rounds/wasip1-joint-worker-contexts-20261007-r33/macos-both-repositories-bundle-qualified.json'
q=json.loads(proof.read_text());p=C/'macos-both-repositories-qualified-products.tar.zst.part'
assert not p.is_symlink() and p.stat().st_uid==1000 and p.stat().st_size==q['archive_bytes'] and sha(p)==q['archive_sha256']
intent=json.loads((C/'custody-intent.json').read_text());assert q['archive_sha256']==intent['expected_archive_sha256']
command=['/usr/bin/zstd','-q','-d','--long=29','--memory=512MB','-c',str(p)]
reader=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
seen={};total=0
try:
 with tarfile.open(fileobj=reader.stdout,mode='r|') as archive:
  for m in archive:
   assert m.isfile() and m.name in q['payloads'] and m.name not in seen and m.size<=2<<30
   h=hashlib.sha256();payload=archive.extractfile(m);size=0
   while data:=payload.read(1<<20):h.update(data);size+=len(data)
   assert size==m.size and h.hexdigest()==q['payloads'][m.name]
   seen[m.name]=h.hexdigest();total+=size;assert total<2<<30
 reader.stdout.close();code=reader.wait(timeout=30);assert code==0,(code,reader.stderr.read().decode())
finally:
 if reader.poll() is None:reader.kill();reader.wait(timeout=10)
assert seen==q['payloads'] and sha(p)==q['archive_sha256']
out=C/p.name.removesuffix('.part');assert not out.exists()
with p.open('rb') as f:os.fsync(f.fileno())
os.replace(p,out);fd=os.open(C,os.O_RDONLY);os.fsync(fd);os.close(fd)
v=os.statvfs(E);assert v.f_bavail*v.f_frsize>512<<20 and v.f_favail>2048
receipt=dict(passed=True,archive=str(out),archive_sha256=sha(out),archive_bytes=out.stat().st_size,all_archive_payloads_read_back=True,
 payloads=seen,payload_bytes=total,parent_qualification=str(proof),parent_qualification_sha256=sha(proof),original_native_storage_policy_unchanged=True,
 memory_decoder_window_bytes=512<<20,command=command,cgroup=expected,local_source=intent['local_source'])
(C/'macos-custody-qualified.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('archive full-read verified',len(seen),total,flush=True)
