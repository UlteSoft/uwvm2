#!/usr/bin/env python3
"""Fresh r3 Linux actual whole-owner offsets/window vs two executed native steps.

Keeper only, existing 64GiB cgroup. No local/native run is implied by this source.
The immutable fresh runtime/main/source dependency closure is qualified by the
keeper before invocation; emitted binary SHA here is identification, not proof
of an original build argv or an independently qualified runtime object.
"""
import argparse
import importlib.util
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
adapter_path = source / 'tools/debug/dap_adapter.py'
spec = importlib.util.spec_from_file_location('uwvm_actual_native_window_parser', adapter_path)
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)

(out / 'fixture.wat').write_text('(module (func $window (export "_start") (loop $again br $again)))\n')
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
disassembly_header = re.compile(rb'native-disassembly stop=(\d+) thread=(\d+) module=(\d+) function=(\d+) function-generation=(\d+) runtime-epoch=(\d+)\n')
disassembly_row = re.compile(rb'  instruction 0 pc=0x([0-9a-f]+) bytes=([0-9a-f]{2}(?: [0-9a-f]{2}){0,14})  ([^\r\n]+)\n')
stop_identifier = re.compile(rb'^stop-id ([0-9]{1,20})$', re.MULTILINE)
thread_identifier = re.compile(rb'^thread ([0-9]+) module=0 function=0 byte-offset=0 generation=([0-9]+)$', re.MULTILINE)
def copied(console, thread, stop, epoch):
    reply = console.send(f'disassemble {thread} {stop} 1')
    headers = list(disassembly_header.finditer(reply)); rows = list(disassembly_row.finditer(reply))
    assert len(headers) == len(rows) == 1 and b'native-disassembly-end\n' in reply, reply
    identity = tuple(map(int, headers[0].groups()))
    assert identity == (stop, thread, 0, 0, 1, epoch), (identity, reply)
    row = rows[0]
    assert int(row[1], 16) != 0 and re.search(rb'[A-Za-z]', row[3]), reply
    return row
