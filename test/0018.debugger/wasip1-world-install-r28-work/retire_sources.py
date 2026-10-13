from pathlib import Path
import sys,json,hashlib,tarfile,shutil,os
D=Path(__file__).parent;assert sys.argv[1]=='joint-source-retire-r28' and len(sys.argv)==2
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
names=('initial-inputs','vendor-fixed-inputs','declaration-failed-inputs','capture-diagnostic-before-inputs','active-init-before-inputs')
proof=dict(passed=False,scope='Retire ONLY this task ended old immutable source expansions after whole original archive and every payload agree with manifest and expanded files; archives/manifests/failure logs retained',rows=[])
for name in names:
 root=D/name;manifest=D/(name+'.json');archive=D/(name+'.tar.gz')
 assert root.is_dir() and not root.is_symlink() and root.parent==D
 M=json.loads(manifest.read_text());assert all(sha(root/n)==h for n,h in M.items())
 actual={str(p.relative_to(root)) for p in root.rglob('*') if p.is_file()};assert actual==set(M)
 seen={}
 with tarfile.open(str(archive),'r|gz') as t:
  for m in t:
   assert m.isfile() and m.name in M
   with t.extractfile(m) as f:h=hashlib.file_digest(f,'sha256').hexdigest()
   assert h==M[m.name];seen[m.name]=h
 assert seen==M
 row=dict(root=str(root),files=len(M),manifest_sha256=sha(manifest),archive=str(archive),archive_sha256=sha(archive),all_payloads_read_back=True)
 proof['rows'].append(row)
proof['passed']=True;(D/'old-source-expansions-retirement.json').write_text(json.dumps(proof,indent=2)+'\n')
for name in names:shutil.rmtree(D/name)
print('ended own source expansions retired; verified source archives and manifests retained',flush=True)
