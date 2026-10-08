#!/usr/bin/env python3
"""Linux LLVM-full late control capability: live guest, isolation, and fail-closed packets."""
import argparse
import array
import hashlib
import json
import os
from pathlib import Path
import resource
import selectors
import signal
import socket
import subprocess
import time


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--uwvm', required=True, type=Path)
parser.add_argument('--wasm-tools', required=True, type=Path)
parser.add_argument('--source-root', required=True, type=Path)
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--ros', action='store_true')
a = parser.parse_args()
a.uwvm = a.uwvm.resolve()
a.wasm_tools = a.wasm_tools.resolve()
a.source_root = a.source_root.resolve()
a.out = a.out.resolve()
subprocess.run(['bash', str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.out.mkdir(parents=True, exist_ok=False)
rows = []


def escaped(value):
    return ''.join('\\'+format(byte, '02x') for byte in value.encode())


def fixture(name, control_fd=None):
    marker = b'guest-running\n'
    definitions = ''
    exception_prefix = ''
    if name == 'late-eh-control':
        definitions = '(tag $e (param i32)) (func $raise (result i32) i32.const 73 throw $e)'
        exception_prefix = '''(block $caught (result i32)
          (try_table (catch $e $caught) call $raise drop) unreachable)
          i32.const 73 i32.ne if unreachable end'''
    if control_fd is None:
        body = 'i32.const 7 drop'
        path_data = ''
        read_import = ''
    else:
        path = 'proc/self/fd/'+str(control_fd)
        path_data = f'(data (i32.const 256) "{escaped(path)}")'
        read_import = '''(import "wasi_snapshot_preview1" "fd_read" (func $read
          (param i32 i32 i32 i32) (result i32)))'''
        # The guest is deliberately given a root preopen and tries to reopen the
        # management endpoint. Any success is a test failure before the marker.
        body = f'''{exception_prefix}
          i32.const 0 i32.const 512 i32.store
          i32.const 4 i32.const 1 i32.store
          i32.const {control_fd} i32.const 0 i32.const 1 i32.const 100
          call $read i32.eqz if unreachable end
          i32.const 3 i32.const 1 i32.const 256
          i32.const {len(path)} i32.const 0 i64.const 2
          i64.const 0 i32.const 0 i32.const 96 call $open
          i32.eqz if unreachable end
          i32.const 0 i32.const 128 i32.store
          i32.const 4 i32.const {len(marker)} i32.store
          i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write
          if unreachable end
          (loop $spin i32.const 1 drop br $spin)'''
    wat = f'''(module
      (import "wasi_snapshot_preview1" "path_open" (func $open
       (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_write" (func $write
       (param i32 i32 i32 i32) (result i32)))
      {read_import}
      (memory (export "memory") 1)
      {definitions}
      (data (i32.const 128) "{escaped(marker.decode())}")
      {path_data}
      (func (export "_start") {body}))'''
    source = a.out/(name+'.wat')
    binary = a.out/(name+'.wasm')
    source.write_text(wat+'\n')
    subprocess.run([str(a.wasm_tools), 'parse', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(a.wasm_tools), 'validate', str(binary)], check=True)
    return binary, marker


def command(binary, fd=None, mode='run', compiler='jit', compile_mode='full'):
    args = [str(a.uwvm), '-m', mode, '-Rct', '0', '-Rllvm-cache-path', 'disable',
            '-WFE-exceptions']
    if mode == 'run':
        args += (([] if compiler == 'jit' else ['-Rint']) if a.ros else
                 ['-Rcc', compiler, '-Rcm', compile_mode])
    if fd is not None:
        args += ['--debug-jit-control-fd', str(fd)]
    return args + ['--wasip1-mount-dir', '/sandbox', '/', '--run', str(binary)]


class Guest:
    def __init__(self, name):
        self.name = name
        self.host, vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.host.settimeout(5)
        self.binary, self.marker = fixture(name, vm.fileno())
        self.cmd = command(self.binary, vm.fileno())
        self.proc = subprocess.Popen(self.cmd, pass_fds=(vm.fileno(),),
                                     stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
        vm.close()
        self.output = bytearray()
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.proc.stdout, selectors.EVENT_READ)
        try:
            self.wait_marker()
        except BaseException:
            self.close(False)
            raise

    def wait_marker(self):
        deadline = time.monotonic()+35
        while self.marker not in self.output:
            assert time.monotonic() < deadline, (self.name, 'marker timeout', self.output[-5000:])
            events = self.selector.select(max(0, deadline-time.monotonic()))
            assert events, (self.name, 'marker timeout', self.output[-5000:])
            block = os.read(self.proc.stdout.fileno(), 65536)
            assert block, (self.name, 'guest exited before late attach', self.proc.poll(), self.output[-5000:])
            self.output.extend(block)
        assert self.proc.poll() is None, (self.name, 'guest exited', self.proc.returncode)

    def send(self, text):
        assert self.host.send(text.encode()) == len(text), text
        reply = self.host.recv(65537)
        assert reply and len(reply) <= 65536, (text, reply)
        return reply

    def reject_packet(self, data, ancillary=()):
        assert self.host.sendmsg([data], list(ancillary)) == len(data)
        assert self.host.recv(1) == b'', self.name
        assert self.proc.poll() is None, (self.name, 'guest died on rejected packet')

    def close(self, passed):
        try:
            if self.proc.poll() is None:
                self.proc.terminate()
            self.proc.wait(timeout=10)
            self.output.extend(self.proc.stdout.read())
        finally:
            self.selector.close()
            self.host.close()
            (a.out/(self.name+'.log')).write_bytes(self.output)
            rows.append(dict(name=self.name, command=self.cmd, passed=passed,
                             guest_exit=self.proc.returncode, output_bytes=len(self.output)))
            (a.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')


guest = Guest('late-live-control')
try:
    # This first command is sent only after the Wasm fd_write marker was observed.
    state = guest.send('status')
    assert b'running' in state, state
    stopped = guest.send('pause')
    assert b'stopped' in stopped, stopped
    # The inherited capability uses the same source command parser as the
    # console, and a guest without DWARF must fail closed before resuming.
    assert b'error: source stepping unavailable' in guest.send('break-source 0 missing.c:1')
    assert b'thread 1' in guest.send('info threads')
    trace = guest.send('bt 1')
    # Three WASI imports occupy function indices 0..2.
    assert b'#0 module=0 function=3' in trace, trace
    assert b'running' in guest.send('continue')
    assert b'detached; guest continues' in guest.send('quit')
    assert guest.proc.poll() is None
    assert guest.host.recv(1) == b''
except BaseException:
    guest.close(False)
    raise
guest.close(True)

guest = Guest('late-eh-control')
try:
    assert b'running' in guest.send('status')
    assert b'stopped' in guest.send('pause')
    assert b'#0 module=0 function=4' in guest.send('bt 1')
    guest.send('continue')
    assert b'detached; guest continues' in guest.send('quit')
    assert guest.proc.poll() is None
except BaseException:
    guest.close(False)
    raise
guest.close(True)

guest = Guest('oversized-packet')
try:
    guest.reject_packet(b'x'*513)
except BaseException:
    guest.close(False)
    raise
guest.close(True)

guest = Guest('rights-packet')
try:
    with open('/dev/null', 'rb') as fd:
        guest.reject_packet(b'status', [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                          array.array('i', [fd.fileno()]))])
except BaseException:
    guest.close(False)
    raise
guest.close(True)


def guest_null_fds(pid):
    directory = Path(f'/proc/{pid}/fd')
    return sum(1 for fd in directory.iterdir() if os.readlink(fd) == '/dev/null')


guest = Guest('zero-byte-rights-packet')
try:
    before = guest_null_fds(guest.proc.pid)
    with open('/dev/null', 'rb') as donated:
        guest.reject_packet(b'', [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                    array.array('i', [donated.fileno()]))])
    after = guest_null_fds(guest.proc.pid)
    assert after == before, ('zero-byte SCM_RIGHTS leaked a host FD', before, after)
except BaseException:
    guest.close(False)
    raise
guest.close(True)

guest = Guest('truncated-rights-packet')
try:
    before = guest_null_fds(guest.proc.pid)
    with open('/dev/null', 'rb') as donated:
        guest.reject_packet(b'status', [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                                         array.array('i', [donated.fileno()] * 64))])
    after = guest_null_fds(guest.proc.pid)
    assert after == before, ('truncated SCM_RIGHTS leaked host FDs', before, after)
