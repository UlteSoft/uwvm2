from pathlib import Path
import hashlib,json,os,stat,subprocess,datetime
D=Path("/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-source-initializer-20261008-r38"); E=D.parent.parent
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
p=json.loads((E/'storage-policy.json').read_text());assert p['directory_admission_bytes']==6<<30 and p['filesystem_free_reserve_bytes']==32<<30
g=json.loads((D/'guard-analysis-compile-v3b.json').read_text())
q=json.loads((D/'analysis-compile-qualified-v3b.json').read_text())
w=json.loads((D/'wasm-fixture-validation-v3b.json').read_text())
m=json.loads((D/'syntax-overlay-v3.json').read_text())
assert g['passed'] and g['parent_events_unchanged'] and all(x['pidfd_retired'] for x in g['processes'])
assert q['passed'] and len(q['rows'])==16 and len(w)==8 and all(x['passed'] for x in w)
assert len(m)==36 and q['source_manifest_sha256']==sha(D/'syntax-overlay-v3.json')
assert all(sha(D/'syntax-overlay-v3'/key)==value for key,value in m.items())
unique=set();allocated=0;logical=0;pathsum=0;files=0;errors=[]
def add(path):
 global allocated
 s=os.lstat(path);identity=(s.st_dev,s.st_ino)
 if identity not in unique:unique.add(identity);allocated+=s.st_blocks*512
 return s
def bad(error):
 assert Path(error.filename)==E/'lost+found' and isinstance(error,PermissionError),repr(error)
 errors.append(error.filename)
for root,dirs,names in os.walk(E,followlinks=False,onerror=bad):
 add(root)
 for name in dirs:
  if os.path.islink(Path(root)/name):add(Path(root)/name)
 for name in names:
  s=add(Path(root)/name)
  if stat.S_ISREG(s.st_mode):logical+=s.st_size;pathsum+=s.st_blocks*512;files+=1
du=subprocess.run(['du','-s','-B1',str(E)],capture_output=True,text=True)
assert du.returncode in (0,1)
expected="du: cannot read directory '"+str(E/'lost+found')+"': Permission denied"
assert du.stderr.strip() in ('',expected),du.stderr
actual=int(du.stdout.split()[0]);assert 0<=actual-allocated<1<<20
v=os.statvfs(E);h=os.statvfs(E.parent)
own=sum(s.st_blocks*512 for f in D.rglob('*') if not f.is_symlink() and f.is_file() for s in [f.stat()])
assert own<16<<20
boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip();assert boot==g['boot_id']
cg=Path('/sys/fs/cgroup')/g['cgroup'].strip().split('::',1)[1].lstrip('/')
assert int((cg/'memory.max').read_text())==64<<30 and int((cg/'memory.swap.max').read_text())==0
mem={line.split(':')[0]:int(line.split()[1])*1024 for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:')}
reasons=[]
if actual>=p['directory_admission_bytes']:reasons.append('task directory physical allocation lower bound exceeds original 6 GiB admission floor')
host_free=h.f_bavail*h.f_frsize
if host_free<p['filesystem_free_reserve_bytes']:reasons.append('host disk free is below original 32 GiB reserve')
if v.f_bavail*v.f_frsize<p['volume_free_reserve_bytes']:reasons.append('task volume free is below original 512 MiB reserve')
if v.f_favail<p['volume_inode_admission_reserve']:reasons.append('task volume free inodes below original admission reserve')
if mem['MemAvailable']<p['host_memory_admission_reserve_bytes']:reasons.append('host MemAvailable below original admission reserve')
if int((cg/'memory.current').read_text())>=p['shared_cgroup_admission_bytes']:reasons.append('shared cgroup usage exceeds original admission floor')
assert reasons
assert (D/'ram-native-v3-qualified.json').exists()
native=json.loads((D/'ram-native-v3-qualified.json').read_text());ng=json.loads((D/'guard-ram-native-v3.json').read_text())
assert native['passed'] and native['native_runs']==24 and ng['passed'] and ng['parent_events_unchanged'] and ng['ram_disposable_products_retired']
assert all(r['pidfd_retired'] for r in ng['processes'])
result=dict(round=38,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),native_admitted=False,
 refusal_reasons=reasons,storage_policy_sha256=sha(E/'storage-policy.json'),storage_policy=p,
 task_directory_allocated_bytes_lower_bound=actual,unique_inode_allocated_bytes_lower_bound=allocated,
 allocated_regular_file_path_sum_bytes_reference_only=pathsum,logical_regular_file_bytes=logical,regular_file_paths=files,
 unique_readable_inodes=len(unique),allocation_inventory_complete=not bool(errors),
 inaccessible_filesystem_reserved_directories=errors,task_volume_free_bytes=v.f_bavail*v.f_frsize,
 task_volume_free_inodes=v.f_favail,host_filesystem_free_bytes=host_free,host_mem_available_bytes=mem['MemAvailable'],
 new_round_regular_file_allocated_bytes=own,new_round_file_ceiling_bytes=16<<20,boot_id=boot,cgroup=str(cg),
 cgroup_current_bytes=int((cg/'memory.current').read_text()),final_source_manifest_sha256=sha(D/'syntax-overlay-v3.json'),
 qualified_analysis_sha256=sha(D/'analysis-compile-qualified-v3b.json'),guard_sha256=sha(D/'guard-analysis-compile-v3b.json'))
(D/'r38-storage-final.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['native_admitted','refusal_reasons','task_directory_allocated_bytes_lower_bound','host_filesystem_free_bytes','new_round_regular_file_allocated_bytes','allocation_inventory_complete']}))
