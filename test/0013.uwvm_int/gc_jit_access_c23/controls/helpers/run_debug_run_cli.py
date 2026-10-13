#!/usr/bin/env python3
"""Actual standalone run/r restart regression; original Linux 64 GiB cgroup only."""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import termios
import time


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def wasm_fixture():
    def leb(value):
        result = bytearray()
        while value >= 128:
            result.append((value & 127) | 128)
            value >>= 7
        result.append(value)
        return bytes(result)

    def section(tag, contents):
        return bytes([tag]) + leb(len(contents)) + contents

    def name(text):
        data = text.encode()
        return leb(len(data)) + data

    # global g starts at7; _start increments it, stores it, grows memory once,
    # and calls the real breakpoint function. A fresh run must always yield8,
    # two pages, and zero at the independently debug-mutated byte32.
    start = bytes.fromhex('00 23 00 41 01 6a 24 00 41 00 23 00 36 02 00 41 01 40 00 1a 10 01 0b')
    point = bytes.fromhex('00 01 0b')
    code = leb(2) + leb(len(start)) + start + leb(len(point)) + point
    return (b'\0asm\x01\0\0\0' + section(1, bytes.fromhex('01 60 00 00'))
            + section(3, bytes.fromhex('02 00 00')) + section(5, bytes.fromhex('01 01 01 03'))
            + section(6, bytes.fromhex('01 7f 01 41 07 0b'))
            + section(7, leb(2) + name('_start') + bytes([0, 0]) + name('memory') + bytes([2, 0]))
            + section(10, code))


class Console:
    def __init__(self, argv):
        reader, writer = os.pipe()
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.close(writer)
            if os.read(reader, 1) != b'R':
                os._exit(99)
            os.close(reader)
            os.execv(argv[0], argv)
        os.close(reader)
        self.owner = os.pidfd_open(self.pid) if hasattr(os, 'pidfd_open') else None
        self.baseline = termios.tcgetattr(self.fd)
        os.write(writer, b'R')
        os.close(writer)
        self.log = bytearray()
        self.reaped = False

    def wait(self, predicate, offset, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = bytes(self.log[offset:])
            if predicate(data):
                return data
            if select.select([self.fd], [], [], .05)[0]:
                try:
                    block = os.read(self.fd, 65536)
                except OSError as exc:
                    if exc.errno == errno.EIO:
                        break
                    raise
                if not block:
                    break
                self.log.extend(block)
                require(len(self.log) <= 2 * 1024**2, '2MiB transcript limit')
        raise AssertionError('missing actual reply: ' + repr(bytes(self.log[offset:])[-6000:]))

    def command(self, text, marker=None):
        offset = len(self.log)
        os.write(self.fd, text.encode() + b'\n')
        # Input editing redraws the prompt for every key. Only a standalone
        # final prompt after the submitted command proves its reply completed.
        return self.wait(lambda data: re.search(rb'(?:^|\n)\(uwvm-debug\) $', data)
                         and (marker is None or marker in data), offset)

    def finish(self):
        offset = len(self.log)
        os.write(self.fd, b'quit\n')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if select.select([self.fd], [], [], .02)[0]:
                try:
                    block = os.read(self.fd, 65536)
                    self.log.extend(block)
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
            got, code = os.waitpid(self.pid, os.WNOHANG)
            if got:
                self.reaped = True
                require(os.waitstatus_to_exitcode(code) == 0, 'CLI exit0')
                require(b'managed shutdown complete' not in self.log[offset:], 'successful quit is quiet')
                restored = termios.tcgetattr(self.fd)
                expected = self.baseline.copy()
                if hasattr(termios, 'PENDIN'):
                    restored[3] &= ~termios.PENDIN
                    expected[3] &= ~termios.PENDIN
                require(restored == expected, 'terminal restored after restarts')
                return
            time.sleep(.02)
        raise AssertionError('actual guest/CLI retirement deadline')

    def close(self):
        if not self.reaped:
            try:
                if self.owner is not None:
                    signal.pidfd_send_signal(self.owner, signal.SIGKILL)
                else:
                    # An unreaped direct child keeps its PID; no foreign PID can be reused.
                    os.kill(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(self.pid, 0)
        if self.owner is not None:
            os.close(self.owner)
        os.close(self.fd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    args = parser.parse_args()
    group = Path('/sys/fs/cgroup') / args.expected_cgroup.lstrip('/')
    require(Path('/proc/self/cgroup').read_text() == '0::' + args.expected_cgroup + '\n', 'original Linux cgroup required')
    require((group / 'memory.max').read_text().strip() == str(64 << 30), '64GiB memory.max')
    require((group / 'memory.swap.max').read_text().strip() == '0', 'swap0')
    args.out.mkdir(exist_ok=False)
    wasm = args.out / 'restart.wasm'
    wasm.write_bytes(wasm_fixture())
    console = Console([str(args.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', str(wasm)])
    rows = []
    try:
        console.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        console.command('wait', b'prepared; no Wasm instruction executed')
        console.command('breakwasm 0 1 0')
        for spelling in ('run', 'r', 'run'):
            report = console.command(spelling, b'stopped: breakpoint')
            require(b'error:' not in report, report.decode(errors='replace'))
            require(report.count(b'Starting program: ' + os.fsencode(wasm)) == 1, 'one GDB-style start event')
            require(not re.search(rb'(?m)^running\r?$', report), 'no controller running acknowledgement')
            require(b'UWVM debugger.' not in report and b'managed shutdown complete' not in report, 'no restart boilerplate')
            status = console.command('status', b'stop-id ')
            thread = int(re.search(rb'thread ([0-9]+) module=0 function=1', status)[1])
            globals_reply = console.command(f'globals {thread} 0')
            require(re.search(rb'0 i32[^\r\n]*\b8\b', globals_reply), repr(globals_reply))
            memory_reply = console.command('memory 0 0 32 1')
            require(b'memory: 0' in memory_reply and b'ff' not in memory_reply.lower(), repr(memory_reply))
            require(b'error:' not in console.command('memory 0 0 65536 1'), 'fresh run grows exactly from one page')
            require(b'error:' in console.command('memory 0 0 131072 1'), 'fresh run has exactly two pages')
            mutation = console.command(f'set wasm memory 0 0 {thread} 32 bytes ff')
            require(b'applied=1' in mutation, repr(mutation))
            require(b'memory: ff' in console.command('memory 0 0 32 1').lower(), 'real debug mutation visible')
            require(b'error: Invalid arguments' in console.command('run 1'), 'run has no argument tail')
            require(b'stopped: breakpoint' in console.command('status'), 'bad run leaves stop unchanged')
            console.command('')
            require(b'stopped: breakpoint' in console.command('status'), 'empty Enter does not repeat run')
            rows.append({'spelling': spelling, 'global': 8, 'memory_pages': 2, 'mutated_byte_reset': True})
        console.command('c', b'Program exited normally.')
        console.command('r', b'stopped: breakpoint')
        console.command('si', b'stopped: native instruction step')
        console.command('run', b'stopped: breakpoint')
        console.command('r&', b'Starting program: ')
        console.command('wait', b'stopped: breakpoint')
        console.finish()
        result = {'passed': True, 'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
                  'cgroup': args.expected_cgroup, 'rows': rows, 'restart_after_exit': True,
                  'restart_from_native_trap': True, 'background_run': True, 'terminal_restored': True}
        (args.out / 'result.json').write_text(json.dumps(result, indent=2))
        print(json.dumps(result), flush=True)
    finally:
        (args.out / 'transcript.txt').write_bytes(console.log)
        console.close()


if __name__ == '__main__':
    main()
