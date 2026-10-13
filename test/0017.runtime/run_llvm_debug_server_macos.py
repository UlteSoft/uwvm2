#!/usr/bin/env python3
"""Native macOS owner-only broker late attach into a running LLVM-full guest."""

import argparse
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

MARKER = b'broker-guest-running\n'


def escape(value: bytes) -> str:
    return ''.join('\\' + format(byte, '02x') for byte in value)


def wait_for(stream, marker: bytes, transcript: bytearray):
    deadline = time.monotonic() + 45
    with selectors.DefaultSelector() as selector:
        selector.register(stream, selectors.EVENT_READ)
        while marker not in transcript:
            assert time.monotonic() < deadline, ('broker timeout', transcript[-4000:])
            assert selector.select(max(0, deadline - time.monotonic())), transcript[-4000:]
            chunk = os.read(stream.fileno(), 65536)
            assert chunk, ('broker exited', transcript[-4000:])
            transcript.extend(chunk)


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
    server = Path(__file__).resolve().parents[2] / 'tools/debug/secure_server_macos.py'
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-debug-server-') as directory:
        work = Path(directory)
        private = work / 'private'
        private.mkdir(mode=0o700)
        wat = work / 'server.wat'
        wasm = work / 'server.wasm'
        wat.write_text(f'''(module
          (import "wasi_snapshot_preview1" "fd_write" (func $write
            (param i32 i32 i32 i32) (result i32)))
          (memory (export "memory") 1)
          (data (i32.const 128) "{escape(MARKER)}")
          (func $inactive)
          (func (export "_start")
            i32.const 0 i32.const 128 i32.store
            i32.const 4 i32.const {len(MARKER)} i32.store
            i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop
            (loop $spin i32.const 1 drop br $spin)))\n''')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
        body = work / 'good_void.bin'
        body.write_bytes(b'\x00\x0b')
        vm_args = ['-m', 'run', '-Rct', '0', '-Rllvm-call-stack', 'unwind',
                   '-Rllvm-cache-path', 'disable']
        if not args.ros:
            vm_args += ['-Rcc', 'jit', '-Rcm', 'full']
        vm_args += ['--run', str(wasm)]
        broker_args = [sys.executable, str(server), 'serve', '--uwvm', str(binary),
                       '--socket-dir', str(private), '--', *vm_args]
        broker = subprocess.Popen(broker_args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, bufsize=0)
        transcript = bytearray()
        guest_pid = None
        replies = {}
        try:
            wait_for(broker.stdout, MARKER, transcript)
            for line in transcript.splitlines():
                if line.startswith(b'guest pid: '):
                    guest_pid = int(line.split(b': ', 1)[1])
            assert guest_pid is not None, transcript
            assert broker.poll() is None
            # A local same-user process without the random capability cannot
            # forward even a status request to the running VM.
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as unauthorized:
                unauthorized.settimeout(8)
                unauthorized.connect(str(private / 'control.sock'))
                unauthorized.sendall(struct.pack('!I', 32) + b'\x00' * 32)
                try:
                    assert unauthorized.recv(1) == b''
                except ConnectionResetError:
                    pass
            assert broker.poll() is None
            def command(text: str) -> bytes:
                result = subprocess.run([sys.executable, str(server), 'connect',
                                         '--socket-dir', str(private), '--command', text],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        timeout=20)
                assert result.returncode == 0, (text, result.stdout)
                replies[text] = result.stdout.decode(errors='replace')
                return result.stdout
            assert b'running' in command('status')
            assert b'stopped' in command('pause')
            assert b'function replaced; generation 2' in command(f'replace 0 1 1 {body}')
            assert b'stopped: selected participant step' in command('step wasm 1')
            assert b'stopped: native instruction step' in command('step asm 1')
            assert b'detached; guest continues' in command('quit')
            assert broker.wait(timeout=15) == 0
            os.kill(guest_pid, 0)
        finally:
            if broker.poll() is None:
                broker.terminate()
            broker.wait(timeout=15)
            if guest_pid is not None:
                try:
                    os.kill(guest_pid, 15)
                except ProcessLookupError:
                    pass
            transcript.extend(broker.stdout.read())
        # The broker must refuse its own capability directory inside a guest
        # preopen before spawning any VM or publishing a listener.
        overlap = subprocess.run([sys.executable, str(server), 'serve', '--uwvm', str(binary),
                                  '--socket-dir', str(private), '--', *vm_args[:-2],
                                  '--wasip1-mount-dir', '/sandbox', str(work),
                                  '--run', str(wasm)], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=15)
        assert overlap.returncode != 0 and b'overlaps a guest WASI preopen' in overlap.stdout, overlap.stdout
        summary = {'product_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                   'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(),
                   'replies': replies, 'broker_command': broker_args,
                   'invalid_capability_rejected': True,
                   'guest_preopen_overlap_rejected': True}
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            shutil.copy2(wat, args.out / wat.name)
            shutil.copy2(wasm, args.out / wasm.name)
            (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            (args.out / 'broker.log').write_bytes(transcript)
        print('PASS macOS host-only debugger broker late attach, step and replacement')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
