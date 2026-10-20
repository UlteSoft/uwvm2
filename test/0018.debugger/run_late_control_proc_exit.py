#!/usr/bin/env python3
"""Linux inherited-debug-endpoint exit and detach regression.

Run only under the admitted test cgroup. Keep the parent endpoint open while
waiting for natural process retirement; closing it must not be needed to make
WASI proc_exit finish. Compiler-produced guests trap if proc_exit returns.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import select
import signal
import socket
import subprocess
import sys
import time

sys.dont_write_bytecode = True


def sha(path):
    with Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


class Child:
    def __init__(self, argv, log, **kwargs):
        self.process = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT,
                                        start_new_session=True, **kwargs)
        self.pidfd = os.pidfd_open(self.process.pid, 0)

    def wait(self, seconds):
        code = self.process.wait(timeout=seconds)
        watch = select.poll()
        watch.register(self.pidfd, select.POLLIN)
        assert watch.poll(0), 'kernel process retirement must precede success'
        return code

    def close(self):
        try:
            if self.process.poll() is None:
                signal.pidfd_send_signal(self.pidfd, signal.SIGTERM)
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    signal.pidfd_send_signal(self.pidfd, signal.SIGKILL)
                    self.process.wait(timeout=5)
        finally:
            os.close(self.pidfd)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'build-receipt', 'wasm-tools', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--fixed-llvm-full', action='store_true')
    ap.add_argument('--policy', choices=('instruction', 'unwind', 'both'), default='both')
    args = ap.parse_args()
    assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    build = json.loads(args.build_receipt.read_text())
    assert build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after']
    assert sha(args.uwvm) == build['binary_sha256']
    args.out.mkdir(parents=True, exist_ok=False)
    pins = {str(p.resolve()): sha(p) for p in
            [Path(__file__), args.uwvm, args.build_receipt, args.wasm_tools]}
    result = dict(passed=False, scope='actual inherited endpoint and original Wasm lifecycle; no formatter timing',
                  product_source_id=build['source_id'], pins=pins, producers=[], rows=[])

    def save():
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')

    fixtures = [('return', None, 'continue', 0), ('zero', 0, 'continue', 0),
                ('nonzero', 7, 'continue', 7), ('maximum', 4294967295, 'continue', 255),
                ('quit', 21, 'quit', 21), ('disconnect', 23, 'disconnect', 23)]
    try:
        for name, code, action, expected in fixtures:
            wat, wasm = args.out / (name + '.wat'), args.out / (name + '.wasm')
            text = ('(module (memory (export "memory") 1) (func (export "_start") nop))'
                    if code is None else
                    '(module (import "wasi_snapshot_preview1" "proc_exit" (func $exit (param i32))) '
                    '(memory (export "memory") 1) (func (export "_start") '
                    f'i32.const {code} call $exit unreachable))')
            wat.write_text(text + '\n')
            argv = [str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)]
            compiler = None
            try:
                with (args.out / (name + '-producer.log')).open('wb') as log:
                    compiler = Child(argv, log, cwd=args.out)
                    assert compiler.wait(30) == 0
            finally:
                if compiler:
                    compiler.close()
            fixture = dict(name=name, actual_compile_argv=argv, wat_sha256=sha(wat),
                           wasm_sha256=sha(wasm), original_bytes_not_rewritten=True)
            result['producers'].append(fixture)
            save()
            for policy in (('instruction', 'unwind') if args.policy == 'both' else (args.policy,)):
                row = dict(name=name, policy=policy, action=action, expected_process_exit=expected,
                           raw_guest_exit=code, passed=False)
                result['rows'].append(row)
                parent, inherited = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
                inherited.setsockopt(socket.SOL_SOCKET, socket.SO_PASSCRED, 1)
                parent.settimeout(30)
                child = None
                begin = time.monotonic()
                try:
                    # Debug mode arms the real initial stop. The first
                    # authenticated command observes it without requesting a
                    # second pause after an already armed initial pause.
                    parent.sendall(b'wait')
                    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc', 'jit', '-Rcm', 'full']
                    argv = [str(args.uwvm), '--debug-jit-control-fd', str(inherited.fileno()), '-Rdbg',
                            *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                            '-Rllvm-cache-path', 'disable', '--run', str(wasm)]
                    row['actual_argv'] = argv
                    with (args.out / (name + '-' + policy + '-guest.log')).open('wb') as log:
                        child = Child(argv, log, cwd=args.out, stdin=subprocess.DEVNULL,
                                      pass_fds=(inherited.fileno(),))
                        inherited.close()
                        first_deadline = time.monotonic() + 30
                        observations = []
                        while True:
                            first = parent.recv(65536)
                            observations.append(first.decode('utf-8', errors='replace'))
                            assert first and b'error:' not in first, first
                            if b'stop-id ' in first and (b'thread ' in first or first.startswith(b'prepared; no Wasm instruction executed')):
                                break
                            assert time.monotonic() < first_deadline, observations
                            time.sleep(0.001)
                            parent.sendall(b'wait')
                        row['initial_state_observations'] = observations
                        row['first_pause'] = first.decode('utf-8', errors='replace')
                        proc = Path('/proc') / str(child.process.pid)
                        before = (proc / 'stat').read_text()
                        start = int(before[before.rfind(')') + 2:].split()[19])
                        elf = (proc / 'exe').stat()
                        expected_elf = args.uwvm.stat()
                        assert (elf.st_dev, elf.st_ino) == (expected_elf.st_dev, expected_elf.st_ino)
                        watch = select.poll()
                        watch.register(child.pidfd, select.POLLIN)
                        assert not watch.poll(0), 'stopped original process must still be owned and live'
                        membership = (proc / 'cgroup').read_text()
                        assert membership == Path('/proc/self/cgroup').read_text()
                        limits = (proc / 'limits').read_text()
                        status = (proc / 'status').read_text()
                        again = (proc / 'stat').read_text()
                        assert int(again[again.rfind(')') + 2:].split()[19]) == start
                        row['actual_native_process'] = dict(
                            pid=child.process.pid, start_ticks=start,
                            executable_device=elf.st_dev, executable_inode=elf.st_ino,
                            executable_matches_product=True, owned_pidfd_live=True,
                            cgroup_membership=membership, actual_limits=limits,
                            actual_status=status)
                        save()
                        if action == 'disconnect':
                            parent.close()
                        else:
                            parent.sendall(action.encode('ascii'))
                            # A successful terminal command may race natural
                            # endpoint closure. Only the original OS wait below
                            # establishes completion and its real exit code.
                            try:
                                reply = parent.recv(65536)
                            except ConnectionResetError:
                                reply = b''
                            row['resume_reply'] = reply.decode('utf-8', errors='replace')
                            assert b'error:' not in reply, reply
                        row['process_exit'] = child.wait(30)
                        assert row['process_exit'] == expected, row
                        row['kernel_process_retirement_observed'] = True
                        row['parent_endpoint_kept_open_until_process_exit'] = action != 'disconnect'
                    assert sha(wasm) == fixture['wasm_sha256'] and sha(wat) == fixture['wat_sha256']
                    row.update(passed=True, elapsed_seconds=time.monotonic() - begin)
                except BaseException as error:
                    row['error'] = repr(error)
                    raise
                finally:
                    inherited.close()
                    parent.close()
                    if child:
                        child.close()
                    save()
        result['passed'] = all(row['passed'] for row in result['rows'])
    finally:
        result['pins_after'] = {path: sha(path) for path in pins}
        if result['pins_after'] != pins:
            result['passed'] = False
        save()
    assert result['passed']
    print('PASS inherited debug endpoint original guest retirement', len(result['rows']), 'cases')


if __name__ == '__main__':
    main()
