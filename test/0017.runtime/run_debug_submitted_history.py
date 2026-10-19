#!/usr/bin/env python3
"""Production PTY history: submissions, errors, editing and restart in both modes."""
import argparse
import json
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_run_cli import require


def run(binary, wasm, tui, out):
    c = Driver([str(binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', str(wasm)])
    checks = []

    def input_is(text, case):
        require(c.model.line() == '(uwvm-debug) ' + text, (case, c.model.line()))
        checks.append(case)

    try:
        c.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        c.drain()
        if tui:
            c.send('tui enable\n')
        c.send('not-a-command 17\n')
        c.send('memory nope\n')
        c.send('step asm\n')
        c.send('draft to preserve')
        for text in ('step asm', 'memory nope', 'not-a-command 17'):
            c.send(b'\x1b[A')
            input_is(text, 'recall submitted error: ' + text)
        c.send(b'\x1b[B\x1b[B\x1b[B')
        input_is('draft to preserve', 'Down restores unsubmitted draft')
        c.send(b'\x15\x1b[A\x1b[A')
        input_is('memory nope', 'recall syntax error for editing')
        c.send(b'\x17' + b'0 0 0 16\n')
        c.send(b'\x1b[A')
        input_is('memory 0 0 0 16', 'edited resubmission becomes latest history')
        c.send(b'\x1b[A')
        input_is('step asm', 'editing does not overwrite older history')
        c.send(b'\x15abandoned edit\x15\x1b[A')
        input_is('memory nope', 'unsubmitted edits do not insert history')
        # A submitted command resets navigation; identical submissions remain
        # separate chronological entries, and empty whitespace is not an entry.
        c.send(b'\x15status\nstatus\n   \n')
        c.send(b'\x1b[A')
        input_is('status', 'latest duplicate submission')
        c.send(b'\x1b[A')
        input_is('status', 'earlier duplicate submission')
        c.send(b'\x1b[A')
        input_is('memory 0 0 0 16', 'whitespace does not create an entry')
        c.send(b'\x15layout horizontal src wasm asm\nstart\n')
        c.send(b'\x1b[A')
        input_is('start', 'restart retains submitted launch command')
        c.send(b'\x1b[A')
        input_is('layout horizontal src wasm asm', 'restart retains preceding history')
        c.send(b'\x1b[B\x1b[B')
        # Terminal models trim trailing whitespace from the current row.
        require(c.model.line() == '(uwvm-debug)', 'Down returns to fresh empty draft')
        checks.append('restart has fresh draft')
        c.send(b'\x15')
        if tui:
            c.send('tui disable\n')
        c.finish()
        return {'passed': True, 'mode': 'tui' if tui else 'line', 'checks': checks}
    finally:
        (out / ('tui.bin' if tui else 'line.bin')).write_bytes(c.log)
        c.close()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wasm', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    rows = [run(a.uwvm.resolve(), a.wasm.resolve(), mode, a.out) for mode in (False, True)]
    (a.out / 'result.json').write_text(json.dumps({'passed': True, 'rows': rows}, indent=2))


if __name__ == '__main__':
    main()
