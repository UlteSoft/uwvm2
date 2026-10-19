#!/usr/bin/env python3
"""Real PTY help navigation through the production terminal adapter, in cgroup."""
import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path
from run_debug_console_cursor import Console


def navigate(binary, mode, columns=80):
    console = Console(binary, mode, columns)
    rows = []
    try:
        for command, expected in [
            ('help', 'help asm'),
            ('help asm', 'help asm registers'),
            ('help asm\treg\t', 'Native registers'),
            ('help r', 'Start from the beginning'),
            ('h wasm', 'help wasm values'),
            ('help wasm set global', 'Set a global'),
            ('help wasm set global full 2', 'Page 2/'),
            ('help asm 999', 'out of range'),
            ('help made-up', 'Unknown help topic'),
        ]:
            console.model.wraps = 0
            start = len(console.transcript)
            console.send(command)
            os.write(console.fd, b'\n')
            # Each call needs a fresh completion marker, not a previous one.
            deadline = time.monotonic() + 10
            while b'HELP DONE' not in console.transcript[start:]:
                assert time.monotonic() < deadline, 'help command did not complete'
                console.drain()
            console.drain()
            output = bytes(console.transcript[start:])
            readable = re.sub(rb'\x1b\[[0-9;?]*[ -/]*[@-~]', b'', output).decode('utf-8', 'replace')
            if mode == 'line':
                assert expected in readable, (command, expected, readable)
                assert b'\x1b[96m' in output or command.endswith('999') or command.endswith('made-up'), command
                if command == 'help':
                    assert 'replace MODULE' not in readable and 'Usage:' not in readable
                    assert 'Page 1/1' in readable
            else:
                assert '\\x1b[92m' not in readable and '\\x1b[96m' not in readable, 'TUI must not escape help colors into text'
                assert 'HELP DONE' in readable
            rows.append({'command': command, 'expected': expected, 'transcript': output.decode('utf-8', 'backslashreplace')})
            console.model.wraps = 0
        return {'mode': mode, 'columns': columns, 'cases': rows}
    finally:
        console.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert Path('/proc/self/cgroup').read_text() == '0::' + args.expected_cgroup + '\n'
    rows = [navigate(args.binary, mode) for mode in ('line', 'tui')]
    # This probe requests color explicitly; global NO_COLOR wins, even empty.
    for value in ('', '1'):
        result = subprocess.run([str(args.binary), 'render'], env={**os.environ, 'NO_COLOR': value}, capture_output=True, check=True)
        assert b'\x1b' not in result.stdout and b'help asm' in result.stdout
        rows.append({'case': 'NO_COLOR', 'value': value, 'passed': True})
    args.output.write_text(json.dumps({'passed': True, 'scope': 'production help/parser/editor/PTY component; no guest execution claim', 'rows': rows}, indent=2))
    print('PASS help PTY + NO_COLOR')


if __name__ == '__main__':
    main()
