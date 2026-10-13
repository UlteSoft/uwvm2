from pathlib import Path
import json,tarfile,hashlib,resource,subprocess,shlex
B=Path('/Users/liyinan/Documents/MacroModel/src');L=Path(__file__).parent;old=L.with_name('wasip1-joint-worker-contexts-r32-work')/'inputs.json'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();previous=json.loads(old.read_text());M={};A=L/'inputs.tar.gz';assert not A.exists()
with tarfile.open(A,'w:gz',compresslevel=6) as t:
 for n in sorted(previous):
  p=B/n;assert p.is_file() and not p.is_symlink();h=sha(p);t.add(p,arcname=n,recursive=False);assert sha(p)==h;M[n]=h
(L/'inputs.json').write_text(json.dumps(M,indent=2)+'\n');seen={}
with tarfile.open(A,'r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in M and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert M==seen
E='/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17';D=E+'/rounds/wasip1-joint-worker-contexts-20261007-r33'
q=dict(passed=True,files=len(M),manifest_sha256=sha(L/'inputs.json'),source_archive_sha256=sha(A),all_payloads_read_back=True,remote_directory=D,changed_from_r32=sum(previous[n]!=h for n,h in M.items()),memory_upper_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20),limit_bytes=2<<30)
assert q['memory_upper_bytes']<2<<30;(L/'local-source-cut-qualified.json').write_text(json.dumps(q,indent=2)+'\n');print(q,flush=True)
ssh=['ssh','-i','/Users/liyinan/.ssh/id_ed25519','macromodel@100.123.133.75'];scp=['scp','-i','/Users/liyinan/.ssh/id_ed25519'];subprocess.run([*ssh,'mkdir -m 700 '+shlex.quote(D)],check=True)
files=[p for p in L.iterdir() if p.suffix=='.py' and not p.name.startswith(('guard-','suite-'))]+[L/n for n in ('inputs.json','inputs.tar.gz','local-source-cut-qualified.json')]
subprocess.run([*scp,*map(str,files),'macromodel@100.123.133.75:'+D+'/'],check=True);subprocess.run([*scp,*map(str,L.glob('guard-*.py')),'macromodel@100.123.133.75:'+E+'/'],check=True)
code='''from pathlib import Path
E=Path('''+repr(E)+''');p=E/'suite.py';s=p.read_text();a="if sys.argv[1]=='joint-retained-retire-r32':";assert s.count(a)==1
(E/'rounds/wasip1-joint-worker-contexts-20261007-r33/suite-before-registration.py').write_text(s)
head="if sys.argv[1] in ('joint-inputs-r33','joint-linux-integrated-r33','joint-cross-r33','joint-vm-r33','joint-archive-r33','joint-report-r33','joint-product-retire-r33','joint-storage-r33','joint-delivery-r33'):\\n import runpy\\n route={'joint-inputs-r33':'bootstrap.py','joint-linux-integrated-r33':'linux-integrated.py','joint-cross-r33':'cross-integrated.py','joint-vm-r33':'vm.py','joint-archive-r33':'archive.py','joint-report-r33':'report.py','joint-product-retire-r33':'retire_product.py','joint-storage-r33':'final_storage.py','joint-delivery-r33':'delivery.py'}[sys.argv[1]]\\n runpy.run_path(str(D/'rounds/wasip1-joint-worker-contexts-20261007-r33'/route),run_name='__main__')\\nel"
s=s.replace(a,head+a);compile(s,str(p),'exec');p.write_text(s)
''';subprocess.run([*ssh,'python3 -c '+shlex.quote(code)],check=True)
