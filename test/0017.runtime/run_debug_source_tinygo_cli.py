#!/usr/bin/env python3
"""Test actual TinyGo DWARF through a fresh product console in the Linux cgroup.

Compile the adjacent Go fixtures with TinyGo debug information retained. The
consume fixture uses -opt=0 -scheduler=none -gc=leaking; worker uses -opt=1
and the default scheduler. This runner never rewrites compiler-produced Wasm.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import selectors
import subprocess
import sys
import time
from run_debug_source_inline_metadata_cli import Console, sections, sha, u32


class TinyGoConsole(Console):
    def prompt(self) -> bytes:
        marker = b'(uwvm-debug) '
        deadline = time.monotonic() + 180
        with selectors.DefaultSelector() as selector:
            selector.register(self.child.stdout, selectors.EVENT_READ)
            while marker not in self.pending:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not selector.select(remaining):
                    raise AssertionError(('prompt timeout', bytes(self.transcript)[-4000:]))
                data = os.read(self.child.stdout.fileno(), 65536)
                if not data:
                    raise AssertionError(('early exit', self.child.poll(), bytes(self.transcript)[-4000:]))
                self.pending.extend(data)
                self.transcript.extend(data)
        at = self.pending.index(marker) + len(marker)
        reply = bytes(self.pending[:at])
        del self.pending[:at]
        return reply


def named_function(wasm: Path, name: bytes) -> tuple[int, bytes]:
    """Obtain the original function and its expression, excluding local decls."""
    target = None
    imported = 0
    code = None
    for tag, payload in sections(wasm):
        end = len(payload)
        if tag == 2:
            count, at = u32(payload, 0, end)
            for _ in range(count):
                for _ in range(2):
                    size, at = u32(payload, at, end)
                    if size > end - at:
                        raise ValueError('import name bounds')
                    at += size
                if at >= end or payload[at] != 0:
                    raise ValueError('focused TinyGo fixture requires function-only imports')
                _, at = u32(payload, at + 1, end)
                imported += 1
            if at != end:
                raise ValueError('import bounds')
        elif tag == 0:
            length, at = u32(payload, 0, end)
            if payload[at:at + length] != b'name':
                continue
            at += length
            while at < end:
                kind = payload[at]
                size, at = u32(payload, at + 1, end)
                stop = at + size
                if stop > end:
                    raise ValueError('name subsection bounds')
                if kind == 1:
                    count, at = u32(payload, at, stop)
                    for _ in range(count):
                        index, at = u32(payload, at, stop)
                        length, at = u32(payload, at, stop)
                        if length > stop - at:
                            raise ValueError('function name bounds')
                        if payload[at:at + length] == name:
                            if target is not None:
                                raise ValueError('duplicate function name')
                            target = index
                        at += length
                at = stop
        elif tag == 10:
            code = payload
    if target is None or target < imported or code is None:
        raise ValueError('original named local function unavailable')
    count, at = u32(code, 0, len(code))
    expression = None
    for i in range(count):
        size, at = u32(code, at, len(code))
        stop = at + size
        if stop > len(code):
            raise ValueError('Code body bounds')
        if imported + i == target:
            groups, start = u32(code, at, stop)
            for _ in range(groups):
                _, start = u32(code, start, stop)
                if start >= stop or code[start] not in (0x7f, 0x7e, 0x7d, 0x7c):
                    raise ValueError('focused fixture numeric local declarations')
                start += 1
            expression = code[start:stop]
        at = stop
    if at != len(code) or expression is None:
        raise ValueError('Code function count mismatch')
    return target, expression


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm', type=Path, required=True)
    parser.add_argument('--case', choices=('consume', 'sequences', 'worker'), required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if sys.platform != 'linux':
        raise RuntimeError('execute in the controlled Linux test cgroup')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary, wasm = args.uwvm.resolve(strict=True), args.wasm.resolve(strict=True)
    name = {'consume': b'main.consume', 'sequences': b'main.inspect', 'worker': b'main.worker'}[args.case]
    target, expression = named_function(wasm, name)
    offset = 0
    if args.case == 'worker':
        # This exact fixture computes id+10 immediately before storing Observed.
        # Debug byte offsets start AFTER local declarations, unlike DWARF PCs.
        pattern = b'\x20\x00\x41\x0a\x6a\x36\x02\x04'
        if expression.count(pattern) != 1:
            raise ValueError('unique actual worker value-store instruction required')
        offset = expression.index(pattern) + 5
    record = {'binary_sha256': sha(binary), 'wasm_sha256': sha(wasm), 'case': args.case,
              'function': target, 'offset': offset, 'actions': [], 'passed': False}
    def save():
        (out / 'summary.json').write_text(json.dumps(record, indent=2))
    prefix = [str(binary), '-Rdbg']
    if not args.ros:
        prefix += ['-Rcc', 'jit', '-Rcm', 'full']
    prefix += ['-Rct', '0', '-Rllvm-call-stack', 'instruction', '-Rllvm-cache-path', 'disable', '--run', str(wasm)]
    # Admission supervisor observes the executable after this short bootstrap.
    command = [sys.executable, '-c', 'import os,sys,time;time.sleep(.15);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)', *prefix]
    record['command'] = prefix
    console = None
    save()
    try:
        console = TinyGoConsole(command, out / 'console.log')
        def ask(command):
            text = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', console.send(command)).decode(errors='replace')
            record['actions'].append({'command': command, 'reply': text})
            save()
            return text
        reply = ask(f'break 0 {target} {offset}')
        bid = int(re.search(r'breakpoint (\d+)', reply)[1])
        ask('continue')
        for _ in range(64):
            stop = ask('wait')
            if 'stopped: breakpoint' in stop:
                break
            if 'guest exited:' in stop:
                raise AssertionError('guest exited before original breakpoint')
        else:
            raise AssertionError('actual breakpoint did not stop')
        thread = int(re.search(r'thread (\d+) module=0 function=', stop)[1])
        def current():
            trace = ask(f'bt {thread}')
            match = re.search(r'(?m)^stop-id (\d+)', trace)
            return (int(match[1]) if match else None), trace
        sid, _ = current()
        if sid is None:
            raise AssertionError('no actual stop identity')
        ask(f'frames {thread} {sid}')
        ask(f'locals source {thread}')
        ask(f'operands {thread}')
        expected = {'consume': {'p.Grid[1][2]': 15, 'n.Next.Value': 9, 's[1]': 5},
                    'sequences': {'box.Numbers[1]': 22}, 'worker': {'value': 11, 'value + 2': 13}}[args.case]
        for expression, value in expected.items():
            reply = ask(f'print {thread} {sid} {expression}')
            if not re.search(r'(?:value=|i32=)' + str(value) + r'\b', reply):
                raise AssertionError(('actual guest value mismatch', expression, value, reply))
            if reply != ask(f'print {thread} {sid} {expression}'):
                raise AssertionError('same-stop value changed')
        if args.case == 'consume':
            text = ask(f'print {thread} {sid} t')
            if 'unavailable' not in text:
                raise AssertionError('compiler-omitted string pointer must remain unavailable')
        if args.case == 'sequences':
            text = ask(f'print {thread} {sid} box.Text')
            if 'uwvm-tinygo' not in text:
                raise AssertionError(('guest string contents missing', text))
        stale = ask(f'print {thread} {sid + 1000000} value')
        if 'error:' not in stale and 'unavailable' not in stale:
            raise AssertionError('fabricated stop identity accepted')
        ask(f'delete {bid}')
        steps = []
        record['source_out'] = steps
        previous = sid
        for _ in range(4 if args.case == 'worker' else 1):
            reply = ask(f'step source {thread} out')
            settled = reply
            # A wire wait timeout leaves the same authenticated source policy
            # running. It is not a successful finish: await a real stop or exit.
            for _ in range(64):
                if 'error:' in settled:
                    raise AssertionError(('source out failed', settled))
                if re.search(r'(?m)^stop-id \d+', settled) or 'guest exited:' in settled:
                    break
                settled = ask('wait')
            else:
                raise AssertionError('source out never established a complete stop or guest exit')
            now, trace = current()
            steps.append({'reply': reply, 'completion': settled, 'trace': trace})
            save()
            if 'guest exited:' in settled or 'guest exited:' in trace:
                break
            if now is None or now == previous or not re.search(r'(?m)^  source .+:\d+:\d+', trace):
                raise AssertionError(('source out did not reach a fresh mapped stop', settled, trace))
            previous = now
        record['passed'] = True
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        if console is not None:
            try:
                console.finish()
                record['quit_returncode'] = console.child.returncode
            except BaseException as error:
                record['passed'] = False
                record['close_error'] = repr(error)
                raise
            finally:
                record['managed_shutdown_complete'] = b'managed shutdown complete' in console.transcript
                save() # preserve error/cleanup evidence even when finish raises
        save()


if __name__ == '__main__':
    main()
