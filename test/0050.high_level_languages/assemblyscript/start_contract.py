#!/usr/bin/env python3
"""Generate a small regression for instantiation start versus command entry.

Run this on SSH Linux in the existing test cgroup. The ordinary command and an
explicit local-function override must both observe exactly one initializer.
No compiler or third-party binary rewriter is required.
"""
import argparse
import json
from pathlib import Path
import resource
import subprocess


def leb(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def name(value):
    data = value.encode()
    return leb(len(data)) + data


def section(tag, data):
    return bytes([tag]) + leb(len(data)) + data


def module(kind):
    # type 0: fd_write, type 1: () -> (). Function indices: fd_write=0,
    # instantiation initializer=1, exported command=2. Local indices are 0/1.
    types = bytes.fromhex('02 60 04 7f 7f 7f 7f 01 7f 60 00 00')
    imports = leb(1) + name('wasi_snapshot_preview1') + name('fd_write') + bytes([0, 0])
    functions = bytes([2, 1, 1])
    memory = bytes([1, 0, 1])
    global_ = bytes.fromhex('01 7f 01 41 00 0b')
    start_index = 1
    export_function = 1 if kind == 'same-entry' else 2
    exports = (bytes([1 if kind == 'start-only' else 2])
               + name('memory') + bytes([2, 0]))
    if kind != 'start-only':
        exports += name('_start') + bytes([0, export_function])

    def print_at(offset, size):
        # iov at 0, nwritten at 8, fd=1. Positive constants stay below 64.
        return (bytes([0x41, 0, 0x41, offset, 0x36, 2, 0])
                + bytes([0x41, 4, 0x41, size, 0x36, 2, 0])
                + bytes.fromhex('41 01 41 00 41 01 41 08 10 00 1a'))

    initializer_text = b'lib\n' if kind == 'library' else b'init\n'
    init = bytes.fromhex('00 23 00 41 01 6a 24 00') + print_at(32, len(initializer_text)) + bytes([0x0b])
    # Trap if initialization was skipped or executed more than once.
    command = bytes.fromhex('00 23 00 41 01 47 04 40 00 0b') + print_at(40, 6) + bytes([0x0b])
    if kind == 'library':
        command = bytes.fromhex('00 00 0b')  # Library _start must never be called.
    code = leb(2) + leb(len(init)) + init + leb(len(command)) + command
    data = bytes([2])
    for offset, text in [(32, initializer_text), (40, b'entry\n')]:
        data += bytes([0, 0x41, offset, 0x0b]) + leb(len(text)) + text
    result = (b'\0asm\x01\0\0\0' + section(1, types) + section(2, imports)
              + section(3, functions) + section(5, memory) + section(6, global_)
              + section(7, exports) + section(8, leb(start_index))
              + section(10, code) + section(11, data))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for kind in ['command', 'same-entry', 'start-only', 'library']:
        (args.out / (kind + '.wasm')).write_bytes(module(kind))
    if args.uwvm is None:
        return
    mode = ['-Raot'] if args.ros else ['-Rcm', 'full', '-Rcc', 'jit']
    common = [str(args.uwvm), *mode, '-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rclog', 'err']
    records = []
    for kind, override, preload, expected in [
        ('command', [], False, 'init\nentry\n'),
        ('command', ['-Wstart', '1'], False, 'init\nentry\n'),
        ('command', ['-Wstart', '0'], False, 'init\n'),
        ('same-entry', [], False, 'init\n'),
        ('start-only', [], False, 'init\n'),
        ('command', [], True, 'lib\ninit\nentry\n'),
        ('command', ['-Wstart', '1'], True, 'lib\ninit\nentry\n'),
    ]:
        preloads = ['-Wpre', str(args.out / 'library.wasm'), 'L'] if preload else []
        argv = [*common, *override, *preloads, '--run', str(args.out / (kind + '.wasm'))]
        process = subprocess.run(argv, capture_output=True, text=True, timeout=60)
        row = {'kind': kind, 'override': override, 'preload': preload, 'argv': argv, 'exit': process.returncode,
               'stdout': process.stdout, 'stderr': process.stderr,
               'passed': process.returncode == 0 and process.stdout == expected}
        records.append(row)
        print(json.dumps(row), flush=True)
    (args.out / 'report.json').write_text(json.dumps(records, indent=2))
    raise SystemExit(0 if all(row['passed'] for row in records) else 1)


if __name__ == '__main__':
    main()
