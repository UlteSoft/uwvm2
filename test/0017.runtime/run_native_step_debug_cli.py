#!/usr/bin/env python3
"""Real Linux LLVM-full `step asm`: exact native bytes, two steps, cancellation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import subprocess
import time


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--uwvm', type=Path, required=True)
parser.add_argument('--wasm-tools', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--ros', action='store_true')
args = parser.parse_args()
source = args.source_root.resolve(strict=True)
binary = args.uwvm.resolve(strict=True)
wasm_tools = args.wasm_tools.resolve(strict=True)
out = args.out.resolve()
subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
out.mkdir(parents=True, exist_ok=False)
(out / 'fixture.wat').write_text('(module (func (export "_start") (loop $again br $again)))\n')
subprocess.run([wasm_tools, 'parse', out / 'fixture.wat', '-o', out / 'fixture.wasm'], check=True)
subprocess.run([wasm_tools, 'validate', out / 'fixture.wasm'], check=True)


class Console:
    def __init__(self, policy, name):
        self.name = name
        mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        self.command = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0',
                        '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable',
                        '--run', str(out / 'fixture.wasm')]
        self.child = subprocess.Popen(self.command, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.select = selectors.DefaultSelector()
        self.select.register(self.child.stdout, selectors.EVENT_READ)
        self.pending = bytearray()
        self.log = bytearray()
        self.commands = []
        self.prompt()

    def prompt(self):
        marker = b'(uwvm-debug) '
        deadline = time.monotonic() + 25
        while marker not in self.pending:
            assert time.monotonic() < deadline, (self.name, 'prompt timeout', self.log[-4000:])
            ready = self.select.select(max(0, deadline - time.monotonic()))
            assert ready, (self.name, 'prompt timeout', self.log[-4000:])
            chunk = os.read(self.child.stdout.fileno(), 65536)
            assert chunk, (self.name, 'early process exit', self.child.poll(), self.log[-4000:])
            self.pending.extend(chunk)
            self.log.extend(chunk)
        end = self.pending.index(marker) + len(marker)
        reply = bytes(self.pending[:end])
        del self.pending[:end]
        return reply

    def send(self, command):
        self.commands.append(command)
        self.child.stdin.write(command.encode() + b'\n')
        self.child.stdin.flush()
        return self.prompt()

    def close(self):
        self.commands.append('quit')
        self.child.stdin.write(b'quit\n')
        self.child.stdin.flush()
        self.child.stdin.close()
        code = self.child.wait(timeout=10)
        self.log.extend(self.child.stdout.read())
        (out / f'{self.name}.log').write_bytes(self.log)
        assert code == 0, (self.name, code, self.log[-4000:])


native_line = re.compile(rb'native instruction 0x([0-9a-f]+) bytes=([0-9a-f ]+)  ([^\r\n]+?) -> 0x([0-9a-f]+)')
rows = []
for policy in ('instruction', 'unwind'):
    console = Console(policy, f'{policy}-two-steps')
    initial = console.send('step asm 1')
    assert b'error: command or thread is not valid' in initial, initial
    assert b'registered' in console.send('break 0 0 0')
    console.send('continue')
    deadline = time.monotonic() + 10
    while True:
        stopped = console.send('wait')
        if b'stopped: breakpoint' in stopped:
            break
        assert time.monotonic() < deadline, stopped
    assert b'thread 1 module=0 function=0 byte-offset=0' in stopped, stopped
    first = console.send('step asm 1')
    second = console.send('step asm 1')
    first_match = native_line.search(first)
    second_match = native_line.search(second)
    assert first_match and second_match, (first, second)
    assert b'stopped: native instruction step' in first and b'stopped: native instruction step' in second
    assert b'native-pc=0x' in first and b'native-pc=0x' in second
    first_pc, first_bytes, first_asm, first_next = first_match.groups()
    second_pc, second_bytes, second_asm, second_next = second_match.groups()
    assert int(first_next, 16) == int(second_pc, 16), (first, second)
    for captured, mnemonic in ((first_bytes, first_asm), (second_bytes, second_asm)):
        octets = captured.split()
        assert 1 <= len(octets) <= 15 and all(len(x) == 2 for x in octets), captured
        assert re.search(rb'[A-Za-z]', mnemonic), mnemonic
    assert b'native-pc=0x' in console.send('status')
    assert b'backtrace unavailable for this stop' in console.send('bt 1')
    assert b'locals unavailable for this stop' in console.send('locals 1')
    wasm = console.send('step wasm 1')
    assert b'stopped: selected participant step' in wasm, wasm
    console.close()  # second SIGTRAP gate must release; no guest hang
    rows.append({'policy': policy, 'first_pc': first_pc.decode(),
                 'first_bytes': first_bytes.decode(), 'first_asm': first_asm.decode(),
                 'second_pc': second_pc.decode(), 'second_bytes': second_bytes.decode(),
                 'second_asm': second_asm.decode(), 'second_next': second_next.decode(),
                 'commands': console.commands, 'passed': True})
    (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')

(out / 'summary.json').write_text(json.dumps({
    'passed': True, 'executions': len(rows), 'binary_sha256': hashlib.file_digest(binary.open('rb'), 'sha256').hexdigest(),
    'fixture_sha256': hashlib.file_digest((out / 'fixture.wasm').open('rb'), 'sha256').hexdigest(),
    'scope': 'Linux x86-64 LLVM-full native single-step, two exact instructions, LLVM MC bytes and assembly, stale-trace rejection, quit from second trap',
    'cgroup': Path('/proc/self/cgroup').read_text()}, indent=2) + '\n')
print('PASS LLVM-full native assembly CLI instruction/unwind, two steps each', flush=True)
