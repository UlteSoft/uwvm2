#!/usr/bin/env python3
"""Require separately demanded cold tail targets, beyond a successful run.

The supplied summary must describe actual cold/warm/warm-again signed-cache
processes with instruction call-stack policy. Compiler events establish that
the first tail destination was still uncompiled when its caller completed.
The fixture checks fresh locals over two million real Wasm tail transfers.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time

from run_matrix import digest, save, verify


def events(log, kind):
    pattern = r'\[llvm-jit-lazy\] ' + re.escape(kind) + r' .*?\bfn=(\d+)\b'
    return [(match.start(), int(match[1])) for match in re.finditer(pattern, log)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    summary_path = Path(config['summary'])
    deadline = time.monotonic() + config.get('prerequisite_timeout', 3000)
    while not summary_path.is_file():
        state = json.loads(Path(config['execution_stage_status']).read_text())
        assert state['state'] != 'failed', state
        assert time.monotonic() < deadline, 'Waiting for real signed-cache execution'
        time.sleep(10)
    summary_hash = digest(summary_path)
    summary = json.loads(summary_path.read_text())
    assert summary['passed'] and summary['completed'] == len(summary['rows'])
    verify(summary['actual_dependency_pins'])
    for receipt in summary['build_receipts']:
        assert digest(receipt['path']) == receipt['sha256']
    matrix = json.loads(Path(config['matrix_config']).read_text())
    assert digest(config['matrix_config']) == summary['configuration_sha256']
    assert digest(matrix['product']) == summary['product_sha256']
    verify(matrix['pins'])
    fixture_name = 'tail-cold-fresh-locals-2m'
    fixture = next(row for row in json.loads(Path(matrix['fixtures']).read_text())['fixtures']
                   if row['name'] == fixture_name)
    assert digest(fixture['path']) == fixture['sha256']
    rows = []
    for row in summary['rows']:
        if row['fixture'] != fixture_name:
            continue
        assert row['passed'] and row['exit'] == 0
        argv = row['argv']
        assert argv[argv.index('-Rcm') + 1] == 'lazy'
        assert argv[argv.index('-Rct') + 1] == '0', 'Compiler workers would alter initial coldness'
        assert argv[argv.index('-Rllvm-call-stack') + 1] == 'instruction'
        path = Path(row['physical_compiler_log'])
        assert digest(path) == row['compiler_log_sha256']
        log = re.sub(r'\x1b\[[0-9;?]*[ -/]*[@-~]', '', path.read_text())
        requests, starts, ends = [events(log, event) for event in
                                 ('demand-request', 'compile-start', 'compile-end')]
        assert [fn for _, fn in requests] == [2, 0, 1], (row['profile'], requests)
        assert [fn for _, fn in starts] == [2, 0, 1]
        assert [fn for _, fn in ends] == [2, 0, 1]
        assert ends[0][0] < requests[1][0] and ends[1][0] < requests[2][0]
        assert len(row['cache_entries']) == 3, 'Each lazy function needs its own object'
        if row['phase'] == 'cold':
            assert row['signed_cache_hits'] == 0
        else:
            assert row['signed_cache_hits'] == 3
            hit_lines = [line for line in log.splitlines() if 'object-cache-hit module=' in line]
            assert len(hit_lines) == 3 and all('signature_verified=1' in line for line in hit_lines)
        for key, expected in row['cache_entries'].items():
            found = [Path(root) / key for root in row['physical_cache_roots'] if (Path(root) / key).is_file()]
            assert len(found) == 1 and digest(found[0]) == expected
        rows.append(dict(profile=row['profile'], path_mode=row['path_mode'], phase=row['phase'],
                         separately_demanded_functions=[fn for _, fn in requests],
                         tail_target_demand_after_caller_compile=True, compiler_workers=0,
                         cache_objects=3, signed_cache_hits=row['signed_cache_hits'],
                         compiler_log_sha256=row['compiler_log_sha256']))
    assert len(rows) == len(matrix['profiles']) * 2 * 3, len(rows)
    verify(config['pins'])
    verify(matrix['pins'])
    verify(summary['actual_dependency_pins'])
    assert digest(summary_path) == summary_hash
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(config['output'], dict(passed=True, completed=len(rows), rows=rows,
         source_identities=summary['source_identities'], product_sha256=summary['product_sha256'],
         signed_execution_summary_sha256=summary_hash, fixture_sha256=fixture['sha256'],
         configuration_sha256=digest(args.config), code_sha256=digest(__file__),
         scope='Actual lazy instruction-stack-policy processes; caller compiled before tail target demand, checked fresh locals, three signed cache objects. Listed profiles only.'))
    print('PASS witnessed cold lazy tail targets', len(rows), flush=True)


if __name__ == '__main__':
    main()
