#!/usr/bin/env python3
"""Own the frozen stock harness, giving the parent's large DBG memory priority."""
import json
import os
from pathlib import Path
import sys

import linux_control


def main():
    arena = Path('/tmp/high-level-20261017/python')
    own = Path(__file__).resolve().parent
    harness = own / 'stock_stdlib.py'
    assert linux_control.digest(harness) == 'b6e4b38b720fe9b6cc155b0f7d840b127e4f2cca4c62a839aec3692c06fe8f4f'
    os.sched_setaffinity(0, {4})
    guard = linux_control.guard
    before = guard(arena)
    checks = 0
    stops = []

    def priority_guard(path):
        nonlocal checks
        state = guard(path)
        checks += 1
        # First call establishes invoke's receipt. A subsequent guard fires
        # once; cleanup/final metadata must remain able to write its receipt
        # even if the independent DBG still owns more than50GiB afterwards.
        if checks > 1 and not stops and state['anon'] >= 50 << 30:
            stops.append({'shared_anon': state['anon'], 'reason': 'parent DBG priority >=50GiB'})
            raise RuntimeError('stock stopped for parent DBG priority at50GiB shared anonymous memory')
        return state

    linux_control.guard = priority_guard
    record = {'passed': False, 'before': before, 'stops': stops,
              'launcher_sha256': linux_control.digest(Path(__file__)),
              'frozen_harness_sha256': linux_control.digest(harness),
              'shared_anon_priority_limit': 50 << 30, 'owned_rss_limit': 8 << 30}
    try:
        result = linux_control.invoke([
            sys.executable, '-B', str(harness), '--build', str(arena / 'work-r4'),
            '--out', str(arena / 'stock-stdlib-r1'),
            '--wasmtime', '/home/macromodel/.wasmtime/bin/wasmtime', '--timeout', '180'],
            arena / 'stock-stdlib-r1-launcher', timeout=1000)
        record['passed'] = result['passed']
    except BaseException as error:
        record['error'] = repr(error)
    finally:
        record['checks'] = checks
        record['after'] = guard(arena)
        (arena / 'stock-stdlib-r1-priority.json').write_text(json.dumps(record, indent=2) + '\n')
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
