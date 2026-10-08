#!/usr/bin/env python3
"""Host-only macOS Unix broker for an explicitly authorized LLVM-full VM.

The broker is the VM's live direct parent. Only an unnamed inherited socket
enters the VM. Clients use an owner-only local socket and a random capability;
neither the listener path nor capability may overlap a guest WASI preopen.
"""

import argparse
import array
import hmac
import os
from pathlib import Path
import secrets
import signal
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time

MAX_COMMAND = 8448
MAX_REPLY = 65536
CAPABILITY_BYTES = 32
SOCKET_NAME = 'control.sock'
CAPABILITY_NAME = 'capability'
SOL_LOCAL = 0
LOCAL_PEERCRED = 1
LOCAL_PEERPID = 2
LOCAL_PEERTOKEN = 6
XUCRED_BYTES = 76
_TIMEOUT = object()


def identity(channel: socket.socket):
    pid_raw = channel.getsockopt(SOL_LOCAL, LOCAL_PEERPID, 4)
    cred = channel.getsockopt(SOL_LOCAL, LOCAL_PEERCRED, XUCRED_BYTES)
    token = channel.getsockopt(SOL_LOCAL, LOCAL_PEERTOKEN, 32)
    if len(pid_raw) != 4 or len(cred) != XUCRED_BYTES or len(token) != 32:
        raise RuntimeError('incomplete Unix peer identity')
    pid = struct.unpack('=i', pid_raw)[0]
    version, uid, groups = struct.unpack_from('=IIh', cred)
    gid = struct.unpack_from('=I', cred, 12)[0]
    if version != 0 or groups <= 0 or pid <= 1:
        raise RuntimeError('invalid Unix peer credentials')
    return pid, uid, gid, token


def private_directory(raw):
    if raw is None:
        return Path(tempfile.mkdtemp(prefix='uwvm-debug-macos-')), True
    path = Path(raw).absolute()
    info = path.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
        raise RuntimeError('server directory must be a real, owner-only directory')
    return path, False


def reject_guest_preopen_overlap(directory: Path, vm_args: list[str]):
    mount_flags = {
        '--wasip1-mount-dir': 2, '--wasip1-global-mount-dir': 2, '-I1dir': 2,
        '--wasip1-single-mount-dir': 3, '-I1Sdir': 3,
        '--wasip1-group-mount-dir': 3, '-I1Gdir': 3,
    }
    secret_directory = directory.resolve(strict=True)
    for index, argument in enumerate(vm_args):
        offset = mount_flags.get(argument)
        if offset is None:
            continue
        if index + offset >= len(vm_args):
            raise RuntimeError('incomplete WASI mount option')
        host_mount = Path(vm_args[index + offset]).resolve(strict=True)
        if secret_directory == host_mount or host_mount in secret_directory.parents:
            raise RuntimeError('debugger capability directory overlaps a guest WASI preopen')


def recv_exact(channel: socket.socket, size: int):
    data = bytearray()
    while len(data) < size:
        try:
            chunk, ancillary, flags, _ = channel.recvmsg(size - len(data),
                                                          socket.CMSG_SPACE(16 * struct.calcsize('i')))
        except socket.timeout:
            return _TIMEOUT if not data else None
        unexpected = bool(flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC))
        for level, kind, payload in ancillary:
            unexpected = True
            if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
                rights = array.array('i')
                rights.frombytes(payload[:len(payload) - len(payload) % rights.itemsize])
                for descriptor in rights:
                    os.close(descriptor)
        if unexpected:
            raise RuntimeError('unexpected ancillary rights or truncated control message')
        if not chunk:
            return None
        data.extend(chunk)
    return bytes(data)


def read_frame(channel: socket.socket, maximum: int):
    header = recv_exact(channel, 4)
    if header in (None, _TIMEOUT):
        return header
    size = struct.unpack('!I', header)[0]
    if size == 0 or size > maximum:
        raise RuntimeError('invalid bounded frame length')
    payload = recv_exact(channel, size)
    if payload is _TIMEOUT:
        raise RuntimeError('incomplete bounded frame')
    return payload


