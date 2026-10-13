from pathlib import Path
import sys,subprocess,json,hashlib,tarfile,shutil,time,resource,plistlib,os,re
L=Path(__file__).parent
E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17');D=E/'rounds/wasip1-prepared-retirement-20261008-r34'
ssh=['ssh','-i',str(Path.home()/'.ssh/id_ed25519'),'macromodel@100.123.133.75'];scp=['scp','-i',str(Path.home()/'.ssh/id_ed25519')]
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
rows=json.loads((L/'pipeline-macos-r34.json').read_text()) if (L/'pipeline-macos-r34.json').exists() else []
def run(label,argv):
 print('R34 MAC PIPELINE',label,flush=True);start=time.monotonic();q=subprocess.run(argv)
 rows.append(dict(label=label,argv=argv,exit=q.returncode,seconds=time.monotonic()-start));(L/'pipeline-macos-r34.json').write_text(json.dumps(rows,indent=2)+'\n');assert q.returncode==0,label
def plist(argv):return plistlib.loads(subprocess.check_output(argv))
def attach(repo):
 M=L/'macos';assert not M.exists();M.mkdir()
 assert os.statvfs(L).f_bavail*os.statvfs(L).f_frsize>384<<20
 before=set(plist(['/usr/sbin/diskutil','list','-plist'])['AllDisks'])
 q=plist(['/usr/bin/hdiutil','attach','-nomount','-plist','ram://2097152'])
 devs=[r['dev-entry'] for r in q['system-entities'] if r.get('dev-entry')]
 assert len(devs)==1 and re.fullmatch(r'/dev/disk[0-9]+',devs[0]) and devs[0][5:] not in before
 dev=devs[0];info=plist(['/usr/sbin/diskutil','info','-plist',dev])
 assert info['DeviceNode']==dev
 # Only the newly allocated RAM device is ever formatted or detached.
 active=dict(passed=False,device=dev,new_device_not_in_prior_inventory=True,capacity_bytes=1<<30,mountpoint=str(M),repo=repo,original_info=info)
 (L/'active-ramdisk.json').write_text(json.dumps(active,indent=2,default=str)+'\n')
 assert info['TotalSize']==1<<30 and info['WholeDisk']
 run('format-own-ramdisk-'+repo,['/usr/sbin/diskutil','eraseVolume','HFS+','UWVM_R34_'+repo.replace('-','_'),dev])
 run('unmount-own-ramdisk-'+repo,['/usr/sbin/diskutil','unmount',dev]);run('mount-own-ramdisk-'+repo,['/usr/sbin/diskutil','mount','-mountPoint',str(M),dev])
 actual=plist(['/usr/sbin/diskutil','info','-plist',dev]);assert actual['MountPoint']==str(M) and actual['TotalSize']==1<<30 and M.stat().st_dev!=L.stat().st_dev
 active.update(passed=True,mounted_info=actual,physical_disk_reserve_bytes=384<<20,maximum_retained_native_metadata_bytes=8<<20)
 (L/'active-ramdisk.json').write_text(json.dumps(active,indent=2,default=str)+'\n');return active
def detach(active,repo,qualified):
 M=L/'macos';dev=active['device'];info=plist(['/usr/sbin/diskutil','info','-plist',dev]);assert info['DeviceNode']==dev and info['TotalSize']==1<<30 and info['MountPoint']==str(M)
 # Exact native binaries remain recoverable in the fully verified cold archive.
 O=M/repo;receipt=json.loads((O/'qualified.json').read_text());assert all(sha(O/i['binary'])==i['binary_sha256'] for i in receipt['fixtures'].values())
 recovery=json.loads((L/(repo+'-macos-product-retirement.json')).read_text())
 assert recovery['passed'] and all(recovery['payloads'][i['binary']]==i['binary_sha256'] for i in receipt['fixtures'].values())
 retained=L/('macos-evidence-'+repo);assert not retained.exists();retained.mkdir()
 members={str(p.relative_to(O)):sha(p) for p in O.rglob('*') if p.is_file() and not p.is_symlink() and p.suffix!='.exe'}
 assert sum((O/n).stat().st_size for n in members)<8<<20
 for n,h in members.items():p=retained/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(O/n,p);assert sha(p)==h
 run('detach-own-ramdisk-'+repo,['/usr/bin/hdiutil','detach',dev]);assert dev[5:] not in set(plist(['/usr/sbin/diskutil','list','-plist'])['AllDisks'])
 active.update(detached=True,native_products_recoverable=qualified,retained_metadata_directory=str(retained),retained_metadata=members)
 (L/(repo+'-ramdisk-retirement.json')).write_text(json.dumps(active,indent=2,default=str)+'\n');(L/'active-ramdisk.json').unlink();M.rmdir()
