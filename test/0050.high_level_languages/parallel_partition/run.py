#!/usr/bin/env python3
"""Qualify eight real native object partitions, including Wasm3 references/EH."""
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


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def admission(expected):
    assert sys.platform == 'linux'
    assert Path('/proc/self/cgroup').read_text() == '0::' + expected + '\n'
    group = Path('/sys/fs/cgroup') / expected.lstrip('/')
    assert (group / 'memory.max').read_text().strip() == str(64 << 30)
    assert (group / 'memory.swap.max').read_text().strip() == '0'
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return (group / 'memory.events').read_text()


def wasm_contract(path):
    data, pos = path.read_bytes(), 8
    assert data[:8] == b'\0asm\x01\0\0\0'

    def leb():
        nonlocal pos
        value, shift = 0, 0
        while True:
            byte = data[pos]
            pos += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
            shift += 7

    exports, count = {}, 0
    while pos < len(data):
        tag = data[pos]
        pos += 1
        size = leb()
        end = pos + size
        if tag == 3:
            count = leb()
        elif tag == 7:
            for _ in range(leb()):
                length = leb()
                name = data[pos:pos + length].decode()
                pos += length
                kind = data[pos]
                pos += 1
                index = leb()
                if kind == 0:
                    exports[name] = index
        pos = end
    assert count == 26 and all('mark' + str(n) in exports for n in range(16))
    return {'defined_functions': count, 'exports': exports}


