#!/usr/bin/env python3
"""Real production TUI/Tab PTY tests; original 64 GiB Linux cgroup only."""
import argparse
import copy
import errno
import fcntl
import json
import os
from pathlib import Path
import re
import select
import struct
import termios
import time
import sys
from run_debug_run_cli import Console, require
from run_debug_console_cursor import Terminal as VT


class Terminal(VT):
    def __init__(self, columns=80, rows=24):
        super().__init__(columns, rows)
        self.primary = None
        self.alternate = False

    def csi(self, params, final):
        if params == '?1049':
            if final == 'h' and not self.alternate:
                self.primary = (copy.deepcopy(self.grid), self.x, self.y)
                self.grid = [[' '] * self.columns for _ in range(self.rows)]
                self.x = self.y = 0
                self.alternate = True
            elif final == 'l' and self.alternate:
                self.grid, self.x, self.y = self.primary
                self.primary = None
                self.alternate = False
            return
        super().csi(params, final)

    def text(self):
        return '\n'.join(''.join(row).rstrip() for row in self.grid)


class Driver(Console):
    def __init__(self, argv):
        super().__init__(argv)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
        self.model = Terminal()
        self.painted = 0
        self.checks = []

    def sync(self):
        self.model.feed(bytes(self.log[self.painted:]))
        self.painted = len(self.log)

    def drain(self):
        deadline = time.monotonic() + 5
        last = time.monotonic()
        while time.monotonic() < deadline:
            if select.select([self.fd], [], [], .02)[0]:
                try:
                    block = os.read(self.fd, 65536)
                except OSError as exc:
                    if exc.errno == errno.EIO:
                        break
                    raise
                if not block:
                    break
                self.log.extend(block)
                require(len(self.log) < 8 * 1024**2, '8MiB transcript budget')
                last = time.monotonic()
            elif time.monotonic() - last > .2:
                break
        self.sync()

    def send(self, data):
        self.model.wraps = 0
        at = len(self.log)
        os.write(self.fd, data.encode() if isinstance(data, str) else data)
        self.drain()
        return bytes(self.log[at:])

    def record(self, name):
        self.checks.append({'case': name, 'screen': self.model.text(), 'caret': [self.model.y, self.model.x],
                            'alternate': self.model.alternate, 'wraps': self.model.wraps})

    def failure(self, out, error):
        self.sync()
        out.with_suffix('.failure.json').write_text(json.dumps({
            'error': repr(error), 'checks': self.checks, 'screen': self.model.text(),
            'caret': [self.model.y, self.model.x], 'alternate': self.model.alternate,
            'transcript': self.log.decode('utf8', 'backslashreplace')}, ensure_ascii=False, indent=2))

    def ask(self, command, marker=None):
        data = self.command(command, marker)
        self.drain()
        return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', data).replace(b'\r', b'')

    def retire(self, marker):
        self.send('quit\n')
        require(marker in self.log, 'restoration marker')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            got, status = os.waitpid(self.pid, os.WNOHANG)
            if got:
                self.reaped = True
                require(os.waitstatus_to_exitcode(status) == 0, 'exit0')
                require(termios.tcgetattr(self.fd) == self.baseline, 'termios restored')
                require(not self.model.alternate, 'primary screen restored')
                return
            time.sleep(.01)
        raise AssertionError('retirement deadline')


def transient(c, tui):
    # No common prefix among these siblings; Tab leaves the edit buffer intact.
    c.send('info \t\t')
    require('info breakpoints' in c.model.text() and (not tui or 'Tab completion' in c.model.text()), c.model.text())
    require(c.model.line() == '(uwvm-debug) info', ('completion retains input', c.model.line(), c.model.x))
    require(c.model.alternate == tui, 'only TUI candidates use the alternate buffer')
    c.record('TUI' if tui else 'line candidates visible')
    raw = c.send(b'\x1b[D')
    require('Tab completion' not in c.model.text(), 'arrow clears transient candidates')
    require(c.model.line() == '(uwvm-debug) info' and c.model.x == 17, ('arrow restores exact caret', c.model.line(), c.model.x))
    require(c.model.alternate == tui, 'buffer transition follows TUI mode')
    require(c.model.wraps == 0, 'no input or pane autowrap')
    c.record('arrow clears overlay and restores caret')
    c.send(b'\x1b[C\x15')
    # Toggle while a popup is visible and preserve the unsubmitted input.
    c.send('info \t\t')
    c.send(b'\x18a')
    require(c.model.alternate != tui and 'Tab completion' not in c.model.text(), 'toggle dismisses candidates')
    require(c.model.line() == '(uwvm-debug) info', ('toggle retains edit buffer', c.model.line(), c.model.x))
    c.record('toggle clears overlay')
    c.send(b'\x18a\x15')


def component(binary, mode, no_color, out):
    old = os.environ.get('NO_COLOR')
    if no_color:
        os.environ['NO_COLOR'] = '1'
    else:
        os.environ.pop('NO_COLOR', None)
    c = Driver([str(binary), mode])
    try:
        c.wait(lambda data: b'EDITOR READY' in data, 0); c.drain()
        if mode == 'tui':
            require('[src *]' in c.model.text() and '[commands]' in c.model.text() and '| ' in c.model.text(), 'pane borders')
            require((b'95mreturn' in c.log) != no_color, 'source palette follows NO_COLOR')
        transient(c, mode == 'tui')
        c.send('set \t\t');c.send(b'\x03')
        require('Tab completion' not in c.model.text(), 'CtrlC clears candidates')
        c.record('CtrlC cleanup')
        if mode == 'tui':
            c.send('set \t\t');c.send(b'\x15tui disable\n')
            require(not c.model.alternate and 'Tab completion' not in c.model.text(), 'tui disable clears candidates')
            c.send('tui enable\n')
            require(c.model.alternate and '[src *]' in c.model.text(), 'tui enable restores panes')
            c.record('enable/disable cleanup')
        c.retire(b'COLOR RESTORED')
        require(b'\x1b[s' not in c.log and b'\x1b[u' not in c.log, 'portable cursor operations')
        require(no_color == (re.search(rb'\x1b\[(?:9[0-7]|3[0-7]|0)m', c.log) is None), 'SGR policy applies throughout')
        return {'mode': mode, 'no_color': no_color, 'passed': True, 'checks': c.checks,
                'transcript': c.log.decode('utf8', 'backslashreplace')}
    except BaseException as exc:
        c.failure(out, exc)
        raise
    finally:
        c.close()
        if old is None: os.environ.pop('NO_COLOR', None)
        else: os.environ['NO_COLOR'] = old


