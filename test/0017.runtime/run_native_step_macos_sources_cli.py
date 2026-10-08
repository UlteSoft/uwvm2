#!/usr/bin/env python3
"""Build real C/C++/Rust DWARF Wasm and step their source lines on macOS ARM64."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from run_native_step_macos_debug_cli import Console, capped


def default_rustc() -> str | None:
    if shutil.which('rustup'):
        result = subprocess.run(['rustup', 'which', 'rustc'], capture_output=True,
                                text=True, check=False)
        if result.returncode == 0:
            return result.stdout.strip()
    return shutil.which('rustc')


def uleb(data: bytes, at: int, end: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        assert at < end, 'truncated Wasm LEB'
        byte = data[at]
        at += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, at
    raise AssertionError('overlong Wasm LEB')


def export_function(path: Path, name: str) -> int:
    data = path.read_bytes()
    assert data[:8] == b'\0asm\x01\0\0\0'
    at = 8
    while at < len(data):
        section = data[at]
        at += 1
        size, at = uleb(data, at, len(data))
        end = at + size
        assert end <= len(data)
        if section == 7:
            count, at = uleb(data, at, end)
            for _ in range(count):
                length, at = uleb(data, at, end)
                assert length <= end - at
                label = data[at:at + length]
                at += length
                kind = data[at]
                at += 1
                index, at = uleb(data, at, end)
                if label == name.encode() and kind == 0:
                    return index
            break
        at = end
    raise AssertionError(f'{path}: no exported function {name}')


def qualify(watchdog: Path, binary: Path, wasm: Path, suffix: bytes) -> dict:
    entry = export_function(wasm, '_start')
    console = Console(watchdog, binary, None, wasm, 'unwind')
    try:
        assert b'prepared; no Wasm instruction executed' in console.send('status')
        assert b'breakpoint' in console.send(f'break 0 {entry} 0')
        console.send('continue')
        stopped = console.until(b'stopped: breakpoint')
        for _ in range(128):
            if b'  source ' in stopped and suffix in stopped:
                break
            stopped = console.send('step wasm 1')
            assert b'stopped: selected participant step' in stopped, stopped
        else:
            raise AssertionError((wasm, 'source location not reached', stopped))
        first = stopped.split(b'  source ', 1)[1].split(b'\n', 1)[0]
        stepped = console.send('step source 1 into')
        assert b'stopped: selected participant step' in stepped and b'  source ' in stepped, stepped
        second = stepped.split(b'  source ', 1)[1].split(b'\n', 1)[0]
        assert first != second, (wasm, first, second)
        console.send('continue')
        console.until(b'guest exited: 0')
        peak, log = console.close()
        return {'file': wasm.name, 'entry_index': entry,
                'first_source': first.decode(errors='replace'),
                'second_source': second.decode(errors='replace'),
                'peak_process_tree_rss_bytes': peak,
                'binary_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(),
                'log': log}
    except BaseException:
        console.abort()
        raise


def qualify_frame_policy(watchdog: Path, binary: Path, wasm: Path,
                         policy: str, callee_name: str) -> dict:
    """Check that source over/out preserve the real Wasm caller frame on Darwin."""
    caller = export_function(wasm, '_start')
    callee = export_function(wasm, callee_name)
    origin = caller if policy == 'over' else callee
    destination = caller
    console = Console(watchdog, binary, None, wasm, 'unwind')
    try:
        assert b'breakpoint' in console.send(f'break 0 {origin} 0')
        console.send('continue')
        stopped = console.until(b'stopped: breakpoint')
        for _ in range(64):
            assert f'function={origin}'.encode() in stopped, stopped
            if b'  source ' in stopped:
                break
            stopped = console.send('step wasm 1')
            assert b'stopped: selected participant step' in stopped, stopped
        else:
            raise AssertionError((wasm, policy, 'origin source location not reached'))
        stepped = console.send(f'step source 1 {policy}')
        assert b'stopped: selected participant step' in stepped, stepped
        assert f'function={destination}'.encode() in stepped and b'  source ' in stepped, stepped
        console.send('continue')
        console.until(b'guest exited: 0')
        peak, log = console.close()
        return {'policy': policy, 'origin_function': origin,
                'destination_function': destination,
                'peak_process_tree_rss_bytes': peak, 'log': log}
    except BaseException:
        console.abort()
        raise


def qualify_source_break(watchdog: Path, binary: Path, wasm: Path,
                         source_location: str) -> dict:
    source_path, line, _column = source_location.rsplit(':', 2)
    console = Console(watchdog, binary, None, wasm, 'unwind')
    try:
        installed = console.send(f'break-source 0 {source_path}:{line}')
        assert b'breakpoint 1 source=' in installed, installed
        console.send('continue')
        stopped = console.until(b'stopped: breakpoint')
        assert f'  source {source_path}:{line}:'.encode() in stopped, stopped
        peak, log = console.close()
        return {'file': wasm.name, 'source': source_location,
                'peak_process_tree_rss_bytes': peak, 'log': log}
    except BaseException:
        console.abort()
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--clang', type=Path, default=shutil.which('clang'))
    parser.add_argument('--wasm-ld', type=Path, default=shutil.which('wasm-ld'))
    parser.add_argument('--rustc', type=Path, default=default_rustc())
    parser.add_argument('--wasm-tools', type=Path, default=shutil.which('wasm-tools'))
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    assert args.clang and args.wasm_ld and args.rustc and args.wasm_tools
    root = Path(__file__).resolve().parents[2]
    watchdog = root / 'test/0017.runtime/macos_rss_limit.py'
    binary = args.uwvm.resolve(strict=True)
    fixtures = root / 'test/0017.runtime/fixtures'
    rows = []
    frame_rows = []
    source_break_rows = []
    with tempfile.TemporaryDirectory(prefix='uwvm-macos-sources-') as temporary:
        work = Path(temporary)
        for language, source, suffix in [('c', 'debug_source_c.c', b'.c:'),
                                         ('cpp', 'debug_source_cpp.cc', b'.cc:')]:
            for dwarf in (4, 5):
                stem = f'{language}-dwarf{dwarf}'
                obj, wasm = work / (stem + '.o'), work / (stem + '.wasm')
                capped(watchdog, [str(args.clang), '--target=wasm32-unknown-unknown',
                    '-g', f'-gdwarf-{dwarf}', '-O0', '-nostdlib', '-c',
                    str(fixtures / source), '-o', str(obj)])
                capped(watchdog, [str(args.wasm_ld), '--no-entry', '--export-all',
                                  str(obj), '-o', str(wasm)])
                capped(watchdog, [str(args.wasm_tools), 'validate', str(wasm)])
                row = qualify(watchdog, binary, wasm, suffix)
                if args.out:
                    args.out.mkdir(parents=True, exist_ok=True)
                    (args.out / (stem + '.log')).write_bytes(row.pop('log'))
                else:
                    row.pop('log')
                rows.append(row)
                print(f'PASS macOS source {stem}', flush=True)
                source_break = qualify_source_break(watchdog, binary, wasm, row['first_source'])
                if args.out:
                    (args.out / f'{stem}-source-break.log').write_bytes(source_break.pop('log'))
                else:
                    source_break.pop('log')
                source_break_rows.append(source_break)
                print(f'PASS macOS source break {stem}', flush=True)
                for policy in ('over', 'out'):
                    frame = qualify_frame_policy(watchdog, binary, wasm, policy,
                                                  f'debug_source_{language}')
                    if args.out:
                        (args.out / f'{stem}-{policy}.log').write_bytes(frame.pop('log'))
                    else:
                        frame.pop('log')
                    frame_rows.append(frame)
                    print(f'PASS macOS source {stem} {policy}', flush=True)
        rust = work / 'rust-dwarf5.wasm'
        capped(watchdog, [str(args.rustc), '--target', 'wasm32-unknown-unknown',
            '-C', 'panic=abort', '-C', 'debuginfo=2', '-C', 'opt-level=0',
            str(fixtures / 'debug_source_rust.rs'), '-o', str(rust)])
        capped(watchdog, [str(args.wasm_tools), 'validate', str(rust)])
        row = qualify(watchdog, binary, rust, b'.rs:')
        if args.out:
            (args.out / 'rust-dwarf5.log').write_bytes(row.pop('log'))
        else:
            row.pop('log')
        rows.append(row)
        print('PASS macOS source rust-dwarf5', flush=True)
        source_break = qualify_source_break(watchdog, binary, rust, row['first_source'])
        if args.out:
            (args.out / 'rust-dwarf5-source-break.log').write_bytes(source_break.pop('log'))
        else:
            source_break.pop('log')
        source_break_rows.append(source_break)
        print('PASS macOS source break rust-dwarf5', flush=True)
        for policy in ('over', 'out'):
            frame = qualify_frame_policy(watchdog, binary, rust, policy,
                                          'debug_source_rust')
            if args.out:
                (args.out / f'rust-dwarf5-{policy}.log').write_bytes(frame.pop('log'))
            else:
                frame.pop('log')
            frame_rows.append(frame)
            print(f'PASS macOS source rust-dwarf5 {policy}', flush=True)
    if args.out:
        (args.out / 'summary.json').write_text(json.dumps({
            'product_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
            'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'rows': rows, 'source_break_rows': source_break_rows,
            'frame_policy_rows': frame_rows}, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
