#!/usr/bin/env python3
"""Real Linux product inline metadata display, replacement and native-stop closure.

Uses only prior officially verified compiler-produced C/C++/Rust O1 DWARF4/5
fixtures from run_debug_source_dwarf_stage1.py. Every derived module remains
valid Wasm; absent/malformed/external DWARF tests metadata rejection, not Wasm
validation rejection. Requires a fresh product with its actual DWARF link closure.
"""
from __future__ import annotations
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


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def u32(data: bytes, at: int, end: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 35, 7):
        if at >= end:
            raise ValueError('truncated fixture u32')
        byte = data[at]
        at += 1
        if shift == 28 and byte & 0xf0:
            raise ValueError('fixture u32 overflow')
        value |= (byte & 0x7f) << shift
        if not byte & 0x80:
            return value, at
    raise ValueError('overlong fixture u32')


def leb(value: int) -> bytes:
    if not 0 <= value <= 0xffffffff:
        raise ValueError('fixture u32 range')
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7f) | 0x80)
        value >>= 7
    out.append(value)
    return bytes(out)


def sections(path: Path) -> list[tuple[int, bytes]]:
    data = path.read_bytes()
    if data[:8] != b'\0asm\x01\0\0\0':
        raise ValueError('fixture Wasm header')
    at = 8
    rows = []
    while at < len(data):
        kind = data[at]
        size, at = u32(data, at + 1, len(data))
        if size > len(data) - at:
            raise ValueError('fixture section bounds')
        rows.append((kind, data[at:at + size]))
        at += size
    return rows


def named_custom(payload: bytes) -> bytes:
    size, at = u32(payload, 0, len(payload))
    if size > len(payload) - at:
        raise ValueError('fixture custom-name bounds')
    return payload[at:at + size]


def function(path: Path, export: str) -> tuple[int, bytes]:
    target = None
    code = None
    for kind, payload in sections(path):
        if kind == 2:
            count, _ = u32(payload, 0, len(payload))
            if count:
                raise ValueError('focused compiler fixtures must have no imports')
        if kind == 7:
            count, at = u32(payload, 0, len(payload))
            for _ in range(count):
                size, at = u32(payload, at, len(payload))
                if size >= len(payload) - at:
                    raise ValueError('fixture export bounds')
                name = payload[at:at + size]
                at += size
                tag = payload[at]
                index, at = u32(payload, at + 1, len(payload))
                if name == export.encode() and tag == 0:
                    target = index
        if kind == 10:
            code = payload
    if target is None or code is None:
        raise ValueError('fixture exported local function missing')
    count, at = u32(code, 0, len(code))
    bodies = []
    for _ in range(count):
        size, at = u32(code, at, len(code))
        if size > len(code) - at:
            raise ValueError('fixture body bounds')
        bodies.append(code[at:at + size])
        at += size
    if at != len(code) or target >= len(bodies):
        raise ValueError('fixture body/count mismatch')
    return target, bodies[target]


def variant(path: Path, out: Path, kind: str) -> None:
    rows = []
    for tag, payload in sections(path):
        name = named_custom(payload) if tag == 0 else b''
        if kind == 'missing' and name.startswith(b'.debug_'):
            continue
        if kind == 'malformed' and name == b'.debug_info':
            payload = leb(len(name)) + name + b'\xff'
        rows.append((tag, payload))
    if kind == 'external':
        name = b'.gnu_debugaltlink'
        rows.append((0, leb(len(name)) + name + b'never-open-this-file.debug\0'))
    out.write_bytes(b'\0asm\x01\0\0\0' + b''.join(bytes([tag]) + leb(len(payload)) + payload for tag, payload in rows))


class Console:
    def __init__(self, command: list[str], log: Path):
        self.command, self.logpath = command, log
        self.child = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.pending, self.transcript = bytearray(), bytearray()
        try:
            self.prompt()
        except BaseException:
            if self.child.poll() is None:
                self.child.kill()
            self.child.wait(timeout=15)
            self.transcript.extend(self.child.stdout.read())
            self.logpath.write_bytes(self.transcript)
            raise

    def prompt(self) -> bytes:
        marker = b'(uwvm-debug) '
        deadline = time.monotonic() + 40
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

    def send(self, text: str) -> bytes:
        self.child.stdin.write(text.encode() + b'\n')
        self.child.stdin.flush()
        return self.prompt()

    def finish(self) -> None:
        try:
            self.child.stdin.write(b'quit\n')
            self.child.stdin.flush()
            if self.child.wait(timeout=15) != 0:
                raise AssertionError('console quit failed')
        finally:
            if self.child.poll() is None:
                self.child.kill()
                self.child.wait(timeout=15)
            self.transcript.extend(self.child.stdout.read())
            self.logpath.write_bytes(self.transcript)


