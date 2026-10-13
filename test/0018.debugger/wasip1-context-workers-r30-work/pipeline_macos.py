from pathlib import Path
import sys,subprocess,json,hashlib,tarfile,shutil,time,resource
L=Path(__file__).parent
E=Path("/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17");D=Path("/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-context-workers-20261007-r30")
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75']
scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def run(label,argv):
 print('R30 MAC PIPELINE',label,flush=True);start=time.monotonic();p=subprocess.run(argv)
 row=dict(label=label,argv=argv,exit=p.returncode,seconds=time.monotonic()-start);rows.append(row)
 (L/'pipeline-macos-r30.json').write_text(json.dumps(rows,indent=2)+'\n')
 assert p.returncode==0,label
rows=[]
for repo in ('uwvm2','uwvm2-ros'):
 guard='guard-joint-cross-macos-ordinary-12g-r30.py' if repo=='uwvm2' else 'guard-joint-cross-r30.py'
 run('cross-'+repo,[*ssh,'python3','-u',str(E/guard),'joint-cross-r30','macos',repo])
 O=L/'macos'/repo;O.mkdir(parents=True,exist_ok=True)
 names=['fixture.exe','environment-group.exe','checkpoint.wasm','aliases.wasm','group-main.wasm','group-provider.wasm','qualified.json','environment-group-qualified.json']
 assert not any((O/n).exists() for n in names)
 for n in names:run('transfer-'+repo+'-'+n,[*scp,'macromodel@100.123.133.75:'+str(D/'products/macos'/repo/n),str(O/n)])
 for name in ('macos_native.py','macos_monitor.py'):shutil.copyfile(L/name,O/name)
 run('native-'+repo,[sys.executable,'-u',str(L/'macos_native.py'),repo])
 receipt=json.loads((O/'receipt.json').read_text());assert receipt['passed'] and len(receipt['rows'])==6
 members={str(p.relative_to(O)):sha(p) for p in O.rglob('*') if p.is_file() and not p.is_symlink() and p.suffix!='.exe'}
 assert all((O/n).stat().st_size<8<<20 for n in members)
 archive=L/(repo+'-macos-native-evidence.tar.gz');assert not archive.exists()
 with tarfile.open(archive,'w:gz',compresslevel=6) as t:
  for n in members:t.add(O/n,arcname=n,recursive=False)
 observed={}
 with tarfile.open(archive,'r|gz') as t:
  for m in t:
   assert m.isfile() and m.name in members and m.name not in observed
   with t.extractfile(m) as f:observed[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
 assert observed==members
 upper=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20);assert upper<2<<30
 proof=dict(passed=True,archive_sha256=sha(archive),payloads=members,all_payloads_read_back=True,local_packaging_memory_upper_bytes=upper,limit_bytes=2<<30)
 proofpath=L/(repo+'-macos-native-evidence.json');proofpath.write_text(json.dumps(proof,indent=2)+'\n')
 run('native-evidence-transfer-'+repo,[*scp,str(archive),str(proofpath),'macromodel@100.123.133.75:'+str(D)+'/'])
 run('archive-'+repo,[*ssh,'python3','-u',str(E/'guard-joint-archive-r30.py'),'joint-archive-r30','macos',repo])
 run('cold-'+repo,[sys.executable,'-u',str(L/'rehome_product.py'),'macos',repo])
print('R30 both native macOS matrices and remote archive readbacks complete',flush=True)

