# Dedicated caller-owned task guard in the user's original shared cgroup.
# Foreign participants are observed but never adopted, signalled or retired.
import os,sys,time,signal,subprocess,json,select,ctypes,hashlib,resource,traceback,fcntl
from pathlib import Path
D=Path(__file__).parent
assert len(sys.argv)==4 and sys.argv[1]=='joint-binary-cache3-retire-r34' and sys.argv[2] in ('linux-integrated',) and sys.argv[3] in ('uwvm2','uwvm2-ros'), 'exact own verified product compaction routes only'
soft,hard=resource.getrlimit(resource.RLIMIT_NOFILE);resource.setrlimit(resource.RLIMIT_NOFILE,(min(hard,65536),hard))
CG=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
guard_identity=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
C=D.parent/'wasip1-r27-owned-cold-evidence'
assert C.is_dir() and C.stat().st_mode&0o222==0
cold=json.loads((C/'qualification.json').read_text())
assert cold['passed'] and cold['limit_bytes']==2147483648
assert set(p.name for p in C.iterdir())==set(cold['archives'])|{'qualification.json'}
assert all(p.is_file() and not p.is_symlink() and p.stat().st_mode&0o222==0 for p in C.iterdir())
assert sum(p.stat().st_size for p in C.iterdir())<cold['limit_bytes']
assert hashlib.sha256(Path(cold['receipt']).read_bytes()).hexdigest()==cold['receipt_sha256']
expected='0::/'+str(CG).removeprefix('/sys/fs/cgroup/')+'\n'
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='c8d3550f-3a19-40d7-8507-a76c2045ace5'
assert Path('/proc/11166/stat').read_text().split(')')[-1].split()[19]=='22661'
assert (CG/'memory.max').read_text().strip()=='68719476736' and (CG/'memory.swap.max').read_text().strip()=='0'
assert (CG/'cpuset.cpus.effective').read_text().strip()=='0,2,4,6,16-31'
libc=ctypes.CDLL(None,use_errno=True);assert libc.prctl(36,1,0,0,0)==0
os.sched_setaffinity(0,{16})
owned={};history=[];reaped={};root=None;start=time.monotonic();foreign=set();failure=None
# Serialize only this task's suites; never consume simultaneous compiler/VM headroom.
lease=os.open(D/'suite.lock',os.O_RDWR|os.O_CREAT,0o600)
last_lease=0
while True:
 try:fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);break
 except BlockingIOError:
  if time.monotonic()-last_lease>45:print('waiting for this task sole suite lease',flush=True);last_lease=time.monotonic()
  assert time.monotonic()-start<3600,'own suite lease deadline';time.sleep(2)
# Admission can wait behind another suite; recheck before issuing a test child.
phase=sys.argv[1];threshold=54<<30
# This root contains only this task's restored inputs, products, temporary files and receipts.
# These are sampled stopping thresholds, not a filesystem hard quota.
POLICY=json.loads((D/'storage-policy.json').read_text())
assert D.resolve()==Path(POLICY['owned_directory']).resolve()
def occupancy(path):
 logical=allocated=files=0;allocated_inodes=set()
 for parent,dirs,names in os.walk(path,followlinks=False):
  dirs[:]=[n for n in dirs if not (Path(parent)/n).is_symlink()]
  for n in names:
   try:s=(Path(parent)/n).lstat()
   except FileNotFoundError:continue
   logical+=s.st_size;files+=1
   inode=(s.st_dev,s.st_ino)
   if inode not in allocated_inodes:allocated+=s.st_blocks*512;allocated_inodes.add(inode)
 return dict(logical_bytes=logical,allocated_bytes=allocated,files=files)
def available_memory():
 return next(int(line.split()[1])*1024 for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:'))
def space_check(admission=False):
 root_size=occupancy(D);tmp_size=occupancy(D/'tmp');v=os.statvfs(D.parent);free=v.f_bavail*v.f_frsize
 ownv=os.statvfs(D);ownfree=ownv.f_bavail*ownv.f_frsize
 assert D.stat().st_dev!=D.parent.stat().st_dev and ownv.f_blocks*ownv.f_frsize<=POLICY['volume_capacity_bytes'],'bounded separate filesystem required'
 assert ownfree>=POLICY['volume_free_reserve_bytes'],('owned volume free reserve',ownfree)
 inode_reserve=POLICY['volume_inode_admission_reserve'] if admission else POLICY['volume_inode_running_reserve']
 assert ownv.f_favail>=inode_reserve,('owned volume inode reserve',ownv.f_favail,inode_reserve)
 assert free>=POLICY['filesystem_free_reserve_bytes'],('filesystem reserve',free)
 assert root_size['allocated_bytes']<(POLICY['directory_admission_bytes'] if admission else POLICY['directory_stop_bytes']),('owned directory allocation',root_size)
 assert root_size['logical_bytes']<POLICY['directory_logical_stop_bytes'],('owned directory logical size',root_size)
 assert tmp_size['allocated_bytes']<POLICY['temporary_stop_bytes'],('owned temporary allocation',tmp_size)
 assert available_memory()>((8 if admission else 4)<<30),'host memory reserve'
 return dict(directory=root_size,temporary=tmp_size,filesystem_available_bytes=free,owned_volume_available_bytes=ownfree,owned_volume_available_inodes=ownv.f_favail,owned_volume_total_inodes=ownv.f_files,host_memory_available_bytes=available_memory())
initial_storage=space_check(False);storage_peak=initial_storage['directory']['allocated_bytes'];minimum_free=initial_storage['filesystem_available_bytes'];last_space=0
while int((CG/'memory.current').read_text())>threshold:
 fcntl.flock(lease,fcntl.LOCK_UN)
 if time.monotonic()-last_lease>45:print('waiting for original cgroup headroom after own lease',flush=True);last_lease=time.monotonic()
 assert time.monotonic()-start<3600,'headroom after own lease deadline';time.sleep(2)
 while True:
  try:fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB);break
  except BlockingIOError:
   assert time.monotonic()-start<3600,'own suite lease deadline';time.sleep(2)
