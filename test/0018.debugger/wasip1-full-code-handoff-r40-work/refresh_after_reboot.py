"""Refresh authenticated existing environment after reboot; no tests run here."""
from pathlib import Path
import fcntl,hashlib,json,os,stat,subprocess

P=Path("/home/macromodel/Documents/uwvm3-implementation")
E=P/"wasip1-recovery-20261006-r17";D=E/"rounds/wasip1-full-code-handoff-20261008-r40"
A=P/"wasip1-active-environment.json"
oldboot="c8d3550f-3a19-40d7-8507-a76c2045ace5";boot="b5e7a71f-8b40-4f39-8a41-fdce3f8fd2cb"
CG=Path("/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope")
def sha(p):
 with Path(p).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def atomic(path,data,mode=0o600):
 temp=path.with_name("."+path.name+".r40-reboot-"+str(os.getpid())+".tmp")
 with temp.open("xb") as f:
  os.fchmod(f.fileno(),mode);f.write(data);f.flush();os.fsync(f.fileno())
 os.replace(temp,path)
 fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
 try:os.fsync(fd)
 finally:os.close(fd)
assert Path("/proc/sys/kernel/random/boot_id").read_text().strip()==boot
assert (CG/"memory.max").read_text().strip()=="68719476736" and (CG/"memory.swap.max").read_text().strip()=="0"
assert (CG/"cpuset.cpus.effective").read_text().strip()=="0,2,4,6,16-31"
anchor=Path("/proc/10079");assert anchor.stat().st_uid==1000
assert int((anchor/"stat").read_text().rsplit(")",1)[1].split()[19])==20021
assert (anchor/"cgroup").read_text()=="0::/"+str(CG).removeprefix("/sys/fs/cgroup/")+"\n"
assert (anchor/"cmdline").read_bytes()==b"sleep\x00infinity\x00"
assert E.stat().st_dev!=P.stat().st_dev
uuid=subprocess.check_output(["/usr/bin/findmnt","-n","-o","UUID","-T",str(E)],text=True).strip()
assert uuid=="ae4d0a7f-dff9-493d-a216-82058227ed0e"
policy_before=(E/"storage-policy.json").read_bytes();policy=json.loads(policy_before)
assert policy["filesystem_free_reserve_bytes"]==32<<30 and policy["directory_admission_bytes"]==6<<30
assert policy["directory_stop_bytes"]==7<<30 and policy["volume_capacity_bytes"]==8<<30
with (E/"suite.lock").open("r+") as lease:
 fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
 before_bytes=A.read_bytes();before=json.loads(before_bytes);identity=A.stat()
 assert before["active_directory"]==str(E) and before["cgroup"]==str(CG) and before["boot"] in (oldboot,boot)
 g=E/"guard.py";raw=g.read_bytes();text=raw.decode();gs=g.stat()
 backup=D/"guard.py.before-r40-reboot"
 if oldboot in text:
  assert hashlib.sha256(raw).hexdigest()==before["guard_source_sha256"]=="d37e98c8ba75ce21bd2b91b0c45c3c9800e9bda624ea26b85334099ae122883d"
  with backup.open("xb") as f:f.write(raw);f.flush();os.fsync(f.fileno())
  changed=text.replace(oldboot,boot).replace("/proc/11166/stat","/proc/10079/stat").replace("=='22661'","=='20021'")
  compile(changed,str(g),"exec")
  assert g.read_bytes()==raw and g.stat().st_ino==gs.st_ino
  atomic(g,changed.encode(),stat.S_IMODE(gs.st_mode))
 else:
  assert boot in text and "/proc/10079/stat" in text and "=='20021'" in text
 rows=json.loads((D/"ram-native-v1-results.json").read_text())["rows"]
 completed=sum(r["actual_native_runtime_execution"] and r["passed"] for r in rows)
 assert completed==23 and not (D/"ram-native-v1-qualified.json").exists() and not (D/"guard-ram-native-v1.json").exists()
 storage=json.loads((D/"ram-native-v1-storage.json").read_text())
 assert not Path(storage["path"]).exists()
 key="environment_refresh_20261008_reboot_r40"
 fields=dict(passed=True,boot_id=boot,anchor_pid=10079,anchor_birth=20021,cgroup=str(CG),
  memory_max_bytes=64<<30,swap_max_bytes=0,reused_anchor=True,reused_volume=True,mounted_uuid=uuid,
  tests_executed=False,storage_policy_unchanged=True,storage_policy_sha256=hashlib.sha256(policy_before).hexdigest(),
  selected_guard_sha256=sha(g),guard_backup=str(backup),old_boot=oldboot,
  interrupted_native_attempt_completed_cases=23,interrupted_native_suite_qualified=False,
  old_tmpfs_products_cleared_by_reboot=True)
 after=dict(before);after["boot"]=boot;after["guard_source_sha256"]=sha(g);after[key]=fields
 allowed={"boot","guard_source_sha256",key}
 assert all(after[k]==v for k,v in before.items() if k not in allowed)
 if not (D/"active-environment-before-reboot-r40.json").exists():
  with (D/"active-environment-before-reboot-r40.json").open("xb") as f:f.write(before_bytes);f.flush();os.fsync(f.fileno())
 now=A.stat();assert (now.st_dev,now.st_ino,now.st_size,now.st_mtime_ns)==(identity.st_dev,identity.st_ino,identity.st_size,identity.st_mtime_ns)
 assert A.read_bytes()==before_bytes and (E/"storage-policy.json").read_bytes()==policy_before
 atomic(A,(json.dumps(after,indent=2)+"\n").encode(),stat.S_IMODE(identity.st_mode))
 actual=json.loads(A.read_text());assert actual==after
 receipt=dict(**fields,original_fields_preserved=sum(k not in allowed for k in before),
  before_active_sha256=hashlib.sha256(before_bytes).hexdigest(),after_active_sha256=sha(A),cgroup_anchor_created=False)
 with (D/"reboot-recovery-r40-qualified.json").open("x") as f:
  json.dump(receipt,f,indent=2);f.write("\n");f.flush();os.fsync(f.fileno())
 print(json.dumps(receipt))
