#!/usr/bin/env python3
"""Real PTY register pane: current native PC, level changes, aliases and color."""
import argparse, fcntl, hashlib, json, os, re, struct, termios, time
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_rich_tui import RichTerminal
from run_debug_run_cli import require


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.pop('NO_COLOR', None)
    os.environ['TERM'] = 'xterm-256color'
    binary = args.uwvm.resolve()
    c = Driver([str(binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', str(args.wasm.resolve())])
    checks, actions = [], []

    def check(ok, label):
        require(ok, (label, c.model.text()))
        checks.append(label)

    def cmd(text):
        at = len(c.log)
        c.send(text + '\n')
        # Silence alone is not command completion: restart may still be
        # compiling. Require the real cleared edit line and restored caret.
        deadline = time.monotonic() + 60
        while not (len(c.log) > at and c.model.line().rstrip() == '(uwvm-debug)' and
                   c.model.x == len('(uwvm-debug) ')):
            require(time.monotonic() < deadline, ('command completion deadline', text, c.model.text()))
            c.drain()
        raw = bytes(c.log[at:])
        actions.append({'command': text, 'screen': c.model.text()})
        return raw

    def pane():
        return c.model.text().split('[io]')[0].split('[regs', 1)[-1]

    def pc():
        match = re.search(r'\bu(?:32|64) (?:pc|rip|eip) = (0x[0-9a-f]+)', pane())
        check(match is not None, 'current PC visible in register pane')
        check('unavailable' not in pane().lower() and 'native-registers stop=' not in pane(), 'register pane omits protocol and missing rows')
        return int(match[1], 16)

    def wasm_stop():
        check('Wasm stop; use si or ni' in pane(), 'Wasm stop shows native stepping hint')
        check(not re.search(r'\bu(?:32|64) (?:pc|rip|eip) = ', pane()), 'Wasm stop clears previous native values')

    try:
        c.wait(lambda b: b'(uwvm-debug) ' in b, 0)
        c.drain()
        c.ask('start')
        cmd('layout register')
        wasm_stop()
        raw = cmd('si')
        original = pc()
        check(re.search(rb'\x1b\[(?:0;)?96m(?:pc|rip|eip)', raw) and re.search(rb'\x1b\[(?:0;)?94mu64', raw) and re.search(rb'\x1b\[(?:0;)?93m0x', raw), 'register name type and value have distinct colors')
        for alias in ['registers', 'regs', 'register']:
            cmd('layout ' + alias)
            check(pc() == original, 'layout alias never advances PC: ' + alias)
        cmd('tui disable')
        output = c.ask('info registers pc').decode('utf8', 'replace')
        match = re.search(r'\b(?:pc|rip|eip)=(0x[0-9a-f]+)', output)
        check(match and int(match[1], 16) == original, 'pane PC matches real authenticated CLI register query')
        cmd('tui enable')
        cmd('focus register')
        cmd('scroll register down 4096')
        cmd('nw')
        wasm_stop()
        cmd('scroll register up 4096')
        for action in ['ni', 'nw', 'si', 'startw', 'starti', 'nw', 'ni', 'start']:
            cmd(action)
            if action in ['nw', 'startw', 'start']:
                wasm_stop()
            else:
                pc()
        cmd('layout horizontal src wasm asm register')
        saved = args.out / 'before.layout'
        after = args.out / 'after.layout'
        cmd('layout config save ' + str(saved.resolve()))
        cmd('starti')
        cmd('layout config save ' + str(after.resolve()))
        check(saved.read_bytes() == after.read_bytes(), 'restart retains composed register layout')
        cmd('layout register')
        pc()
        cmd('help layout register full')
        check('layout register' in c.model.text() and 'help tui layout full' in c.model.text(), 'register layout help resolves alias')
        c.send('layout reg\t')
        check('register' in c.model.text() and 'regs' in c.model.text(), 'register alias Tab candidates')
        c.send('\x15')
        for width, height in [(48, 16), (120, 42), (80, 24)]:
            fcntl.ioctl(c.fd, termios.TIOCSWINSZ, struct.pack('HHHH', height, width, 0, 0))
            c.model = RichTerminal(width, height)
            c.model.alternate = True
            c.model.primary = ([[' '] * width for _ in range(height)], 0, 0)
            c.painted = len(c.log)
            c.drain()
            cmd('layout register')
            pc()
        cmd('tui disable')
        c.finish()
    finally:
        (args.out / 'transcript.bin').write_bytes(c.log)
        (args.out / 'actions.json').write_text(json.dumps(actions, indent=2))
        c.close()
    result = {'passed': True, 'binary': str(binary), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'checks': checks}
    (args.out / 'result.json').write_text(json.dumps(result, indent=2))
    print('PASS real register pane:', len(checks), 'checks')


if __name__ == '__main__':
    main()
