#!/usr/bin/env python3
"""Native macOS ARM64 late LLVM-full control over a sealed Unix stream."""

import argparse
import array
import fcntl
import hashlib
import json
import os
from pathlib import Path
import selectors
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

MARKER = b'guest-running\n'
MAX_REPLY = 65536


def wat_escape(value: bytes) -> str:
    return ''.join('\\' + format(byte, '02x') for byte in value)


def make_fixture(tools: Path, work: Path, fd: int) -> Path:
    relative = f'dev/fd/{fd}'.encode()
    wat = work / 'late-control.wat'
    wasm = work / 'late-control.wasm'
    wat.write_text(f'''(module
      (import "wasi_snapshot_preview1" "path_open" (func $open
        (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_write" (func $write
        (param i32 i32 i32 i32) (result i32)))
      (import "wasi_snapshot_preview1" "fd_read" (func $read
        (param i32 i32 i32 i32) (result i32)))
      (memory (export "memory") 1)
      (data (i32.const 128) "{wat_escape(MARKER)}")
      (data (i32.const 256) "{wat_escape(relative)}")
      (func $inactive)
      (func (export "_start")
        i32.const 0 i32.const 512 i32.store
        i32.const 4 i32.const 1 i32.store
        i32.const {fd} i32.const 0 i32.const 1 i32.const 100
        call $read i32.eqz if unreachable end
        i32.const 3 i32.const 1 i32.const 256
        i32.const {len(relative)} i32.const 0 i64.const 2
        i64.const 0 i32.const 0 i32.const 96 call $open
        i32.eqz if unreachable end
        i32.const 0 i32.const 128 i32.store
        i32.const 4 i32.const {len(MARKER)} i32.store
        i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write
        if unreachable end
        (loop $spin i32.const 1 drop br $spin)))\n''')
    subprocess.run([str(tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(tools), 'validate', str(wasm)], check=True)
    return wasm


def command(binary: Path, wasm: Path, fd: int | None, ros: bool,
            mode: str = 'run', compiler: str = 'jit', compile_mode: str = 'full') -> list[str]:
    argv = [str(binary), '-m', mode, '-Rct', '0', '-Rllvm-call-stack', 'unwind',
            '-Rllvm-cache-path', 'disable']
    if mode == 'run':
        if ros:
            if compiler == 'int':
                argv.append('-Rint')
        else:
            argv += ['-Rcc', compiler, '-Rcm', compile_mode]
    if fd is not None:
        argv += ['--debug-jit-control-fd', str(fd)]
    argv += ['--wasip1-mount-dir', '/sandbox', '/', '--run', str(wasm)]
    return argv


def receive_exact(channel: socket.socket, count: int) -> bytes:
    data = bytearray()
    while len(data) < count:
        chunk = channel.recv(count - len(data))
        if not chunk:
            raise EOFError('debug control closed')
        data.extend(chunk)
    return bytes(data)


def exchange(channel: socket.socket, command_text: str) -> bytes:
    command_bytes = command_text.encode('ascii')
    assert 0 < len(command_bytes) <= 512
    channel.sendall(struct.pack('!I', len(command_bytes)) + command_bytes)
    size = struct.unpack('!I', receive_exact(channel, 4))[0]
    assert 0 <= size <= MAX_REPLY, size
    return receive_exact(channel, size)


def wait_marker(process: subprocess.Popen, marker: bytes) -> bytes:
    observed = bytearray()
    deadline = time.monotonic() + 45
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        while marker not in observed:
            assert process.poll() is None, (process.returncode, observed[-5000:])
            assert time.monotonic() < deadline, observed[-5000:]
            events = selector.select(max(0, deadline - time.monotonic()))
            assert events, ('marker timeout', observed[-5000:])
            chunk = os.read(process.stdout.fileno(), 65536)
            assert chunk, ('guest exited before marker', process.poll(), observed[-5000:])
            observed.extend(chunk)
    return bytes(observed)


def run_live(binary: Path, tools: Path, work: Path, ros: bool, rows: list[dict]) -> Path:
    host, raw_vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    protected = fcntl.fcntl(raw_vm.fileno(), fcntl.F_DUPFD, 100)
    raw_vm.close()
    vm = socket.socket(fileno=protected)
    host.settimeout(8)
    wasm = make_fixture(tools, work, vm.fileno())
    body = work / 'good_void.bin'
    body.write_bytes(b'\x00\x0b')
    argv = command(binary, wasm, vm.fileno(), ros)
    process = subprocess.Popen(argv, pass_fds=(vm.fileno(),),
                               stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, bufsize=0)
    vm.close()
    output = bytearray()
    try:
        output.extend(wait_marker(process, MARKER))
        assert b'running' in exchange(host, 'status')
        source_rejection = exchange(host, 'break-source 0 missing.c:1')
        assert b'error: source stepping unavailable' in source_rejection, source_rejection
        assert b'stopped' in exchange(host, 'pause')
        assert b'thread 1' in exchange(host, 'info threads')
        assert b'#0 module=0 function=4' in exchange(host, 'bt 1')
        replaced = exchange(host, f'replace 0 3 1 {body}')
        assert b'function replaced; generation 2' in replaced, replaced
        wasm_step = exchange(host, 'step wasm 1')
        assert b'stopped: selected participant step' in wasm_step, wasm_step
        native_step = exchange(host, 'step asm 1')
        assert b'stopped: native instruction step' in native_step, native_step
        assert b'running' in exchange(host, 'continue')
        assert b'detached; guest continues' in exchange(host, 'quit')
        assert process.poll() is None
        assert host.recv(1) == b''
        rows.append(dict(case='late_pause_wasm_native_step_replace', passed=True,
                         source_rejection=source_rejection.decode(errors='replace'),
                         native_step=native_step.decode(errors='replace'),
                         replacement=replaced.decode(errors='replace')))
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=15)
        output.extend(process.stdout.read())
        host.close()
        (work / 'live.log').write_bytes(output)
    return wasm