def range_page(console, thread, stop, epoch, count=1, byte_offset=0, instruction_offset=0, symbols=True):
    status = console.send('status').removesuffix(b'(uwvm-debug) ').decode('utf-8', 'strict')
    state, _, locations, _ = adapter.parse_status(status)
    assert state == 'stopped' and len(locations) == 1 and locations[0]['id'] == thread and locations[0]['stop_id'] == stop, status
    reply = console.send(f'disassemble-range {thread} {stop} {count} {byte_offset} {instruction_offset} {int(symbols)}')
    identity, rows = adapter.parse_native_disassembly_range(
        reply.removesuffix(b'(uwvm-debug) ').decode('utf-8','strict'), locations[0],
        count, byte_offset, instruction_offset, symbols)
    assert identity[:4] == (stop,thread,0,0) and identity[5] == epoch and len(rows) == count, (identity,rows)
    fresh = console.send('status').removesuffix(b'(uwvm-debug) ').decode('utf-8','strict')
    assert adapter.parse_status(fresh)[2] == locations, fresh
    return identity, rows

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
    thread_match = thread_identifier.search(stopped); stop_match = stop_identifier.search(stopped)
    assert thread_match and stop_match, stopped
    thread, epoch = map(int, thread_match.groups()); first_stop = int(stop_match[1])
    before_first = copied(console, thread, first_stop, epoch)
    cooperative_image, cooperative_rows = range_page(console, thread, first_stop, epoch)
    assert int(cooperative_rows[0]['address'],16) == int(before_first[1],16)
    assert cooperative_rows[0]['instructionBytes'] == before_first[2].decode()
    assert cooperative_rows[0].get('symbol') == 'window', cooperative_rows
    # Same request shape as VS Code initial pane (-200,400,resolveSymbols):
    # CLI pages remain <=32; bytes NEVER cross this actual physical owner.
    window_rows = []; window_identity = None
    for start in range(0,400,32):
        identity, page = range_page(console,thread,first_stop,epoch,min(32,400-start),0,-200+start)
        if window_identity is None: window_identity = identity
        assert identity == window_identity, (window_identity,identity)
        window_rows.extend(page)
    assert len(window_rows) == 400 and any(row['presentationHint']=='normal' for row in window_rows), window_rows
    assert window_rows[200]['address'] == cooperative_rows[0]['address']
    assert all(row['address']=='-1' and 'instructionBytes' not in row for row in window_rows if row['presentationHint']=='invalid')
    # Bytes before the actual function entry remain invalid, regardless of
    # nearby function/adapter/section addresses which the VM happens to own.
    outside_delta = cooperative_image[7] - cooperative_image[6] - 1
    _, outside = range_page(console,thread,first_stop,epoch,3,outside_delta,0)
    assert all(row['presentationHint']=='invalid' and row['address']=='-1' for row in outside), outside

    first = console.send(f'step asm {thread}')
    first_native_stop = stop_identifier.search(first)
    assert first_native_stop and int(first_native_stop[1]) != first_stop, first
    before_second = copied(console, thread, int(first_native_stop[1]), epoch)
    native_image, native_rows = range_page(console,thread,int(first_native_stop[1]),epoch)
    assert native_image[7:] == cooperative_image[7:] and native_image[6] != cooperative_image[6]
    assert native_rows[0]['address'] == f"0x{int(before_second[1],16):x}"
    assert native_rows[0]['instructionBytes'] == before_second[2].decode()
    # A real backwards instruction request must reconstruct exactly the
    # previous executed boundary, not scan backwards byte-by-byte.
    _, back = range_page(console,thread,int(first_native_stop[1]),epoch,1,0,-1)
    positions = [index for index,row in enumerate(window_rows) if row['presentationHint']=='normal' and row['address']==native_rows[0]['address']]
    assert len(positions)==1, (positions,native_rows,window_rows)
    # The executed instruction may branch; the previous LINEAR decoded
    # instruction need not be the previous executed instruction.
    prior_row = window_rows[positions[0]-1] if positions[0] else {'address':'-1','presentationHint':'invalid'}
    assert back[0]['address']==prior_row['address'] and back[0]['presentationHint']==prior_row['presentationHint'], (back,prior_row)
    if prior_row['presentationHint']=='normal':
        assert back[0]['instructionBytes']==prior_row['instructionBytes'], (back,prior_row)
    stale_range = console.send(f'disassemble-range {thread} {first_stop} 1 0 0 1')
    assert b'error: command or thread is not valid' in stale_range, stale_range

    # Old numeric label is only a stale-request check, never code authority.
    stale = console.send(f'disassemble {thread} {first_stop} 1')
    assert b'error: command or thread is not valid' in stale, stale
    unchanged = console.send('status'); assert stop_identifier.search(unchanged)[1] == first_native_stop[1], unchanged
    second = console.send(f'step asm {thread}')
    first_match = native_line.search(first)
    second_match = native_line.search(second)
    assert first_match and second_match, (first, second)
    assert b'stopped: native instruction step' in first and b'stopped: native instruction step' in second
    assert b'native-pc=0x' in first and b'native-pc=0x' in second
    first_pc, first_bytes, first_asm, first_next = first_match.groups()
    second_pc, second_bytes, second_asm, second_next = second_match.groups()
    assert int(first_next, 16) == int(second_pc, 16), (first, second)
    assert int(before_first[1], 16) == int(first_pc, 16) and before_first[2] == first_bytes, (before_first.groups(), first)
    assert int(before_second[1], 16) == int(second_pc, 16) and before_second[2] == second_bytes, (before_second.groups(), second)
    assert b'no current cooperative local snapshot' in console.send(f'locals source {thread}'), 'native code copy must not restore source values'
    assert b'invalid command' in console.send(f'disassemble {thread} 0x1234 1')

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
    'scope': 'Linux x86-64 fresh r3 private whole owner, actual -200/400 paged window/symbol, before-owner filler, true backward boundary, vs two native steps; stale/source/address refusal/quit',
    'adapter_source_sha256': hashlib.file_digest(adapter_path.open('rb'),'sha256').hexdigest(),
    'runner_source_sha256': hashlib.file_digest(Path(__file__).open('rb'),'sha256').hexdigest(),
    'cgroup': Path('/proc/self/cgroup').read_text()}, indent=2) + '\n')
print('PASS actual r3 owned native window/offset/symbol vs two LLVM-full steps, instruction/unwind', flush=True)
