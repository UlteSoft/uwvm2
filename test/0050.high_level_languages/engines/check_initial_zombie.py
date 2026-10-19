#!/usr/bin/env python3
"""Native Linux lifecycle probes; no Wasm guest, collector or benchmark runs."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import resource
import select
import signal
import subprocess
import sys

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helpers', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.helpers))
    from build_products import admission, digest
    from owned_process import remember_process, terminate_group, owned_process_record, _stat
    from vtune_reports import durable_json, require
    before = admission()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    require(os.sched_getaffinity(0) == {8}, 'probe is assigned CPU 8')
    args.out.mkdir(parents=True, exist_ok=False)
    data = {'passed': False, 'scope': 'baseline direct-zombie race plus existing live-helper/foreign ownership contracts',
            'does_not_qualify_a_patch_or_performance': True, 'admission_before': before,
            'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'helper_sha256': digest(args.helpers / 'owned_process.py'), 'test_sha256': digest(__file__)}
    libc = ctypes.CDLL(None, use_errno=True)
    old = ctypes.c_int()
    require(libc.prctl(37, ctypes.byref(old), 0, 0, 0) == 0, 'GET_CHILD_SUBREAPER failed')
    require(libc.prctl(36, 1, 0, 0, 0) == 0, 'SET_CHILD_SUBREAPER failed')
    children, witnesses, leader, foreign = [], {}, None, None

    def retain(pid):
        witnesses[pid] = os.pidfd_open(pid)

    def ended(fd):
        poller = select.poll()
        poller.register(fd, select.POLLIN | select.POLLHUP | select.POLLERR)
        return bool(poller.poll(0))

    try:
        zombie = subprocess.Popen(['/bin/false'], start_new_session=True)
        children.append(zombie)
        retain(zombie.pid)
        wait = os.waitid(os.P_PID, zombie.pid, os.WEXITED | os.WNOWAIT)
        stat = _stat(zombie.pid)
        require(wait.si_pid == zombie.pid and zombie.returncode is None and stat['state'] == 'Z'
                and stat['ppid'] == os.getpid() and stat['pgrp'] == stat['session'] == zombie.pid
                and ended(witnesses[zombie.pid]), 'did not construct a real unreaped direct-child zombie')
        data['direct_zombie_before_registration'] = stat
        try:
            remember_process(zombie, force=True)
        except AssertionError as error:
            data['baseline_initial_registration_rejection'] = str(error)
        else:
            raise AssertionError('baseline unexpectedly accepted direct zombie; review helper/version')
        try:
            terminate_group(zombie)
        except AssertionError as error:
            data['baseline_cleanup_registration_rejection'] = str(error)
        else:
            raise AssertionError('baseline unexpectedly accepted zombie during cleanup')
        require(zombie.wait(timeout=2) == 1 and not Path(f'/proc/{zombie.pid}').exists(), 'direct zombie was not reaped')
        data['baseline_race_reproduced'] = True
        foreign = subprocess.Popen(['/bin/sleep', '120'], start_new_session=True)
        children.append(foreign)
        retain(foreign.pid)
        data['foreign_before'] = _stat(foreign.pid)
        code = ("import subprocess,sys; p=subprocess.Popen(['/bin/sleep','120']); "
                "print(p.pid,flush=True); assert sys.stdin.buffer.read(1)==b'X'")
        leader = subprocess.Popen([sys.executable, '-u', '-c', code], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        children.append(leader)
        retain(leader.pid)
        remember_process(leader, force=True)
        require(select.select([leader.stdout], [], [], 10)[0], 'owned helper startup timeout')
        helper_pid = int(leader.stdout.readline())
        retain(helper_pid)
        data['owned_helper_before'] = _stat(helper_pid)
        ownership = remember_process(leader, force=True)
        members = {item['identity']['pid'] for item in ownership['members']}
        require({leader.pid, helper_pid} <= members and foreign.pid not in members,
                'live parent/helper discovery or foreign exclusion failed')
        data['ownership_before_leader_exit'] = ownership
        leader.stdin.write(b'X')
        leader.stdin.flush()
        leader.stdin.close()
        require(leader.wait(timeout=5) == 0, 'owned leader failed')
        terminate_group(leader, term_timeout=1, kill_timeout=2)
        data['ownership_after_cleanup'] = owned_process_record(leader)
        require(data['ownership_after_cleanup']['closed'] and ended(witnesses[helper_pid]), 'owned helper remains live')
        require(os.waitpid(helper_pid, 0)[0] == helper_pid, 'owned helper not reaped by probe subreaper')
        data['foreign_after'] = _stat(foreign.pid)
        require(data['foreign_after'] == data['foreign_before'] and not ended(witnesses[foreign.pid])
                and all(item['pid'] != foreign.pid for item in data['ownership_after_cleanup']['signals']),
                'unrelated session was touched')
        data['live_anchor_helper_and_foreign_probe_passed'] = True
        data['passed'] = True
    except BaseException as error:
        data['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        if leader is not None:
            try:
                terminate_group(leader, term_timeout=.1, kill_timeout=2)
            except Exception as error:
                data['ownership_cleanup_error'] = repr(error)
        for pid, fd in witnesses.items():
            try:
                if not ended(fd):
                    signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
        for child in children:
            child.wait(timeout=5)
        for pid, fd in witnesses.items():
            try:
                os.waitpid(pid, os.WNOHANG)
            except ChildProcessError:
                pass
            os.close(fd)
        require(libc.prctl(36, old.value, 0, 0, 0) == 0, 'restore child-subreaper failed')
        data['all_fixture_pids_absent'] = all(not Path(f'/proc/{pid}').exists() for pid in witnesses)
        data['admission_after'] = admission()
        data['passed'] = (data['passed'] and data['all_fixture_pids_absent']
                          and data['admission_before'] == data['admission_after']
                          and digest(args.helpers / 'owned_process.py') == data['helper_sha256'])
        durable_json(args.out / 'receipt.json', data)
    print(json.dumps(data, indent=2), flush=True)


if __name__ == '__main__':
    main()