def actual(a):
    c = Driver([str(a.uwvm), '-Rdbg', '-Rct', '0', '-Rllvm-call-stack', 'instruction',
                '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)])
    try:
        c.wait(lambda data: b'(uwvm-debug) ' in data, 0);c.drain()
        c.ask('wait', b'prepared; no Wasm instruction executed')
        transient(c, False)
        c.send('set source ' + str(a.out.parent / 'missing-directory' / 'file') + '\t')
        require('Path completion unavailable' in c.model.text() and not c.model.alternate, 'unavailable path is a transient notice')
        c.send(b'\x15')
        require('Path completion unavailable' in c.model.text() and not c.model.alternate, 'path notice remains in primary scrollback')
        c.record('unavailable path notice cleanup')
        line = next(i for i, text in enumerate(a.source.read_text().splitlines(), 1) if 'UX_LEAF_READY' in text)
        c.ask(f'breaksource 0 {a.source}:{line}', b'breakpoint ')
        c.ask('run', b'stopped: breakpoint')
        before = c.ask('status', b'stop-id ')
        stop = re.search(rb'stop-id (\d+)', before)[1]
        c.send('tui enable\n')
        require('[src *]' in c.model.text() and '=> ' in c.model.text() and '[commands]' in c.model.text(), c.model.text())
        require(b'95mreturn' in c.log and b'96m1' in c.log, 'real DWARF source lexical colors')
        c.record('real DWARF source and colors')
        transient(c, True)
        c.send('layout src asm\n')
        require('[src *]' in c.model.text() and '[asm]' in c.model.text(), 'split pane boundaries')
        c.record('split source/asm panes')
        # Real dynamic source-variable provider uses the same transient surface.
        c.send('print \t')
        require('Tab completion' in c.model.text() and 'counter' in c.model.text(), 'live source completion overlay')
        c.send(b'\x15')
        require('Tab completion' not in c.model.text(), 'dynamic candidates erased')
        c.record('dynamic completion cleanup')
        c.send('help up\n')
        require('\\x1b' not in c.model.text(), 'diagnostic colors do not become escape text in command pane')
        c.send('tui disable\n')
        after = c.ask('status', b'stop-id ')
        require(re.search(rb'stop-id (\d+)', after)[1] == stop, 'UI and Tab never resume or mutate stop')
        c.ask('continue', b'Program exited normally.')
        c.retire(b'Program exited normally.')
        return {'passed': True, 'checks': c.checks, 'stop_unchanged': stop.decode(),
                'transcript': c.log.decode('utf8', 'backslashreplace')}
    except BaseException as exc:
        c.failure(a.out, exc)
        raise
    finally:
        c.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'uwvm', 'wasm', 'source', 'out'):p.add_argument('--' + name, type=Path, required=name in ('binary', 'out'))
    scope = p.add_mutually_exclusive_group()
    scope.add_argument('--components-only', action='store_true')
    scope.add_argument('--actual-only', action='store_true')
    p.add_argument('--expected-cgroup')
    a = p.parse_args()
    if a.source is not None:a.source=a.source.resolve()
    if not a.components_only and any(getattr(a, name) is None for name in ('uwvm', 'wasm', 'source')):
        p.error('actual tests require --uwvm, --wasm and --source')
    if sys.platform=='linux' or a.expected_cgroup:
        require(bool(a.expected_cgroup),'Linux tests require the bounded cgroup')
        cg = Path('/sys/fs/cgroup') / a.expected_cgroup.lstrip('/')
        require(Path('/proc/self/cgroup').read_text() == '0::' + a.expected_cgroup + '\n', 'original cgroup')
        require((cg/'memory.max').read_text().strip() == str(64 << 30) and (cg/'memory.swap.max').read_text().strip() == '0', '64GiB/swap0')
    rows = []
    try:
        if not a.actual_only:
            for mode in ('line', 'tui'):
                for no_color in (False, True):rows.append(component(a.binary, mode, no_color, a.out))
        if not a.components_only:
            rows.append({'actual': actual(a)})
        result = {'passed': True, 'platform':sys.platform, 'scope': 'real PTY line/TUI palette, source lexer, pane boundaries, transient static/dynamic Tab cleanup, caret/termios restoration, real LLVM-full guest stop unchanged', 'rows': rows, 'components_only': a.components_only, 'actual_only': a.actual_only}
    except BaseException as exc:
        a.out.write_text(json.dumps({'passed': False, 'error': repr(exc), 'rows': rows}, ensure_ascii=False, indent=2));raise
    a.out.write_text(json.dumps(result, ensure_ascii=False, indent=2))
    print('PASS colorful TUI and transient Tab:', 'components' if a.components_only else 'real LLVM-full DWARF guest' if a.actual_only else 'components and real LLVM-full DWARF guest')


if __name__ == '__main__':main()
