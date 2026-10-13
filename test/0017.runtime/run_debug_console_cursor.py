#!/usr/bin/env python3
"""Actual production console keyboard/read-line/render tests in Linux cgroup.
The VT model deliberately ignores optional CSI s/u, as a VT100 terminal does.
This component test does not claim guest execution or JIT admission coverage.
"""
import argparse
import codecs
import errno
import fcntl
import json
import os
import pty
import select
import signal
import struct
import subprocess
import termios
import time
import unicodedata
from pathlib import Path


def cells(c):
    if unicodedata.combining(c) or c in '\u200b\u200c\u200d\ufeff' or '\ufe00' <= c <= '\ufe0f':
        return 0
    return 2 if unicodedata.east_asian_width(c) in ('W', 'F') else 1


class Terminal:
    def __init__(self, columns, rows=24):
        self.columns, self.rows = columns, rows
        self.x = self.y = self.wraps = 0
        self.grid = [[' '] * columns for _ in range(rows)]
        self.decoder = codecs.getincrementaldecoder('utf-8')()
        self.pending = ''

    def feed(self, data):
        self.pending += self.decoder.decode(data)
        while self.pending:
            c = self.pending[0]
            if c == '\x1b':
                if len(self.pending) < 2:
                    return
                if self.pending[1] != '[':
                    self.pending = self.pending[2:]
                    continue
                end = next((i for i in range(2, len(self.pending)) if '@' <= self.pending[i] <= '~'), None)
                if end is None:
                    return
                params, final = self.pending[2:end], self.pending[end]
                self.pending = self.pending[end + 1:]
                self.csi(params, final)
                continue
            self.pending = self.pending[1:]
            if c == '\r':
                self.x = 0
            elif c == '\n':
                if self.y == self.rows - 1:
                    self.grid.pop(0)
                    self.grid.append([' '] * self.columns)
                else:
                    self.y += 1
            elif c >= ' ':
                width = min(cells(c), self.columns)
                if width == 0:
                    continue
                if self.x + width > self.columns:
                    self.wraps += 1
                    self.x = 0
                    self.y = min(self.y + 1, self.rows - 1)
                self.grid[self.y][self.x] = c
                if width == 2:
                    self.grid[self.y][self.x + 1] = ''
                self.x += width

    def csi(self, params, final):
        if params.startswith('?'):
            if params == '?1049':
                self.grid = [[' '] * self.columns for _ in range(self.rows)]
                self.x = self.y = 0
            return
        parts = [int(s or '0') for s in params.split(';')]
        n = parts[0] or 1
        if final == 'C':
            self.x = min(self.columns - 1, self.x + n)
        elif final == 'D':
            self.x = max(0, self.x - n)
        elif final in ('H', 'f'):
            self.y = min(self.rows - 1, max(0, n - 1))
            self.x = min(self.columns - 1, max(0, (parts[1] if len(parts) > 1 else 1) - 1))
        elif final == 'K':
            if parts[0] == 2:
                self.grid[self.y] = [' '] * self.columns
            else:
                self.grid[self.y][self.x:] = [' '] * (self.columns - self.x)
        elif final == 'J' and parts[0] == 2:
            self.grid = [[' '] * self.columns for _ in range(self.rows)]
        # In particular, CSI s/u are NOT supported by this terminal model.

    def line(self):
        return ''.join(self.grid[self.y]).rstrip()


