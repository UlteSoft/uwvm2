#!/usr/bin/env python3
"""Actual Linux debug-jit input sealing, including procfd, hardlinks and PTYs."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import pty
import resource
import selectors
import subprocess
import termios
import time
import tty

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--uwvm', type=Path, required=True)
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--wasm-tools', type=Path, required=True)
p.add_argument('--ros', action='store_true')
a = p.parse_args()
a.uwvm = a.uwvm.resolve()
a.out = a.out.resolve()
a.source_root = a.source_root.resolve()
subprocess.run(['bash', str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.out.mkdir(parents=True, exist_ok=False)
rows = []


def record(name, command, code, passed, commands=()):
    rows.append(dict(name=name, command=list(map(str, command)), exit=code,
                     passed=passed, console_commands=list(commands)))
    (a.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')


def wat_bytes(value):
    return ''.join('\\'+format(c, '02x') for c in value.encode())


def fixture(name, actions, denied_stdio=False):
    """An action is (relative path, WASI oflags, base rights, expected errno)."""
    data = []
    instructions = []
    cursor = 1024
    for path, flags, rights, expected in actions:
        data.append(f'(data (i32.const {cursor}) "{wat_bytes(path)}")')
        instructions.append(f'''i32.const 3 i32.const 1 i32.const {cursor}
          i32.const {len(path.encode())} i32.const {flags} i64.const {rights}
          i64.const 0 i32.const 0 i32.const 64 call $open
          i32.const {expected} i32.ne if unreachable end''')
        if expected == 0:
            instructions.append('i32.const 64 i32.load call $close if unreachable end')
        cursor += len(path.encode())+1
    if denied_stdio:
        for fd in range(3):
            instructions.append(f'''i32.const {fd} i32.const 0 i32.const 1
              i32.const 8 call $read i32.const 76 i32.ne if unreachable end''')
    marker = name+'-guest-passed\n'
    wat = f'''(module
      (import "wasi_snapshot_preview1" "path_open" (func $open
       (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_read" (func $read (param i32 i32 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
      (memory (export "memory") 1)
      (data (i32.const 0) "\\10\\00\\00\\00\\01\\00\\00\\00")
      (data (i32.const 256) "{wat_bytes(marker)}")
      {' '.join(data)}
      (func (export "_start") {' '.join(instructions)}
       i32.const 0 i32.const 256 i32.store
       i32.const 4 i32.const {len(marker.encode())} i32.store
       i32.const 1 i32.const 0 i32.const 1 i32.const 8 call $write if unreachable end))'''
    source = a.out/(name+'.wat')
    binary = a.out/(name+'.wasm')
    source.write_text(wat+'\n')
    subprocess.run([str(a.wasm_tools), 'parse', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(a.wasm_tools), 'validate', str(binary)], check=True)
    return binary, marker.encode()


def command(binary, mount):
    # The explicit root preopen in the procfd/PTY tests is a hostile test fixture,
    # not a claim that access to /proc/self/mem or arbitrary host files is safe.
    return [str(a.uwvm), '-m', 'debug-jit', '-Rct', '0', '-Rllvm-call-stack',
            'instruction', '-Rllvm-cache-path', 'disable', '--wasip1-mount-dir',
            '/sandbox', str(mount), '--run', str(binary)]


class Console:
    def __init__(self, name, cmd, terminal=False):
        self.name, self.cmd = name, cmd
        self.log, self.pending, self.commands = bytearray(), bytearray(), []
        self.master = None
        if terminal:
            master, slave = pty.openpty()
            self.master = master
            self.slave_name = os.ttyname(slave)
            tty.setraw(slave)

            def session():
                os.setsid()
                fcntl.ioctl(0, termios.TIOCSCTTY, 0)

            # The WAT must use the actual slave name selected for this process.
            binary, self.marker = fixture(name, [('dev/tty', 0, 2, 2),
                (self.slave_name.lstrip('/'), 0, 64, 2)], denied_stdio=True)
            self.cmd = command(binary, '/')
            try:
                self.child = subprocess.Popen(self.cmd, stdin=slave, stdout=slave,
                                              stderr=slave, preexec_fn=session)
            finally:
                os.close(slave)
            self.fd = master
        else:
            self.child = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.fd = self.child.stdout.fileno()
        self.select = selectors.DefaultSelector()
        self.select.register(self.fd, selectors.EVENT_READ)
        try:
            self.prompt()
        except BaseException:
            self.close(False)
            raise

    def write(self, data):
        if self.master is not None:
            assert os.write(self.master, data) == len(data)
        else:
            self.child.stdin.write(data)
            self.child.stdin.flush()

    def prompt(self):
        end, token = time.monotonic()+30, b'(uwvm-debug) '
        while token not in self.pending:
            assert time.monotonic() < end, (self.name, 'prompt timeout', self.log[-6000:])
            assert self.select.select(max(0, end-time.monotonic())), (self.name, 'prompt timeout')
            chunk = os.read(self.fd, 65536)
            assert chunk, (self.name, 'early exit', self.child.poll(), self.log[-6000:])
            self.pending.extend(chunk)
            self.log.extend(chunk)
        at = self.pending.index(token)+len(token)
        reply = bytes(self.pending[:at])
        del self.pending[:at]
        return reply

    def send(self, value):
        self.commands.append(value)
        self.write(value.encode()+b'\n')
        return self.prompt()

    def close(self, passed):
        try:
            if passed:
                self.commands.append('quit')
                self.write(b'quit\n')
                assert self.child.wait(timeout=10) == 0, (self.name, self.child.returncode)
            else:
                if self.child.poll() is None:
                    self.child.kill()
                self.child.wait()
        finally:
            (a.out/(self.name+'.log')).write_bytes(self.log)
            record(self.name, self.cmd, self.child.returncode, passed, self.commands)
            self.select.close()
            if self.master is not None:
                os.close(self.master)
                self.master = None


def interactive(name, cmd, marker=None, terminal=False):
    console = Console(name, cmd, terminal)
    try:
        console.send('continue')
        reply = console.send('wait')
        assert b'guest exited: 0' in reply and b'timed out' not in reply, reply
        assert (console.marker if terminal else marker) in console.log, console.log[-6000:]
    except BaseException:
        console.close(False)
        raise
    console.close(True)


proc, marker = fixture('pipe-procfd', [('proc/self/fd/0', 0, 2, 54), ('dev/fd/0', 0, 64, 76)])
interactive('pipe-procfd', command(proc, '/'), marker)

work = a.out/'regular-work'
work.mkdir()
commands = b'continue\nwait\nquit\n'
input_file, alias = work/'commands', work/'commands-hardlink'
input_file.write_bytes(commands)
os.link(input_file, alias)
regular, marker = fixture('regular-hardlink', [('commands', 0, 2, 2),
    ('commands-hardlink', 0, 64, 2), ('commands-hardlink', 8, 64, 2), ('benign', 9, 64, 0)])
cmd = command(regular, work)
with input_file.open('rb') as source:
    result = subprocess.run(cmd, stdin=source, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=40)
(a.out/'regular-hardlink.log').write_bytes(result.stdout)
passed = (result.returncode == 0 and marker in result.stdout and b'guest exited: 0' in result.stdout
          and b'timed out' not in result.stdout and input_file.read_bytes() == commands
          and alias.read_bytes() == commands and (work/'benign').exists())
record('regular-hardlink', cmd, result.returncode, passed, commands.decode().splitlines())
assert passed, result.stdout[-6000:]

interactive('pty-aliases', None, terminal=True)

# Reject launch-owned output aliases before printing even the first prompt.
# Bound a failing implementation's command/output feedback instead of permitting
# an unbounded file or pipe write during this negative test.
def output_limit():
    resource.setrlimit(resource.RLIMIT_FSIZE, (1024*1024, 1024*1024))


same = a.out/'same-input-output'
same.write_bytes(b'quit\n')
with same.open('rb') as source, same.open('ab', buffering=0) as output:
    result = subprocess.run(command(regular, work), stdin=source, stdout=output,
                            stderr=subprocess.PIPE, timeout=15, preexec_fn=output_limit)
(a.out/'same-file-output.log').write_bytes(result.stderr)
passed = result.returncode != 0 and b'[fatal]' in result.stderr and same.read_bytes() == b'quit\n'
record('same-file-output', command(regular, work), result.returncode, passed)
assert passed, (result.returncode, result.stderr, same.read_bytes()[:1024])

read_fd, write_fd = os.pipe()
try:
    result = subprocess.run(command(regular, work), stdin=read_fd, stdout=write_fd,
                            stderr=subprocess.PIPE, timeout=15)
finally:
    os.close(write_fd)
    leaked = os.read(read_fd, 4096)
    os.close(read_fd)
(a.out/'same-fifo-output.log').write_bytes(result.stderr)
passed = result.returncode != 0 and b'[fatal]' in result.stderr and not leaked
record('same-fifo-output', command(regular, work), result.returncode, passed)
assert passed, (result.returncode, result.stderr, leaked)

sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
(a.out/'summary.json').write_text(json.dumps(dict(passed=True, actual_VM=True,
    profiles=5, repository='uwvm2-ros' if a.ros else 'uwvm2', binary_sha256=sha(a.uwvm),
    runner_sha256=sha(Path(__file__)), source_root=str(a.source_root),
    cgroup=Path('/proc/self/cgroup').read_text(), fixtures={p.name: sha(p) for p in a.out.glob('*.wasm')},
    restriction='Host file-memory capabilities such as /proc/self/mem are outside this isolation claim.'), indent=2)+'\n')
print('PASS actual debug-jit sealed input: 5 CLI processes, procfd/hardlink/PTY and output aliases')
