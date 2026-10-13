from pathlib import Path
import json,hashlib,tarfile,os,shutil,sys
D=Path(__file__).parent;E=D.parent.parent;C=E.parent/'wasip1-r27-owned-cold-evidence'
assert sys.argv[1]=='joint-cold-product-r28' and len(sys.argv)==4
platform,repo=sys.argv[2:];assert platform in ('linux-integrated','windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
p=D/(repo+'-'+platform+'-product-retirement.json');r=json.loads(p.read_text());source=Path(r['archive']);target=C/('r28-'+source.name)
assert r['passed'] and r['all_payloads_read_back'] and sha(source)==r['archive_sha256']
assert source.parent==D and source.is_file() and not source.is_symlink() and not target.exists()
q=json.loads((C/'qualification.json').read_text());assert q['passed'] and q['limit_bytes']==2<<30
before=sum(p.stat().st_size for p in C.iterdir());assert before+source.stat().st_size+(1<<20)<q['limit_bytes']
with tarfile.open(source,'r|gz') as t:
 seen={}
 for m in t:
  assert m.isfile() and m.name in r['payloads']
  with t.extractfile(m) as f:h=hashlib.file_digest(f,'sha256').hexdigest()
  assert h==r['payloads'][m.name];seen[m.name]=h
assert seen==r['payloads']
os.chmod(C,0o700)
try:
 with source.open('rb') as f,target.open('xb') as out:shutil.copyfileobj(f,out,1048576)
 assert sha(target)==r['archive_sha256'];os.chmod(target,0o444)
 link=D/('.'+source.name+'.cold-link');assert not link.exists();link.symlink_to(target);os.replace(link,source)
 assert source.is_symlink() and source.resolve()==target and sha(source)==r['archive_sha256']
 proof_path=D/(repo+'-'+platform+'-cold-relocation.json')
 proof=dict(passed=True,scope='Only this task newly qualified compressed native products; exact whole archive and every payload read back before atomic source-path symlink replacement',source=str(source),target=str(target),archive_sha256=r['archive_sha256'],all_payloads_read_back=True,retirement_sha256=sha(p),released_hot_bytes=target.stat().st_size,cold_limit_bytes=q['limit_bytes'])
 proof_path.write_text(json.dumps(proof,indent=2)+'\n')
 q['archives'][target.name]=r['archive_sha256'];q.setdefault('r28_product_relocations',[]).append(dict(path=str(proof_path),sha256=sha(proof_path)))
 temp=C/'qualification.new.json';temp.write_text(json.dumps(q,indent=2)+'\n');os.chmod(temp,0o444);os.replace(temp,C/'qualification.json')
 assert set(p.name for p in C.iterdir())==set(q['archives'])|{'qualification.json'} and sum(p.stat().st_size for p in C.iterdir())<q['limit_bytes']
finally:os.chmod(C,0o500)
print('qualified R28 product archive moved to bounded readonly cold storage',platform,repo,proof['released_hot_bytes'],flush=True)