for repo in sys.argv[1:]:
 assert repo in ('uwvm2','uwvm2-ros')
 active=None
 try:
   guard='guard-joint-cross-macos-ordinary-12g-r34.py' if repo=='uwvm2' else 'guard-joint-cross-r34.py'
   qualified=L/(repo+'-macos-cross-qualified.json');assert qualified.exists() and json.loads(qualified.read_text())['passed']
   active=attach(repo);O=L/'macos'/repo;O.mkdir()
   q=json.loads(qualified.read_text());names=list({i['binary'] for i in q['fixtures'].values()} | {i['wasm'] for i in q['fixtures'].values()} | {'qualified.json'})
   for n in names:run('transfer-'+repo+'-'+n,[*scp,'macromodel@100.123.133.75:'+str(D/'products/macos'/repo/n),str(O/n)])
   for name in ('macos_native.py','macos_monitor.py'):shutil.copyfile(L/name,O/name)
   run('native-'+repo,[sys.executable,'-u',str(L/'macos_native.py'),repo])
   receipt=json.loads((O/'receipt.json').read_text());assert receipt['passed'] and len(receipt['rows'])==6
   members={str(p.relative_to(O)):sha(p) for p in O.rglob('*') if p.is_file() and not p.is_symlink() and p.suffix!='.exe'}
   assert sum((O/n).stat().st_size for n in members)<8<<20
   archive=L/(repo+'-macos-native-evidence.tar.gz');assert not archive.exists()
   with tarfile.open(archive,'w:gz',compresslevel=6) as t:
    for n in members:t.add(O/n,arcname=n,recursive=False)
   observed={}
   with tarfile.open(archive,'r|gz') as t:
    for m in t:
     assert m.isfile() and m.name in members and m.name not in observed
     with t.extractfile(m) as f:observed[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
   assert observed==members
   upper=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20)+(1<<30);assert upper<2<<30
   proof=dict(passed=True,archive_sha256=sha(archive),payloads=members,all_payloads_read_back=True,local_packaging_memory_upper_bytes=upper,owned_ramdisk_bytes=1<<30,limit_bytes=2<<30)
   proofpath=L/(repo+'-macos-native-evidence.json');proofpath.write_text(json.dumps(proof,indent=2)+'\n')
   run('native-evidence-transfer-'+repo,[*scp,str(archive),str(proofpath),'macromodel@100.123.133.75:'+str(D)+'/'])
   run('archive-'+repo,[*ssh,'python3','-u',str(E/'guard-joint-archive-r34.py'),'joint-archive-r34','macos',repo])
   # Release the full RAM capacity before the bounded archive decoder runs.
   run('recovery-receipt-'+repo,[*scp,'macromodel@100.123.133.75:'+str(D/(repo+'-macos-product-retirement.json')),str(L/(repo+'-macos-product-retirement.json'))]);detach(active,repo,True);active=None
   # Qualified archive remains on original bounded Linux volume; no additional local cold region.
 except BaseException:
  # Failed native trials retain all raw binaries remotely; retire only this
  # exact new RAM device, after copying bounded local diagnostics.
  if active is None and (L/'active-ramdisk.json').exists():active=json.loads((L/'active-ramdisk.json').read_text())
  if active is not None:
   dev=active['device'];info=plist(['/usr/sbin/diskutil','info','-plist',dev])
   assert info['DeviceNode']==dev and info['TotalSize']==1<<30 and active['new_device_not_in_prior_inventory']
   M=L/'macos';diag=L/('failed-native-macos-'+repo);diag.mkdir(exist_ok=False)
   members={str(p.relative_to(M)):sha(p) for p in M.rglob('*') if p.is_file() and not p.is_symlink() and p.suffix!='.exe'}
   assert sum((M/n).stat().st_size for n in members)<8<<20
   for n,h in members.items():p=diag/n;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(M/n,p);assert sha(p)==h
   run('detach-own-failed-ramdisk-'+repo,['/usr/bin/hdiutil','detach',dev])
   (L/(repo+'-failed-ramdisk-retirement.json')).write_text(json.dumps(dict(device=dev,retired=True,diagnostics=members,native_tests_passed=False),indent=2)+'\n')
   (L/'active-ramdisk.json').unlink();M.rmdir()
  raise
print('R34 both native macOS matrices and remote archive readbacks complete',flush=True)