except BaseException:
    guest.close(False)
    raise
guest.close(True)

peer_binary, peer_marker = fixture('launcher-exit', 999)
handoff, launch_handoff = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
handoff.settimeout(35)
launcher_pid = os.fork()
if launcher_pid == 0:
    try:
        handoff.close()
        control, vm_endpoint = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        cmd = command(peer_binary, vm_endpoint.fileno())
        child = subprocess.Popen(cmd, pass_fds=(vm_endpoint.fileno(),),
                                 stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
        vm_endpoint.close()
        watch = selectors.DefaultSelector()
        watch.register(child.stdout, selectors.EVENT_READ)
        observed = bytearray()
        deadline = time.monotonic()+30
        while peer_marker not in observed and time.monotonic() < deadline:
            if not watch.select(max(0, deadline-time.monotonic())):
                break
            block = os.read(child.stdout.fileno(), 65536)
            if not block:
                break
            observed.extend(block)
        if peer_marker not in observed:
            os.kill(child.pid, signal.SIGTERM)
            launch_handoff.send(b'error: '+observed[-1000:])
            os._exit(1)
        launch_handoff.sendmsg([str(child.pid).encode()], [(socket.SOL_SOCKET,
            socket.SCM_RIGHTS, array.array('i', [control.fileno()]))])
        # The external launcher exits after guest entry. The VM must observe
        # pidfd death and shut down even the sealed duplicate of its endpoint.
        os._exit(0)
    except BaseException as failure:
        launch_handoff.send(('error: '+repr(failure)).encode()[:1000])
        os._exit(1)

launch_handoff.close()
vm_pid = None
try:
    payload, ancillary, flags, _ = handoff.recvmsg(128, socket.CMSG_SPACE(4),
                                                  socket.MSG_CMSG_CLOEXEC)
    assert (flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC)) == 0 and payload.isdigit(), (payload, flags)
    vm_pid = int(payload)
    passed = array.array('i')
    assert len(ancillary) == 1 and ancillary[0][:2] == (socket.SOL_SOCKET, socket.SCM_RIGHTS)
    passed.frombytes(ancillary[0][2])
    assert len(passed) == 1
    _, status = os.waitpid(launcher_pid, 0)
    assert os.waitstatus_to_exitcode(status) == 0, status
    with socket.socket(fileno=passed[0]) as peer:
        peer.settimeout(5)
        assert peer.recv(1) == b'', 'VM retained a live channel after launcher exit'
    os.kill(vm_pid, 0)
    rows.append(dict(name='launcher-exit', command=command(peer_binary, '<inherited>'),
                     passed=True, guest_pid=vm_pid, expected='pidfd shutdown'))
