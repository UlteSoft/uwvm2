from pathlib import Path
import json,tarfile,hashlib,resource,subprocess,shlex
B=Path('/Users/liyinan/Documents/MacroModel/src');L=Path(__file__).parent
old=L.with_name('wasip1-joint-worker-contexts-r33-work')/'inputs.json';previous=json.loads(old.read_text());names=set(previous)
for repo in ('uwvm2','uwvm2-ros'):
 for root in ('src','third-parties/fast_io/include','third-parties/bizwen/include','third-parties/boost_unordered/include'):
  base=B/repo/root
  for p in base.rglob('*'):
   if p.is_file() and not p.is_symlink():names.add(str(p.relative_to(B)))
 for n in ('debug_checkpoint_prepared_retirement_runtime.cc','debug_wasip1_prepared_retirement_runtime.cc'):
  names.add(repo+'/test/0017.runtime/'+n)
sha=lambda p:hashlib.file_digest(p.open('rb'),'sha256').hexdigest();M={};A=L/'inputs.tar.gz';assert not A.exists()
with tarfile.open(A,'w:gz',compresslevel=6) as t:
 for n in sorted(names):
  p=B/n;assert p.is_file() and not p.is_symlink() and p.stat().st_size<8<<20;h=sha(p);t.add(p,arcname=n,recursive=False);assert sha(p)==h;M[n]=h
(L/'inputs.json').write_text(json.dumps(M,indent=2)+'\n');seen={}
with tarfile.open(A,'r|gz') as t:
 for m in t:
  assert m.isfile() and m.name in M and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert M==seen
E='/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17';D='/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-prepared-retirement-20261008-r34'
q=dict(passed=True,files=len(M),manifest_sha256=sha(L/'inputs.json'),source_archive_sha256=sha(A),all_payloads_read_back=True,remote_directory=D,memory_upper_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20),limit_bytes=2<<30)
assert q['memory_upper_bytes']<2<<30;(L/'local-source-cut-qualified.json').write_text(json.dumps(q,indent=2)+'\n');print(q,flush=True)
ssh=['ssh','-i','/Users/liyinan/.ssh/id_ed25519','macromodel@100.123.133.75'];scp=['scp','-i','/Users/liyinan/.ssh/id_ed25519'];subprocess.run([*ssh,'mkdir -m 700 '+shlex.quote(D)],check=True)
files=[p for p in L.iterdir() if p.suffix=='.py' and not p.name.startswith('guard-')]+[L/n for n in ('inputs.json','inputs.tar.gz','local-source-cut-qualified.json')]
subprocess.run([*scp,*map(str,files),'macromodel@100.123.133.75:'+D+'/'],check=True)
subprocess.run([*scp,*map(str,L.glob('guard-*.py')),'macromodel@100.123.133.75:'+E+'/'],check=True)
subprocess.run([*ssh,'python3','-u',E+'/guard-joint-inputs-r34.py','joint-inputs-r34'],check=True)
