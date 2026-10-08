#!/usr/bin/env python3
"""Native/QEMU Linux -Rdbg: actual Wasm numeric code, registers and isolated steps.

Keeper only, existing 64GiB cgroup. No local/native run is implied by this source.
The immutable fresh runtime/main/source dependency closure is qualified by the
keeper before invocation; emitted binary SHA here is identification, not proof
of an original build argv or an independently qualified runtime object.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import subprocess
import sys
import time


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--uwvm', type=Path, required=True)
parser.add_argument('--wasm-tools', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--ros', action='store_true')
execution = parser.add_mutually_exclusive_group(required=True)
execution.add_argument('--qemu', type=Path)
execution.add_argument('--native-linux', action='store_true')
parser.add_argument('--sysroot', type=Path)
parser.add_argument('--library-path')
parser.add_argument('--architecture', required=True)
parser.add_argument('--policy', action='append', choices=('instruction', 'unwind'),
                    help='Explicit qualified call-stack policies; default tests both')
parser.add_argument('--qemu-cpu')
parser.add_argument('--extra-locals', type=int, default=0,
                    help='Exercise private snapshot instrumentation with up to 10000 actual locals')
parser.add_argument('--native-walk-timeout', type=int, default=60,
                    help='Seconds for the complete strict native walk; increase for QEMU or large instrumentation')
parser.add_argument('--prompt-timeout', type=int, default=60)
args = parser.parse_args()
if not 1 <= args.native_walk_timeout <= 1800 or not 1 <= args.prompt_timeout <= 600:
    parser.error('native walk timeout must be 1..1800 and prompt timeout 1..600')
if not 0 <= args.extra_locals < 10000:
    parser.error('--extra-locals must be between 0 and 9999')
source = args.source_root.resolve(strict=True)
binary = args.uwvm.resolve(strict=True)
wasm_tools = args.wasm_tools.resolve(strict=True)
if args.native_linux:
    assert sys.platform == 'linux' and os.uname().machine == args.architecture
    assert args.sysroot is None and args.library_path is None and args.qemu_cpu is None
    qemu = sysroot = None
    launcher = []
else:
    assert args.sysroot is not None and args.library_path
    qemu = args.qemu.resolve(strict=True)
    sysroot = args.sysroot.resolve(strict=True)
    launcher = [str(qemu), '-U', 'LD_LIBRARY_PATH', '-E',
                'LD_LIBRARY_PATH=' + args.library_path, '-L', str(sysroot)]
    if args.qemu_cpu:
        launcher.extend(['-cpu', args.qemu_cpu])
out = args.out.resolve()
subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
out.mkdir(parents=True, exist_ok=False)
(out / 'fixture.wat').write_text('(module\n  (func (export "_start") (local $value i32)' + ' (local i32)' * args.extra_locals + '\n    i32.const 17 local.set $value\n    (loop $again\n      local.get $value i32.const 41 i32.xor\n      local.set $value\n      local.get $value i32.const 1 i32.rotl\n      local.set $value\n      br $again)))\n')
subprocess.run([wasm_tools, 'parse', out / 'fixture.wat', '-o', out / 'fixture.wasm'], check=True)
subprocess.run([wasm_tools, 'validate', out / 'fixture.wasm'], check=True)


class Console:
    def __init__(self, policy, name):
        self.name = name
        mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        self.command = [*launcher, str(binary), '-Rdbg', *mode, '-Rct', '0',
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
        deadline = time.monotonic() + args.prompt_timeout
        while marker not in self.pending:
            assert time.monotonic() < deadline, (self.name, 'prompt timeout', self.log[-4000:])
            ready = self.select.select(max(0, deadline - time.monotonic()))
            assert ready, (self.name, 'prompt timeout', self.log[-4000:])
            chunk = os.read(self.child.stdout.fileno(), 65536)
            assert chunk, (self.name, 'early process exit', self.child.poll(), self.log[-4000:])
            self.pending.extend(chunk)
            self.log.extend(chunk)
            (out / f'{self.name}.log').write_bytes(self.log)
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
        code = self.child.wait(timeout=30)
        self.log.extend(self.child.stdout.read())
        (out / f'{self.name}.log').write_bytes(self.log)
        assert code == 0, (self.name, code, self.log[-4000:])


native_line = re.compile(rb'native instruction 0x([0-9a-f]+) bytes=([0-9a-f ]+)  ([^\r\n]+?) -> 0x([0-9a-f]+)')
disassembly_header = re.compile(rb'native-disassembly stop=(\d+) thread=(\d+) module=(\d+) function=(\d+) function-generation=(\d+) runtime-epoch=(\d+) origin=(?:native-instruction-stop|safepoint-code-view)\n')
disassembly_row = re.compile(rb'  instruction 0 pc=0x([0-9a-f]+) bytes=([0-9a-f]{2}(?: [0-9a-f]{2}){0,14})  ([^\r\n]+)\n')
hidden_row = re.compile(rb'  instruction 0 pc=0x([0-9a-f]+) unavailable\n')
stop_identifier = re.compile(rb'^stop-id ([0-9]{1,20})$', re.MULTILINE)
thread_identifier = re.compile(rb'^thread ([0-9]+) module=0 function=0 byte-offset=\d+ generation=([0-9]+)(?: native-pc=0x[0-9a-f]+)?$', re.MULTILINE)
numeric_register = re.compile(rb'^  ((?:r|x|w|l|i|o|g|a|t|s|v|f|d)\d+|at|eax|ebx|ecx|edx|esi|edi|rax|rbx|rcx|rdx|rsi|rdi)=0x([?0-9a-f]+)$', re.MULTILINE)


def copied(console, thread, stop, epoch):
    reply = console.send(f'disassemble {thread} {stop} 1')
    headers = list(disassembly_header.finditer(reply))
    if not headers:
        # A cooperative capture can lack an exact instruction position. It
        # grants no permission to print a whole native function or VM bytes.
        assert b'error:' in reply and b'bytes=' not in reply, reply
        return None, False
    assert len(headers) == 1 and b'native-disassembly-end\n' in reply, reply
    identity = tuple(map(int, headers[0].groups()))
    assert identity == (stop, thread, 0, 0, 1, epoch), (identity, reply)
    row = disassembly_row.search(reply)
    if row is not None:
        assert int(row[1], 16) != 0 and re.search(rb'[A-Za-z]', row[3]), reply
        return row, False
    assert hidden_row.search(reply) and b'bytes=' not in reply, reply
    return None, True



# Physical ABI infrastructure must stay hidden at every genuine native stop.
# These names come from the fixed public target bank; the saved private context
# may contain them solely to resume the selected Wasm activation.
hidden_abi_registers = {
    'x86_64': ('rsp', 'rbp', 'rflags'),
    'aarch64': ('x18', 'fp', 'lr', 'sp', 'cpsr'),
    'i686': ('esp', 'ebp', 'eflags'),
    'ppc32': ('r1', 'r2', 'r13', 'r31', 'lr', 'ctr', 'msr', 'cr', 'xer'),
    'ppc64': ('r1', 'r2', 'r13', 'r31', 'lr', 'ctr', 'msr', 'cr', 'xer'),
    'ppc64le': ('r1', 'r2', 'r13', 'r31', 'lr', 'ctr', 'msr', 'cr', 'xer'),
    'mips64': ('zero', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra', 'status', 'hi', 'lo'),
    'mips64el': ('zero', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra', 'status', 'hi', 'lo'),
    'riscv64': ('zero', 'ra', 'sp', 'gp', 'tp', 's0', 'status'),
    'loongarch64': ('zero', 'ra', 'tp', 'sp', 'r21', 'fp', 'status'),
    'sparc64': ('g0', 'g6', 'g7', 'o6', 'o7', 'i6', 'i7', 'npc', 'tstate', 'y'),
    's390x': ('r11', 'r14', 'r15', 'psw'),
    'armhf': ('r11', 'sp', 'lr', 'cpsr'),
}
register_bank = {'ppc64-be': 'ppc64', 'ppc64-le': 'ppc64le',
                 'mips64-be': 'mips64', 'mips64-le': 'mips64el'}.get(args.architecture, args.architecture)
assert register_bank in hidden_abi_registers, 'unqualified target register bank'

rows = []
for policy in args.policy or ('instruction', 'unwind'):
    console = Console(policy, f'{policy}-numeric-native-steps')
    try:
        initial = console.send('step asm 1')
        assert b'error: command or thread is not valid' in initial, initial
        assert b'registered' in console.send('break 0 0 0')
        console.send('continue')
        deadline = time.monotonic() + args.native_walk_timeout
        while True:
            stopped = console.send('wait')
            if b'stopped: breakpoint' in stopped:
                break
            assert time.monotonic() < deadline, stopped
        match = thread_identifier.search(stopped)
        assert match and stop_identifier.search(stopped), stopped
        thread, epoch = map(int, match.groups())
        current = stopped
        observations = []
        hidden = retained = transitions = 0
        positive_registers = []
        for attempt in range(512):
            assert time.monotonic() < deadline, (policy, 'native walk deadline', observations)
            previous_stop = int(stop_identifier.search(current)[1])
            before, was_hidden = copied(console, thread, previous_stop, epoch)
            hidden += was_hidden
            # Exercise both preserved SI spelling and genuine NI operation.
            operation = f'step asm {thread}' if attempt % 2 == 0 else f'ni {thread}'
            following = console.send(operation)
            if b'stopped: native instruction step' not in following:
                assert b'error:' in following and native_line.search(following) is None, following
                # An error response need not contain a stop summary. Read the
                # actual retained controller state and prove its identity.
                retained_status = console.send('status')
                retained_stop = stop_identifier.search(retained_status)
                assert retained_stop and int(retained_stop[1]) == previous_stop, (following, retained_status)
                retained += 1
                current = console.send(f'step wasm {thread}')
                assert stop_identifier.search(current) and int(stop_identifier.search(current)[1]) > previous_stop, current
                continue
            transitions += 1
            new_stop = int(stop_identifier.search(following)[1])
            assert new_stop > previous_stop and b'native-pc=0x' in following, following
            executed = native_line.search(following)
            if before is not None:
                assert executed is not None, (before.groups(), following)
                pc, octets, text, successor = executed.groups()
                assert pc == before[1] and octets == before[2], (before.groups(), following)
                assert re.search(rb'[A-Za-z]', text) and 1 <= len(octets.split()) <= 15, following
                observations.append({'operation': operation, 'old_stop': previous_stop, 'new_stop': new_stop,
                                     'pc': pc.decode(), 'bytes': octets.decode(), 'asm': text.decode(),
                                     'successor': successor.decode()})
            else:
                # The public executed-instruction sink must suppress the same
                # unproved scaffolding as disassembly before execution.
                assert executed is None, following
            stale = console.send(f'disassemble {thread} {previous_stop} 1')
            assert b'error: command or thread is not valid' in stale and b'bytes=' not in stale, stale
            status = console.send('status')
            assert int(stop_identifier.search(status)[1]) == new_stop, status
            assert b'backtrace unavailable for this stop' in console.send(f'bt {thread}')
            assert b'locals unavailable for this stop' in console.send(f'locals {thread}')
            assert b'no current cooperative local snapshot' in console.send(f'locals source {thread}')
            assert b'invalid command' in console.send(f'disassemble {thread} 0x1234 1')
            registers = console.send('info all-registers')
            assert b'native-registers stop=' in registers and b'native-registers end' in registers, registers
            assert re.search(rb'^  (?:rsp|esp|sp|r1|o6|r15)=unavailable$', registers, re.MULTILINE), registers
            for register_name in hidden_abi_registers[register_bank]:
                pattern = rb'^  ' + re.escape(register_name.encode()) + rb'=unavailable$'
                assert re.search(pattern, registers, re.MULTILINE), (
                    policy, args.architecture, 'exposed or missing ABI infrastructure',
                    register_name, registers)
            # A PC alone, fabricated zero registers, and an entirely unavailable
            # register bank cannot satisfy positive physical Wasm register support.
            for register in numeric_register.finditer(registers):
                if register[1] not in (b'r1', b'o6', b'r15', b'sp'):
                    positive_registers.append({'name': register[1].decode(), 'bits': register[2].decode(), 'stop': new_stop})
            current = following
            if len(observations) >= 2 and positive_registers and hidden and retained:
                break
        assert len(observations) >= 2 and positive_registers and hidden and retained and transitions, (
            policy, observations, positive_registers, hidden, retained, transitions)
        wasm = console.send(f'step wasm {thread}')
        assert stop_identifier.search(wasm) and int(stop_identifier.search(wasm)[1]) > int(stop_identifier.search(current)[1]), wasm
        rows.append({'policy': policy, 'executed_public_instructions': observations,
                     'positive_physical_numeric_registers': positive_registers,
                     'hidden_scaffolding_observations': hidden, 'retained_boundary_refusals': retained,
                     'actual_native_transitions': transitions, 'hidden_abi_registers': list(hidden_abi_registers[register_bank]),
                     'commands': console.commands, 'passed': True})
        (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
    finally:
        console.close()  # genuine trap gate / worker must retire during quit

(out / 'summary.json').write_text(json.dumps({
    'passed': True, 'policies': [r['policy'] for r in rows], 'executions': len(rows), 'binary_sha256': hashlib.file_digest(binary.open('rb'), 'sha256').hexdigest(),
    'fixture_sha256': hashlib.file_digest((out / 'fixture.wasm').open('rb'), 'sha256').hexdigest(),
    'scope': 'Linux fresh -Rdbg product: actual copied Wasm numeric bytes vs executed SI/NI, positive physical numeric registers, hidden scaffold and retained transfer refusal, stale stop, source/native stack unavailable, no address input, managed drain',
    'actual_local_count': 1 + args.extra_locals,
    'native_walk_timeout': args.native_walk_timeout, 'prompt_timeout': args.prompt_timeout,
    'architecture': args.architecture,
    'execution': 'native-linux' if args.native_linux else 'qemu-linux',
    'qemu_sha256': hashlib.file_digest(qemu.open('rb'), 'sha256').hexdigest() if qemu else None,
    'sysroot': str(sysroot) if sysroot else None,
    'library_path': args.library_path,
    'cgroup': Path('/proc/self/cgroup').read_text()}, indent=2) + '\n')
print('PASS actual Wasm numeric native bytes/SI/NI/registers, hidden VM stack and scaffold, policies=' + ','.join(r['policy'] for r in rows), flush=True)