space_check(False)
start=time.monotonic()
before=(CG/'memory.events').read_text()
owned_rss_peak=0;parent_memory_peak=0
def retired(fd,seconds=0):
 poll=select.poll();poll.register(fd,select.POLLIN);return bool(poll.poll(int(seconds*1000)))
def identify(pid):
 p=Path('/proc')/str(pid); fields=p.joinpath('stat').read_text().split(')')[-1].split()
 return dict(pid=pid,birth=int(fields[19]),ppid=int(fields[1]),state=fields[0],cg=p.joinpath('cgroup').read_text(),uid=p.stat().st_uid,argv=p.joinpath('cmdline').read_bytes().decode(errors='replace').split('\0')[:-1])
def bind(pid,parent=None):
 fd=os.pidfd_open(pid); ident=identify(pid)
 assert ident['uid']==1000 and (parent is None or ident['ppid'] in (parent,os.getpid())), ('child ownership',ident,parent)
 if parent is not None:assert parent in owned, ('unbound parent',parent)
 owned[pid]={'fd':fd,'identity':ident};history.append(ident)
def scan():
 for parent,item in list(owned.items()):
  if retired(item['fd']):continue
  try:
   current=identify(parent)
   if retired(item['fd']):continue
   assert current['birth']==item['identity']['birth'] and current['uid']==1000 and current['cg']==expected, ('bound identity changed',current)
   assert set(os.sched_getaffinity(parent))<=set(range(16,32)), ('affinity',parent)
   task=Path('/proc')/str(parent)/'task'
   for tid in task.iterdir():
    try:children=tid.joinpath('children').read_text().split()
    except FileNotFoundError:continue
    for pid in map(int,children):
     if pid not in owned:
      try:bind(pid,parent)
      except (ProcessLookupError,FileNotFoundError):continue
      if retired(owned[pid]['fd']):continue
      try:child_ident=identify(pid)
      except (FileNotFoundError,ProcessLookupError):
       assert retired(owned[pid]['fd'],.05), ('disappeared bound child',pid)
       continue
      if retired(owned[pid]['fd']):continue
      assert child_ident['cg']==expected, ('live child cgroup',child_ident)
  except (FileNotFoundError,ProcessLookupError):
   assert retired(item['fd'],.05)
 for pid in map(int,(CG/'cgroup.procs').read_text().split()):
  if pid!=11166 and pid not in owned:foreign.add(pid)
