from pathlib import Path
import sys,json,hashlib,tarfile,resource,subprocess
L=Path(__file__).parent;E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-joint-worker-contexts-20261007-r33'
platform,repo=sys.argv[1:3];assert platform in ('linux-integrated','windows','freebsd','macos') and repo in ('uwvm2','uwvm2-ros')
C=Path('/Users/liyinan/Documents/MacroModel/wasip1-r33-owned-cold-evidence');C.mkdir(mode=0o700,exist_ok=True)
assert C.is_dir() and not C.is_symlink() and all(p.is_file() and not p.is_symlink() for p in C.iterdir())
previous=Path('/Users/liyinan/Documents/MacroModel/wasip1-r29-owned-cold-evidence');assert previous.is_dir() and not previous.is_symlink()
previous_bytes=sum(p.stat().st_size for p in previous.iterdir());assert previous_bytes<1<<30 and all(p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 for p in previous.iterdir())
limit=256<<20;sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
import sys as _sys
_sys.path.insert(0,str(Path(__file__).parent))
import qualified_archive as qa
scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')];ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75']
name=repo+'-'+platform;receipt=L/(name+'-product-retirement.json')
subprocess.run([*scp,'macromodel@100.123.133.75:'+str(D/receipt.name),str(receipt)],check=True)
A=json.loads(receipt.read_text());assert A['passed'] and A['all_payloads_read_back']
assert sum(p.stat().st_size for p in C.iterdir())+A['archive_bytes']<limit
regions=list(C.parent.glob('wasip1-r*-owned-cold-evidence'));assert all(p.is_dir() and not p.is_symlink() for p in regions)
assert sum(p.stat().st_size for region in regions for p in region.iterdir() if p.is_file())+A['archive_bytes']<2<<30
target=C/(name+'-qualified-products.tar.zst');assert not target.exists()
subprocess.run([*scp,'macromodel@100.123.133.75:'+A['archive'],str(target)],check=True)
assert target.stat().st_size==A['archive_bytes'] and sha(target)==A['archive_sha256']
seen={}
with qa.open_reader(target) as archive:
 for m in archive:
  assert m.isfile() and m.name in A['payloads'] and m.name not in seen
  with archive.extractfile(m) as inp:seen[m.name]=hashlib.file_digest(inp,'sha256').hexdigest()
assert seen==A['payloads'];target.chmod(0o444)
upper=qa.local_memory_upper();assert upper<2<<30
proof=dict(passed=True,archive=str(target),archive_bytes=target.stat().st_size,archive_sha256=sha(target),payloads=seen,all_payloads_read_back=True,source_manifest_sha256=sha(L/'inputs.json'),limit_bytes=limit,cold_bytes=sum(p.stat().st_size for p in C.iterdir()),local_cold_aggregate_limit_bytes=2<<30,local_cold_aggregate_bytes=sum(p.stat().st_size for region in regions for p in region.iterdir() if p.is_file()),decoder=qa.decoder_identity(),memory_upper_bytes=upper,native_tests_unchanged=True)
p=L/(name+'-local-cold-qualified.json');p.write_text(json.dumps(proof,indent=2)+'\n')
subprocess.run([*scp,str(p),'macromodel@100.123.133.75:'+str(D)+'/'],check=True)
subprocess.run([*ssh,'python3','-u',str(E/'guard-joint-product-retire-r33.py'),'joint-product-retire-r33',platform,repo],check=True)
print('R33 recoverable product moved to bounded local cold directory',name,flush=True)
