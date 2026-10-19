#!/usr/bin/env python3
"""Real Linux pidfd lifecycle proof in the existing 64 GiB cgroup, no VM/profile."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time


def readable(fd):
    poller = select.poll()
    poller.register(fd, select.POLLIN)
    return bool(poller.poll(0))


def line(process):
    assert select.select([process.stdout], [], [], 15)[0], 'fixture startup timeout'
    return process.stdout.readline().decode().strip()


def identity(pid):
    fields = Path(f'/proc/{pid}/stat').read_bytes().rpartition(b')')[2].split()
    return {'pid': pid, 'ppid': int(fields[1]), 'pgrp': int(fields[2]),
            'session': int(fields[3]), 'start': int(fields[19]),
            'cgroup': Path(f'/proc/{pid}/cgroup').read_text().strip(),
            'affinity': sorted(os.sched_getaffinity(pid))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helpers', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    assert sys.platform == 'linux'
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission, digest
    from owned_process import remember_process, terminate_group, owned_process_record
    before = admission()
    assert os.sched_getaffinity(0) == {4, 5}, 'this functional test is assigned CPUs 4-5'
    args.out.mkdir(parents=True, exist_ok=False)
    helper = args.helpers / 'owned_process.py'
    record = {'passed': False, 'admission_before': before, 'helper_sha256': digest(helper),
              'test_sha256': digest(__file__), 'scope': 'real owned-helper lifecycle; not a performance test'}
    libc = ctypes.CDLL(None, use_errno=True)
    old_subreaper = ctypes.c_int()
    assert libc.prctl(37, ctypes.byref(old_subreaper), 0, 0, 0) == 0
    assert libc.prctl(36, 1, 0, 0, 0) == 0
    # Adopt our explicit helpers when their leader exits, so this test can also
    # reap them and prove no owned orphan or zombie remains after cleanup.
    child = foreign = None
    witnesses, owned_pids = {}, []
    cleanup_errors = []
    try:
        foreign_code = ("import ctypes,time; c=ctypes.CDLL(None); "
                        "assert c.prctl(15,ctypes.c_char_p(b'\\xffuwvm-foreign'),0,0,0)==0; "
                        "print('FOREIGN_READY',flush=True); time.sleep(120)")
        foreign = subprocess.Popen([sys.executable, '-c', foreign_code], stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        witnesses[foreign.pid] = os.pidfd_open(foreign.pid)
        assert line(foreign) == 'FOREIGN_READY'
        record['unrelated_before'] = identity(foreign.pid)
        helper_code = ("import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                       "print('HELPER_READY',flush=True); time.sleep(120)")
        leader_code = ("import json,subprocess,sys; code=" + repr(helper_code) + "; "
                       "a=subprocess.Popen([sys.executable,'-c',code],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE); "
                       "b=subprocess.Popen([sys.executable,'-c',code],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,start_new_session=True); "
                       "assert a.stdout.readline()==b'HELPER_READY\\n'; "
                       "assert b.stdout.readline()==b'HELPER_READY\\n'; "
                       "print(json.dumps({'grouped':a.pid,'detached':b.pid}),flush=True); "
                       "assert sys.stdin.buffer.read(1)==b'X'")
        child = subprocess.Popen([sys.executable, '-c', leader_code], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        remember_process(child, force=True)
        payload = json.loads(line(child))
        owned_pids = [payload['grouped'], payload['detached']]
        for pid in owned_pids:
            witnesses[pid] = os.pidfd_open(pid)
        record['observed_children'] = {name: identity(pid) for name, pid in payload.items()}
        assert record['observed_children']['grouped']['session'] == child.pid
        assert record['observed_children']['detached']['session'] == payload['detached']
        remembered = remember_process(child, force=True)
        captured = {member['identity']['pid'] for member in remembered['members']}
        assert {child.pid, *owned_pids} <= captured and foreign.pid not in captured, remembered
        record['ownership_before_leader_exit'] = remembered
        child.stdin.write(b'X')
        child.stdin.flush()
        child.stdin.close()
        assert child.wait(timeout=5) == 0
        record['leader_reaped_before_cleanup'] = True
        assert all(not readable(witnesses[pid]) for pid in owned_pids), 'helper exited before lifecycle test'
        remember_process(child, force=True)
        terminate_group(child, term_timeout=.2, kill_timeout=3)
        ledger = owned_process_record(child)
        record['ownership_after_cleanup'] = ledger
        assert ledger['closed'] and not any(member['pidfd_retained'] for member in ledger['members'])
        for pid in owned_pids:
            numbers = {event['signal'] for event in ledger['signals'] if event['pid'] == pid}
            assert {signal.SIGTERM, signal.SIGKILL} <= numbers, ledger
            assert readable(witnesses[pid]), 'owned helper is still live'
            got, status = os.waitpid(pid, os.WNOHANG)
            assert got == pid and os.waitstatus_to_exitcode(status) == -signal.SIGKILL, (pid, got, status)
            assert not Path(f'/proc/{pid}').exists(), 'owned helper was not reaped'
        terminate_group(child)  # Idempotent even after pidfds were closed.
        record['no_owned_orphans_or_zombies'] = True
        record['unrelated_after'] = identity(foreign.pid)
        assert record['unrelated_before'] == record['unrelated_after']
        assert foreign.poll() is None and not readable(witnesses[foreign.pid])
        assert all(event['pid'] != foreign.pid for event in ledger['signals'])
        record['unrelated_session_untouched'] = True
    except BaseException as error:
        record['error'] = repr(error)
    finally:
        if child is not None:
            try:
                terminate_group(child, term_timeout=.1, kill_timeout=2)
            except Exception as error:
                cleanup_errors.append(repr(error))
        for pid, fd in witnesses.items():
            try:
                if not readable(fd):
                    signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(fd)
        if child is not None:
            child.wait(timeout=5)
        if foreign is not None:
            foreign.wait(timeout=5)
        for pid in owned_pids:
            try:
                os.waitpid(pid, 0)
            except ChildProcessError:
                pass
        assert libc.prctl(36, old_subreaper.value, 0, 0, 0) == 0
        record['cleanup_errors'] = cleanup_errors
        record['admission_after'] = admission()
        record['helper_unchanged'] = digest(helper) == record['helper_sha256']
        record['all_fixture_pids_retired'] = all(not Path(f'/proc/{pid}').exists() for pid in witnesses)
        record['passed'] = (not cleanup_errors and 'error' not in record and record['helper_unchanged']
                            and record['all_fixture_pids_retired'] and before == record['admission_after'])
        (args.out / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(record, indent=2), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
