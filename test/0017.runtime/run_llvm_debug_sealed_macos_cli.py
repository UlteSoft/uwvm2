#!/usr/bin/env python3
"""Exercise Darwin debug-jit input sealing through real WASI path_open calls."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import selectors
import subprocess
import sys
import tempfile
import time
import tty


def wat_string(value: str) -> str:
    return ''.join(f'\\{octet:02x}' for octet in value.encode())


def capped(watchdog: Path, command: list[str], **kwargs) -> subprocess.CompletedProcess:
    result = subprocess.run([sys.executable, str(watchdog), '--', *command],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    assert b'MACOS_RSS_LIMIT_BYTES=4294967296' in result.stdout, result.stdout[-4000:]
    peaks = [int(value) for value in re.findall(rb'PEAK_PROCESS_TREE_RSS_BYTES=(\d+)', result.stdout)]
    assert peaks and max(peaks) <= 4 * 1024**3, result.stdout[-4000:]
    return result


def fixture(watchdog: Path, wasm_tools: Path, directory: Path, name: str,
            paths: list[tuple[str, int, int, bool]]) -> tuple[Path, bytes]:
    """Each path is (relative path, WASI oflags, rights, should_open)."""
    data, instructions = [], []
    address = 1024
    for path, flags, rights, should_open in paths:
        data.append(f'(data (i32.const {address}) "{wat_string(path)}")')
        instructions.append(f'''i32.const 3 i32.const 1 i32.const {address}
            i32.const {len(path.encode())} i32.const {flags} i64.const {rights}
            i64.const 0 i32.const 0 i32.const 64 call $open
            {"if unreachable end" if should_open else "i32.eqz if unreachable end"}''')
        if should_open:
            instructions.append('i32.const 64 i32.load call $close if unreachable end')
        address += len(path.encode()) + 1
    marker = (name + '-guest-passed\n').encode()
    wat = f'''(module
      (import "wasi_snapshot_preview1" "path_open" (func $open
       (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
      (memory (export "memory") 1)
      (data (i32.const 256) "{wat_string(marker.decode())}")
      {' '.join(data)}
      (func (export "_start") {' '.join(instructions)}
       i32.const 0 i32.const 256 i32.store
       i32.const 4 i32.const {len(marker)} i32.store
       i32.const 1 i32.const 0 i32.const 1 i32.const 8 call $write if unreachable end))'''
    source = directory / (name + '.wat')
    wasm = directory / (name + '.wasm')
    source.write_text(wat + '\n')
    result = capped(watchdog, [str(wasm_tools), 'parse', str(source), '-o', str(wasm)])
    assert result.returncode == 0, result.stdout
    result = capped(watchdog, [str(wasm_tools), 'validate', str(wasm)])
    assert result.returncode == 0, result.stdout
    return wasm, marker


def command(binary: Path, wasm: Path, host_mount: Path) -> list[str]:
    return [str(binary), '-m', 'debug-jit', '-Rct', '0', '-Rllvm-call-stack',
            'instruction', '-Rllvm-cache-path', 'disable', '--wasip1-mount-dir',
            '/sandbox', str(host_mount), '--run', str(wasm)]


class Console:
    def __init__(self, watchdog: Path, name: str, command_line: list[str],
                 terminal_pair: tuple[int, int] | None = None):
        self.name, self.log, self.pending = name, bytearray(), bytearray()
        self.master = None
        self.slave_hold = None
        if terminal_pair is not None:
            master, slave = terminal_pair
            self.master = master
            # Keep one parent-side slave descriptor until the watchdog footer
            # has been drained; Darwin otherwise drops queued PTY output on
            # the final slave close immediately after child exit.
            self.slave_hold = os.dup(slave)
            self.slave_path = os.ttyname(slave)
            tty.setraw(slave)
            self.child = subprocess.Popen([sys.executable, str(watchdog), '--', *command_line],
                                          stdin=slave, stdout=slave, stderr=slave)
            os.close(slave)
            self.fd = master
        else:
            self.child = subprocess.Popen([sys.executable, str(watchdog), '--', *command_line],
                                          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT)
            self.fd = self.child.stdout.fileno()
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.fd, selectors.EVENT_READ)
        self.prompt()

    def prompt(self) -> bytes:
        marker = b'(uwvm-debug) '
        end = time.monotonic() + 30
        while marker not in self.pending:
            ready = self.selector.select(max(0, end - time.monotonic()))
            assert ready, (self.name, 'prompt timeout', self.log[-4000:])
            chunk = os.read(self.fd, 65536)
            assert chunk, (self.name, 'early exit', self.child.poll(), self.log[-4000:])
            self.pending.extend(chunk)
            self.log.extend(chunk)
        index = self.pending.index(marker) + len(marker)
        reply = bytes(self.pending[:index])
        del self.pending[:index]
        return reply

    def send(self, text: str) -> bytes:
        data = text.encode() + b'\n'
        if self.master is None:
            self.child.stdin.write(data)
            self.child.stdin.flush()
        else:
            assert os.write(self.master, data) == len(data)
        return self.prompt()

    def quit(self) -> None:
        data = b'quit\n'
        if self.master is None:
            self.child.stdin.write(data)
            self.child.stdin.flush()
        else:
            assert os.write(self.master, data) == len(data)

    def finish(self, marker: bytes) -> bytes:
        self.send('continue')
        end = time.monotonic() + 30
        while True:
            reply = self.send('wait')
            if b'guest exited: 0' in reply:
                break
            assert time.monotonic() < end, (self.name, 'guest timeout', reply)
        assert marker in self.log, (self.name, self.log[-4000:])
        self.quit()
        assert self.child.wait(timeout=15) == 0, (self.name, self.child.returncode, self.log[-4000:])
        if self.master is None:
            self.log.extend(self.child.stdout.read())
        else:
            deadline = time.monotonic() + 5
            while b'COMMAND_EXIT=' not in self.log:
                assert self.selector.select(max(0, deadline - time.monotonic())), (self.name, self.log[-4000:])
                chunk = os.read(self.master, 65536)
                assert chunk, (self.name, self.log[-4000:])
                self.log.extend(chunk)
        assert b'MACOS_RSS_LIMIT_BYTES=4294967296' in self.log, (self.name, self.log[-4000:])
        peaks = re.findall(rb'PEAK_PROCESS_TREE_RSS_BYTES=(\d+)', self.log)
        assert peaks and max(map(int, peaks)) <= 4 * 1024**3
        return bytes(self.log)

    def close(self) -> None:
        if self.child.poll() is None:
            self.child.kill()
            self.child.wait()
        self.selector.close()
        if self.master is not None:
            os.close(self.master)
        if self.slave_hold is not None:
            os.close(self.slave_hold)


def negative_alias_child(kind: str, binary: Path, wasm: Path,
                         mount: Path, path: Path | None) -> int:
    """Separate watchdog child so its footer never writes into the alias."""
    if kind == 'file':
        assert path is not None
        original = path.read_bytes()
        with path.open('rb') as command_input, path.open('ab', buffering=0) as output:
            result = subprocess.run(command(binary, wasm, mount), stdin=command_input,
                                    stdout=output, stderr=subprocess.PIPE, timeout=15)
        assert result.returncode != 0 and b'[fatal]' in result.stderr, result.stderr
        assert path.read_bytes() == original, (original, path.read_bytes())
    else:
        assert kind == 'pipe'
        read_fd, write_fd = os.pipe()
        try:
            result = subprocess.run(command(binary, wasm, mount), stdin=read_fd,
                                    stdout=write_fd, stderr=subprocess.PIPE, timeout=15)
        finally:
            os.close(write_fd)
            leaked = os.read(read_fd, 4096)
            os.close(read_fd)
        assert (result.returncode != 0 and b'[fatal]' in result.stderr and not leaked), \
            (result.returncode, result.stderr, leaked)
    print(f'PASS negative {kind} output alias; VM exit={result.returncode}')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    watchdog = root / 'test/0017.runtime/macos_rss_limit.py'
    binary = args.uwvm.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-sealed-cli-') as temporary:
        work = Path(temporary)
        logs: dict[str, bytes] = {}
        pipe_wasm, pipe_marker = fixture(watchdog, wasm_tools, work, 'pipe-fd',
                                         [('dev/fd/0', 0, 2, False)])
        console = Console(watchdog, 'pipe-fd', command(binary, pipe_wasm, Path('/')))
        try:
            logs['pipe-fd'] = console.finish(pipe_marker)
        finally:
            console.close()

        mount = work / 'mount'
        mount.mkdir()
        commands = b'continue\nwait\nquit\n'
        source = mount / 'commands'
        alias = mount / 'commands-hardlink'
        source.write_bytes(commands)
        os.link(source, alias)
        regular_wasm, regular_marker = fixture(watchdog, wasm_tools, work, 'regular-hardlink',
            [('commands', 0, 2, False), ('commands-hardlink', 8, 64, False),
             ('benign', 9, 64, True)])
        with source.open('rb') as command_input:
            result = capped(watchdog, command(binary, regular_wasm, mount), stdin=command_input,
                            timeout=30)
        logs['regular-hardlink'] = result.stdout
        assert result.returncode == 0 and regular_marker in result.stdout, result.stdout[-4000:]
        assert b'guest exited: 0' in result.stdout and source.read_bytes() == commands
        assert alias.read_bytes() == commands and (mount / 'benign').exists()

        master, slave = pty.openpty()
        live_slave = os.ttyname(slave)
        pty_wasm, pty_marker = fixture(watchdog, wasm_tools, work, 'pty-alias',
                                      [(live_slave.lstrip('/'), 0, 64, False)])
        console = Console(watchdog, 'pty-alias', command(binary, pty_wasm, Path('/')),
                          (master, slave))
        try:
            logs['pty-alias'] = console.finish(pty_marker)
        finally:
            console.close()

        same_file = work / 'same-input-output'
        same_file.write_bytes(b'quit\n')
        for kind, name, alias in [('file', 'same-file-output', same_file),
                                  ('pipe', 'same-pipe-output', None)]:
            arguments = [sys.executable, str(Path(__file__).resolve()),
                         '--negative-alias-child', kind, str(binary),
                         str(regular_wasm), str(mount)]
            if alias is not None:
                arguments.append(str(alias))
            result = capped(watchdog, arguments, timeout=20)
            logs[name] = result.stdout
            assert (result.returncode == 0 and f'PASS negative {kind}'.encode() in result.stdout), \
                result.stdout[-4000:]
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            for name, log in logs.items():
                (args.out / (name + '.log')).write_bytes(log)
            (args.out / 'summary.json').write_text(json.dumps({
                'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'fixtures': sorted(logs),
                'wasm_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in work.glob('*.wasm')},
                'passed': True}, indent=2) + '\n')
        print('PASS macOS debug-jit sealed input: real WASI pipe, hardlink, PTY, output aliases')
    return 0


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--negative-alias-child':
        arguments = sys.argv[2:]
        assert len(arguments) in (4, 5)
        sys.exit(negative_alias_child(arguments[0], Path(arguments[1]), Path(arguments[2]),
                                      Path(arguments[3]), Path(arguments[4]) if len(arguments) == 5 else None))
    sys.exit(main())
