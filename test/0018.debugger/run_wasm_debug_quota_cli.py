#!/usr/bin/env python3
"""Exercise actual paused LLVM-full GC quota failures in the Linux test cgroup."""
import argparse
import json
import re
import resource
import subprocess
from pathlib import Path
import run_wasm_operand_preview_cli as preview
from run_wasm_uncaught_import_cli import request, sha, status
from run_wasm_uncaught_cli import stopped
from wasm_debug_quota_cases import quota_examples

QUOTA = b'bounded Wasm state query exceeded its quota'
STALE = b'stop or code generation is stale'


def inspect(console, case, before, baseline):
    thread = before['thread']; stop = before['stop_id']; size = case['size']
    pages = 0
    def get(command):
        packet = console.send(command)
        assert len(packet) <= 32768 + len(b'(uwvm-debug) '), (command, len(packet))
        if b'wasm-stop ' in packet:
            assert f'wasm-stop {stop}\n'.encode() in packet, packet
        return packet
    def page(command, value=None):
        nonlocal pages
        packet = get(command); pages += 1
        assert b'Wasm members object=' in packet and b'unavailable:' not in packet, packet
        if value is not None:
            assert f'  1 i32 = {value} mutable\n'.encode() in packet, packet
        return packet
    def label(command, depth):
        packet = get(command)
        match = re.search(rb'^Wasm path session=([1-9][0-9]*) handle=([1-9][0-9]*) depth=([0-9]+) view=3 protocol=1$', packet, re.M)
        assert match and int(match[3]) == depth, packet
        return int(match[1]), int(match[2])
    def reject(command, diagnostic):
        packet = get(command)
        assert b'Wasm path unavailable:' in packet and diagnostic in packet, packet
        assert b'Wasm path session=' not in packet and b'object #' not in packet, packet
        return packet
    prefix = get(f'operands {thread} 0 0 1')
    assert b'operand 0 i64 = -991\n' in prefix and b'object #' not in prefix, prefix
    marker = case.get('marker', 1)
    scalar = get(f'locals wasm {thread} 0 {marker} 1')
    assert f'local {marker} i32 = {size if case["kind"] in ("array", "chain") else case.get("initial_marker", 0)}\n'.encode() in scalar, scalar
    graph = get(f'table {thread} 0 0 0 64' if case['kind']=='reply' else f'locals wasm {thread} 0 0 1')
    if case['kind'] == 'array':
        assert b'Wasm state truncated:' in graph and b'members=513 first=0 next=256 more=yes' in graph, graph
        members = re.findall(rb'^  ([0-9]+) i32 = ([0-9]+) mutable$', graph, re.M)
        assert [(int(i), int(v)) for i, v in members] == [(i, 1000+i) for i in range(256)], graph
        recovered = []
        for first in range(0, size, 64):
            packet = page(f'members locals {thread} 0 0 0 0 {first} 64')
            rows = [(int(i), int(v)) for i, v in re.findall(rb'^  ([0-9]+) i32 = ([0-9]+) mutable$', packet, re.M)]
            assert rows == [(i, 1000+i) for i in range(first, min(size, first+64))], packet
            recovered += rows
        assert recovered == [(i, 1000+i) for i in range(size)]
        end = page(f'members locals {thread} 0 0 0 0 {size} 1')
        assert b'count=1' in end and not re.search(rb'^  [0-9]+ i32 =', end, re.M), end
        invalid = request(console, f'members locals {thread} 0 0 0 0 0 65')
        assert invalid.startswith(b'error:'), invalid
    elif case['kind'] == 'chain':
        if size == 128:
            assert len(re.findall(rb'^object #[0-9]+ struct ', graph, re.M)) == 128, graph
            values = [int(v) for v in re.findall(rb'^  1 i32 = ([0-9]+) mutable$', graph, re.M)]
            assert values == list(range(1127, 999, -1)), graph
        else:
            assert QUOTA in graph and b'object #' not in graph and b'local 0 ' not in graph, graph
        page(f'members locals {thread} 0 0 0 0 0 2', 1000+size-1)
        session, handle = label(f'path create locals {thread} 0 0 0 0', 0)
        depth = 0
        while depth < size-1:
            edges = min(16, size-1-depth); depth += edges
            session, handle = label(f'path extend {session} {handle}' + ' 0'*edges, depth)
        page(f'path members {session} {handle} 0 2', 1000)
        again = get(f'locals wasm {thread} 0 0 1')
        assert (QUOTA in again) == (size == 129), again
    elif case['kind'] == 'reply':
        trailer = re.search(rb'^Wasm state truncated: rows=64 objects=([0-9]+)$', graph, re.M)
        assert trailer and 64 <= int(trailer[1]) < 128, graph
        assert 30000 < len(graph) <= 32768 + len(b'(uwvm-debug) '), len(graph)
        rows = re.findall(rb'^table 0 element ([0-9]+) \(ref null type-index=1000 module=0\) = struct #([0-9]+)$', graph, re.M)
        assert [(int(i), int(v)) for i,v in rows] == [(i, i+1) for i in range(64)], graph
        blocks = re.findall(rb'^object #([0-9]+) struct module=0 type=1000 members=4 first=0 next=(0|4) more=(yes|no)\n((?:  .*\n)+)', graph, re.M)
        assert len(blocks) == int(trailer[1]), graph
        for ordinal, (obj, next_, more, body) in enumerate(blocks, 1):
            assert int(obj)==ordinal
            if ordinal<=64:
                assert (next_, more)==(b'4', b'no') and body == b''.join(
                    f'  {i} (ref null type-index=1000 module=0) = struct #{ordinal+64} mutable\n'.encode() for i in range(4)), body
            else:
                assert (next_, more)==(b'0', b'yes') and body==b'  member page; continue with original root/path and next\n', body
        root = page(f'members table {thread} 0 0 0 63 0 4')
        assert root.count(b'= struct #2 mutable\n') == 4, root
        child = page(f'members table {thread} 0 0 0 63 0 4 0')
        assert b'  0 (ref null type-index=1000 module=0) = struct #1 mutable\n' in child, child
        assert b'  1 (ref null type-index=1000 module=0) = struct #2 mutable\n' in child, child
        assert child.count(b'= struct #2 mutable\n')==3, child
    else:
        assert len(re.findall(rb'^object #[0-9]+ struct ', graph, re.M)) == 1, graph
        assert b'  0 (ref null type-index=0 module=0) = struct #1 mutable\n' in graph, graph
        handles = [label(f'path create locals {thread} 0 0 0 0', 0) for _ in range(128)]
        reject(f'path create locals {thread} 0 0 0 0', QUOTA)
        for session, handle in (handles[0], handles[-1]):
            page(f'path members {session} {handle} 0 2', 31)
        session, previous = handles[0]
        session, handle = label(f'path extend {session} {previous}' + ' 0'*16, 16)
        reject(f'path members {session} {previous} 0 1', STALE)
        page(f'path members {session} {handle} 0 2', 31)
        assert b'Wasm paths cleared' in get('path clear')
        reject(f'path members {session} {handle} 0 1', STALE)
        session, handle = label(f'path create locals {thread} 0 0 0 0', 0)
        assert handle > previous
        for depth in range(16, 4097, 16):
            session, handle = label(f'path extend {session} {handle}' + ' 0'*16, depth)
        page(f'path members {session} {handle} 0 2', 31)
        packet = reject(f'path extend {session} {handle} 0', STALE if baseline else QUOTA)
        page(f'path members {session} {handle} 0 2', 31)
        for command in (f'set wasm member {thread} handle {session} {handle} path 0 at 1 bits i32 00000020',
                        f'set wasm global 0 1 {thread} from handle {session} {handle} path 0'):
            refused = get(command)
            assert b'Wasm mutation v=3 status=' in refused and (STALE if baseline else QUOTA) in refused, refused
            assert b' applied=0 ' in refused, refused
            page(f'path members {session} {handle} 0 2', 31)
        sink = get(f'globals {thread} 0 1 1')
        assert b'global 1 (ref null type-index=0 module=0) = null\n' in sink, sink
        reject(f'path members {session+1} {handle} 0 1', STALE)
        page(f'path members {session} {handle} 0 2', 31)
        get(f'locals wasm {thread} 0 0 1')
        if not baseline:
            assert b'Wasm paths cleared' in get('path clear')
            full = []
            for _ in range(8):
                session, handle = label(f'path create locals {thread} 0 0 0 0', 0)
                for depth in range(16, 4097, 16):
                    session, handle = label(f'path extend {session} {handle}' + ' 0'*16, depth)
                full.append((session, handle))
            for session, handle in (full[0], full[-1]):
                page(f'path members {session} {handle} 0 2', 31)
            # A zero-edge handle still fits when 8*4096 retained indices fill
            # the aggregate quota. Its first retained edge must be refused.
            session, empty = label(f'path create locals {thread} 0 0 0 0', 0)
            reject(f'path extend {session} {empty} 0', QUOTA)
            page(f'path members {session} {empty} 0 2', 31)
            session, handle = full[0]
            page(f'path members {session} {handle} 0 2', 31)
            assert b'Wasm paths cleared' in get('path clear')
            reject(f'path members {session} {handle} 0 1', STALE)
            session, fresh = label(f'path create locals {thread} 0 0 0 0', 0)
            assert fresh > empty
            session, handle = label(f'path extend {session} {fresh} 0', 1)
            page(f'path members {session} {handle} 0 2', 31)
    after, _ = status(console)
    assert after == before, ('read/refusal changed the actual stop', before, after)
    return dict(member_pages=pages, exact_stop_preserved=True,
        aggregate_path_indices=32768 if case['kind']=='cycle' and not baseline else None,
        quota_diagnostic=('old incorrect stale diagnostic' if baseline and case['kind']=='cycle' else
            'explicit quota' if case['kind']=='cycle' or case['name']=='chain-129' else 'bounded preview or exact boundary'),
        last_path=None if case['kind'] in ('array', 'reply') else [session, handle])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'binary', 'wasm-tools', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--jit-policy', choices=('default', 'max'), required=True)
    parser.add_argument('--call-stack-policy', action='append', choices=('instruction', 'unwind'))
    parser.add_argument('--runner-prefix-json', type=Path)
    parser.add_argument('--case', action='append')
    parser.add_argument('--old-path-diagnostic', action='store_true')
    args = parser.parse_args(); args.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(args.source_root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    runner = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(x, str) and x for x in runner)
    with args.binary.open('rb') as image:
        header = image.read(20)
    assert header[:7] == b'\x7fELF\x02\x01\x01' and int.from_bytes(header[18:20], 'little') in (62, 183, 243)
    cases = [c for c in quota_examples() if args.case is None or c['name'] in args.case]
    assert cases and (args.case is None or {c['name'] for c in cases} == set(args.case))
    inputs = dict(binary_sha256=sha(args.binary), harness_sha256=sha(__file__),
        cases_sha256=sha(Path(__file__).with_name('wasm_debug_quota_cases.py')),
        jit_policy=args.jit_policy, runner_prefix=runner, runner_sha256=sha(runner[0]) if runner else None,
        target_elf_machine=int.from_bytes(header[18:20], 'little'), old_path_diagnostic=args.old_path_diagnostic,
        cgroup=Path('/proc/self/cgroup').read_text())
    (args.out/'inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    rows = []
    for case in cases:
        path = args.out/(case['name']+'.wat'); wasm = path.with_suffix('.wasm')
        path.write_text(case['wat']+'\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(path), '-o', str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True)
        sites, dump = preview.markers(args.wasm_tools, wasm)
        (path.with_suffix('.dump')).write_text(dump); assert len(sites[0]) == 2
        for policy in args.call_stack_policy or ['instruction', 'unwind']:
            console = None
            row = dict(case=case['name'], policy=policy, actual_VM=True, observations=[], passed=False)
            try:
                argv = [*runner, str(args.binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
                    '-Rllvm-call-stack', policy, '-Rllvm-policy', args.jit_policy,
                    '-WFE-gc', '-WFE-reference-types', '-WFE-function-references', '--run', str(wasm)]
                row['argv'] = argv
                console = preview.OperandConsole(argv, args.out/(case['name']+'-'+policy+'.log'))
                console.prompt_timeout = 600 if case['kind']=='reply' and runner else 120
                console.prompt(); assert b'prepared; no Wasm instruction executed' in console.send('status')
                for offset in sites[0]:
                    assert b'registered' in console.send(f'break 0 0 {offset}')
                console.send('continue'); stopped(console, 60); before, _ = status(console)
                assert (before['function'], before['offset']) == (0, sites[0][0]), before
                observed = inspect(console, case, before, args.old_path_diagnostic)
                row['observations'].append(dict(stop=before, **observed))
                console.send('continue'); stopped(console, 60); after, _ = status(console)
                assert after['stop_id'] > before['stop_id'] and after['offset'] == sites[0][1], after
                marker = case.get('marker', 1)
                scalar = console.send(f'locals wasm {after["thread"]} 0 {marker} 1')
                assert f'local {marker} i32 = 777\n'.encode() in scalar, scalar
                assert b'operand 0 i64 = -991\n' in console.send(f'operands {after["thread"]} 0 0 1')
                if observed['last_path']:
                    session, handle = observed['last_path']
                    stale = console.send(f'path members {session} {handle} 0 1')
                    assert STALE in stale and b'object #' not in stale, stale
                    fresh = console.send(f'path create locals {after["thread"]} 0 0 0 0')
                    assert b'Wasm path session=' in fresh, fresh
                row['observations'].append(dict(stop=after, guest_marker=777,
                    old_paths_retired=True if observed['last_path'] else None))
                console.send('continue')
                import time
                deadline = time.monotonic()+60
                while True:
                    packet = console.send('status')
                    if b'guest exited:' in packet:
                        assert b'guest exited: 0' in packet, packet
                        break
                    assert b'running' in packet and time.monotonic() < deadline, packet
                    time.sleep(.005)
                row['passed'] = True
            finally:
                try:
                    if console is not None:
                        try:
                            console.close()
                            assert console.child.returncode == 0, console.child.returncode
                        except BaseException as error:
                            row['passed'] = False
                            row['retirement_error'] = f'{type(error).__name__}: {error}'
                            raise
                        finally:
                            row['actual_reaped_returncode'] = console.child.returncode
                finally:
                    rows.append(row); (args.out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
    assert all(r['passed'] for r in rows)
    print(json.dumps(dict(actual_VM_sessions=len(rows), typed_pauses=2*len(rows), passed=True)), flush=True)


if __name__ == '__main__':
    main()
