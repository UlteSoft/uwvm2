#!/usr/bin/env python3
"""Released stock r2 functional oracle run, with priority for parent DBG jobs."""
import json
import os
from pathlib import Path
import resource

import linux_control

A = Path('/tmp/high-level-20261017')
P = A / 'uwvm2-ros/test/0050.high_level_languages'
products = A / 'fd-readdir-candidate-r2/products'
helper = P / 'python/stock_stdlib.py'
policy = P / 'engines/r4-preparation/stock-load-args-policy.json'
expected = {'uwvm2': '31ad5edb0149b6c0315614416bd491a612234860cfefad9fb3a2d3a7adb06f09',
            'uwvm2-stable': '4d200c723504904f8206d75498f6ba0dbc562926edbd8b78e611e7a9419321c2',
            'uwvm2-ros': '6e18792a93074734976b76277c1fd44bb0a00678110ab2dca0a4cd86df7cb9dc'}
assert linux_control.digest(helper) == '634d591afe6d843cc6c9756c014f9dd439f0123ad71a70d2acc533ab42885cd2'
assert linux_control.digest(P / 'run_matrix.py') == '97cb91ddfdb9d3cde1ae4419e3bcfe3a476c6f595c9bb205734f51a13a2000f3'
assert linux_control.digest(policy) == '38bb55cd06ee91e4eb0cb480a189b6853457a6b21110cc157c8ba29b6bd9607e'
assert linux_control.digest(P / 'python/stock_stdlib_probe.py') == 'cc1ed7faeb9b2a9eb56420f3deafe7db5fae7fda19b24047427e9fd54465b6ce'
assert all(linux_control.digest(products / name / 'uwvm') == sha for name, sha in expected.items())
os.sched_setaffinity(0, {4})
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
read_guard = linux_control.guard
record = {'passed': False, 'before': read_guard(A), 'guard_stops': [],
          'controller_sha256': linux_control.digest(Path(__file__)),
          'helper_sha256': linux_control.digest(helper), 'policy_sha256': linux_control.digest(policy),
          'products': expected, 'shared_anon_priority_limit': 50 << 30,
          'owned_rss_limit': 8 << 30, 'disk_floor': 2 << 30}


def guarded(path):
    state = read_guard(path)
    if state['anon'] >= 50 << 30 and not record['guard_stops']:
        record['guard_stops'].append({'reason': 'parent DBG shared anonymous priority50GiB', 'state': state})
        raise RuntimeError('stock stopped for parent DBG memory priority')
    return state


linux_control.guard = guarded
out = A / 'python/stock-stdlib-r2'
launch = A / 'python/stock-stdlib-r2-launcher'
summary = A / 'python/stock-stdlib-r2-priority.json'
assert not summary.exists() and not out.exists() and not launch.exists()
try:
    argv = ['/usr/bin/python3', str(helper), '--build', str(A / 'python/work-r4'),
            '--out', str(out), '--wasmtime', '/home/macromodel/.wasmtime/bin/wasmtime',
            '--products', str(products), '--load-args-policy', str(policy), '--timeout', '180']
    record['argv'] = argv
    record['process'] = linux_control.invoke(argv, launch, timeout=1000)
    result = json.loads((out / 'report.json').read_bytes())
    record['stock_report_sha256'] = linux_control.digest(out / 'report.json')
    record['passed'] = bool(result['passed'] and not record['guard_stops'])
except BaseException as error:
    record['passed'] = False
    record['error'] = repr(error)
    if (launch / 'receipt.json').exists():
        record['process'] = json.loads((launch / 'receipt.json').read_bytes())
    raise
finally:
    record['after'] = read_guard(A)
    summary.write_text(json.dumps(record, indent=2) + '\n')
assert record['passed'], 'stock r2 did not pass all actual engines/probes'
