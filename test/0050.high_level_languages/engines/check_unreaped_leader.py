#!/usr/bin/env python3
"""Exercise only real tiny native children for the initial-zombie ownership fix."""
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
    import bench_engines
    import owned_process
    from owned_process import remember_process, terminate_group, owned_process_record, _stat
    from vtune_reports import durable_json, require
    before = admission()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    require(os.sched_getaffinity(0) == {8}, 'probe is assigned CPU 8')
    args.out.mkdir(parents=True, exist_ok=False)
    record = {'passed': False, 'purpose': 'real native lifecycle proof; no Wasm guest or performance run',
              'helper_sha256': digest(args.helpers / 'owned_process.py'), 'test_sha256': digest(__file__),
              'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
              'admission_before': before, 'cases': []}
    libc, old = ctypes.CDLL(None, use_errno=True), ctypes.c_int()
    require(libc.prctl(37, ctypes.byref(old), 0, 0, 0) == 0 and libc.prctl(36, 1, 0, 0, 0) == 0,
            'subreaper setup failed')
    children, witnesses = [], {}

    def retain(pid):
        witnesses[pid] = os.pidfd_open(pid)

    def ended(fd):
        poller = select.poll()
        poller.register(fd, select.POLLIN | select.POLLHUP | select.POLLERR)
        return bool(poller.poll(0))

    def spawn(argv, **kwargs):
        child = subprocess.Popen(argv, start_new_session=True, **kwargs)
        children.append(child)
        retain(child.pid)
        return child

    def zombie(child):
        result = os.waitid(os.P_PID, child.pid, os.WEXITED | os.WNOWAIT)
        stat = _stat(child.pid)
        require(result.si_pid == child.pid and child.returncode is None and stat['state'] == 'Z'
                and stat['ppid'] == os.getpid() and stat['pgrp'] == stat['session'] == child.pid
                and ended(witnesses[child.pid]), 'real unreaped direct leader was not constructed')
        return stat

    try:
        child = spawn(['/bin/false'])
        initial = zombie(child)
        owner_record = remember_process(child, force=True)
        require(owner_record['initial_leader_already_ended'] and not child._uwvm_owned_process._group_anchor()
                and child._uwvm_owned_process._unreaped_leader_anchor(), 'zombie was not a separate historical group anchor')
        wrong = dict(initial, start=initial['start'] + 1)
        require(child._uwvm_owned_process._capture(wrong, 'leader', lambda current: True, initial_leader=True) is None,
                'deliberately wrong start witness accepted')
        terminate_group(child, term_timeout=.2, kill_timeout=1)
        final = owned_process_record(child)
        require(child.returncode == 1 and final['closed'] and not final['signals'], 'ended-only leader was signalled/not reaped')
        require(not child._uwvm_owned_process._unreaped_leader_anchor(), 'closed/reaped anchor retained')
        record['cases'].append({'name': 'initial-unreaped-false-and-wrong-start', 'passed': True,
                                'initial': initial, 'wrong_start_is_explicit_fault_witness': wrong, 'final': final})
        live = spawn([sys.executable, '-c', "import sys; assert sys.stdin.buffer.read(1)==b'X'"],
                     stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        original_stat, boundary = owned_process._stat, {}
        def cross_boundary(pid):
            value = original_stat(pid)
            if pid == live.pid and not boundary:
                require(value is not None and value['state'] != 'Z', 'first registration stat was not live')
                boundary['actual_first_stat'] = value
                live.stdin.write(b'X')
                live.stdin.flush()
                live.stdin.close()
                os.waitid(os.P_PID, live.pid, os.WEXITED | os.WNOWAIT)
                boundary['actual_second_stat'] = original_stat(pid)
            return value
        owned_process._stat = cross_boundary
        try:
            crossed = remember_process(live, force=True)
        finally:
            owned_process._stat = original_stat
        require(crossed['initial_leader_already_ended'] and boundary['actual_second_stat']['state'] == 'Z',
                'real live-to-Z initial registration boundary failed')
        require(live.poll() == 0 and not live._uwvm_owned_process._unreaped_leader_anchor(),
                'Popen poll did not immediately revoke the unreaped group anchor')
        live._uwvm_owned_process.observe(force=True)
        terminate_group(live, term_timeout=.2, kill_timeout=1)
        require(owned_process_record(live)['closed'] and not owned_process_record(live)['signals'],
                'already ended/reaped boundary process was signalled')
        record['cases'].append({'name': 'live-to-z-boundary-and-poll-revocation', 'passed': True,
                                'boundary_is_real_stat_then_waitid_wnowait': boundary,
                                'final': owned_process_record(live)})
        reaped = spawn(['/bin/true'])
        require(reaped.wait(timeout=5) == 0, 'reaped fixture failed')
        try:
            remember_process(reaped, force=True)
        except AssertionError as error:
            record['cases'].append({'name': 'already-reaped-refused', 'passed': True, 'error': str(error)})
        else:
            raise AssertionError('already reaped leader accepted')
        foreign = spawn(['/bin/sleep', '120'])
        foreign_before = _stat(foreign.pid)
        code = "import subprocess; p=subprocess.Popen(['/bin/sleep','120']); print(p.pid,flush=True)"
        leader = spawn([sys.executable, '-u', '-c', code], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        require(select.select([leader.stdout], [], [], 10)[0], 'helper pid timeout')
        helper_pid = int(leader.stdout.readline())
        retain(helper_pid)
        leader_before = zombie(leader)
        helper_before = _stat(helper_pid)
        require(helper_before['pgrp'] == helper_before['session'] == leader.pid
                and not ended(witnesses[helper_pid]), 'helper is not alive in the original new group')
        observed = remember_process(leader, force=True)
        members = {item['identity']['pid']: item for item in observed['members']}
        require(observed['initial_leader_already_ended'] and helper_pid in members
                and members[helper_pid]['origin'] == 'unreaped-leader-session-group'
                and foreign.pid not in members, 'unreaped leader helper discovery/foreign exclusion failed')
        terminate_group(leader, term_timeout=.5, kill_timeout=2)
        final = owned_process_record(leader)
        require(final['closed'] and leader.returncode == 0 and ended(witnesses[helper_pid]), 'helper not retired')
        require(os.waitpid(helper_pid, 0)[0] == helper_pid, 'helper not reaped')
        require(not ended(witnesses[foreign.pid]) and _stat(foreign.pid)['start'] == foreign_before['start']
                and all(item['pid'] == helper_pid for item in final['signals']), 'foreign or ended leader was signalled')
        record['cases'].append({'name': 'zombie-first-same-session-helper-and-foreign', 'passed': True,
                                'leader_before': leader_before, 'helper_before': helper_before,
                                'foreign_before': foreign_before, 'observed': observed, 'final': final,
                                'foreign_alive_and_untouched': True})
        # Force only the registration scheduling boundary, using waitid WNOWAIT
        # on a real direct child; invoke and its actual /proc/RSS checks remain
        # unchanged. This must retire safely and remain an invalid sample.
        original_remember = bench_engines.remember_process
        def delayed_remember(process, force=False):
            if getattr(process, '_uwvm_owned_process', None) is None:
                os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOWAIT)
            return original_remember(process, force=force)
        bench_engines.remember_process = delayed_remember
        try:
            result = bench_engines.invoke(['/bin/false'], dict(os.environ), args.out / 'invoke-false', timeout=5)
        finally:
            bench_engines.remember_process = original_remember
        require(result['exit'] == 1 and result['retired'] and not result['cleanup_error'] and result['error']
                and result['process_ownership']['initial_leader_already_ended']
                and result['process_ownership']['closed'] and not result['process_ownership']['signals'],
                'invoke must retain safe retirement but reject the invalid early-exit sample')
        record['cases'].append({'name': 'invoke-delayed-registration-remains-na', 'passed': True,
                                'scheduling_boundary': 'real waitid WNOWAIT before registration',
                                'qualified_as_performance': False, 'result': result})
        record['passed'] = True
    except BaseException as error:
        record['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        for child in children:
            if owned_process_record(child) is not None:
                terminate_group(child, term_timeout=.1, kill_timeout=2)
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
        require(libc.prctl(36, old.value, 0, 0, 0) == 0, 'subreaper restore failed')
        record['all_explicit_fixture_pids_absent'] = all(not Path(f'/proc/{pid}').exists() for pid in witnesses)
        record['admission_after'] = admission()
        record['passed'] = (record['passed'] and record['all_explicit_fixture_pids_absent']
                            and before == record['admission_after']
                            and digest(args.helpers / 'owned_process.py') == record['helper_sha256'])
        durable_json(args.out / 'receipt.json', record)
    print(json.dumps({'passed': record['passed'], 'cases': [item['name'] for item in record['cases']],
                      'all_fixture_pids_absent': record['all_explicit_fixture_pids_absent']}), flush=True)


if __name__ == '__main__':
    main()
