#!/usr/bin/env python3
"""Record a small Linux-cgroup workload without changing shared environment state."""
import argparse
import hashlib
import importlib.util
import json
import os
import resource
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--cwd', type=Path, required=True)
parser.add_argument('--timeout', type=int, default=120)
parser.add_argument('--rss-mib', type=int, default=2048)
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
if args.command[:1] == ['--']:
    args.command = args.command[1:]
spec = importlib.util.spec_from_file_location('wsea_cgroup_probe', Path(__file__).resolve().parents[1] / 'ros_cgroup/probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
before = probe.probe()
if before['status'] != 'PASS' or not set(os.sched_getaffinity(0)) <= {16, 17}:
    print('BLOCKED: requires original cgroup and taskset -c 16,17')
    sys.exit(77)
disk_before = shutil.disk_usage(args.cwd)
if disk_before.free < 3 * 1024**3:
    print('BLOCKED: original filesystem has less than 3 GiB free')
    sys.exit(77)
args.report.parent.mkdir(parents=True, exist_ok=True)
log_path = args.report.with_suffix('.log')
report = {'status': 'FAIL', 'argv': args.command, 'cwd': str(args.cwd), 'cgroup_before': before,
          'affinity': sorted(os.sched_getaffinity(0)), 'max_aggregate_rss_bytes': 0,
          'processes': {}, 'started_unix': int(time.time()), 'filesystem_before': dict(disk_before._asdict()),
          'driver_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
started = time.monotonic()
with log_path.open('wb') as output:
    process = subprocess.Popen(args.command, cwd=args.cwd, stdout=output, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        while True:
            pids = [process.pid]
            aggregate = 0
            for pid in pids:
                directory = Path('/proc') / str(pid)
                try:
                    pids.extend(int(v) for v in (directory / 'task' / str(pid) / 'children').read_text().split())
                    status = dict(line.split(':', 1) for line in (directory / 'status').read_text().splitlines() if ':' in line)
                    rss = int(status.get('VmRSS', '0 kB').split()[0]) * 1024
                    aggregate += rss
                    membership = (directory / 'cgroup').read_text().splitlines()
                    stat_text = (directory / 'stat').read_text()
                    birth = stat_text[stat_text.rfind(')') + 2:].split()[19]
                    if membership != before['membership']:
                        raise RuntimeError('owned process left original cgroup')
                    current = report['processes'].setdefault(str(pid), {
                        'pid': pid, 'membership': membership, 'cmdline': (directory / 'cmdline').read_bytes().replace(b'\0', b' ').decode(errors='replace'),
                        'affinity': sorted(os.sched_getaffinity(pid)), 'max_rss_bytes': 0, 'start_time_ticks': birth})
                    if not set(os.sched_getaffinity(pid)) <= {16, 17}:
                        raise RuntimeError('owned process left selected lightweight CPU affinity')
                    current['max_rss_bytes'] = max(current['max_rss_bytes'], rss)
                except (FileNotFoundError, ProcessLookupError):
                    continue
            report['max_aggregate_rss_bytes'] = max(report['max_aggregate_rss_bytes'], aggregate)
            if aggregate > args.rss_mib * 1024 * 1024:
                raise RuntimeError('owned aggregate RSS exceeded lightweight window')
            if time.monotonic() - started > args.timeout:
                raise RuntimeError('owned workload timeout')
            if log_path.stat().st_size > 16 * 1024 * 1024:
                raise RuntimeError('owned workload output exceeded bound')
            if process.poll() is not None:
                break
            time.sleep(0.05)
        report['returncode'] = process.returncode
        report['status'] = 'PASS' if process.returncode == 0 else 'FAIL'
    except Exception as error:
        report['error'] = str(error)
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait(timeout=5)
    finally:
        report['elapsed_seconds'] = round(time.monotonic() - started, 3)
        after = probe.probe()
        report['cgroup_after'] = after
        disk_after = shutil.disk_usage(args.cwd)
        report['filesystem_after'] = dict(disk_after._asdict())
        if disk_after.free < 3 * 1024**3:
            report['status'] = 'FAIL'
        if after['status'] != 'PASS' or before['cgroup_incarnation'] != after['cgroup_incarnation']:
            report['status'] = 'FAIL'
        for old, new in zip(before['cgroup_ancestors'], after['cgroup_ancestors']):
            if old.get('memory.events') is None:
                continue
            previous = dict(line.split() for line in old['memory.events'].splitlines())
            current = dict(line.split() for line in new['memory.events'].splitlines())
            if any(current.get(key) != previous.get(key) for key in ['oom', 'oom_kill']):
                report['status'] = 'FAIL'
        report['log_sha256'] = hashlib.sha256(log_path.read_bytes()).hexdigest()
        survivors = []
        for pid, record in report['processes'].items():
            try:
                stat_text = (Path('/proc') / pid / 'stat').read_text()
                if stat_text[stat_text.rfind(')') + 2:].split()[19] == record['start_time_ticks']:
                    survivors.append(int(pid))
            except FileNotFoundError:
                pass
        report['remaining_owned_processes'] = survivors
        if survivors:
            report['status'] = 'FAIL'
        args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: report[key] for key in ['status', 'elapsed_seconds', 'max_aggregate_rss_bytes']}))
sys.exit(0 if report['status'] == 'PASS' else 1)
