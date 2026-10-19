#!/usr/bin/env python3
"""Black-box WASIp1 exact-count pointer-table canary and memory-end checks."""
import argparse
import hashlib
import json
from pathlib import Path

from run_entry_edges import (FD_WRITE, VOID, admission, call, const, export, function_type,
                             import_function, ordinary, print_at, require_zero, wasm)


def fixture(kind, count, end):
    table = 65536 - count * 4 if end else 128
    guard = table - 4 if end else table + count * 4
    load32 = lambda address: const(address) + bytes.fromhex('28 02 00')
    store32 = lambda address, value: const(address) + const(value) + bytes.fromhex('36 02 00')
    body = require_zero(const(16) + const(20) + call(1))
    body += require_zero(load32(16) + const(count) + b'\x47')
    body += store32(guard, 0x11223344)
    # The string-buffer tail is independent of the pointer table and remains
    # caller-owned too. Array entries point at separately NUL-terminated strings.
    body += const(4096) + load32(20) + b'\x6a\x24\x00'
    body += b'\x23\x00' + const(0xa5) + bytes.fromhex('3a 00 00')
    body += require_zero(const(table) + const(4096) + call(2))
    body += require_zero(load32(guard) + const(0x11223344) + b'\x47')
    body += require_zero(bytes.fromhex('23 00 2d 00 00') + const(0xa5) + b'\x47')
    text = b'wasi-vector-ok\n'
    body += print_at(0, 32, text)
    return wasm([FD_WRITE, function_type([0x7f, 0x7f], [0x7f]), VOID],
                [import_function('wasi_snapshot_preview1', 'fd_write', 0),
                 import_function('wasi_snapshot_preview1', kind + '_sizes_get', 1),
                 import_function('wasi_snapshot_preview1', kind + '_get', 1)],
                [2], [body], [export('memory', 2, 0), export('_start', 0, 3)],
                memory=True, globals_=[bytes.fromhex('7f 01 41 00 0b')], data=[(32, text)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--repository', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'], default='uwvm2')
    args = parser.parse_args()
    assert args.uwvm or args.wasmtime
    before = admission(args.expected_cgroup)
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': before, 'runs': [], 'fixtures': {},
              'wasmtime_skip': 'CLI cannot configure argc=0; zero-argument count is checked by UWVM and native units'}
    values = {0: [], 1: ['prog'], 3: ['prog', '世界', '']}
    for kind in ('args', 'environ'):
        for count in (0, 1, 3):
            for end in (False, True):
                label = f'{kind}-{count}-' + ('end' if end else 'canary')
                target = args.out / (label + '.wasm')
                target.write_bytes(fixture(kind, count, end))
                report['fixtures'][label] = hashlib.sha256(target.read_bytes()).hexdigest()
                runners = []
                if args.uwvm:
                    mode = ['-Rcm', 'full', '-Rcc', 'jit'] if args.repository == 'uwvm2' else ['-Raot']
                    options = ['--wasip1-global-noinherit-system-environment']
                    if kind == 'args':
                        options += ['--wasip1-global-force-args', str(count), *values[count]]
                    else:
                        for index in range(count):
                            options += ['--wasip1-global-add-or-replace-environment', 'K' + str(index), values[count][index]]
                    runners.append((args.repository, [str(args.uwvm.resolve()), *mode, '-Rct', '0',
                                                      '-Rllvm-cache-path', 'disable', *options, '--run', str(target)]))
                if args.wasmtime and not (kind == 'args' and count == 0):
                    options, tail = [], []
                    if kind == 'args':
                        options = ['--argv0', 'prog']
                        tail = values[count][1:]
                    else:
                        for index in range(count):
                            options += ['--env', 'K' + str(index) + '=' + values[count][index]]
                    runners.append(('wasmtime', [str(args.wasmtime.resolve()), 'run', *options, str(target), *tail]))
                for runner, command in runners:
                    row = ordinary(command, 'wasi-vector-ok\n', 0, args.out / (runner + '-' + label))
                    report['runs'].append({'runner': runner, 'fixture': label, **row})
                    print(json.dumps({'runner': runner, 'fixture': label, 'passed': row['passed'], 'exit': row.get('exit')}), flush=True)
    report['admission_after'] = admission(args.expected_cgroup)
    assert report['admission_before']['memory_events'] == report['admission_after']['memory_events']
    report['passed'] = all(row['passed'] for row in report['runs'])
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
