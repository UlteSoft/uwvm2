#!/usr/bin/env python3
"""Finite native-POSIX component tests, run only by the admitted SSH keeper.
No VM stop/source/native privilege is inferred from this component pass.
"""
import argparse
import errno
import json
import os
import pty
import select
import signal
import subprocess
import termios
import time
from pathlib import Path


def check_cgroup(expected):
    if not expected or not Path('/proc/self/cgroup').exists():
        raise RuntimeError('SSH Linux keeper cgroup path is mandatory; no local fallback')
    line = next((s for s in Path('/proc/self/cgroup').read_text().splitlines() if s.startswith('0::')), None)
    if line != '0::' + expected:
        raise RuntimeError(f'wrong actual cgroup: {line!r}')
    maximum = (Path('/sys/fs/cgroup') / expected.lstrip('/') / 'memory.max').read_text().strip()
    if maximum != str(64 * 1024**3):
        raise RuntimeError('actual cgroup must retain exactly 64 GiB limit')


def wait_text(fd, wanted, transcript, deadline, occurrences=1):
    if transcript.count(wanted) >= occurrences:
        return
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], max(0, min(.05, deadline - time.monotonic())))
        if not ready:
            continue
        try:
            data = os.read(fd, 4096)
        except OSError as exc:
            if exc.errno == errno.EIO:
                break
            raise
        if not data:
            break
        transcript.extend(data)
        if transcript.count(wanted) >= occurrences:
            return
    raise AssertionError(f'missing {wanted!r}: {bytes(transcript)!r}')


def spawn_pty(binary, mode):
    reader, writer = os.pipe()
    pid, fd = pty.fork()
    if pid == 0:
        os.close(writer)
        if os.read(reader, 1) != b'R':
            os._exit(99)
        os.close(reader)
        os.execv(str(binary), [str(binary), mode])
    os.close(reader)
    original = termios.tcgetattr(fd)  # child is blocked until this baseline is captured
    os.write(writer, b'R')
    os.close(writer)
    return pid, fd, original


def reap(pid, deadline):
    while time.monotonic() < deadline:
        got, status = os.waitpid(pid, os.WNOHANG)
        if got:
            return os.waitstatus_to_exitcode(status)
        time.sleep(.01)
    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)
    raise AssertionError('finite 5-second component deadline expired')


def pty_case(binary, mode, event):
    pid, fd, original = spawn_pty(binary, mode)
    transcript = bytearray()
    deadline = time.monotonic() + 5
    reaped = False
    try:
        wait_text(fd, b'PROBE READY' if mode == 'origin' else b'KEYBOARD READY', transcript, deadline)
        if event == 'terminal':
            if mode != 'origin':
                os.write(fd, b'partial-dangerous-command')
                time.sleep(.05)
            os.write(fd, b'\x03')
        elif event == 'external':
            os.kill(pid, signal.SIGINT)
        elif event == 'history':
            os.write(fd, b'status\n')
            wait_text(fd, b'LINE status', transcript, deadline)
            os.write(fd, b'\x1b[A\n')
            wait_text(fd, b'LINE status', transcript, deadline, occurrences=2)
            os.write(fd, b'\x04')
        elif event == 'eof':
            os.write(fd, b'\x04')
        if mode == 'origin':
            wait_text(fd, b'PROBE code=', transcript, deadline)
        elif event in ('terminal', 'external'):
            wait_text(fd, b'KEYBOARD INTERRUPTED size=0', transcript, deadline)
            os.write(fd, b'quit\n')
            wait_text(fd, b'KEYBOARD RESTORED same=1', transcript, deadline)
        else:
            wait_text(fd, b'KEYBOARD EOF', transcript, deadline)
            wait_text(fd, b'KEYBOARD RESTORED same=1', transcript, deadline)
        result = reap(pid, deadline)
        reaped = True
        if result != 0:
            raise AssertionError(f'component exit {result}')
        if mode != 'origin' and termios.tcgetattr(fd) != original:
            raise AssertionError('terminal attributes were not restored on scoped exit')
        return {'mode': mode, 'event': event, 'exit': result, 'transcript': transcript.decode('utf-8', 'backslashreplace')}
    finally:
        if not reaped:
            try:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            except ProcessLookupError:
                pass
        os.close(fd)


class PipeCaseFailure(AssertionError):
    def __init__(self, row):
        self.row = row
        super().__init__('pipe component failure: ' + json.dumps(row, ensure_ascii=False))