def rejection_cases(binary: Path, wasm: Path, ros: bool, work: Path, rows: list[dict]) -> None:
    variants = [('interpreter', 'run', 'int', 'full'),
                ('console-mode', 'debug-jit', 'jit', 'full'),
                ('validation-mode', 'validation', 'jit', 'full')]
    if not ros:
        variants += [('jit-lazy', 'run', 'jit', 'lazy'),
                     ('tiered', 'run', 'tiered', 'full')]
    for name, mode, compiler, compile_mode in variants:
        host, vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            argv = command(binary, wasm, vm.fileno(), ros, mode, compiler, compile_mode)
            completed = subprocess.run(argv, pass_fds=(vm.fileno(),), stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        finally:
            host.close()
            vm.close()
        (work / (name + '.log')).write_bytes(completed.stdout)
        # A JIT-only Mac build may reject an unavailable compiler at parsing
        # time, before the run-mode fatal check is reached.
        parser_rejected = name in ('interpreter', 'tiered') and \
            (b'Invalid runtime compiler' in completed.stdout or
             b'invalid parameter: -Rint' in completed.stdout)
        mode_rejected = b'[fatal]' in completed.stdout and \
            b'unsupported in the current mode' in completed.stdout
        assert completed.returncode != 0 and (parser_rejected or mode_rejected), \
            (name, completed.stdout[-4000:])
        rows.append(dict(case=name, passed=True,
                         expected='unavailable compiler' if parser_rejected else 'fatal unsupported mode'))

    # A non-socket descriptor and a named socket must not be silently adopted.
    with open(os.devnull, 'rb') as invalid:
        argv = command(binary, wasm, invalid.fileno(), ros)
        completed = subprocess.run(argv, pass_fds=(invalid.fileno(),), stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        assert completed.returncode != 0 and b'Unable to authorize' in completed.stdout
    rows.append(dict(case='regular_fd_rejected', passed=True))

    # A socket reachable through guest stdio would let Wasm drive management
    # input without authorization. The startup seal rejects this alias.
    host, vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        argv = command(binary, wasm, vm.fileno(), ros)
        completed = subprocess.run(argv, pass_fds=(vm.fileno(),), stdin=vm,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        assert completed.returncode != 0 and b'Unable to authorize' in completed.stdout, completed.stdout
    finally:
        host.close()
        vm.close()
    rows.append(dict(case='guest_stdio_alias_rejected', passed=True))

    for name in ('oversized_frame', 'rights_frame'):
        host, raw_vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        protected = fcntl.fcntl(raw_vm.fileno(), fcntl.F_DUPFD, 100)
        raw_vm.close()
        vm = socket.socket(fileno=protected)
        assert vm.fileno() == 100, vm.fileno()
        host.settimeout(8)
        argv = command(binary, wasm, vm.fileno(), ros)
        process = subprocess.Popen(argv, pass_fds=(vm.fileno(),), stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        vm.close()
        output = bytearray()
        try:
            output.extend(wait_marker(process, MARKER))
            if name == 'oversized_frame':
                host.sendall(struct.pack('!I', 513))
            else:
                with open(os.devnull, 'rb') as attached:
                    payload = struct.pack('!I', 6) + b'status'
                    assert host.sendmsg([payload], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                         array.array('i', [attached.fileno()]))]) == len(payload)
            assert host.recv(1) == b'', name
            assert process.poll() is None, (name, process.returncode)
            rows.append(dict(case=name, passed=True, expected='channel closed; guest alive'))
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=15)
            output.extend(process.stdout.read())
            host.close()
            (work / (name + '.log')).write_bytes(output)


def peer_exit_case(binary: Path, wasm: Path, ros: bool, work: Path, rows: list[dict]) -> None:
    # The forked launcher is the VM's authenticated direct parent. It sends
    # only its own endpoint to the test supervisor, then exits. The VM must
    # revoke the channel even though another process still holds that endpoint.
    supervisor, launcher = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    supervisor.settimeout(35)
    launcher_pid = os.fork()
    if launcher_pid == 0:
        try:
            supervisor.close()
            host, raw_vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
            protected = fcntl.fcntl(raw_vm.fileno(), fcntl.F_DUPFD, 100)
            raw_vm.close()
            vm = socket.socket(fileno=protected)
            process = subprocess.Popen(command(binary, wasm, vm.fileno(), ros),
                                       pass_fds=(vm.fileno(),), stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
            vm.close()
            wait_marker(process, MARKER)
            payload = str(process.pid).encode()
            launcher.sendmsg([payload], [(socket.SOL_SOCKET, socket.SCM_RIGHTS,
                              array.array('i', [host.fileno()]))])
            os._exit(0)
        except BaseException:
            try:
                launcher.send(b'error')
            except OSError:
                pass
            os._exit(1)
    launcher.close()
    vm_pid = None
    try:
        payload, ancillary, flags, _ = supervisor.recvmsg(64, socket.CMSG_SPACE(4))
        assert payload.isdigit() and not flags, (payload, flags)
        assert len(ancillary) == 1 and ancillary[0][:2] == (socket.SOL_SOCKET, socket.SCM_RIGHTS)
        passed = array.array('i')
        passed.frombytes(ancillary[0][2])
        assert len(passed) == 1
        vm_pid = int(payload)
        _, status = os.waitpid(launcher_pid, 0)
        assert os.waitstatus_to_exitcode(status) == 0, status
        with socket.socket(fileno=passed[0]) as host:
            host.settimeout(8)
            assert host.recv(1) == b'', 'VM retained peer channel after direct parent exited'
        os.kill(vm_pid, 0)
        rows.append(dict(case='launcher_exit_revokes_channel', passed=True, guest_pid=vm_pid))
    finally:
        supervisor.close()
        if vm_pid is not None:
            try:
                os.kill(vm_pid, 15)
            except ProcessLookupError:
                pass


def wrong_parent_case(binary: Path, wasm: Path, ros: bool, rows: list[dict]) -> None:
    # Creating the pair in a grandparent and handing it to an intermediary
    # must not make that intermediary an authorized debugger peer.
    host, vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    reader, writer = os.pipe()
    launcher_pid = os.fork()
    if launcher_pid == 0:
        try:
            host.close()
            os.close(reader)
            result = subprocess.run(command(binary, wasm, vm.fileno(), ros),
                                    pass_fds=(vm.fileno(),), stdin=subprocess.DEVNULL,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
            os.write(writer, struct.pack('!i', result.returncode) + result.stdout[:4000])
            os._exit(0)
        except BaseException:
            os._exit(1)
    vm.close()
    os.close(writer)
    try:
        evidence = os.read(reader, 4096)
        _, status = os.waitpid(launcher_pid, 0)
        assert os.waitstatus_to_exitcode(status) == 0 and len(evidence) >= 4, evidence
        exit_code = struct.unpack('!i', evidence[:4])[0]
        assert exit_code != 0 and b'Unable to authorize' in evidence[4:], evidence
        rows.append(dict(case='wrong_parent_rejected', passed=True))
    finally:
        os.close(reader)
        host.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', required=True, type=Path)
    parser.add_argument('--wasm-tools', type=Path, default=shutil.which('wasm-tools'))
    parser.add_argument('--out', type=Path)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'darwin' or not args.wasm_tools:
        parser.error('macOS and wasm-tools are required')
    binary = args.uwvm.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-control-') as temporary:
        work = Path(temporary)
        rows = []
        wasm = run_live(binary, Path(args.wasm_tools), work, args.ros, rows)
        rejection_cases(binary, wasm, args.ros, work, rows)
        peer_exit_case(binary, wasm, args.ros, work, rows)
        wrong_parent_case(binary, wasm, args.ros, rows)
        summary = {'product_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                   'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(),
                   'rows': rows}
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            for file in work.iterdir():
                if file.is_file():
                    shutil.copy2(file, args.out / file.name)
            (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print('PASS macOS late LLVM-full control, pause, wasm/native step, replacement and mode gates')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
