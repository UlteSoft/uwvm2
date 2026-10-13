"""Bounded Linux actual native regression in the original 64 GiB cgroup.

Fresh compiler objects, links and guests use only the owned 768 MiB tmpfs
directory. The persistent four-OS suite remains refused by its disk guards;
this route keeps a 16 MiB persistent log/source ceiling and 6 GiB RSS limit.
"""
from pathlib import Path
import ctypes, fcntl, hashlib, json, os, resource, select, signal, subprocess, sys, time, traceback
D=Path(__file__).parent; E=D.parent.parent
CG=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
expected='0::/'+str(CG).removeprefix('/sys/fs/cgroup/')+'\n'
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='b5e7a71f-8b40-4f39-8a41-fdce3f8fd2cb'
assert Path('/proc/10079/stat').read_text().rsplit(')',1)[1].split()[19]=='20021'
assert (CG/'memory.max').read_text().strip()=='68719476736' and (CG/'memory.swap.max').read_text().strip()=='0'
assert (CG/'cpuset.cpus.effective').read_text().strip()=='0,2,4,6,16-31'
policy=json.loads((E/'storage-policy.json').read_text())
assert policy['filesystem_free_reserve_bytes']==32<<30 and policy['directory_admission_bytes']==6<<30
assert ctypes.CDLL(None).prctl(36,1,0,0,0)==0
lease=os.open(E/'suite.lock',os.O_RDWR); fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
assert int((CG/'memory.current').read_text())<50<<30
os.sched_setaffinity(0,{16})
start=time.monotonic(); owned={}; root=None; failure=None; peak=0; cg_peak=0; ram_peak=0; ram_cleanup=False; events=(CG/'memory.events').read_text()
def identify(pid):
    p=Path('/proc')/str(pid); fields=(p/'stat').read_text().rsplit(')',1)[1].split()
    return dict(pid=pid,birth=int(fields[19]),ppid=int(fields[1]),uid=p.stat().st_uid,
                cgroup=(p/'cgroup').read_text(),argv=(p/'cmdline').read_bytes().decode(errors='replace').split('\0')[:-1])
def dead(fd):
    poll=select.poll(); poll.register(fd,select.POLLIN); return bool(poll.poll(0))
def bind(pid,parent=None):
    fd=os.pidfd_open(pid)
    try:
        ident=identify(pid); assert ident['uid']==1000 and (parent is None or ident['ppid'] in (parent,os.getpid()))
        owned[pid]=dict(fd=fd,identity=ident)
    except BaseException: os.close(fd); raise
def scan():
    total=0
    for pid,item in list(owned.items()):
        if dead(item['fd']): continue
        try:
            now=identify(pid)
            if dead(item['fd']): continue
            assert now['birth']==item['identity']['birth'] and now['cgroup']==expected and now['uid']==1000
            assert set(os.sched_getaffinity(pid))<=set(range(16,32))
            for task in (Path('/proc')/str(pid)/'task').iterdir():
                try: children=(task/'children').read_text().split()
                except FileNotFoundError: continue
                for child in map(int,children):
                    if child not in owned:
                        try: bind(child,pid)
                        except (FileNotFoundError,ProcessLookupError): pass
            total+=int((Path('/proc')/str(pid)/'statm').read_text().split()[1])*os.sysconf('SC_PAGE_SIZE')
        except (FileNotFoundError,ProcessLookupError): assert dead(item['fd'])
    return total
def limits():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0)); resource.setrlimit(resource.RLIMIT_FSIZE,(1<<30,1<<30))
    resource.setrlimit(resource.RLIMIT_AS,(64<<30,64<<30))