def terminate(signum,frame):raise KeyboardInterrupt('owned suite cancelled')
for sig in (signal.SIGTERM,signal.SIGINT):signal.signal(sig,terminate)
try:
 env=dict(os.environ)
 for key in ['LD_PRELOAD','LD_AUDIT','UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT']:env.pop(key,None)
 t='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm'
 env.update(TMPDIR=str(D/'tmp'),TMP=str(D/'tmp'),TEMP=str(D/'tmp'),LD_LIBRARY_PATH=t+'/lib:'+t+'/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu',RAYON_NUM_THREADS='1',UWVM_TEST_CPUSET='0,2,4,6,16-31',PYTHONDONTWRITEBYTECODE='1')
 boot='import os,sys,signal;os.sched_setaffinity(0,set(range(16,32)));os.kill(os.getpid(),signal.SIGSTOP);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)'
 def limits():resource.setrlimit(resource.RLIMIT_FSIZE,(((3<<30) if phase=='portable-path-r18' and sys.argv[2]=='freebsd-setup' else min(int(env.get('UWVM_RECOVERY_FILE_LIMIT_BYTES','1073741824')),1<<30)),)*2);resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 root=subprocess.Popen([sys.executable,'-u','-c',boot,sys.executable,'-u',str(D/'rounds/wasip1-prepared-retirement-20261008-r34/archive_binary_before_probe3.py'),*sys.argv[1:]],cwd=D,env=env,preexec_fn=limits)
 bind(root.pid)
 pid,status=os.waitpid(root.pid,os.WUNTRACED);assert pid==root.pid and os.WIFSTOPPED(status)
 assert identify(root.pid)['birth']==owned[root.pid]['identity']['birth']
 helper=['docker','run','--rm','--cap-drop=ALL','--cap-add=DAC_OVERRIDE','--network=none','--read-only','--security-opt=no-new-privileges','--cgroupns=host','--pid=host','--cpuset-cpus=16','--memory=33554432','--memory-swap=33554432','-v',str(CG/'cgroup.procs')+':/admit:rw','debian:sid','sh','-c','echo "$1" > /admit','sh',str(root.pid)]
 subprocess.run(helper,check=True,timeout=30,stdout=subprocess.DEVNULL)
 assert identify(root.pid)['cg']==expected
 signal.pidfd_send_signal(owned[root.pid]['fd'],signal.SIGCONT)
 while root.poll() is None:
  scan()
  assert time.monotonic()-start<3600,'suite deadline'
  rss=0
  for pid,item in owned.items():
   if retired(item['fd']):continue
   try:rss+=int((Path('/proc')/str(pid)/'statm').read_text().split()[1])*os.sysconf('SC_PAGE_SIZE')
   except (FileNotFoundError,ProcessLookupError):assert retired(item['fd'],.05),'RSS disappeared before owned pidfd retirement'
  owned_rss_peak=max(owned_rss_peak,rss);parent_memory_peak=max(parent_memory_peak,int((CG/'memory.current').read_text()))
  assert rss<((2<<30) if sys.argv[1] in ('before','components') else (9<<29) if sys.argv[1]=='group' else (9<<29) if sys.argv[1] in ('runtime','runtime-before','runtime-tests') else (2<<30)),'owned process RSS limit'
  assert int((CG/'memory.current').read_text())<60<<30,'shared cgroup 4GiB reserve'
  if time.monotonic()-last_space>1:
   observed_storage=space_check();last_space=time.monotonic();storage_peak=max(storage_peak,observed_storage['directory']['allocated_bytes']);minimum_free=min(minimum_free,observed_storage['filesystem_available_bytes'])
  time.sleep(.03)
 scan()
 observed_storage=space_check();storage_peak=max(storage_peak,observed_storage['directory']['allocated_bytes']);minimum_free=min(minimum_free,observed_storage['filesystem_available_bytes'])
except BaseException as e:failure=traceback.format_exc()
finally:
 for pid,item in owned.items():
  if not retired(item['fd']):
   try:signal.pidfd_send_signal(item['fd'],signal.SIGKILL)
   except ProcessLookupError:assert retired(item['fd'],.05)
 if root is not None:root.wait(timeout=20)
 until=time.monotonic()+20
 while any(not retired(item['fd']) for item in owned.values()) and time.monotonic()<until:time.sleep(.03)
 while True:
  try:pid,status=os.waitpid(-1,os.WNOHANG)
  except ChildProcessError:break
  if pid==0:break
  reaped[pid]=os.waitstatus_to_exitcode(status)
 after=(CG/'memory.events').read_text()
 rows=[dict(pid=pid,birth=item['identity']['birth'],pidfd_retired=bool(retired(item['fd']))) for pid,item in owned.items()]
 passed=failure is None and root is not None and root.returncode==0 and all(r['pidfd_retired'] for r in rows)
 proof=dict(passed=passed,error=failure,actual_root_exit=root.returncode if root else None,cgroup=expected,limits={'owned_rss_limit_bytes':((2<<30) if sys.argv[1] in ('before','components') else (9<<29) if sys.argv[1]=='group' else (9<<29) if sys.argv[1] in ('runtime','runtime-before','runtime-tests') else (2<<30)),'memory.max':(CG/'memory.max').read_text().strip(),'memory.swap.max':(CG/'memory.swap.max').read_text().strip(),'cpuset':(CG/'cpuset.cpus.effective').read_text().strip()},owned_peak_aggregate_rss_upper_bytes=owned_rss_peak,parent_peak_memory_current_bytes=parent_memory_peak,parent_events_unchanged=before==after,memory_events_before=before,memory_events_after=after,foreign_pids_observed_not_owned=sorted(foreign),history=history,retirement=rows,reaped=reaped,guard_sha256=guard_identity,boot_id=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),anchor_pid=11166,anchor_birth=22661,storage_policy=POLICY,initial_storage=initial_storage,storage_peak_allocated_bytes=storage_peak,minimum_filesystem_available_bytes=minimum_free,final_storage=occupancy(D))
 proof_path=D/('guard-'+('-'.join(sys.argv[1:]))+'.json')
 if proof_path.exists():proof_path.rename(D/('guard-history-'+('-'.join(sys.argv[1:]))+'-'+str(time.time_ns())+'.json'))
 proof_path.write_text(json.dumps(proof,indent=2)+'\n')
 for item in owned.values():os.close(item['fd'])
 print(json.dumps({'guard_passed':passed,'error':failure,'actual_root_exit':root.returncode if root else None,'owned_processes':len(owned),'foreign_processes_untouched':len(foreign)}),flush=True)
 raise SystemExit(0 if passed else 1)