def partition_evidence(text):
    assert 'object-emit-fallback' not in text and 'object-emit-skip' not in text, text[-5000:]
    assert 'object-cache-hit' not in text, 'cold uncached partition proof required'
    rows = re.findall(r'object-emit-end[^\r\n]*functions=([0-9]+) objects=([0-9]+)[^\r\n]*', text)
    assert rows and all(int(functions) >= 16 and int(objects) == 8 for functions, objects in rows), text[-5000:]
    lines = [line for line in text.splitlines() if 'object-emit-end' in line]
    return [{'defined_functions': int(functions), 'objects': int(objects), 'log_line': line}
            for (functions, objects), line in zip(rows, lines, strict=True)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path)
    parser.add_argument('--runtime-tests', type=Path)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--repository', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    parser.add_argument('--reference-only', action='store_true')
    args = parser.parse_args()
    wasm_tools = args.wasm_tools or args.arena / 'tools/wasm-tools-1.262.0/wasm-tools'
    before = admission(args.expected_cgroup)
    args.out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent
    expected = (source / 'expected.stdout').read_text()
    wasm = args.out / 'parallel-partition.wasm'
    build = [str(wasm_tools), 'parse', str(source / 'fixture.wat'), '-o', str(wasm)]
    built = subprocess.run(build, capture_output=True, text=True, timeout=60)
    (args.out / 'build-stderr.log').write_text(built.stderr)
    assert built.returncode == 0, built.stderr
    contract = wasm_contract(wasm)
    report = {'passed': False, 'scope': 'real eight-object code emission and native execution, ordinary and managed Wasm debugger',
              'memory_events_before': before, 'build_argv': build, 'source_sha256': digest(source / 'fixture.wat'),
              'wasm_sha256': digest(wasm), 'contract': contract, 'tools': {}, 'runs': []}
    for label, tool in [('wasmtime', args.wasmtime), ('wasm-tools', wasm_tools)]:
        report['tools'][label] = {'path': str(tool), 'sha256': digest(tool),
                                  'version': subprocess.check_output([str(tool), '--version'], text=True).strip()}
    assert report['tools']['wasmtime']['version'].startswith('wasmtime 42.0.1 ')
    assert report['tools']['wasm-tools']['version'].startswith('wasm-tools 1.262.0 (')

    def store():
        (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')

    reference_cmd = [str(args.wasmtime), 'run', '-W', 'exceptions=y', '-W', 'function-references=y', str(wasm)]
    ref = subprocess.run(reference_cmd, capture_output=True, text=True, timeout=60)
    reference = {'runner': 'wasmtime', 'mode': 'ordinary', 'argv': reference_cmd,
                 'exit': ref.returncode, 'stdout': ref.stdout, 'stderr': ref.stderr,
                 'passed': ref.returncode == 0 and ref.stdout == expected}
    report['runs'].append(reference)
    store()
    assert reference['passed'], reference
    if not args.reference_only:
        sys.path.insert(0, str(args.runtime_tests or args.arena / 'uwvm2-ros/test/0017.runtime'))
        from run_debug_run_cli import Console
        os.environ.update(TERM='dumb', NO_COLOR='1')
        for repo in args.repository or ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            binary = args.arena / 'products' / repo / 'uwvm'
            identity = json.loads((binary.parent / 'identity.json').read_text())
            assert identity['binary_sha256'] == digest(binary)
            mode = ['-Rcm', 'full', '-Rcc', 'jit'] if repo == 'uwvm2' else ['-Raot']
            base = [str(binary), *mode, '-Rct', '7', '-Rllvm-cache-path', 'disable', '-Rclog', 'err',
                    '-WFE-exceptions', '-WFE-function-references', '-WFE-reference-types']
            for debug in [False, True]:
                folder = args.out / repo / ('debug' if debug else 'ordinary')
                folder.mkdir(parents=True)
                command = [*base, *(['-Rdbg'] if debug else []), '--run', str(wasm)]
                row = {'runner': repo, 'mode': 'debug' if debug else 'ordinary', 'argv': command,
                       'product': identity, 'passed': False}
                report['runs'].append(row)
                store()
                if not debug:
                    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
                    (folder / 'stdout.log').write_text(result.stdout)
                    (folder / 'stderr.log').write_text(result.stderr)
                    row.update(exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
                    store()
                    row['objects'] = partition_evidence(result.stderr)
                    assert result.returncode == 0 and result.stdout == expected, row
                else:
                    row['actions'] = []
                    console = Console(command)
                    def action(command):
                        at, started = len(console.log), time.monotonic()
                        os.write(console.fd, command.encode() + b'\n')
                        reply = console.wait(lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), at, 120)
                        row['actions'].append({'command': command, 'seconds': time.monotonic() - started,
                                               'reply': reply.decode(errors='replace')})
                        store()
                        return reply
                    try:
                        console.wait(lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), 0, 120)
                        assert Path(f'/proc/{console.pid}/cgroup').read_text() == Path('/proc/self/cgroup').read_text()
                        entry = action('startw')
                        found = re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=0\b', entry)
                        assert found and int(found[2]) == contract['exports']['_start'], entry
                        thread = int(found[1])
                        status = action('status')
                        old_stop = int(re.search(rb'stop-id (\d+)', status)[1])
                        stepped = action(f'step wasm {thread} into')
                        assert b'stopped:' in stepped and b'error:' not in stepped, stepped
                        status = action('status')
                        assert int(re.search(rb'stop-id (\d+)', status)[1]) != old_stop
                        thrower = contract['exports']['thrower']
                        bp = action(f'bw 0 {thrower} 0')
                        assert re.search(rb'[Bb]reakpoint \d+', bp), bp
                        hit = action('continue')
                        assert b'stopped: breakpoint' in hit and f'function={thrower} byte-offset=0'.encode() in hit, hit
                        trace = action(f'bt {thread}')
                        assert f'function={thrower} byte-offset=0'.encode() in trace, trace
                        assert f"function={contract['exports']['catcher']} ".encode() in trace, trace
                        locals_reply = action(f'locals wasm {thread} 0 0 8')
                        assert b'error:' not in locals_reply, locals_reply
                        action('disable')
                        done = action('continue')
                        assert b'Program exited normally.' in done, done
                        assert expected.strip() in done.decode(errors='replace').replace('\r', '').splitlines(), done
                        console.finish()
                        row['normal_retirement'] = True
                        row['exit'] = 0
                        row['objects'] = partition_evidence(console.log.decode(errors='replace'))
                    finally:
                        (folder / 'console.log').write_bytes(console.log)
                        console.close()
                        store()
                assert digest(binary) == identity['binary_sha256'] and digest(wasm) == report['wasm_sha256']
                row['passed'] = True
                store()
                print(json.dumps({'runner': repo, 'mode': row['mode'], 'passed': True, 'objects': row['objects']}), flush=True)
    report['memory_events_after'] = admission(args.expected_cgroup)
    before_events = dict(line.split() for line in before.splitlines())
    after_events = dict(line.split() for line in report['memory_events_after'].splitlines())
    assert all(before_events[k] == after_events[k] for k in ['oom', 'oom_kill', 'oom_group_kill'])
    report['passed'] = all(row['passed'] for row in report['runs'])
    store()
    print(json.dumps({'passed': report['passed'], 'runs': len(report['runs']), 'wasm_sha256': report['wasm_sha256']}), flush=True)


if __name__ == '__main__':
    main()
