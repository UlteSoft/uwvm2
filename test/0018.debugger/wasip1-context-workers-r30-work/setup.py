from pathlib import Path
import json,hashlib,tarfile,io
B=Path('/Users/liyinan/Documents/MacroModel/src');L=Path(__file__).parent
E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17')
remote=E/'rounds/wasip1-context-workers-20261007-r30'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
old=json.loads((L.parent/'wasip1-dispatch-r29-work/inputs.json').read_text())
names=set(old)
for repo in ('uwvm2','uwvm2-ros'):
 for tree in ('src','third-parties/fast_io/include','third-parties/fast_io/share','third-parties/boost_unordered/include','third-parties/bizwen/include'):
  names.update(str(p.relative_to(B)) for p in (B/repo/tree).rglob('*') if p.is_file() and not p.is_symlink())
names={n for n in names if (B/n).is_file() and not (B/n).is_symlink()}
manifest={}
assert not (L/'inputs.tar.gz').exists()
with tarfile.open(L/'inputs.tar.gz','w:gz',compresslevel=6) as archive:
 for name in sorted(names):
  payload=(B/name).read_bytes();assert len(payload)<8<<20
  manifest[name]=hashlib.sha256(payload).hexdigest()
  info=tarfile.TarInfo(name);info.size=len(payload);info.mode=0o444;info.mtime=0
  archive.addfile(info,io.BytesIO(payload))
(L/'inputs.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
observed={}
with tarfile.open(L/'inputs.tar.gz','r|gz') as archive:
 for info in archive:
  assert info.isfile() and info.name in manifest and info.name not in observed
  with archive.extractfile(info) as source:observed[info.name]=hashlib.file_digest(source,'sha256').hexdigest()
assert observed==manifest
for name in ('linux-integrated.py','cross-integrated.py','vm.py','archive.py','report.py'):
 s=(L/name).read_text().replace('r29','r30').replace('R29','R30')
 if name=='vm.py':s=s.replace('checks=205','checks=207').replace('else 210','else 212').replace('checks=482','checks=483')
 if name=='archive.py':s=s[:s.index('# Same authenticated caller-owned cgroup')]
 if name=='report.py':
  s=s.replace('len(M)==8767','len(M)=='+str(len(manifest))).replace('else 210','else 212').replace('else (205','else (207').replace('else 210)','else 212)').replace('4*210','4*212').replace('4*205','4*207')
  a=s.index("proof['historical_local_cold']");b=s.index("proof['linux_fast_path_regression_runs']",a);s=s[:a]+s[b:]
  s=s.replace("and sha(A['archive'])==A['archive_sha256']","")
  needle="  assert A['passed'] and A['all_payloads_read_back'] "
  assert needle in s
  s=s.replace(needle,needle+"\n  relocation=read(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json'));retired=read(D/(repo+'-'+nativeplatform+'-local-cold-retirement.json'))\n  assert relocation['passed'] and relocation['all_payloads_read_back'] and relocation['archive_sha256']==A['archive_sha256'] and relocation['payloads']==A['payloads'] and relocation['limit_bytes']==1<<30 and relocation['memory_upper_bytes']<2<<30\n  assert retired['passed'] and retired['archive_sha256']==A['archive_sha256'] and retired['qualification_sha256']==sha(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json')) and not Path(A['archive']).exists()\n  guard(E/('guard-joint-product-retire-r30-'+nativeplatform+'-'+repo+'.json'))")
  s=s.replace("compiled_product_archive=dict(path=A['archive'],sha256=A['archive_sha256'])","compiled_product_archive=dict(path=relocation['archive'],sha256=A['archive_sha256'],recovery_qualification=evidence(D/(repo+'-'+nativeplatform+'-local-cold-qualified.json')))")
  s=s.replace("scope='Final private WASIp1 native dispatch contexts and external trace binding policy; genuine module/environment/memory sharing; both repositories, four native operating systems'","scope='Genuine FastIO native workers validate candidate WASIp1 per-module memory TLS, nested selection, simultaneous shared environments, restoration and join; custom resolver policy refuses before invocation; both repositories and four native OSes'")
  s=s.replace("'Install live GC roots and enroll actual new startup workers'","'Enroll actual new startup execution workers and install their live roots during complete publication'")
 (L/name).write_text(s)
for p in L.glob('guard-*-r29.py'):
 s=p.read_text().replace('r29','r30').replace('R29','R30');(L/p.name.replace('r29','r30')).write_text(s)
s=(L/'guard-joint-archive-r30.py').read_text().replace('joint-archive-r30','joint-product-retire-r30')
(L/'guard-joint-product-retire-r30.py').write_text(s)
s=(L/'suite.py').read_text();routes={'inputs':'bootstrap.py','linux-integrated':'linux-integrated.py','cross':'cross-integrated.py','vm':'vm.py','archive':'archive.py','report':'report.py','product-retire':'retire_product.py','storage':'final_storage.py','delivery':'delivery.py'}
mapping={'joint-'+phase+'-r30':file for phase,file in routes.items()}
needle="if sys.argv[1] in ('joint-inputs-r29'"
assert needle in s
s=s.replace(needle,"if sys.argv[1] in "+repr(tuple(mapping))+":\n import runpy\n route="+repr(mapping)+"[sys.argv[1]]\n runpy.run_path(str(D/'rounds/wasip1-context-workers-20261007-r30'/route),run_name='__main__')\nel"+needle,1)
(L/'suite-r30.py').write_text(s)
proof=dict(passed=True,files=len(manifest),manifest_sha256=sha(L/'inputs.json'),source_archive_sha256=sha(L/'inputs.tar.gz'),all_payloads_read_back=True,remote_directory=str(remote),changed_from_r29=sum(old.get(n)!=h for n,h in manifest.items()))
(L/'local-source-cut-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(proof),flush=True)
