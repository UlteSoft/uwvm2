"""Real pidfd error-path regressions; admitted existing64GiB SSH Linux only.

The surrounding keeper owns this test, observes resources and bounds output.
Two real children are deliberately fault-injected in parent operations. A
separate authenticated duplicate pidfd is held solely for test failure cleanup;
no unanchored PID/PGID signals are used. Nothing was executed during authorship.
"""
import hashlib, os
from pathlib import Path
import select, signal, stat, subprocess, sys, types

def read(path):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        a=os.fstat(fd)
        if not stat.S_ISREG(a.st_mode)or not 0<a.st_size<=1<<20:raise ValueError('bounded actual helper Source required')
        raw=os.read(fd,a.st_size+1);b=os.fstat(fd);c=os.lstat(path)
        def key(x):return tuple(getattr(x,'st_'+n)for n in('dev','ino','mode','size','mtime_ns','ctime_ns','nlink'))
        if len(raw)!=a.st_size or key(a)!=key(b)or key(a)!=key(c):raise ValueError('helper Source changed')
        return raw
    finally:os.close(fd)

def main():
    if sys.platform!='linux'or len(sys.argv)!=3:raise RuntimeError('outer admitted Linux keeper and actual helper SHA required')
    cg='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
    if Path('/proc/self/cgroup').read_text().strip()!='0::'+cg:raise RuntimeError('existing cgroup required')
    root=Path('/sys/fs/cgroup')/cg.lstrip('/')
    if(root/'memory.max').read_text().strip()!=str(64<<30)or(root/'memory.swap.max').read_text().strip()!='0':raise RuntimeError('64GiB/swap0 required')
    path=Path(sys.argv[1]);raw=read(path)
    if hashlib.sha256(raw).hexdigest()!=sys.argv[2]:raise ValueError('externally admitted exact helper SHA differs')
    helper=types.ModuleType('_actual_owned_retirement_regression');helper.__file__=str(path)
    exec(compile(raw,str(path),'exec',dont_inherit=True),helper.__dict__)
    records=[]
    for case in('pidfd_send_permission_failure','leader_join_failure'):
        barrier_r,barrier_w=os.pipe();child=None;emergency=None;actual_wait=None;actual_send=signal.pidfd_send_signal
        marker=PermissionError('owned-retirement-injected-signal-failure')if case.startswith('pidfd')else TimeoutError('owned-retirement-injected-wait-failure')
        try:
            # Block before the actual child workload until helper authentication
            # and the independent retained-pidfd fallback have both completed.
            code='import os,sys,time; f=int(sys.argv[1]); assert os.read(f,1)==b"G"; os.close(f); time.sleep(30)'
            child=subprocess.Popen([sys.executable,'-I','-S','-B','-c',code,str(barrier_r)],start_new_session=True,pass_fds=(barrier_r,),stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            os.close(barrier_r);barrier_r=None;helper.remember_process(child,force=True)
            owner=child._uwvm_owned_process;leader=owner.members[(child.pid,owner.leader_start)]
            emergency=os.dup(leader['fd']);actual_wait=child.wait
            if os.write(barrier_w,b'G')!=1:raise RuntimeError('real child barrier failed')
            os.close(barrier_w);barrier_w=None
            if case.startswith('pidfd'):
                def failed_send(*args,**kwargs):raise marker
                signal.pidfd_send_signal=failed_send
            else:
                def failed_wait(*args,**kwargs):raise marker
                child.wait=failed_wait
            try:helper.terminate_group(child,term_timeout=.1,kill_timeout=.1)
            except BaseException as error:
                if error is not marker:raise
            else:raise AssertionError('injected real parent operation did not fail')
            finally:signal.pidfd_send_signal=actual_send;child.wait=actual_wait
            failed=helper.owned_process_record(child)
            if failed['closed']or not all(x['pidfd_retained']for x in failed['members']):raise AssertionError('failed retirement lost authenticated ownership')
            if not failed.get('retirement_errors'):raise AssertionError('failed retirement diagnostic missing')
            # Retry cleanup only, never the child workload. Same genuine owner
            # and pidfds must close after actual natural/forced child join.
            helper.terminate_group(child,term_timeout=1,kill_timeout=1)
            done=helper.owned_process_record(child)
            if not done['closed']or any(x['pidfd_retained']for x in done['members'])or child.returncode is None:raise AssertionError('actual retry did not retire child')
            records.append({'case':case,'failed_owner_retained':True,'retry_actual_closed':True,'signals':done['signals'],'retirement_errors':done['retirement_errors']})
        finally:
            signal.pidfd_send_signal=actual_send
            if child is not None and actual_wait is not None:child.wait=actual_wait
            for fd in(barrier_r,barrier_w):
                if fd is not None:os.close(fd)
            if child is not None:
                # Dedicated test emergency owner: only this retained pidfd,
                # never child.pid as a signal target. A failure is not a PASS.
                if emergency is not None:
                    poller=select.poll();poller.register(emergency,select.POLLIN|select.POLLHUP|select.POLLERR)
                    if not poller.poll(0):actual_send(emergency,signal.SIGKILL)
                actual_wait(timeout=3)if actual_wait is not None else child.wait(timeout=3)
                owner=getattr(child,'_uwvm_owned_process',None)
                if owner is not None and not owner.closed:
                    if owner.living():raise RuntimeError('actual owned child still live; do not hand off resources')
                    owner.close()
            if emergency is not None:os.close(emergency)
    import json
    print(json.dumps({'status':'ACTUAL_OWNED_RETIREMENT_FAILURE_RECOVERY_ONLY','cases':records}))

if __name__=='__main__':main()
