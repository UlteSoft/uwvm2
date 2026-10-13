#!/usr/bin/env python3
"""Actual source breakpoint/module discovery; original Linux 64 GiB cgroup only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from run_debug_run_cli import Console, require, wasm_fixture


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasm', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    a = parser.parse_args()
    require(Path('/proc/self/cgroup').read_text() == '0::' + a.expected_cgroup + '\n', 'original cgroup')
    cg = Path('/sys/fs/cgroup') / a.expected_cgroup.lstrip('/')
    require((cg / 'memory.max').read_text().strip() == str(64 << 30), '64GiB')
    require((cg / 'memory.swap.max').read_text().strip() == '0', 'swap0')
    a.out.mkdir(exist_ok=False)
    rows = []
    prefix = [str(a.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-call-stack', 'instruction',
              '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']

    def session(label, path, preloads=()):
        argv = prefix[:]
        for file, name in preloads:
            argv += ['-Wpre', str(file), name]
        c = Console([*argv, '--run', str(path)])
        c.wait(lambda data: b'(uwvm-debug) ' in data, 0)
        return c

    def ask(c, label, command, marker=None):
        raw = c.command(command, marker.encode() if marker else None)
        # Store readable replies without terminal editing/color escape sequences.
        reply = re.sub(rb'\x1b\[[0-9;?]*[A-Za-z]', b'', raw).replace(b'\r', b'')
        rows.append({'session': label, 'command': command, 'reply': reply.decode(errors='replace')})
        return reply

    c = session('debug', a.wasm.resolve())
    try:
        listing = ask(c, 'debug', 'bw', 'Wasm modules')
        require(b'0 = ' + a.wasm.name.encode() in listing and b'source breakpoints available' in listing, repr(listing))
        ask(c, 'debug', 'bs 0', 'Usage: bs 0 FILE:LINE')
        ask(c, 'debug', 'bw 0', 'Usage: bw 0 FUNCTION BYTE_OFFSET')
        ask(c, 'debug', 'help bw', 'Set a Wasm breakpoint')
        ask(c, 'debug', 'help bs', 'Set a source breakpoint')
        ask(c, 'debug', 'help breaksource', 'Set a source breakpoint')
        ask(c, 'debug', 'info modules', 'Page 1/1')
        ask(c, 'debug', 'i mo 1', 'Page 1/1')
        for command in ['bw name.wasm', 'bw name.wasm foo.cc:20', 'bs name.wasm', 'bs name.wasm foo.cc:20',
                        'bs +0', 'bs -1', 'bs 1x', 'bs 184467440737095516160']:
            require(b'not a filename or module name' in ask(c, 'debug', command), command)
        for command in ['info modules 0', 'info modules -1', 'info modules +1', 'info modules 2x',
                        'info modules 184467440737095516160', 'info modules 1 extra']:
            require(b'error:' in ask(c, 'debug', command), command)
        ask(c, 'debug', 'info modules 2', 'out of range')
        ask(c, 'debug', 'bs 999', 'out of range')
        ask(c, 'debug', 'bs 18446744073709551615', 'out of range')
        wrong = ask(c, 'debug', 'bs 0 /does-not-match.cc:20', 'matches this FILE:LINE')
        require(b'lack -g' not in wrong, 'wrong path must not claim missing debug symbols')
        ask(c, 'debug', 'bs 0 1 0', 'Usage: bs MODULE_ID FILE:LINE')
        ask(c, 'debug', 'info breakpoints', 'no breakpoints')
        symbol = ask(c, 'debug', 'bw 0 leaf', 'breakpoint ')
        require(b'error:' not in symbol, repr(symbol))
        symbol_id = re.search(rb'breakpoint ([0-9]+)', symbol)[1].decode()
        ask(c, 'debug', 'r', 'stopped: breakpoint')
        ask(c, 'debug', 'delete ' + symbol_id)
        ask(c, 'debug', 'c', 'Program exited normally.')
        ask(c, 'debug', 'startw', 'stopped: selected participant step')
        ask(c, 'debug', 'info breakpoints', 'no breakpoints')
        line = next(i for i, text in enumerate(a.source.read_text().splitlines(), 1) if 'UX_LEAF_READY' in text)
        point = ask(c, 'debug', f'bs 0 {a.source.resolve()}:{line}', 'breakpoint ')
        require(b'error:' not in point, repr(point))
        identifier = re.search(rb'breakpoint ([0-9]+)', point)[1].decode()
        ask(c, 'debug', 'delete ' + identifier)
        point = ask(c, 'debug', f'breaksource 0 {a.source.resolve()}:{line}', 'breakpoint ')
        require(b'error:' not in point, repr(point))
        ask(c, 'debug', 'r', 'stopped: breakpoint')
        value = ask(c, 'debug', 'print counter')
        require(re.search(rb'counter = 2\b', value), repr(value))
        ask(c, 'debug', 'info modules', 'source breakpoints available')
        ask(c, 'debug', 'c', 'Program exited normally.')
        ask(c, 'debug', 'info modules', 'Wasm modules')
        c.finish()
    finally:
        (a.out / 'debug-transcript.txt').write_bytes(c.log)
        c.close()

    no_debug = a.out / '123.wasm'
    no_debug.write_bytes(wasm_fixture())
    c = session('no-debug', no_debug)
    try:
        ask(c, 'no-debug', 'bw', 'no source debug mapping')
        text = ask(c, 'no-debug', 'bs 0', 'without -g')
        require(b'123.wasm' in text and b'stripped' in text, repr(text))
        ask(c, 'no-debug', 'bs 0 missing.cc:1', 'has no usable source breakpoint locations')
        ask(c, 'no-debug', 'breaksource 0 missing.cc:1', 'has no usable source breakpoint locations')
        ask(c, 'no-debug', 'break-source 0 missing.cc:1', 'Undefined command')
        ask(c, 'no-debug', 'bs 123.wasm missing.cc:1', 'not a filename')
        ask(c, 'no-debug', 'bw 0 0 0', 'breakpoint ')
        ask(c, 'no-debug', 'r', 'stopped: breakpoint')
        c.finish()
    finally:
        (a.out / 'no-debug-transcript.txt').write_bytes(c.log)
        c.close()

    # Nine distinct loaded files have numeric display names unrelated to IDs.
    preloads = []
    for index in range(9):
        path = a.out / f'preload-{index}.wasm'
        path.write_bytes(wasm_fixture())
        preloads.append((path, str(800 + index)))
    c = session('numeric-names', no_debug, preloads)
    try:
        seen = {}
        numeric_names = {}
        for page in range(1, 4):
            text = ask(c, 'numeric-names', f'info modules {page}', f'Page {page}/3')
            ids = re.findall(rb'(?m)^  ([0-9]+) = ([^\n]+)', text)
            require(1 <= len(ids) <= 4, 'bounded page')
            for identifier, file in ids:
                number = int(identifier)
                require(number not in seen, 'no duplicate module ID across pages')
                seen[number] = file.decode()
            named_rows = re.findall(rb'(?m)^  ([0-9]+) = [^\n]+\n    name="([0-9]+)";', text)
            numeric_names.update({int(identifier): name.decode() for identifier, name in named_rows})
            require((b'Next: info modules ' in text) == (page < 3), 'page navigation')
        require(len(seen) == 10 and set(seen) == set(range(10)), seen)
        require(set(seen.values()) == {no_debug.name, *[x.name for x, _ in preloads]}, seen)
        require(set(numeric_names.values()) == {str(800 + index) for index in range(9)}, numeric_names)
        identifier, name = next(iter(numeric_names.items()))
        full = ask(c, 'numeric-names', f'bs {identifier}', f'Usage: bs {identifier} FILE:LINE')
        require(b'name="' + name.encode() + b'"' in full, 'ID selects the actual numerically named module')
        ask(c, 'numeric-names', 'bs ' + name, 'out of range')
        ask(c, 'numeric-names', 'bw ' + name, 'out of range')
        ask(c, 'numeric-names', 'bw preload-0.wasm', 'not a filename')
        ask(c, 'numeric-names', 'info modules 4', 'out of range')
        c.finish()
    finally:
        (a.out / 'numeric-names-transcript.txt').write_bytes(c.log)
        c.close()
    (a.out / 'actions.json').write_text(json.dumps(rows, indent=2))
    result = {'passed': True, 'scope': 'real bw=breakwasm, bs=breaksource; module IDs/files and numeric names; three-page module discovery; no-g diagnosis and wrong-path distinction',
              'command_count': len(rows), 'binary_sha256': hashlib.sha256(a.uwvm.read_bytes()).hexdigest(), 'terminal_restored': True,
              'numeric_named_preloads': 9, 'paged_actual_modules': 10}
    (a.out / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