def qualify(prefix: list[str], wasm: Path, export: str, log: Path,
            expected_reason: bytes | None = None, replacement: Path | None = None, native: bool = False) -> dict:
    target, body = function(wasm, export)
    if replacement is not None:
        replacement.write_bytes(body)  # the officially validated exact same ABI/body
    console = Console([*prefix, '--run', str(wasm)], log)
    seen_inline = False
    try:
        if b'prepared; no Wasm instruction executed' not in console.send('status'):
            raise AssertionError('not actual prepared console')
        if replacement is not None:
            reply = console.send(f'replace 0 {target} 1 {replacement}')
            if b'function replaced; generation 2' not in reply:
                raise AssertionError(reply)
        if b'breakpoint' not in console.send(f'break 0 {target} 0'):
            raise AssertionError('target emitted entry safe point missing')
        console.send('continue')
        stopped = b''
        for _ in range(20):
            stopped = console.send('wait')
            if b'stopped: breakpoint' in stopped:
                break
        else:
            raise AssertionError(stopped)
        match = re.search(rb'thread (\d+) module=0 function=' + str(target).encode() + rb' byte-offset=', stopped)
        if not match:
            raise AssertionError(stopped)
        participant = int(match.group(1))
        seen_reason = False
        for _ in range(128):
            trace = console.send(f'bt {participant}')
            current = re.search(rb'thread \d+ module=0 function=(\d+) byte-offset=', trace)
            if current is None or int(current.group(1)) != target:
                break
            has_inline = b'\n  inline ' in trace and b'\n  inline metadata unavailable:' not in trace
            if expected_reason is not None:
                if has_inline:
                    raise AssertionError(('stale/invalid metadata was displayed', trace))
                seen_reason |= expected_reason in trace
            else:
                seen_inline |= has_inline
            if seen_inline and native:
                stepped = console.send(f'step asm {participant}')
                if b'native instruction 0x' not in stepped:
                    raise AssertionError(('native backend did not actually step', stepped))
                after = console.send(f'bt {participant}')
                if b'  native-pc=0x' not in after or b'native trap has no current Wasm source position' not in after or b'\n  inline ' in after.replace(b'\n  inline metadata unavailable:', b'\n  unavailable:'):
                    raise AssertionError(('native trap reused stale source/inline metadata', after))
                break
            stepped = console.send(f'step wasm {participant}')
            if b'guest exited:' in stepped:
                break
        if expected_reason is None and not seen_inline:
            raise AssertionError(('no actual inline chain observed', wasm, bytes(console.transcript)[-4000:]))
        if expected_reason is not None and not seen_reason:
            raise AssertionError(('expected explicit unavailable reason not observed', expected_reason, bytes(console.transcript)[-4000:]))
        return {'wasm_sha256': sha(wasm), 'export': export, 'function_index': target,
                'replacement': replacement is not None, 'native_step': native, 'log': str(log), 'passed': True}
    finally:
        console.finish()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--fixture-dir', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--native-step', action='store_true', help='Also require a real qualified Linux x86-64 machine step')
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('Linux cgroup qualification only')
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary = args.uwvm.resolve(strict=True)
    fixture_dir = args.fixture_dir.resolve(strict=True)
    rows = []
    for policy in ('instruction', 'unwind'):
        mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                  '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
        for language in ('c', 'cpp', 'rust'):
            for version in (4, 5):
                wasm = fixture_dir / f'{language}-dwarf{version}-O1.wasm'
                subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
                export = 'source_outer_' + language
                rows.append(qualify(prefix, wasm, export, out / f'{policy}-{language}-v{version}.log'))
        baseline = fixture_dir / 'c-dwarf5-O1.wasm'
        rows.append(qualify(prefix, baseline, 'source_outer_c', out / f'{policy}-replacement.log',
                    b'source generation or stopped position is no longer current', out / f'{policy}-same.body'))
        for kind, reason in (('missing', b'no embedded metadata bound to this source'),
                             ('malformed', b'invalid or unsupported embedded metadata'),
                             ('external', b'invalid or unsupported embedded metadata')):
            wasm = out / f'{kind}.wasm'
            if not wasm.exists():
                variant(baseline, wasm, kind)
            # These modules are VALID Wasm with deliberately unavailable DWARF.
            subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
            rows.append(qualify(prefix, wasm, 'source_outer_c', out / f'{policy}-{kind}.log', reason))
        if args.native_step:
            rows.append(qualify(prefix, baseline, 'source_outer_c', out / f'{policy}-native.log', native=True))
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'product_sha256': sha(binary), 'cases': rows,
        'runtime_values': False, 'physical_caller_pc_fabrication': False,
        'kernel_native_step_qualified': args.native_step}, indent=2) + '\n')
    print(f'PASS actual inline metadata display: {len(rows)} product cases')


if __name__ == '__main__':
    main()