def pipe_case(binary, mode):
    child = subprocess.Popen([str(binary), mode], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    transcript = bytearray()
    deadline = time.monotonic() + 5
    try:
        wait_text(child.stdout.fileno(), b'KEYBOARD READY', transcript, deadline)
        if mode == 'fifo-description':
            wait_text(child.stdout.fileno(), b'FIFO READY', transcript, deadline)
            child.stdin.write(b'Z')
            child.stdin.flush()  # keep writer open: no EOF can conceal EAGAIN
            wait_text(child.stdout.fileno(), b'FIFO EAGAIN original-flags-unchanged=1', transcript, deadline)
        else:
            wait_text(child.stdout.fileno(), b'PIPE WORKER READY', transcript, deadline)
            os.kill(child.pid, signal.SIGINT)
            wait_text(child.stdout.fileno(), b'PIPE WORKER INTERRUPTED reader-sigint-blocked=1', transcript, deadline)
        wait_text(child.stdout.fileno(), b'KEYBOARD RESTORED same=1', transcript, deadline)
        result = child.wait(timeout=max(.01, deadline - time.monotonic()))
        if result != 0:
            raise AssertionError(f'pipe component exit {result}')
        return {'mode': mode, 'exit': result, 'transcript': transcript.decode('utf-8', 'backslashreplace')}
    except BaseException as exc:
        # Keep the writer open while obtaining actual natural retirement; EOF
        # must not conceal the EAGAIN assertion. Cleanup kills only this exact
        # owned Popen child after a separate finite wait, never a process name.
        before_cleanup = child.poll()
        killed_for_cleanup = False
        if before_cleanup is None:
            try:
                child.wait(timeout=.2)
            except subprocess.TimeoutExpired:
                killed_for_cleanup = True
                child.kill()
                child.wait(timeout=1)
        remaining, diagnostic = child.communicate(timeout=1)
        row = {'mode': mode, 'error': repr(exc), 'exit_before_cleanup': before_cleanup,
               'exit': child.returncode, 'killed_for_cleanup': killed_for_cleanup,
               'transcript': (bytes(transcript) + remaining).decode('utf-8', 'backslashreplace'),
               'stderr': diagnostic.decode('utf-8', 'backslashreplace')}
        raise PipeCaseFailure(row) from exc
    finally:
        if child.poll() is None:
            child.kill()
            child.wait(timeout=1)
        child.stdin.close()
        child.stdout.close()
        child.stderr.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cgroup', required=True)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--keyboard', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    check_cgroup(args.cgroup)
    rows = []
    try:
        completed = subprocess.run([str(args.editor)], text=True, capture_output=True, timeout=5)
        rows.append({'case': 'editor', 'exit': completed.returncode, 'stdout': completed.stdout, 'stderr': completed.stderr})
        if completed.returncode != 0 or 'PASS editing' not in completed.stdout:
            raise AssertionError('editor/alias qualification failed')
        for mode, expected in [('self-default', -signal.SIGINT), ('self-ignore', 0), ('self-custom', 0),
                               ('self-info', 0), ('self-reset', -signal.SIGINT), ('change-handler', 0)]:
            completed = subprocess.run([str(args.keyboard), mode], input='', text=True, capture_output=True, timeout=5)
            rows.append({'case': mode, 'exit': completed.returncode, 'expected': expected, 'stdout': completed.stdout, 'stderr': completed.stderr})
            if completed.returncode != expected or (expected == 0 and ('RESTORED same=1' not in completed.stdout or (mode.startswith('self-') and 'pending=0' not in completed.stdout))):
                raise AssertionError('guest/self signal minted pause or changed predecessor disposition')
        for mode, event in [('origin', 'terminal'), ('origin', 'external'), ('edit', 'terminal'),
                            ('edit', 'external'), ('edit', 'history'), ('edit', 'eof')]:
            row = pty_case(args.keyboard, mode, event)
            rows.append(row)
            if mode == 'origin':
                fields = row['transcript'].split('PROBE code=', 1)[1].splitlines()[0].split()
                code = int(fields[0])
                numbers = dict(item.split('=', 1) for item in fields[1:])
                if event == 'terminal' and (code != int(numbers['SI_KERNEL']) or int(numbers['pid']) != 0):
                    raise AssertionError('actual Linux terminal SIGINFO origin differs; no unproved authorization fallback')
                if event == 'external' and (int(numbers['pid']) != os.getpid() or int(numbers['uid']) != os.getuid()):
                    raise AssertionError('actual external-owner SIGINFO mismatch')
        for mode in ('fifo-description', 'pipe-worker'):
            rows.append(pipe_case(args.keyboard, mode))
        result = {'status': 'PASS component only', 'full_product_tested': False, 'dap_tested': False,
                  'full_vm_retirement_ack_tested': False, 'rows': rows}
    except BaseException as exc:
        if isinstance(exc, PipeCaseFailure):
            rows.append(exc.row)  # Preserve the failed actual child evidence.
        result = {'status': 'FAIL', 'error': repr(exc), 'full_product_tested': False, 'rows': rows}
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        raise
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    main()
