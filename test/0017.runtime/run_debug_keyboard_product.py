#!/usr/bin/env python3
"""Actual current full-product PTY Ctrl+C/EOF tests; Linux keeper cgroup only.
Binary build-proof pins are mandatory. No fake stop/frame or native permission.
This does not claim DAP/native NI cancellation/source stepping/full VM finalizers.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import subprocess
import termios
import time


def require(value, why):
    if not value:
        raise AssertionError(why)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def cgroup_check(expected):
    require(next(s for s in Path('/proc/self/cgroup').read_text().splitlines() if s.startswith('0::')) == '0::' + expected,
            'original keeper cgroup is mandatory')
    cg = Path('/sys/fs/cgroup') / expected.lstrip('/')
    require((cg / 'memory.max').read_text().strip() == str(64 * 1024**3), '64GiB memory.max')
    require((cg / 'memory.swap.max').read_text().strip() == '0', 'original swap0')


class Console:
    def __init__(self, command, timeout):
        reader, writer = os.pipe()
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.close(writer)
            if os.read(reader, 1) != b'R':
                os._exit(99)
            os.close(reader)
            os.execv(command[0], command)
        os.close(reader)
        self.baseline = termios.tcgetattr(self.fd)
        os.write(writer, b'R')
        os.close(writer)
        self.deadline = time.monotonic() + timeout
        self.log = bytearray()
        self.reaped = False

    def write(self, data):
        require(time.monotonic() < self.deadline, 'case deadline before write')
        os.write(self.fd, data)

    def wait(self, predicate, start=0):
        while time.monotonic() < self.deadline:
            suffix = bytes(self.log[start:])
            if predicate(suffix):
                return suffix
            ready, _, _ = select.select([self.fd], [], [], min(.05, max(0, self.deadline - time.monotonic())))
            if not ready:
                continue
            try:
                data = os.read(self.fd, 65536)
            except OSError as exc:
                if exc.errno == errno.EIO:
                    break
                raise
            if not data:
                break
            self.log.extend(data)
            require(len(self.log) <= 1024 * 1024, 'bounded 1MiB transcript')
        raise AssertionError('missing actual reply: ' + repr(bytes(self.log[start:])[-4000:]))

    def send(self, text, marker):
        start = len(self.log)
        self.write(text.encode() + b'\n')
        return self.wait(lambda data: marker in data, start)

    def stop(self, start):
        return self.wait(lambda data: b'^C' in data and b'stop-id ' in data and b'(uwvm-debug) ' in data, start)

    def exit(self, expected):
        while time.monotonic() < self.deadline:
            got, status = os.waitpid(self.pid, os.WNOHANG)
            if got:
                self.reaped = True
                code = os.waitstatus_to_exitcode(status)
                require(code == expected, f'actual exit {code}, expected {expected}')
                return code
            # Drain output without making prompt labels evidence of a stop.
            if select.select([self.fd], [], [], .02)[0]:
                try:
                    data = os.read(self.fd, 65536)
                    self.log.extend(data)
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
            require(len(self.log) <= 1024 * 1024, 'bounded transcript on exit')
        raise AssertionError('finite process exit deadline')

    def cleanup(self):
        if not self.reaped:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except ProcessLookupError:
                pass
        os.close(self.fd)


def stop_id(data):
    matches = re.findall(rb'stop-id ([0-9]+)', data)
    require(matches, 'actual nonzero stopped reply')
    value = int(matches[-1])
    require(value > 0, 'actual nonzero stop identifier')
    return value


def run_case(command, timeout, proc_raise=False):
    con = Console(command, timeout)
    row = {'command': command, 'passed': False, 'proc_raise': proc_raise}
    try:
        con.wait(lambda data: b'UWVM LLVM full debugger.' in data and b'(uwvm-debug) ' in data)
        initial = con.send('status', b'prepared; no Wasm instruction executed')
        initial_id = stop_id(initial)
        if proc_raise:
            con.write(b'continue\n')
            row['exit'] = con.exit(-signal.SIGINT)
            require(b'^C' not in con.log and b'stopped: pause' not in con.log,
                    'guest proc_raise must preserve default termination, not mint debug pause')
        else:
            # Input cancellation is measured at a real initial stop. A partial
            # mutating command is never allowed to execute a truncated suffix.
            con.write(b'partial-command-must-not-execute')
            con.wait(lambda data: b'partial-command-must-not-execute' in data)
            start = len(con.log)
            con.write(b'\x03')
            cancelled = con.stop(start)
            require(stop_id(cancelled) == initial_id, 'idle input cancellation must preserve the stop')
            require(b'error: invalid command' not in cancelled, 'partial command executed on cancel')
            con.send('continue', b'running')
            start = len(con.log)
            con.write(b'\x03')
            stopped = con.stop(start)
            require(b'stopped: pause' in stopped and b'thread ' in stopped, 'running guest must produce genuine captured pause')
            require(stop_id(stopped) != initial_id, 'running pause needs fresh stop lifetime')
            current = stop_id(stopped)
            # A real command is in controller.execute(wait), not sitting in
            # read_console_line. Ctrl+C must request actual all-thread park
            # without waiting for the blind two-second observe deadline.
            con.send('continue&', b'running')  # explicit background, then the real synchronous wait
            start = len(con.log)
            con.write(b'wait\n')
            con.wait(lambda data: b'wait' in data, start)
            time.sleep(.05)
            sent = time.monotonic()
            os.kill(con.pid, signal.SIGINT)  # actual external same-UID source
            sync = con.stop(start)
            elapsed = time.monotonic() - sent
            require(elapsed < 1.5, f'blind synchronous deadline delayed Ctrl+C: {elapsed}')
            require(b'timed out' not in sync and b'stopped: pause' in sync and b'thread ' in sync,
                    'sync wait cancellation needs real captured Wasm pause')
            require(stop_id(sync) != current, 'new run/interrupt has fresh stop lifetime')
            current = stop_id(sync)
            row['synchronous_wait_interrupt_seconds'] = elapsed
            status = con.send('status', b'stop-id ')
            require(stop_id(status) == current, 'same stopped session remains usable')
            # Actual editor history and deliberate eligible blank repetition.
            history = con.send('\x1b[A', b'stop-id ')
            require(stop_id(history) == current, 'Up recalls current status command')
            repeat = con.send('', b'stop-id ')
            require(stop_id(repeat) == current, 'blank Enter repeats eligible status')
            con.write(b'\x04')
            row['exit'] = con.exit(0)
            require(termios.tcgetattr(con.fd) == con.baseline, 'Ctrl+D must restore actual terminal modes')
        row.update(passed=True, initial_stop=initial_id, full_vm_retirement_ack_tested=False,
                   dap_tested=False, source_stepping_tested=False, native_boundaries_tested=False)
    except BaseException as exc:
        row['error'] = repr(exc)
    finally:
        row['transcript'] = bytes(con.log).decode('utf-8', 'backslashreplace')
        con.cleanup()
    return row


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'build-proof', 'wasm-tools', 'source-root', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--binary-sha256', required=True)
    p.add_argument('--source-id', required=True)
    p.add_argument('--cgroup', required=True)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--timeout', type=float, default=12)
    a = p.parse_args()
    cgroup_check(a.cgroup)
    require(0 < a.timeout <= 20, 'bounded product-case timeout')
    binary = a.binary.resolve(strict=True)
    require(digest(binary) == a.binary_sha256, 'actual immutable rebuilt binary SHA')
    proof = json.loads(a.build_proof.read_text())
    require(proof.get('passed') is True and proof.get('source_id') == a.source_id and
            proof.get('binary_sha256') == a.binary_sha256, 'actual successful build proof pins')
    own = Path(__file__).resolve().parent
    output = a.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixtures = {}
    for name in ('live-gc-eh', 'guest-proc-raise'):
        source = own / (name + '.wat')
        wasm = output / (name + '.wasm')
        subprocess.run([str(a.wasm_tools), 'parse', str(source), '-o', str(wasm)], check=True, timeout=10)
        subprocess.run([str(a.wasm_tools), 'validate', str(wasm)], check=True, timeout=10)
        fixtures[name] = wasm
    rows = []
    for policy in ('instruction', 'unwind'):
        mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        prefix = [str(binary), '-Rdbg', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                  '-Rllvm-cache-path', 'disable', '-WFE-exceptions', '-WFE-gc', '-WFE-function-references']
        for name in ('live-gc-eh', 'guest-proc-raise'):
            row = run_case([*prefix, '--run', str(fixtures[name])], a.timeout, name == 'guest-proc-raise')
            row['policy'] = policy
            row['fixture'] = name
            rows.append(row)
            (output / 'actual-runs.json').write_text(json.dumps(rows, indent=2) + '\n')
            cgroup_check(a.cgroup)
    summary = {'passed': all(row['passed'] for row in rows), 'source_id': a.source_id,
               'binary_sha256': a.binary_sha256, 'rows': rows, 'full_vm_retirement_ack_tested': False,
               'claim': 'current real Linux product CLI input/running Ctrl+C, history/eligible-repeat, EOF terminal cleanup, guest proc_raise default disposition only'}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return 0 if summary['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
