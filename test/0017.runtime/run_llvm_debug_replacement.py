#!/usr/bin/env python3
"""Bounded production LLVM debug-full replacement smoke test."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import selectors
import socket
import subprocess
import time

def read_until(stream, marker, pending, transcript, timeout=60):
    deadline = time.monotonic() + timeout
    with selectors.DefaultSelector() as selector:
        selector.register(stream, selectors.EVENT_READ)
        while marker not in pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not selector.select(remaining):
                raise AssertionError(('timeout', marker, bytes(transcript)[-4000:]))
            chunk = os.read(stream.fileno(), 65536)
            if not chunk:
                raise AssertionError(('exit', marker, bytes(transcript)[-4000:]))
            pending.extend(chunk)
            transcript.extend(chunk)
    end = pending.index(marker) + len(marker)
    result = bytes(pending[:end])
    del pending[:end]
    return result

def control_fd_probe(args, rows):
    marker = b'hot-replace-fd-ready\n'
    data = ''.join('\\' + format(byte, '02x') for byte in marker)
    wat = args.out / 'late-control.wat'
    wasm = args.out / 'late-control.wasm'
    wat.write_text(f'''(module
      (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
      (memory (export "memory") 1)
      (data (i32.const 128) "{data}")
      (func $inactive)
      (func (export "_start")
        i32.const 0 i32.const 128 i32.store
        i32.const 4 i32.const {len(marker)} i32.store
        i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop
        (loop $spin br $spin)))\n''')
    subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
    body = args.out / 'good_void.bin'
    body.write_bytes(b'\x00\x0b')
    host, vm = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    host.settimeout(15)
    command = [str(args.uwvm), '-m', 'run', '-Rct', '0', '-Rllvm-call-stack', 'unwind',
               '-Rllvm-cache-path', 'disable']
    if not args.ros:
        command += ['-Rcc', 'jit', '-Rcm', 'full']
    command += ['--debug-jit-control-fd', str(vm.fileno()), '--wasip1-mount-dir', '/sandbox', '/',
                '--run', str(wasm)]
    process = subprocess.Popen(command, pass_fds=(vm.fileno(),), stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    vm.close()
    transcript = bytearray()
    def send(text):
        request = text.encode()
        assert host.send(request) == len(request)
        return host.recv(65537)
    try:
        read_until(process.stdout, marker, bytearray(), transcript, 45)
        assert b'running' in send('status')
        assert b'stopped' in send('pause')
        result = send(f'replace 0 1 1 {body}')
        rows.append({'case': 'late_fd_inactive_replaced', 'output': result.decode(errors='replace')})
        assert b'function replaced; generation 2' in result, result
        result = send(f'replace 0 2 1 {body}')
        rows.append({'case': 'late_fd_active_frame_rejected', 'output': result.decode(errors='replace')})
        assert b'active on a stopped Wasm stack' in result, result
        assert b'stopped' in send('status')
        assert b'running' in send('continue')
        assert b'detached; guest continues' in send('quit')
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=15)
        transcript.extend(process.stdout.read())
        host.close()
        (args.out / 'control-fd.log').write_bytes(transcript)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--replacements', type=int, default=2, choices=(0, 1, 2))
    parser.add_argument('--call-existing', action='store_true')
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    args.out.mkdir(parents=True, mode=0o700, exist_ok=False)
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    wat = args.out / 'replace.wat'
    wasm = args.out / 'replace.wasm'
    wat.write_text('''(module
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (memory 1)
  (data (i32.const 128) "replaced-value=3\\0a")
  (func $value (result i32) i32.const 51)
  (func (export "_start")
    i32.const 143 call $value i32.store8
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 17 i32.store
    i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop))\n''')
    if args.call_existing:
        wat.write_text(wat.read_text().replace('  (func $value (result i32)',
            '  (func $helper (result i32) i32.const 54)\n  (func $value (result i32)'))
    subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
    # First replacement grows the expression: its new offset 3 is executable
    # even though offset 3 was outside the original function body.
    payloads = {'good4': b'\x00\x41\x00\x1a\x41\x34\x0b', 'good5': b'\x00\x41\x35\x0b',
                'malformed': b'\x00\xff\x0b', 'wrong_result': b'\x00\x42\x01\x0b'}
    if args.call_existing:
        payloads['good4'] = b'\x00\x41\x00\x1a\x10\x01\x0b'
    for name, data in payloads.items():
        (args.out / (name + '.bin')).write_bytes(data)
    command = [str(args.uwvm), '-m', 'debug-jit', '-Rct', '0', '-Rllvm-call-stack', 'instruction',
               '-Rllvm-cache-path', 'disable', '--run', str(wasm)]
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    pending = bytearray()
    transcript = bytearray()
    prompt = b'(uwvm-debug) '
    rows = []
    def reply(text):
        process.stdin.write(text.encode() + b'\n')
        process.stdin.flush()
        return read_until(process.stdout, prompt, pending, transcript)
    try:
        read_until(process.stdout, prompt, pending, transcript)
        result = reply('status')
        assert b'prepared; no Wasm instruction executed' in result, result
        def replace(generation, payload):
            return reply(f'replace 0 {2 if args.call_existing else 1} {generation} {args.out / (payload + ".bin")}')
        if args.replacements:
            target = 2 if args.call_existing else 1
            assert b'breakpoint 1' in reply(f'break 0 {target} 0')
            result = replace(1, 'malformed')
            rows.append({'case': 'malformed_rejected', 'output': result.decode(errors='replace')})
            assert b'replacement body failed WebAssembly validation' in result, result
            result = replace(1, 'wrong_result')
            rows.append({'case': 'wrong_result_rejected', 'output': result.decode(errors='replace')})
            assert b'replacement body failed WebAssembly validation' in result, result
            result = replace(1, 'good4')
            rows.append({'case': 'generation_2', 'output': result.decode(errors='replace')})
            assert b'function replaced; generation 2' in result, result
            assert b'no breakpoints' in reply('info breakpoints')
            result = reply(f'break 0 {target} 3')
            rows.append({'case': 'new_generation_breakpoint', 'output': result.decode(errors='replace')})
            assert b'breakpoint 2' in result, result
            if args.replacements == 1:
                assert b'breakpoint deleted' in reply('delete 2')
        if args.replacements == 2:
            result = replace(1, 'good5')
            rows.append({'case': 'stale_generation_rejected', 'output': result.decode(errors='replace')})
            assert b'function generation changed' in result, result
            result = replace(2, 'good5')
            rows.append({'case': 'generation_3', 'output': result.decode(errors='replace')})
            assert b'function replaced; generation 3' in result, result
            assert b'no breakpoints' in reply('info breakpoints')
            assert b'no emitted executable debug safe point' in reply(f'break 0 {target} 3')
        result = reply('continue')
        rows.append({'case': 'continue', 'output': result.decode(errors='replace')})
        deadline = time.monotonic() + 20
        while True:
            result = reply('status')
            if b'guest exited: 0' in result:
                rows.append({'case': 'new_function_exit', 'output': result.decode(errors='replace')})
                break
            assert time.monotonic() < deadline, result
            time.sleep(.01)
        process.stdin.write(b'quit\n')
        process.stdin.flush()
        assert process.wait(timeout=15) == 0, process.returncode
        transcript.extend(process.stdout.read())
        expected_value = 6 if args.call_existing and args.replacements == 1 else 3 + args.replacements
        assert (b'replaced-value=' + str(expected_value).encode() + b'\n') in transcript, bytes(transcript)[-4000:]
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        (args.out / 'console.log').write_bytes(transcript)
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
    control_fd_probe(args, rows)
    (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
    (args.out / 'summary.json').write_text(json.dumps({
        'passed': True, 'cases': len(rows), 'source': str(args.source_root),
        'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
    }, indent=2) + '\n')
    print('PASS LLVM debug-full hot replacement', len(rows), 'checks')

if __name__ == '__main__':
    main()
