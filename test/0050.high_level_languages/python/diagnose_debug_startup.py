#!/usr/bin/env python3
"""Bounded native backtrace of an owned Python/Wasm debugger startup.

This diagnostic stops the native process briefly with GDB. It is excluded from
functional qualification and performance measurements; it executes no GDB calls
inside the target and does not change its memory or debugger policy.
"""
import argparse
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build_products import admission, digest
from owned_process import remember_process, terminate_group, owned_process_record
from run_matrix import argv_for, process_memory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--delay', type=int, default=20)
    args = parser.parse_args()
    assert 1 <= args.delay <= 60
    before = admission()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.sched_setaffinity(0, {4})
    sys.path.insert(0, str(args.arena / 'uwvm2-ros/test/0017.runtime'))
    from run_debug_run_cli import Console
    case, = [row for row in json.loads(args.manifest.read_text()) if not row.get('skip_debug')]
    binary = args.arena / 'products/uwvm2/uwvm'
    case = dict(case, debug_compile_threads=0)
    command = argv_for(binary, 'uwvm2', case, True)
    args.out.mkdir(parents=True, exist_ok=False)
    receipt = {'passed': False, 'performance_eligible': False, 'functional_qualification': False,
               'scope': __doc__, 'before': before, 'argv': command,
               'binary_sha256': digest(binary), 'wasm_sha256': digest(case['wasm']),
               'helper_sha256': digest(__file__), 'memory': []}
    console = gdb = None
    try:
        console = Console(command)
        deadline = time.monotonic() + 5
        while digest(Path(f'/proc/{console.pid}/exe')) != receipt['binary_sha256']:
            assert time.monotonic() < deadline, 'owned child exec deadline'
            time.sleep(.02)
        def identity():
            proc = Path(f'/proc/{console.pid}')
            stat = (proc / 'stat').read_text().rpartition(')')[2].split()
            assert (proc / 'cgroup').read_text() == Path('/proc/self/cgroup').read_text()
            assert digest(proc / 'exe') == receipt['binary_sha256']
            return {'pid': console.pid, 'start': int(stat[19]), 'parent': int(stat[1])}
        receipt['target'] = identity()
        started = time.monotonic()
        while time.monotonic() - started < args.delay:
            memory = process_memory(console.pid)
            receipt['memory'].append({'elapsed': time.monotonic() - started, **memory})
            assert memory['VmRSS'] < 10 << 30, 'diagnostic memory bound'
            time.sleep(.25)
        assert identity() == receipt['target']
        gdb_path = Path('/usr/bin/gdb')
        receipt['gdb_sha256'] = digest(gdb_path)
        gdb_command = [str(gdb_path), '-nx', '-nh', '-batch',
                       '-ex', 'set debuginfod enabled off',
                       '-ex', 'set auto-load python-scripts off',
                       '-ex', 'set pagination off', '-ex', 'set confirm off',
                       '-ex', f'attach {console.pid}', '-ex', 'thread apply all bt 15',
                       '-ex', 'detach', '-ex', 'quit']
        receipt['gdb_argv'] = gdb_command
        with (args.out / 'gdb.stdout.log').open('wb') as output, (args.out / 'gdb.stderr.log').open('wb') as error:
            gdb = subprocess.Popen(gdb_command, stdout=output, stderr=error, start_new_session=True)
            remember_process(gdb)
            deadline = time.monotonic() + 45
            while True:
                remember_process(gdb)
                assert time.monotonic() < deadline, 'native backtrace deadline'
                code = gdb.poll()
                if code is not None:
                    receipt['gdb_exit'] = code
                    break
                time.sleep(.1)
        assert identity() == receipt['target']
        receipt['memory_after'] = process_memory(console.pid)
        output = (args.out / 'gdb.stdout.log').read_text(errors='replace')
        assert code == 0 and '#0' in output and 'detached' in output.lower(), 'native backtrace not obtained'
        receipt['passed'] = True
    except BaseException as error:
        receipt['error'] = repr(error)
    finally:
        if gdb is not None:
            terminate_group(gdb)
            receipt['gdb_retirement'] = owned_process_record(gdb)
        if console is not None:
            console.close()
            receipt['target_reaped'] = True
            (args.out / 'console.log').write_bytes(console.log)
        receipt['after'] = admission()
        assert before['memory_events'] == receipt['after']['memory_events']
        (args.out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'passed': receipt['passed'], 'error': receipt.get('error'), 'out': str(args.out)}))
    raise SystemExit(0 if receipt['passed'] else 1)


if __name__ == '__main__':
    main()
