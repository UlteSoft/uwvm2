#!/usr/bin/env python3
"""Real PTY regression for private snapshot helper names and repeated JIT restarts."""
import argparse
import json
import os
import re
import signal
from pathlib import Path

from run_debug_pane_symbols import fixture
from run_debug_run_cli import Console, require


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--iterations', type=int, default=32)
    parser.add_argument('--expected-cgroup')
    args = parser.parse_args()
    require(1 <= args.iterations <= 128, 'bounded iteration count')
    if args.expected_cgroup:
        require(Path('/proc/self/cgroup').read_text().strip() == '0::' + args.expected_cgroup, 'cgroup')
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ['TERM'] = 'xterm-256color'
    os.environ.pop('NO_COLOR', None)
    # An exception, rather than default signal termination, runs child cleanup.
    def deadline(signum, frame):
        raise TimeoutError('restart qualification exceeded 300 seconds')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(300)
    rows = []
    try:
        for i in range(args.iterations):
            wasm = args.out / f'{i}.wasm'
            wasm.write_bytes(fixture(bool(i % 2)))
            console = Console([str(args.uwvm.resolve()), '-Rdbg', '-Rct', '0',
                               '-Rllvm-cache-path', 'disable', '--run', str(wasm.resolve())])
            row = {'iteration': i, 'passed': False, 'commands': []}
            try:
                console.wait(lambda data: b'(uwvm-debug) ' in data, 0)
                for command, marker in [('starti', b'native-pc='), ('ni', b'native instruction step'),
                                        ('startw', b'first Wasm instruction'), ('nw', b'stopped:'),
                                        ('starti', b'native-pc='), ('ni', b'native instruction step')]:
                    reply = console.command(command, marker)
                    plain = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', reply)
                    require(b'error:' not in plain and b'[fatal]' not in plain, (command, plain))
                    row['commands'].append(command)
                console.finish()
                row['passed'] = True
            except Exception as error:
                row['error'] = str(error)
                raise
            finally:
                (args.out / f'{i}.bin').write_bytes(console.log)
                console.close()
                rows.append(row)
                (args.out / 'result.json').write_text(json.dumps({
                    'passed': len(rows) == args.iterations and all(r['passed'] for r in rows),
                    'iterations': rows, 'cgroup': args.expected_cgroup}, indent=2))
            print('PASS restart iteration', i, flush=True)
    finally:
        signal.alarm(0)


if __name__ == '__main__':
    main()
