#!/usr/bin/env python3
"""Real macOS ARM64 LLVM-full source, Wasm, and native debugger sequence."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import subprocess
import sys
import tempfile
import time


SOURCE = '''__attribute__((visibility("default")))
int uwvm_debug_dummy(int value) { return value; }

__attribute__((visibility("default")))
int debug_source_c(int value) {
    int adjusted = value + 7;
    adjusted = adjusted * 3;
    return adjusted;
}
'''
IMPORTER = '''(module
  (import "p" "debug_source_c" (func $source (param i32) (result i32)))
  (func (export "_start")
    i32.const 5 call $source i32.const 36 i32.ne if unreachable end))
'''
DEAD_BRANCH = '''(module
  (func (export "_start")
    i32.const 0
    if
      i32.const 1
      drop
    end))
'''
NATIVE_LINE = re.compile(
    rb'native instruction 0x([0-9a-fA-F]+) bytes=([0-9a-fA-F ]+)  ([^\r\n]+?) -> 0x([0-9a-fA-F]+)'
)


def capped(watchdog: Path, argv: list[str]) -> bytes:
    completed = subprocess.run([sys.executable, str(watchdog), '--', *argv],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               check=False)
    if completed.returncode:
        raise RuntimeError((argv, completed.returncode, completed.stdout[-6000:]))
    if b'MACOS_RSS_LIMIT_BYTES=4294967296' not in completed.stdout:
        raise RuntimeError(('missing 4 GiB watchdog evidence', argv))
    return completed.stdout


class Console:
    def __init__(self, watchdog: Path, binary: Path, provider: Path | None,
                 importer: Path, policy: str):
        self.policy = policy
        command = [sys.executable, str(watchdog), '--', str(binary),
                   '-m', 'debug-jit', '-Rct', '0', '-Rllvm-call-stack', policy,
                   '-Rllvm-cache-path', 'disable']
        if provider is not None:
            command.extend(['-Wpre', str(provider), 'p'])
        command.extend(['--run', str(importer)])
        self.child = subprocess.Popen(
            command,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=0)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.child.stdout, selectors.EVENT_READ)
        self.pending = bytearray()
        self.log = bytearray()
        self.commands = []
        self.prompt()

    def prompt(self) -> bytes:
        marker = b'(uwvm-debug) '
        deadline = time.monotonic() + 40
        while marker not in self.pending:
            if time.monotonic() >= deadline:
                raise RuntimeError((self.policy, 'prompt timeout', self.log[-5000:]))
            ready = self.selector.select(max(0, deadline - time.monotonic()))
            if not ready:
                raise RuntimeError((self.policy, 'prompt timeout', self.log[-5000:]))
            chunk = os.read(self.child.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError((self.policy, 'early process exit',
                                    self.child.poll(), self.log[-5000:]))
            self.pending.extend(chunk)
            self.log.extend(chunk)
        end = self.pending.index(marker) + len(marker)
        result = bytes(self.pending[:end])
        del self.pending[:end]
        return result

    def send(self, command: str) -> bytes:
        self.commands.append(command)
        self.child.stdin.write(command.encode() + b'\n')
        self.child.stdin.flush()
        return self.prompt()

    def until(self, marker: bytes) -> bytes:
        deadline = time.monotonic() + 25
        while True:
            reply = self.send('wait')
            if marker in reply:
                return reply
            if time.monotonic() >= deadline:
                raise RuntimeError((self.policy, 'wait timeout', marker, reply))

    def close(self) -> tuple[int, bytes]:
        self.commands.append('quit')
        self.child.stdin.write(b'quit\n')
        self.child.stdin.flush()
        self.child.stdin.close()
        status = self.child.wait(timeout=30)
        self.log.extend(self.child.stdout.read())
        self.selector.close()
        if status:
            raise RuntimeError((self.policy, status, self.log[-6000:]))
        peaks = re.findall(rb'PEAK_PROCESS_TREE_RSS_BYTES=(\d+)', self.log)
        if not peaks or b'MACOS_RSS_LIMIT_BYTES=4294967296' not in self.log:
            raise RuntimeError((self.policy, 'missing 4 GiB watchdog evidence'))
        peak = max(int(value) for value in peaks)
        if peak > 4 * 1024**3:
            raise RuntimeError((self.policy, 'RSS limit exceeded', peak))
        return peak, bytes(self.log)

    def abort(self) -> None:
        if self.child.poll() is None:
            self.child.kill()
        self.child.wait()
        self.selector.close()


def verify_steps(console: Console) -> dict:
    assert b'prepared; no Wasm instruction executed' in console.send('status')
    # The provider has dummy function 0 and source function 1. The importer
    # also has _start at function 1 after its imported function 0.
    for module in (0, 1):
        assert b'breakpoint' in console.send(f'break {module} 1 0')
    console.send('continue')
    stopped = console.until(b'stopped: breakpoint')
    for _ in range(64):
        if b'  source ' in stopped and b'debug_source_c.c:' in stopped:
            break
        stopped = console.send('step wasm 1')
        assert b'stopped: selected participant step' in stopped, stopped
    else:
        raise RuntimeError(('source location never appeared', stopped))
    source = console.send('step source 1 into')
    assert b'stopped: selected participant step' in source and b'  source ' in source, source
    wasm = console.send('step wasm 1')
    assert b'stopped: selected participant step' in wasm, wasm
    first = console.send('step asm 1')
    second = console.send('step asm 1')
    first_match, second_match = NATIVE_LINE.search(first), NATIVE_LINE.search(second)
    assert first_match and second_match, (first, second)
    assert b'stopped: native instruction step' in first and b'stopped: native instruction step' in second
    assert int(first_match.group(4), 16) == int(second_match.group(1), 16), (first, second)
    for match in (first_match, second_match):
        octets = match.group(2).split()
        assert len(octets) == 4 and all(len(item) == 2 for item in octets), match.group(2)
        assert re.search(rb'[A-Za-z]', match.group(3)), match.group(3)
    assert b'native-pc=0x' in console.send('status')
    return {
        'policy': console.policy,
        'source': source.decode(errors='replace'),
        'wasm': wasm.decode(errors='replace'),
        'first_native': first.decode(errors='replace'),
        'second_native': second.decode(errors='replace'),
        'commands': console.commands,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, default=shutil.which('wasm-tools'))
    parser.add_argument('--clang', type=Path, default=shutil.which('clang'))
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    watchdog = root / 'test/0017.runtime/macos_rss_limit.py'
    binary = args.uwvm.resolve(strict=True)
    if not args.clang or not args.wasm_tools:
        parser.error('clang and wasm-tools are required')
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-debug-cli-') as temporary:
        work = Path(temporary)
        provider_c = work / 'debug_source_c.c'
        provider = work / 'provider.wasm'
        importer_wat = work / 'importer.wat'
        importer = work / 'importer.wasm'
        provider_c.write_text(SOURCE)
        importer_wat.write_text(IMPORTER)
        capped(watchdog, [str(args.clang), '--target=wasm32-unknown-unknown',
            '-O0', '-g', '-gdwarf-4', '-nostdlib', '-Wl,--no-entry',
            '-Wl,--export=uwvm_debug_dummy', '-Wl,--export=debug_source_c',
            '-Wl,--export-memory', '-Wl,--initial-memory=131072',
            str(provider_c), '-o', str(provider)])
        capped(watchdog, [str(args.wasm_tools), 'parse', str(importer_wat),
                           '-o', str(importer)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(provider)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(importer)])
        dead_wat = work / 'dead_branch.wat'
        dead_wasm = work / 'dead_branch.wasm'
        dead_wat.write_text(DEAD_BRANCH)
        capped(watchdog, [str(args.wasm_tools), 'parse', str(dead_wat), '-o', str(dead_wasm)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(dead_wasm)])
        results = []
        dead = Console(watchdog, binary, None, dead_wasm, 'instruction')
        try:
            rejected = dead.send('break 0 0 4')
            assert b'error: Wasm byte offset has no emitted executable debug safe point' in rejected, rejected
            assert b'breakpoint 1' in dead.send('break 0 0 0')
            dead.send('continue')
            dead.until(b'stopped: breakpoint')
            assert b'breakpoint deleted' in dead.send('delete 1')
            dead.send('continue')
            dead.until(b'guest exited: 0')
            dead_peak, dead_log = dead.close()
        except BaseException:
            dead.abort()
            raise
        if args.out:
            args.out.mkdir(parents=True, exist_ok=True)
            (args.out / 'dead_branch.log').write_bytes(dead_log)
        print(f'PASS MACOS_DEBUG_JIT_DEAD_BRANCH RSS={dead_peak}', flush=True)
        for policy in ('instruction', 'unwind'):
            console = Console(watchdog, binary, provider, importer, policy)
            try:
                result = verify_steps(console)
                peak, log = console.close()  # cancel from the second native trap
            except BaseException:
                console.abort()
                raise
            result['peak_process_tree_rss_bytes'] = peak
            results.append(result)
            if args.out:
                args.out.mkdir(parents=True, exist_ok=True)
                (args.out / f'{policy}.log').write_bytes(log)
            print(f'PASS MACOS_DEBUG_JIT_{policy.upper()} SOURCE_WASM_NATIVE RSS={peak}', flush=True)
        if args.out:
            summary = {
                'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'provider_sha256': hashlib.sha256(provider.read_bytes()).hexdigest(),
                'importer_sha256': hashlib.sha256(importer.read_bytes()).hexdigest(),
                'dead_branch_sha256': hashlib.sha256(dead_wasm.read_bytes()).hexdigest(),
                'dead_branch_peak_process_tree_rss_bytes': dead_peak,
                'results': results,
            }
            (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
