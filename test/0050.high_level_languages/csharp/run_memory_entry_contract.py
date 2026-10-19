#!/usr/bin/env python3
"""Check linear-memory/data/growth survival across initializer and command entries."""
import argparse
import hashlib
import json
from pathlib import Path

from run_entry_edges import (FD_WRITE, VOID, admission, const, export, import_function,
                             ordinary, print_at, require_zero, wasm)


def memory_fixture(grow):
    initial, command_text = b'memory-init\n', b'memory-entry-ok\n'
    store32 = lambda address, value: const(address) + const(value) + bytes.fromhex('36 02 00')
    store64 = lambda address, value: const(address) + const(value, True) + bytes.fromhex('37 03 00')
    check32 = lambda address, value: require_zero(const(address) + bytes.fromhex('28 02 00') + const(value) + b'\x47')
    check64 = lambda address, value: require_zero(const(address) + bytes.fromhex('29 03 00') + const(value, True) + b'\x52')
    initializer = store32(128, 0x11223344) + store64(136, (1 << 53) - 1) + store32(65532, 0x12345678)
    command = check32(128, 0x11223344) + check64(136, (1 << 53) - 1) + check32(65532, 0x12345678)
    if grow:
        initializer += require_zero(const(1) + bytes.fromhex('40 00') + const(1) + b'\x47')
        initializer += store32(65536, 0x67876543)
        command += require_zero(bytes.fromhex('3f 00') + const(2) + b'\x47') + check32(65536, 0x67876543)
    initializer += print_at(0, 32, initial)
    command += print_at(0, 64, command_text)
    return wasm([FD_WRITE, VOID], [import_function('wasi_snapshot_preview1', 'fd_write', 0)],
                [1, 1], [initializer, command], [export('memory', 2, 0), export('_start', 0, 2)],
                memory=True, start=1, data=[(32, initial), (64, command_text), (128, bytes.fromhex('78 56 34 12'))])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--repository', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'], default='uwvm2')
    args = parser.parse_args()
    assert args.wasmtime or args.uwvm
    before = admission(args.expected_cgroup)
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': before, 'runs': [], 'fixtures': {}}
    for grow in (False, True):
        label = 'memory-grow' if grow else 'memory-data'
        target = args.out / (label + '.wasm')
        target.write_bytes(memory_fixture(grow))
        report['fixtures'][label] = hashlib.sha256(target.read_bytes()).hexdigest()
        runners = []
        if args.wasmtime:
            runners.append(('wasmtime', [str(args.wasmtime.resolve()), 'run', str(target)]))
        if args.uwvm:
            mode = ['-Rcm', 'full', '-Rcc', 'jit'] if args.repository == 'uwvm2' else ['-Raot']
            runners.append((args.repository, [str(args.uwvm.resolve()), *mode, '-Rct', '0',
                                             '-Rllvm-cache-path', 'disable', '--run', str(target)]))
        for runner, command in runners:
            row = ordinary(command, 'memory-init\nmemory-entry-ok\n', 0, args.out / (runner + '-' + label))
            report['runs'].append({'runner': runner, 'fixture': label, **row})
    report['admission_after'] = admission(args.expected_cgroup)
    assert report['admission_before']['memory_events'] == report['admission_after']['memory_events']
    report['passed'] = all(row['passed'] for row in report['runs'])
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