try:
    boot='import os,sys,signal;os.sched_setaffinity(0,set(range(16,32)));os.kill(os.getpid(),signal.SIGSTOP);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)'
    root=subprocess.Popen([sys.executable,'-u','-c',boot,sys.executable,'-u',str(D/'ram_native_regression-v2.py')],preexec_fn=limits,
                         env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
    bind(root.pid); pid,status=os.waitpid(root.pid,os.WUNTRACED); assert pid==root.pid and os.WIFSTOPPED(status)
    subprocess.run(['docker','run','--rm','--cap-drop=ALL','--cap-add=DAC_OVERRIDE','--network=none','--read-only',
        '--security-opt=no-new-privileges','--cgroupns=host','--pid=host','--cpuset-cpus=16','--memory=33554432',
        '--memory-swap=33554432','-v',str(CG/'cgroup.procs')+':/admit:rw','debian:sid','sh','-c',
        'echo "$1" > /admit','sh',str(root.pid)],check=True,timeout=30,stdout=subprocess.DEVNULL)
    assert identify(root.pid)['cgroup']==expected; signal.pidfd_send_signal(owned[root.pid]['fd'],signal.SIGCONT)
    while root.poll() is None:
        peak=max(peak,scan()); cg_peak=max(cg_peak,int((CG/'memory.current').read_text()))
        assert peak<(6<<30) and cg_peak<60<<30 and time.monotonic()-start<3600
        # No native-suite reserve is changed. Only this bounded analysis output is charged.
        assert sum(p.lstat().st_blocks*512 for p in D.rglob('*') if p.is_file() and not p.is_symlink())<16<<20
        ram_lease=D/'ram-native-v2-storage.json'
        if ram_lease.exists():
            record=json.loads(ram_lease.read_text());ram=Path(record['path'])
            assert ram==Path('/dev/shm')/('uwvm-checkpoint-r40-reboot-v2-'+str(root.pid)) and not ram.is_symlink()
            actual=ram.stat();assert actual.st_dev==record['device'] and actual.st_ino==record['inode'] and actual.st_uid==1000
            assert record['creator_pid']==root.pid and record['creator_birth']==owned[root.pid]['identity']['birth']
            ram_bytes=0
            for f in ram.rglob('*'):
                try:
                    assert not f.is_symlink()
                    if f.is_file():ram_bytes+=f.stat().st_blocks*512
                except FileNotFoundError:pass
            ram_peak=max(ram_peak,ram_bytes);assert ram_bytes<768<<20
        available=next(int(l.split()[1])*1024 for l in Path('/proc/meminfo').read_text().splitlines() if l.startswith('MemAvailable:'))
        assert available>4<<30
        time.sleep(.02)
    scan()
except BaseException: failure=traceback.format_exc()
finally:
    for item in owned.values():
        if not dead(item['fd']):
            try: signal.pidfd_send_signal(item['fd'],signal.SIGKILL)
            except ProcessLookupError: pass
    if root is not None: root.wait(timeout=20)
    until=time.monotonic()+20
    while any(not dead(i['fd']) for i in owned.values()) and time.monotonic()<until: time.sleep(.02)
    while True:
        try: pid,status=os.waitpid(-1,os.WNOHANG)
        except ChildProcessError: break
        if pid==0: break
    records=[dict(**i['identity'],pidfd_retired=dead(i['fd'])) for i in owned.values()]
    passed=failure is None and root is not None and root.returncode==0 and all(r['pidfd_retired'] for r in records)
    ram_inventory=[]
    if (D/'ram-native-v2-storage.json').exists() and all(r['pidfd_retired'] for r in records):
        import shutil
        record=json.loads((D/'ram-native-v2-storage.json').read_text());ram=Path(record['path'])
        assert root is not None and ram==Path('/dev/shm')/('uwvm-checkpoint-r40-reboot-v2-'+str(root.pid)) and not ram.is_symlink()
        actual=ram.stat();assert actual.st_dev==record['device'] and actual.st_ino==record['inode'] and actual.st_uid==1000
        assert record['creator_pid']==root.pid and record['creator_birth']==owned[root.pid]['identity']['birth']
        for f in ram.rglob('*'):
            assert not f.is_symlink()
            if f.is_file():ram_inventory.append(dict(name=str(f.relative_to(ram)),size=f.stat().st_size,
                sha256=hashlib.file_digest(f.open('rb'),'sha256').hexdigest()))
        shutil.rmtree(ram);ram_cleanup=not ram.exists()
    receipt=dict(passed=passed,error=failure,exit=root.returncode if root else None,cgroup=expected,boot_id='b5e7a71f-8b40-4f39-8a41-fdce3f8fd2cb',
        owned_peak_aggregate_rss_upper_bytes=peak,cgroup_peak_bytes=cg_peak,processes=records,
        parent_events_unchanged=events==(CG/'memory.events').read_text(),
        validation_kind='bounded tmpfs Linux native regression',actual_native_runtime_execution=True,original_native_storage_policy_unchanged=True,
        original_persistent_native_suite_admission_still_refused=True,new_analysis_file_ceiling_bytes=16<<20,
        host_available_bytes=os.statvfs(E.parent).f_bavail*os.statvfs(E.parent).f_frsize,
        ram_peak_allocated_bytes=ram_peak,ram_ceiling_bytes=768<<20,ram_disposable_products_retired=ram_cleanup,ram_final_inventory=ram_inventory,
        guard_sha256=hashlib.file_digest(Path(__file__).open('rb'),'sha256').hexdigest())
    (D/'guard-ram-native-v2.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for i in owned.values(): os.close(i['fd'])
    print(json.dumps(dict(passed=passed,error=failure,exit=receipt['exit'],processes=len(records))),flush=True)
    sys.exit(0 if passed else 1)
