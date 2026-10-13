#!/usr/bin/env python3
"""Pause, restart and quit a real numeric infinite loop with no requested sites.

All commands go through the production PTY console. A zero exit, finite guest
join and terminal restoration are checked by Console.finish, not inferred from
a controller acknowledgement. Run in the qualified Linux test cgroup.
"""
import argparse
import hashlib
import json
import os
import re
import time
from pathlib import Path
from run_debug_run_cli import Console
from run_debug_native_macos_cancel import fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    wasm = args.out.resolve() / 'numeric-infinite.wasm'
    wasm.write_bytes(fixture())
    os.environ.update(TERM='dumb', NO_COLOR='1')
    c = Console([str(args.uwvm.resolve()), '-Rdbg', '-Rct', '0',
                 '-Rllvm-cache-path', 'disable', '-Rclog', 'err', '--run', str(wasm)])
    actions, rounds = [], []

    def command(text):
        at = len(c.log)
        os.write(c.fd, text.encode() + b'\n')
        reply = c.wait(lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), at, 60)
        assert b'error:' not in reply, reply[-3000:]
        actions.append({'command': text, 'reply': reply.decode(errors='replace')})
        return reply

    try:
        c.wait(lambda b: b'(uwvm-debug) ' in b, 0, 120)
        for i in range(6):
            assert b'stopped:' in command('startw')
            command('c &')
            time.sleep(.03)
            assert re.search(rb'(?:^|\r?\n)running\r?\n', command('status'))
            row = {'round': i, 'running_without_breakpoints': True}
            if i % 2 == 0:
                started = time.monotonic()
                command('pause')
                deadline = started + 3
                while True:
                    status = command('status')
                    if b'stopped:' in status:
                        break
                    assert time.monotonic() < deadline, status
                    time.sleep(.01)
                thread = re.search(rb'thread (\d+) module=0 function=1', status)[1].decode()
                actual = command('globals ' + thread + ' 0')
                value = re.search(rb'global 0 i32 = (\d+)', actual)
                assert value and int(value[1]) > 0, actual
                row.update(pause_seconds=time.monotonic() - started, actual_counter=int(value[1]))
            rounds.append(row)
        # The last round remains running: quit must force cold collection and
        # perform actual managed cleanup even though every numeric gate was off.
        started = time.monotonic()
        c.finish()
        result = {'passed': True, 'rounds': rounds, 'quit_seconds': time.monotonic() - started,
                  'terminal_restored': True, 'actual_exit': 0,
                  'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
                  'fixture_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest()}
        (args.out / 'result.json').write_text(json.dumps(result, indent=2))
        print(json.dumps(result), flush=True)
    finally:
        (args.out / 'transcript.bin').write_bytes(c.log)
        (args.out / 'actions.json').write_text(json.dumps(actions, indent=2))
        c.close()


if __name__ == '__main__':
    main()
