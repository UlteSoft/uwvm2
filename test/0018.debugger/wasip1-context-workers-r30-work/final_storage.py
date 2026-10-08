from pathlib import Path
import os,json,hashlib,time
D=Path(__file__).parent;E=D.parent.parent;C=E.parent/'wasip1-r27-owned-cold-evidence'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
q=json.loads((D/'world-install-final-qualified.json').read_text())
assert q['passed'] and q['native_program_runs']==48 and q['counted_assertions']==14472
cg=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
assert Path('/proc/self/cgroup').read_text().strip()=='0::/'+str(cg).removeprefix('/sys/fs/cgroup/')
assert (cg/'memory.max').read_text().strip()=='68719476736' and (cg/'memory.swap.max').read_text().strip()=='0'
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='c8d3550f-3a19-40d7-8507-a76c2045ace5'
assert Path('/proc/11166/stat').read_text().split(')')[-1].split()[19]=='22661'
policy=json.loads((E/'storage-policy.json').read_text())
seen=set();allocated=logical=files=0
for parent,dirs,names in os.walk(E,followlinks=False):
 dirs[:]=[n for n in dirs if not (Path(parent)/n).is_symlink()]
 for n in names:
  s=(Path(parent)/n).lstat();logical+=s.st_size;files+=1
  ident=(s.st_dev,s.st_ino)
  if ident not in seen:allocated+=s.st_blocks*512;seen.add(ident)
v=os.statvfs(E);host=os.statvfs(E.parent)
cold=json.loads((C/'qualification.json').read_text());cold_bytes=sum(p.stat().st_size for p in C.iterdir())
assert cold['passed'] and cold['limit_bytes']==2<<30 and cold_bytes<2<<30
assert set(p.name for p in C.iterdir())==set(cold['archives'])|{'qualification.json'}
assert all(p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 for p in C.iterdir())
assert E.stat().st_dev!=E.parent.stat().st_dev and v.f_blocks*v.f_frsize<=8<<30
assert allocated<policy['directory_admission_bytes'] and v.f_bavail*v.f_frsize>=policy['volume_free_reserve_bytes']
assert v.f_favail>=policy['volume_inode_admission_reserve'] and host.f_bavail*host.f_frsize>=policy['filesystem_free_reserve_bytes']
proof=dict(local_product_cold_limit_bytes=1<<30,local_product_cold_directory='/Users/liyinan/Documents/MacroModel/wasip1-r30-owned-cold-evidence',passed=True,timestamp_ns=time.time_ns(),cgroup=Path('/proc/self/cgroup').read_text(),boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),anchor_pid=11166,anchor_birth=22661,memory_max_bytes=64<<30,swap_max_bytes=0,owned_directory=str(E),directory_allocated_bytes=allocated,directory_logical_bytes=logical,files=files,owned_volume_capacity_bytes=v.f_blocks*v.f_frsize,owned_volume_free_bytes=v.f_bavail*v.f_frsize,owned_volume_free_inodes=v.f_favail,host_filesystem_free_bytes=host.f_bavail*host.f_frsize,cold_evidence_bytes=cold_bytes,cold_checked_limit_bytes=2<<30,cold_qualification_sha256=sha(C/'qualification.json'),source_manifest_sha256=sha(D/'inputs.json'),final_qualification_sha256=sha(D/'world-install-final-qualified.json'),hard_capacity_scope='8 GiB ext4 private hot filesystem; cold 2 GiB limit is checked by authenticated guards',foreign_cleanup_performed=False)
(D/'final-storage-qualified.json').write_text(json.dumps(proof,indent=2)+'\n')
A=E.parent/'wasip1-active-environment.json';state=json.loads(A.read_text())
state.update(latest_round=str(D),last_round_test_status='R30 private WASIp1 real native dispatch workers and resolver policy qualified; complete joint world restore/replay remains unfinished.',last_status='R30: both repositories / four native OS / 48 runs / 14472 assertions plus 8 Linux graph regressions.',latest_status='R30 private real native dispatch workers and resolver policy qualified. Same 64 GiB cgroup, 8 GiB hot filesystem and 2 GiB checked cold evidence limit.',latest_round_evidence=dict(round='R30',qualification=str(D/'world-install-final-qualified.json'),qualification_sha256=sha(D/'world-install-final-qualified.json'),source_manifest_sha256=sha(D/'inputs.json'),complete_world_restore=False),final_storage_r30=proof)
state['readonly_historical_cold_evidence'].update(bytes=cold_bytes,limit_bytes=2<<30,qualification=str(C/'qualification.json'),qualification_sha256=sha(C/'qualification.json'))
backup=D/'active-environment-before-final-r30.json';assert not backup.exists();backup.write_bytes(A.read_bytes())
temporary=A.with_name(A.name+'.r30.tmp');assert not temporary.exists();temporary.write_text(json.dumps(state,indent=2)+'\n');os.replace(temporary,A)
assert json.loads(A.read_text())['latest_round_evidence']['qualification_sha256']==sha(D/'world-install-final-qualified.json')
print('R30 final storage qualified',allocated,v.f_bavail*v.f_frsize,cold_bytes,flush=True)
