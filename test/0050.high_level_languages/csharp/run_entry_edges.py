#!/usr/bin/env python3
"""Exercise start/command ABI, import aliases and managed init-time proc_exit.

Generate ordinary core Wasm directly, then run in the existing SSH Linux cgroup.
No binary rewriting or additional compiler installation is required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys
import time


def uleb(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def sleb(value):
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        done = (value == 0 and byte & 64 == 0) or (value == -1 and byte & 64 != 0)
        result.append(byte if done else byte | 128)
        if done:
            return bytes(result)


def vector(items):
    return uleb(len(items)) + b''.join(items)


def name(text):
    data = text.encode()
    return uleb(len(data)) + data


def section(tag, contents):
    return bytes([tag]) + uleb(len(contents)) + contents


def function_type(parameters=(), results=()):
    return b'\x60' + vector([bytes([item]) for item in parameters]) + vector([bytes([item]) for item in results])


FD_WRITE = function_type([0x7f] * 4, [0x7f])
VOID = function_type()


def import_function(module, field, type_index):
    return name(module) + name(field) + b'\x00' + uleb(type_index)


def import_global(module, field):
    return name(module) + name(field) + bytes([3, 0x7f, 1])


def export(field, kind, index):
    return name(field) + bytes([kind]) + uleb(index)


def const(value, wide=False):
    return bytes([0x42 if wide else 0x41]) + sleb(value)


def call(index):
    return b'\x10' + uleb(index)


def print_at(fd_write_index, offset, text):
    # One WASI ciovec at 0; nwritten at 8. Strings live elsewhere in memory.
    return (const(0) + const(offset) + bytes.fromhex('36 02 00')
            + const(4) + const(len(text)) + bytes.fromhex('36 02 00')
            + const(1) + const(0) + const(1) + const(8) + call(fd_write_index) + b'\x1a')


def require_zero(value):
    return value + bytes.fromhex('04 40 00 0b')


def wasm(types, imports, function_types, bodies, exports, *, memory=False, globals_=(), start=None, data=()):
    result = b'\0asm\x01\0\0\0' + section(1, vector(types))
    if imports:
        result += section(2, vector(imports))
    if function_types:
        result += section(3, vector([uleb(item) for item in function_types]))
    if memory:
        result += section(5, bytes.fromhex('01 00 01'))
    if globals_:
        result += section(6, vector(globals_))
    if exports:
        result += section(7, vector(exports))
    if start is not None:
        result += section(8, uleb(start))
    if bodies:
        encoded = [b'\x00' + body + b'\x0b' for body in bodies]
        result += section(10, vector([uleb(len(body)) + body for body in encoded]))
    if data:
        result += section(11, vector([b'\x00' + const(offset) + b'\x0b' + uleb(len(text)) + text for offset, text in data]))
    return result


def fixtures():
    init_text, abi_text = b'abi-init\n', b'abi-ok\n'
    wide = (1 << 40) + 5
    initializer = bytes.fromhex('23 00 41 01 6a 24 00') + print_at(0, 32, init_text)
    typed = (require_zero(bytes.fromhex('23 00 41 01 47'))
             + require_zero(bytes.fromhex('20 00') + const(7) + b'\x47')
             + require_zero(bytes.fromhex('20 01') + const(wide, True) + b'\x52')
             + print_at(0, 64, abi_text) + const(wide + 7, True))
    abi = wasm([FD_WRITE, VOID, function_type([0x7f, 0x7e], [0x7e])],
               [import_function('wasi_snapshot_preview1', 'fd_write', 0)], [1, 2],
               [initializer, typed], [export('memory', 2, 0), export('typed', 0, 2)],
               memory=True, globals_=[bytes.fromhex('7f 01 41 00 0b')], start=1,
               data=[(32, init_text), (64, abi_text)])

    alias_text, entry_text = b'alias-init\n', b'alias-entry\n'
    provider = wasm([FD_WRITE, VOID], [import_function('wasi_snapshot_preview1', 'fd_write', 0)], [1],
                    [bytes.fromhex('23 00 41 01 6a 24 00') + print_at(0, 32, alias_text)],
                    [export('memory', 2, 0), export('initialize', 0, 1), export('calls', 3, 0)],
                    memory=True, globals_=[bytes.fromhex('7f 01 41 00 0b')], data=[(32, alias_text)])
    forwarder = wasm([VOID], [import_function('P', 'initialize', 0), import_global('P', 'calls')], [], [],
                     [export('initialize', 0, 0), export('calls', 3, 0)])
    command = require_zero(bytes.fromhex('23 00 41 01 47')) + print_at(1, 32, entry_text)
    alias_modules = {}
    for same in (False, True):
        alias_modules['alias-same.wasm' if same else 'alias-command.wasm'] = wasm(
            [VOID, FD_WRITE],
            [import_function('M', 'initialize', 0), import_function('wasi_snapshot_preview1', 'fd_write', 1), import_global('M', 'calls')],
            [0], [command],
            [export('memory', 2, 0), export('_start', 0, 0 if same else 2), export('check', 0, 2)],
            memory=True, start=0, data=[(32, entry_text)])

    exit_text, forbidden = b'exit-init\n', b'BAD-COMMAND\n'
    exiting = wasm([FD_WRITE, function_type([0x7f]), VOID],
                   [import_function('wasi_snapshot_preview1', 'fd_write', 0), import_function('wasi_snapshot_preview1', 'proc_exit', 1)],
                   [2, 2], [print_at(0, 32, exit_text) + const(17) + call(1) + b'\x00',
                            print_at(0, 64, forbidden)],
                   [export('memory', 2, 0), export('_start', 0, 3)],
                   memory=True, start=2, data=[(32, exit_text), (64, forbidden)])
    return {'typed-abi.wasm': abi, 'provider.wasm': provider, 'forwarder.wasm': forwarder,
            'exit-during-init.wasm': exiting, **alias_modules}


def admission(expected):
    assert sys.platform == 'linux', 'Execute on SSH Linux only'
    membership = Path('/proc/self/cgroup').read_text()
    assert membership == '0::' + expected + '\n', 'Use the existing cgroup'
    group = Path('/sys/fs/cgroup') / expected.lstrip('/')
    assert (group / 'memory.max').read_text().strip() == str(64 << 30), '64 GiB memory.max required'
    assert (group / 'memory.swap.max').read_text().strip() == '0', 'Existing swap limit changed'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return {'membership': membership.strip(), 'memory_events': (group / 'memory.events').read_text()}


def ordinary(argv, expected_stdout, expected_exit, folder):
    row = {'argv': argv, 'passed': False}
    start = time.monotonic()
    try:
        process = subprocess.run(argv, capture_output=True, text=True, timeout=60)
        row.update({'exit': process.returncode, 'stdout': process.stdout, 'stderr': process.stderr,
                    'seconds': time.monotonic() - start,
                    'passed': process.returncode == expected_exit and process.stdout == expected_stdout
                              and 'BAD-COMMAND' not in process.stdout})
    except Exception as error:
        row['error'] = repr(error)
    folder.mkdir()
    (folder / 'result.json').write_text(json.dumps(row, indent=2))
    return row


def managed_exit(Console, argv, folder):
    row = {'argv': argv, 'passed': False, 'actions': []}
    console = None
    folder.mkdir()
    try:
        console = Console(argv)
        console.wait(lambda data: b'(uwvm-debug) ' in data, 0, 60)
        assert Path(f'/proc/{console.pid}/cgroup').read_text() == Path('/proc/self/cgroup').read_text()
        assert os.sched_getaffinity(console.pid) == os.sched_getaffinity(0)

        def ask(command):
            reply = console.command(command)
            reply = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', reply)
            row['actions'].append({'command': command, 'reply': reply.decode(errors='replace')})
            return reply

        # startw stops at the actual initializer entry, not at the selected command.
        stopped = ask('startw')
        assert re.search(rb'thread \d+ module=0 function=2 byte-offset=0', stopped), stopped
        reply = ask('continue')
        for _ in range(8):
            if b'Program exited' in reply:
                break
            reply = ask('wait')
        expected = b'Program exited with code 021.'
        assert expected in reply, reply
        assert expected in ask('status'), 'Guest exit code 17 must remain observable'
        assert b'exit-init' in console.log and b'BAD-COMMAND' not in console.log
        console.finish()  # Normal quit; requires exit0, terminal restore and bounded real waitpid.
        assert b'error: managed shutdown' not in console.log
        assert b'physical EOF:' not in console.log and b'cleanup incomplete' not in console.log
        row.update(passed=True, guest_exit_code=17, normal_quit_reaped=True,
                   terminal_restored=True, source_language_claim=False)
    except Exception as error:
        row['error'] = repr(error)
    finally:
        if console is not None:
            (folder / 'console.log').write_bytes(console.log)
            console.close()
        (folder / 'result.json').write_text(json.dumps(row, indent=2))
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--repository', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'], default='uwvm2')
    parser.add_argument('--runtime-tests', type=Path, default=Path(__file__).resolve().parents[3] / 'test/0017.runtime')
    args = parser.parse_args()
    if not args.uwvm and not args.wasmtime:
        parser.error('Select at least one actual runtime')
    before = admission(args.expected_cgroup)
    os.environ.update(TERM='dumb', NO_COLOR='1')
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': before, 'fixtures': {}, 'runs': [],
              'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'runners': {}}
    for label, binary in [('wasmtime', args.wasmtime), ('uwvm', args.uwvm)]:
        if binary:
            report['runners'][label] = {'path': str(binary.resolve()),
                                        'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
    for filename, contents in fixtures().items():
        (args.out / filename).write_bytes(contents)
        report['fixtures'][filename] = hashlib.sha256(contents).hexdigest()

    if args.wasmtime:
        base = [str(args.wasmtime.resolve()), 'run']
        preloads = ['--preload', 'P=' + str(args.out / 'provider.wasm'), '--preload', 'M=' + str(args.out / 'forwarder.wasm')]
        commands = [
            ('wasmtime-typed-abi', [*base, '--invoke', 'typed', str(args.out / 'typed-abi.wasm'), '7', str((1 << 40) + 5)], 'abi-init\nabi-ok\n1099511627788\n', 0),
            ('wasmtime-alias-command', [*base, *preloads, str(args.out / 'alias-command.wasm')], 'alias-init\nalias-entry\n', 0),
            # Explicit check observes exactly one instantiation start. Wasmtime's
            # automatic _start call would intentionally call a same-index export twice.
            ('wasmtime-alias-same', [*base, *preloads, '--invoke', 'check', str(args.out / 'alias-same.wasm')], 'alias-init\nalias-entry\n', 0),
            ('wasmtime-exit-init', [*base, str(args.out / 'exit-during-init.wasm')], 'exit-init\n', 17),
        ]
        for label, argv, stdout, code in commands:
            row = ordinary(argv, stdout, code, args.out / label)
            report['runs'].append({'case': label, **row})

    if args.uwvm:
        mode = ['-Rcm', 'full', '-Rcc', 'jit'] if args.repository == 'uwvm2' else ['-Raot']
        base = [str(args.uwvm.resolve()), *mode, '-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rclog', 'err']
        preloads = ['-Wpre', str(args.out / 'provider.wasm'), 'P', '-Wpre', str(args.out / 'forwarder.wasm'), 'M']
        commands = [
            ('uwvm-typed-abi', [*base, '-Wstart', '1', '7', str((1 << 40) + 5), '--run', str(args.out / 'typed-abi.wasm')], 'abi-init\nabi-ok\n', 0),
            ('uwvm-alias-command', [*base, *preloads, '--run', str(args.out / 'alias-command.wasm')], 'alias-init\nalias-entry\n', 0),
            ('uwvm-alias-same', [*base, *preloads, '--run', str(args.out / 'alias-same.wasm')], 'alias-init\n', 0),
            ('uwvm-exit-init', [*base, '--run', str(args.out / 'exit-during-init.wasm')], 'exit-init\n', 17),
        ]
        for label, argv, stdout, code in commands:
            row = ordinary(argv, stdout, code, args.out / label)
            report['runs'].append({'case': label, **row})
        sys.path.insert(0, str(args.runtime_tests.resolve()))
        from run_debug_run_cli import Console
        argv = [str(args.uwvm.resolve()), '-Rdbg', *mode, '-Rct', '0', '-Rllvm-cache-path', 'disable',
                '--run', str(args.out / 'exit-during-init.wasm')]
        report['runs'].append({'case': 'uwvm-managed-exit-init', **managed_exit(Console, argv, args.out / 'uwvm-managed-exit-init')})

    report['admission_after'] = admission(args.expected_cgroup)
    report['passed'] = bool(report['runs']) and all(row['passed'] for row in report['runs'])
    assert report['admission_before']['memory_events'] == report['admission_after']['memory_events'], 'cgroup memory event changed'
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    for row in report['runs']:
        print(json.dumps({'case': row['case'], 'passed': row['passed'], 'exit': row.get('exit'), 'error': row.get('error')}), flush=True)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
