#!/usr/bin/env python3
"""Own and guard a Linux-only run_matrix process, preserving failure receipts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import time

from build_products import admission
from owned_process import remember_process, terminate_group, owned_process_record


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--repository', required=True, choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    p.add_argument('--cpu', type=int, default=21)
    p.add_argument('--timeout', type=int, default=14400)
    p.add_argument('--skip-debug', action='store_true')
    p.add_argument('--case', action='append')
    p.add_argument('--rss-limit-gib', type=int, default=48)
    a = p.parse_args()
    before = admission()
    assert 1 <= a.rss_limit_gib <= 56
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.sched_setaffinity(0, {a.cpu})
    log = a.out.with_suffix('.launcher.log')
    receipt = a.out.with_suffix('.launcher.json')
    assert not a.out.exists() and not receipt.exists() and not log.exists()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    cg = Path('/sys/fs/cgroup') / Path('/proc/self/cgroup').read_text().strip()[3:].lstrip('/')
    command = [sys.executable, '-B', str(Path(__file__).with_name('run_matrix.py')),
               '--arena', str(a.arena), '--manifest', str(a.manifest), '--out', str(a.out),
               '--repository', a.repository, '--samples', '1', '--cpu', str(a.cpu)]
    if a.skip_debug:
        command.append('--skip-debug')
    for name in a.case or []:
        command.extend(['--case', name])
    row = {'passed': False, 'before': before, 'argv': command,
           'manifest_sha256': hashlib.sha256(a.manifest.read_bytes()).hexdigest(),
           'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
           'peak_owned_rss': 0, 'peak_shared_anon': 0}
    child = None
    started = time.monotonic()
    try:
        with log.open('wb') as output:
            child = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                     start_new_session=True)
            remember_process(child)
            while True:
                record = remember_process(child)
                total = 0
                for member in record['members']:
                    try:
                        identity = member['identity']
                        proc = Path(f"/proc/{identity['pid']}")
                        start = int((proc / 'stat').read_text().rpartition(')')[2].split()[19])
                        if start != identity['start']:
                            continue
                        status = (proc / 'status').read_text()
                        total += next(int(line.split()[1]) * 1024 for line in status.splitlines()
                                      if line.startswith('VmRSS:'))
                    except (FileNotFoundError, ProcessLookupError, StopIteration):
                        pass
                anon = int(dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon'])
                row['peak_owned_rss'] = max(row['peak_owned_rss'], total)
                row['peak_shared_anon'] = max(row['peak_shared_anon'], anon)
                assert total < a.rss_limit_gib << 30 and anon < 58 << 30, 'functional memory guard'
                assert time.monotonic() - started < a.timeout, 'functional deadline'
                code = child.poll()
                if code is not None:
                    row['exit'] = code
                    break
                time.sleep(.5)
        result = json.loads((a.out / 'report.json').read_text())
        assert row['exit'] == 0 and result['passed'], 'functional report failed'
        row['passed'] = True
    except BaseException as error:
        row['error'] = repr(error)
    finally:
        if child is not None:
            terminate_group(child)
            row['owned_retirement'] = owned_process_record(child)
        row['seconds'] = time.monotonic() - started
        row['after'] = admission()
        if row['before']['memory_events'] != row['after']['memory_events']:
            row['passed'] = False
            row['memory_events_changed'] = True
        receipt.write_text(json.dumps(row, indent=2) + '\n')
    print(json.dumps({'passed': row['passed'], 'receipt': str(receipt), 'seconds': row['seconds']}), flush=True)
    raise SystemExit(0 if row['passed'] else 1)


if __name__ == '__main__':
    main()
