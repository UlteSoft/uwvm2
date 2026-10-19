#!/usr/bin/env python3
"""Read-only host PMU consumer snapshot before a controlled sampling window.

Record process identity and relevant descriptor types only, never command lines.
This point-in-time observation cannot exclude a consumer starting afterwards.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from build_products import admission


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    before = admission()
    assert not args.out.exists(), 'retain each independent PMU observation'
    rows, unreadable, races = [], 0, 0
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            stat_before = (proc / 'stat').read_text().rpartition(')')[2].split()
            relevant = []
            for fd in (proc / 'fd').iterdir():
                try:
                    target = os.readlink(fd)
                except FileNotFoundError:
                    continue
                if target == 'anon_inode:[perf_event]' or target.startswith(('/dev/sep', '/dev/pax', '/dev/emon')):
                    relevant.append({'fd': int(fd.name), 'type': target})
            stat_after = (proc / 'stat').read_text().rpartition(')')[2].split()
            if stat_before[19] != stat_after[19]:
                races += 1
                continue
            if relevant:
                rows.append({'pid': int(proc.name), 'start_ticks': int(stat_before[19]),
                             'name': (proc / 'comm').read_text().strip(), 'descriptors': relevant})
        except (FileNotFoundError, ProcessLookupError):
            races += 1
        except PermissionError:
            unreadable += 1
    after = admission()
    report = {'scope': 'read-only point-in-time descriptor observation; no foreign processes signalled',
              'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
              'helper_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'before': before, 'after': after, 'consumers': rows,
              'unreadable_processes': unreadable, 'process_races': races,
              'observed_idle': not rows and unreadable == 0,
              'limitation': 'does not reserve PMUs or observe other consumers for the whole subsequent window'}
    assert before['membership'] == after['membership'] and before['memory_events'] == after['memory_events']
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'receipt': str(args.out), 'observed_idle': report['observed_idle'],
                      'consumers': len(rows), 'unreadable': unreadable, 'races': races}))
    raise SystemExit(0 if report['observed_idle'] else 2)


if __name__ == '__main__':
    main()