def send_frame(channel: socket.socket, payload: bytes, maximum: int):
    if not 0 < len(payload) <= maximum:
        raise RuntimeError('frame outside protocol bound')
    channel.sendall(struct.pack('!I', len(payload)) + payload)


def accept_client(listener: socket.socket, guest_pid: int):
    try:
        client, _ = listener.accept()
    except socket.timeout:
        return None
    try:
        peer = identity(client)
        if peer[0] == guest_pid or peer[1] != os.geteuid() or peer[2] != os.getegid():
            raise RuntimeError('unauthorized debugger peer')
        os.kill(peer[0], 0)
        client.settimeout(0.5)
        return client, peer
    except BaseException:
        client.close()
        raise


def serve_client(client: socket.socket, peer, vm: socket.socket, capability: bytes, guest):
    deadline = time.monotonic() + 10
    while guest.poll() is None and time.monotonic() < deadline:
        if identity(client) != peer:
            raise RuntimeError('debugger client identity changed')
        os.kill(peer[0], 0)
        packet = read_frame(client, CAPABILITY_BYTES)
        if packet is _TIMEOUT:
            continue
        if packet is None or not hmac.compare_digest(packet, capability):
            raise RuntimeError('invalid debugger capability')
        send_frame(client, b'ready\n', MAX_REPLY)
        break
    else:
        raise RuntimeError('debugger authentication timed out')
    idle = time.monotonic() + 30
    while guest.poll() is None:
        if identity(client) != peer:
            raise RuntimeError('debugger client identity changed')
        os.kill(peer[0], 0)
        packet = read_frame(client, MAX_COMMAND)
        if packet is _TIMEOUT:
            if time.monotonic() >= idle:
                return False
            continue
        if packet is None or packet == b'disconnect':
            return False
        idle = time.monotonic() + 30
        send_frame(vm, packet, MAX_COMMAND)
        reply = read_frame(vm, MAX_REPLY)
        if reply in (None, _TIMEOUT):
            raise RuntimeError('VM debugger control channel closed or timed out')
        send_frame(client, reply, MAX_REPLY)
        if packet == b'quit':
            return True
    return False


def serve(arguments):
    directory, remove_directory = private_directory(arguments.socket_dir)
    vm_args = list(arguments.vm_args)
    if vm_args and vm_args[0] == '--':
        vm_args.pop(0)
    try:
        if not vm_args:
            raise RuntimeError('guest command after -- is required')
        reject_guest_preopen_overlap(directory, vm_args)
    except BaseException:
        if remove_directory:
            directory.rmdir()
        raise
    socket_path = directory / SOCKET_NAME
    capability_path = directory / CAPABILITY_NAME
    if len(os.fsencode(socket_path)) >= 104:
        raise RuntimeError('Unix socket path exceeds macOS sockaddr_un limit')
    capability = secrets.token_bytes(CAPABILITY_BYTES)
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0)
    descriptor = os.open(capability_path, flags, 0o600)
    try:
        os.write(descriptor, capability)
        os.fsync(descriptor)
    finally:
        os.close(descriptor)
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    guest = None
    detached = False
    bound = False
    try:
        listener.bind(str(socket_path))
        bound = True
        os.chmod(socket_path, 0o600)
        listener.listen(2)
        listener.settimeout(0.25)
        broker, vm_endpoint = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            command = [str(arguments.uwvm), '--debug-jit-control-fd', str(vm_endpoint.fileno()), *vm_args]
            guest = subprocess.Popen(command, pass_fds=(vm_endpoint.fileno(),))
        finally:
            vm_endpoint.close()
        broker.settimeout(6)
        print(f'debug server: {socket_path}', flush=True)
        print(f'guest pid: {guest.pid}', flush=True)
        print(f'client: {Path(__file__).name} connect --socket-dir {directory}', flush=True)
        while guest.poll() is None:
            try:
                accepted = accept_client(listener, guest.pid)
            except (OSError, RuntimeError) as error:
                print(f'debug client rejected: {error}', file=sys.stderr, flush=True)
                continue
            if accepted is None:
                continue
            client, peer = accepted
            try:
                with client:
                    detached = serve_client(client, peer, broker, capability, guest)
            except (OSError, RuntimeError) as error:
                print(f'debug client disconnected: {error}', file=sys.stderr, flush=True)
            if detached:
                break
        broker.close()
        if detached:
            print('debugger detached; guest continues without a control endpoint', flush=True)
            return 0
        return guest.wait()
    finally:
        listener.close()
        if bound:
            socket_path.unlink(missing_ok=True)
        capability_path.unlink(missing_ok=True)
        if remove_directory:
            directory.rmdir()
        if guest is not None and not detached and guest.poll() is None:
            guest.terminate()
            try:
                guest.wait(timeout=5)
            except subprocess.TimeoutExpired:
                guest.kill()
                guest.wait()


