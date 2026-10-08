"""Read-only frontend analysis in the original 64 GiB cgroup.

Original native-suite admission remains refused. This route allows no compiler
objects, linking, guest execution or VM; its own new files have a 16 MiB ceiling.
"""
from pathlib import Path
import ctypes, fcntl, hashlib, json, os, resource, select, signal, subprocess, sys, time, traceback
D=Path(__file__).parent; E=D.parent.parent
CG=Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
expected='0::/'+str(CG).removeprefix('/sys/fs/cgroup/')+'\n'
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='c8d3550f-3a19-40d7-8507-a76c2045ace5'
assert Path('/proc/11166/stat').read_text().rsplit(')',1)[1].split()[19]=='22661'
assert (CG/'memory.max').read_text().strip()=='68719476736' and (CG/'memory.swap.max').read_text().strip()=='0'
assert (CG/'cpuset.cpus.effective').read_text().strip()=='0,2,4,6,16-31'
policy=json.loads((E/'storage-policy.json').read_text())
assert policy['filesystem_free_reserve_bytes']==32<<30 and policy['directory_admission_bytes']==6<<30
assert ctypes.CDLL(None).prctl(36,1,0,0,0)==0
lease=os.open(E/'suite.lock',os.O_RDWR); fcntl.flock(lease,fcntl.LOCK_EX|fcntl.LOCK_NB)
assert int((CG/'memory.current').read_text())<54<<30
os.sched_setaffinity(0,{16})
start=time.monotonic(); owned={}; root=None; failure=None; peak=0; cg_peak=0; events=(CG/'memory.events').read_text()
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
    resource.setrlimit(resource.RLIMIT_CORE,(0,0)); resource.setrlimit(resource.RLIMIT_FSIZE,(8<<20,8<<20))
    resource.setrlimit(resource.RLIMIT_AS,(8<<30,8<<30))
try:
    boot='import os,sys,signal;os.sched_setaffinity(0,set(range(16,32)));os.kill(os.getpid(),signal.SIGSTOP);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)'
    root=subprocess.Popen([sys.executable,'-u','-c',boot,sys.executable,'-u',str(D/'analysis_compile_matrix-v1.py')],preexec_fn=limits,
                         env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
    bind(root.pid); pid,status=os.waitpid(root.pid,os.WUNTRACED); assert pid==root.pid and os.WIFSTOPPED(status)
    subprocess.run(['docker','run','--rm','--cap-drop=ALL','--cap-add=DAC_OVERRIDE','--network=none','--read-only',
        '--security-opt=no-new-privileges','--cgroupns=host','--pid=host','--cpuset-cpus=16','--memory=33554432',
        '--memory-swap=33554432','-v',str(CG/'cgroup.procs')+':/admit:rw','debian:sid','sh','-c',
        'echo "$1" > /admit','sh',str(root.pid)],check=True,timeout=30,stdout=subprocess.DEVNULL)
    assert identify(root.pid)['cgroup']==expected; signal.pidfd_send_signal(owned[root.pid]['fd'],signal.SIGCONT)
    while root.poll() is None:
        peak=max(peak,scan()); cg_peak=max(cg_peak,int((CG/'memory.current').read_text()))
        assert peak<(9<<29) and cg_peak<60<<30 and time.monotonic()-start<1800
        # No native-suite reserve is changed. Only this bounded analysis output is charged.
        assert sum(p.lstat().st_blocks*512 for p in D.rglob('*') if p.is_file() and not p.is_symlink())<16<<20
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
    receipt=dict(passed=passed,error=failure,exit=root.returncode if root else None,cgroup=expected,boot_id='c8d3550f-3a19-40d7-8507-a76c2045ace5',
        owned_peak_aggregate_rss_upper_bytes=peak,cgroup_peak_bytes=cg_peak,processes=records,
        parent_events_unchanged=events==(CG/'memory.events').read_text(),
        validation_kind='compiler analysis only',actual_native_runtime_execution=False,original_native_storage_policy_unchanged=True,
        original_native_suite_admission_still_refused=True,new_analysis_file_ceiling_bytes=16<<20,
        host_available_bytes=os.statvfs(E.parent).f_bavail*os.statvfs(E.parent).f_frsize,
        guard_sha256=hashlib.file_digest(Path(__file__).open('rb'),'sha256').hexdigest())
    (D/'guard-analysis-compile-v1.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for i in owned.values(): os.close(i['fd'])
    print(json.dumps(dict(passed=passed,error=failure,exit=receipt['exit'],processes=len(records))),flush=True)
    sys.exit(0 if passed else 1)