finally:
    handoff.close()
    if vm_pid is not None:
        try:
            os.kill(vm_pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

finite, _ = fixture('ordinary-no-control')
plain = command(finite)
result = subprocess.run(plain, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=30)
(a.out/'ordinary-no-control.log').write_bytes(result.stdout)
assert result.returncode == 0 and b'(uwvm-debug)' not in result.stdout, result.stdout[-5000:]
rows.append(dict(name='ordinary-no-control', command=plain, passed=True,
                 guest_exit=result.returncode))

incompatible = [('interpreter', 'run', 'int', 'full'),
                ('console-mode', 'debug-jit', 'jit', 'full')]
if not a.ros:
    incompatible += [('lazy-mode', 'run', 'jit', 'lazy'),
                     ('tiered-mode', 'run', 'tiered', 'full')]
for name, mode, compiler, compile_mode in incompatible:
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    try:
        cmd = command(finite, right.fileno(), mode, compiler, compile_mode)
        result = subprocess.run(cmd, pass_fds=(right.fileno(),), stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
    finally:
        left.close()
        right.close()
    (a.out/(name+'.log')).write_bytes(result.stdout)
    assert result.returncode != 0 and b'[fatal]' in result.stdout and \
           b'unsupported in the current mode' in result.stdout, (name, result.stdout[-5000:])
    rows.append(dict(name=name, command=cmd, passed=True, expected='fatal unsupported mode'))

for alias in ('stdin', 'stdout'):
    left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    try:
        cmd = command(finite, right.fileno())
        result = subprocess.run(cmd, pass_fds=(right.fileno(),),
            stdin=right if alias == 'stdin' else subprocess.DEVNULL,
            stdout=right if alias == 'stdout' else subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=30)
    finally:
        left.close()
        right.close()
    diagnostic = result.stderr + (result.stdout or b'')
    (a.out/(alias+'-alias.log')).write_bytes(diagnostic)
    assert result.returncode != 0 and b'[fatal]' in diagnostic and \
           b'Unable to authorize debug-jit control FD' in diagnostic, (alias, diagnostic[-5000:])
    rows.append(dict(name=alias+'-alias', command=cmd, passed=True,
                     expected='fatal stdio alias'))

cmd = command(finite, 999)
result = subprocess.run(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=30)
(a.out/'invalid-descriptor.log').write_bytes(result.stdout)
assert result.returncode != 0 and b'[fatal]' in result.stdout and \
       b'Unable to authorize debug-jit control FD' in result.stdout, result.stdout[-5000:]
rows.append(dict(name='invalid-descriptor', command=cmd, passed=True,
                 expected='fatal unauthorised descriptor'))

(a.out/'runs.json').write_text(json.dumps(rows, indent=2)+'\n')
(a.out/'summary.json').write_text(json.dumps(dict(passed=True, processes=len(rows),
    binary_sha256=hashlib.file_digest(a.uwvm.open('rb'), 'sha256').hexdigest(),
    scope='Late control after guest marker and Core3 EH throw/catch; denied inherited WASI FD and procfd reopen; pause/source-break rejection/backtrace/continue/detach; '
          'oversized/SCM_RIGHTS packet closure including zero-byte ancillary without FD leak; peer PID exit; '
          'no-flag run; unsupported modes, stdio aliases and invalid FD.'), indent=2)+'\n')
print('PASS LLVM-full late control FD', len(rows), 'processes')