class Console:
    def __init__(self, binary, mode, columns):
        reader, writer = os.pipe()
        pid, self.fd = pty.fork()
        if pid == 0:
            os.close(writer)
            if os.read(reader, 1) != b'R':
                os._exit(99)
            os.close(reader)
            os.execv(str(binary), [str(binary), mode])
        os.close(reader)
        self.pid, self.pidfd = pid, os.pidfd_open(pid)
        self.reaped = False
        self.original = termios.tcgetattr(self.fd)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack('HHHH', 24, columns, 0, 0))
        self.model = Terminal(columns)
        self.transcript = bytearray()
        self.checks = []
        os.write(writer, b'R')
        os.close(writer)
        self.drain(b'EDITOR READY')
        self.model.wraps = 0  # Fixture banner is outside the bounded input row.

    def drain(self, wanted=None):
        deadline, last = time.monotonic() + 5, time.monotonic()
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.fd], [], [], .02)
            if ready:
                try:
                    data = os.read(self.fd, 65536)
                except OSError as exc:
                    if exc.errno == errno.EIO:
                        break
                    raise
                if not data:
                    break
                self.transcript.extend(data)
                self.model.feed(data)
                last = time.monotonic()
            if (wanted is None or wanted in self.transcript) and time.monotonic() - last >= .06:
                return
        if wanted is not None and wanted not in self.transcript:
            raise AssertionError(f'missing {wanted!r}: {bytes(self.transcript[-2048:])!r}')

    def send(self, text, line=None, column=None):
        os.write(self.fd, text.encode() if isinstance(text, str) else text)
        self.drain()
        if line is not None:
            assert self.model.line() == line, (self.model.line(), line)
        if column is not None:
            assert self.model.x == column, (self.model.x, column)
        assert self.model.wraps == 0, ('input wrapped', self.model.wraps)
        self.checks.append({'line': self.model.line(), 'column': self.model.x, 'wraps': self.model.wraps})

    def submit(self, expected):
        os.write(self.fd, b'\n')
        self.drain(('RESULT ' + expected + '\r\n').encode())
        self.model.wraps = 0  # Unbounded fixture diagnostics are not input redraw.

    def close(self):
        try:
            if not self.reaped:
                os.write(self.fd, b'quit\n')
                self.drain(b'CURSOR RESTORED')
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    got, status = os.waitpid(self.pid, os.WNOHANG)
                    if got:
                        self.reaped = True
                        assert os.waitstatus_to_exitcode(status) == 0
                        assert termios.tcgetattr(self.fd) == self.original, 'termios not restored'
                        return
                    time.sleep(.01)
                raise AssertionError('component did not retire')
        finally:
            if not self.reaped:
                try:
                    signal.pidfd_send_signal(self.pidfd, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                os.waitpid(self.pid, 0)
                self.reaped = True
            os.close(self.pidfd)
            os.close(self.fd)


def case(binary, mode, name, columns=80):
    c = Console(binary, mode, columns)
    try:
        if name == 'ascii':
            c.send('status', '(uwvm-debug) status', 19)
            c.send(b'\x1b[D\x1b[D', '(uwvm-debug) status', 17)
            c.send('X', '(uwvm-debug) statXus', 18)
            c.send(b'\x1b[C\x1b[3~', '(uwvm-debug) statXu', 19)
            c.submit('statXu')
        elif name == 'ss3':
            c.send('abcdef')
            c.send(b'\x1bOD' * 3 + b'\x1bOC' + b'X\x1b[3~', '(uwvm-debug) abcdXf', 18)
            c.send(b'\x1bOH!\x1bOF\x1bOD\x7f', '(uwvm-debug) !abcdf', 18)
            c.submit('!abcdf')
        elif name == 'unicode':
            c.send('源éz', '(uwvm-debug) 源éz', 17)
            c.send(b'\x1b[D\x1bOD', '(uwvm-debug) 源éz', 15)
            c.send('中', '(uwvm-debug) 源中éz', 17)
            c.send(b'\x1b[C\x7f', '(uwvm-debug) 源中z', 17)
            c.submit('源中z')
        elif name == 'long':
            c.send('abcdefghijklmnopqrstuvwxyz', '(uwvm-debug) qrstuvwxyz', 23)
            c.send(b'\x1b[H', '(uwvm-debug) abcdefghij', 13)
            c.send(b'\x1b[C' * 3 + b'X', '(uwvm-debug) abcXdefghi', 17)
            c.send(b'\x1b[F', '(uwvm-debug) qrstuvwxyz', 23)
            c.submit('abcXdefghijklmnopqrstuvwxyz')
        elif name == 'resize':
            c.send('abcdefghijklmnopqrstuvwxyz')
            fcntl.ioctl(c.fd, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 24, 0, 0))
            c.model = Terminal(24)
            c.send(b'\x1b[D', '(uwvm-debug) pqrstuvwxy', 23)
            c.send(b'\x1b[H', '(uwvm-debug) abcdefghij', 13)
            c.send('X', '(uwvm-debug) Xabcdefghi', 14)
            c.submit('Xabcdefghijklmnopqrstuvwxyz')
        elif name == 'history':
            c.send('status'); c.submit('status')
            c.send(b'\x1b[A\x1b[D\x1b[D' + b'X', '(uwvm-debug) statXus', 18)
            c.submit('statXus')
        elif name == 'tiny':
            c.send('源abcdef\x1b[D\x1b[D\x1b[C')
            c.send(b'\x1b[H' + b'X')
            c.submit('X源abcdef')
        else:
            raise AssertionError(name)
        assert b'\x1b[s' not in c.transcript and b'\x1b[u' not in c.transcript
        return {'mode': mode, 'case': name, 'columns': columns, 'checks': c.checks,
                'transcript': c.transcript.decode('utf-8', 'backslashreplace')}
    finally:
        c.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    cg = Path('/sys/fs/cgroup') / args.expected_cgroup.lstrip('/')
    assert Path('/proc/self/cgroup').read_text() == '0::' + args.expected_cgroup + '\n'
    assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
    rows = []
    try:
        result = subprocess.run([str(args.binary), 'unit'], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and 'PASS cursor unit' in result.stdout, (result.returncode, result.stdout, result.stderr)
        rows.append({'case': 'unit', 'exit': result.returncode, 'stdout': result.stdout})
        for mode in ('line', 'tui'):
            for name in ('ascii', 'ss3', 'unicode', 'long', 'resize', 'history'):
                rows.append(case(args.binary, mode, name, 24 if name == 'long' else 80))
            for columns in (1, 2, 8, 14):
                rows.append(case(args.binary, mode, 'tiny', columns))
        result = {'passed': True, 'scope': 'production console input component, both line/TUI real PTY; VT model without CSI s/u', 'rows': rows}
    except BaseException as exc:
        result = {'passed': False, 'error': repr(exc), 'rows': rows}
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        raise
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print('PASS cursor real PTY: 20 cases + unit')


if __name__ == '__main__':
    main()