def connect(arguments):
    directory, _ = private_directory(arguments.socket_dir)
    descriptor = os.open(directory / CAPABILITY_NAME, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0))
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
            raise RuntimeError('debugger capability file is not private')
        capability = os.read(descriptor, CAPABILITY_BYTES + 1)
        if len(capability) != CAPABILITY_BYTES:
            raise RuntimeError('invalid debugger capability size')
    finally:
        os.close(descriptor)
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as channel:
        channel.settimeout(8)
        channel.connect(str(directory / SOCKET_NAME))
        send_frame(channel, capability, CAPABILITY_BYTES)
        if read_frame(channel, MAX_REPLY) != b'ready\n':
            raise RuntimeError('debugger authorization rejected')
        for command in arguments.commands:
            payload = command.encode('ascii')
            if not 0 < len(payload) <= MAX_COMMAND:
                raise RuntimeError('command exceeds 8448 bytes')
            send_frame(channel, payload, MAX_COMMAND)
            reply = read_frame(channel, MAX_REPLY)
            if reply in (None, _TIMEOUT):
                raise RuntimeError('debugger server closed')
            sys.stdout.buffer.write(reply)
            sys.stdout.buffer.flush()
            if payload == b'quit':
                return 0
        if arguments.commands:
            return 0
        while True:
            try:
                line = input('(uwvm-debug-server) ')
            except EOFError:
                return 0
            payload = line.encode('ascii')
            if not 0 < len(payload) <= MAX_COMMAND:
                print('error: command must contain 1..8448 ASCII bytes')
                continue
            send_frame(channel, payload, MAX_COMMAND)
            reply = read_frame(channel, MAX_REPLY)
            if reply in (None, _TIMEOUT):
                raise RuntimeError('debugger server closed')
            sys.stdout.buffer.write(reply)
            sys.stdout.buffer.flush()
            if payload == b'quit':
                return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subcommands = parser.add_subparsers(dest='mode', required=True)
    host = subcommands.add_parser('serve', help='launch a VM with an authenticated local control server')
    host.add_argument('--uwvm', required=True, type=Path)
    host.add_argument('--socket-dir', type=Path,
                      help='existing owner-only directory; default is a fresh private directory')
    host.add_argument('vm_args', nargs=argparse.REMAINDER)
    client = subcommands.add_parser('connect', help='send commands through a running host broker')
    client.add_argument('--socket-dir', required=True, type=Path)
    client.add_argument('--command', dest='commands', action='append', default=[])
    arguments = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('macOS secure debugger server requires Darwin Unix credentials')
    if arguments.mode == 'serve':
        def stop_on_signal(_number, _frame):
            raise KeyboardInterrupt
        signal.signal(signal.SIGTERM, stop_on_signal)
    try:
        return serve(arguments) if arguments.mode == 'serve' else connect(arguments)
    except KeyboardInterrupt:
        return 130
    except (OSError, RuntimeError, UnicodeEncodeError) as error:
        print(f'debug server: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
