#!/usr/bin/env python3
"""Actual standard-library DWARF at a guest stop through UnixBroker and DAP stdio.

The Wasm must be compiled unchanged from fixtures/pretty_containers.cc with -g.
Only execute inside the existing 64GiB/no-swap Linux cgroup. Layout DATA tests,
compiler production and native-build qualification have separate receipts.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import time
import run_dap_current_broker as live
import run_dap_tinygo_objects as base

sys.dont_write_bytecode = True


def session(args, folder, root):
    folder.mkdir(mode=0o700)
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'actual_pretty_dap')
    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'run', *mode, '-Rct', '0', '-Rllvm-call-stack', args.policy,
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = dict(passed=False, policy=args.policy, summaries=[], expansions=[], actual_dap_stdio=True)
    try:
        gate = dap.UnixBroker(str(server.directory))
        try:
            first = gate.request('pause')
            live.require('error:' not in first and 'guest exited:' not in first, 'actual queued first pause', first)
        finally:
            gate.close()
        client_folder = folder / 'session-0'
        client_folder.mkdir(mode=0o700)
        client = base.TypedClient(root / 'tools/debug/dap_adapter.py', server.directory, client_folder, 'source')
        server.clients.append(client)
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_CONTAINERS_STOP' in text)
        points = client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'actual standard-library fixture source breakpoint', points)
        client.request('continue', {'threadId': live.actual_location(client)['thread']})
        for _ in range(64):
            stopped = client.evaluate('wait')
            if b'stopped: breakpoint' in stopped:
                break
            live.require(b'guest exited:' not in stopped, 'guest exited before container fixture stop', stopped)
        else:
            raise AssertionError('actual pretty-container source stop timeout')
        position = live.actual_location(client)
        stack = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
        frames = [row for row in stack if row.get('source', {}).get('path') == str(args.source)
                  and row.get('line') == line and 'pretty_container_probe' in row['name']]
        live.require(len(frames) == 1, 'actual standard-library producer source frame', stack)
        frame = frames[0]
        record.update(position=position, frame=frame, breakpoint=points[0])

        def readonly(row):
            live.require(row['presentationHint']['attributes'] == ['readOnly'] and 'evaluateName' not in row
                         and 'memoryReference' not in row, 'opaque copied pretty display grants no selector/address', row)

        def members(reference, **page):
            rows = client.request('variables', dict(variablesReference=reference, **page))['body']['variables']
            for row in rows:
                readonly(row)
            return rows

        def vector(reference, values, label):
            all_rows = members(reference)
            indexed = members(reference, filter='indexed')
            live.require([row['name'] for row in indexed] == [f'[{i}]' for i in range(len(values))], 'original copied vector indices', (label, indexed))
            live.require([row['value'] for row in indexed] == list(map(str, values)), 'actual standard-library vector elements', (label, indexed))
            live.require(members(reference, filter='indexed', start=1, count=2) == indexed[1:3], 'copied vector DAP indexed page')
            live.require(members(reference, start=1024) == [], 'copied vector finite final page')
            record['expansions'].append(dict(label=label, rows=all_rows, indexed=indexed))
            return indexed

        scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
        source_scope = next(row['variablesReference'] for row in scopes if row['name'] == 'Source variables')
        source_variables = client.request('variables', {'variablesReference': source_scope})['body']['variables']
        scope_values = next(row for row in source_variables if row['name'] == 'values')
        readonly(scope_values)
        live.require(scope_values['variablesReference'] > 0 and re.search(r'\bsize=8(?=\s|$)', scope_values['value']) is not None, 'source scope retains expandable vector summary', scope_values)
        vector(scope_values['variablesReference'], range(7, 15), 'Source variables values')
        expected = {
            'values': ('size=8', 'capacity=8'), 'nested': ('size=2',),
            'short_text': ('length=5', 'small'), 'long_text': ('length=52', 'longer'),
            'view': ('length=5', 'small'), 'fixed': ('array', 'size=3'), 'span': ('size=8',),
            'present': ('engaged', '7'), 'absent': ('nullopt',), 'variant': ('index=1',),
            'unique': ('stored pointer',), 'shared': ('shared_ptr', 'use_count=1', '8'),
            'weak': ('weak_ptr', 'use_count=1', 'pointee not read'),
            'pair': ('pair',), 'tuple': ('tuple',),
        }
        vector_reference = 0
        for context in ('watch', 'hover', 'variables'):
            for name, fragments in expected.items():
                body = client.request('evaluate', {'expression': name, 'context': context, 'frameId': frame['id']})['body']
                readonly(body)
                live.require(all((re.search(r'\b' + re.escape(fragment) + r'(?=\s|$)', body['result']) is not None) if re.fullmatch(r'size=[0-9]+', fragment) else fragment in body['result'] for fragment in fragments), 'actual library profile summary', (context, name, body))
                live.require(body['variablesReference'] > 0, 'summary retains already copied raw fields/elements', (name, body))
                record['summaries'].append(dict(context=context, expression=name, response=body))
                if name == 'values':
                    vector(body['variablesReference'], range(7, 15), context + ' values')
                    vector_reference = body['variablesReference']
                if name == 'fixed':
                    vector(body['variablesReference'], range(7, 10), context + ' std::array')
                if name == 'nested':
                    outer = members(body['variablesReference'], filter='indexed')
                    live.require([row['name'] for row in outer] == ['[0]', '[1]'], 'two actually copied nested vector roots', outer)
                    for i, child in enumerate(outer):
                        live.require(re.search(r'\bsize=2(?=\s|$)', child['value']) is not None and child['variablesReference'] > 0, 'nested container summary remains expandable', child)
                        vector(child['variablesReference'], range(7 + i * 2, 9 + i * 2), context + f' nested[{i}]')
        # DAP references are retired on the next real guest execution. No copied
        # summary, guest pointer spelling or synthetic label can refresh them.
        # This client is at the source level. Statement requests a real source
        # step; instruction requests native stepping, which requires a native
        # trap and is deliberately rejected at this instrumented source stop.
        client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        commands = [('stepIn', {'threadId': position['thread'], 'granularity': 'statement'}),
                    ('variables', {'variablesReference': vector_reference}),
                    ('variables', {'variablesReference': source_scope}),
                    ('evaluate', {'expression': 'values', 'context': 'watch', 'frameId': frame['id']})]
        replies = [client.response(seq) for seq in client.queue(commands)]
        live.require(replies[0]['success'] and all(not row['success'] for row in replies[1:]), 'real step retires all pretty/source references', replies)
        record['retirement'] = replies
        after = live.actual_location(client)
        live.require(after['stop'] > position['stop'], 'actual new stop after source statement step', after)
        client.request('continue', {'threadId': after['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                live.require(b'guest exited: 0' in exited, 'standard-library guest natural oracle exit', exited)
                break
        else:
            raise AssertionError('actual pretty-container guest did not finish')
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original pretty broker OS wait')
        record.update(passed=True, natural_guest_exit=exited.decode())
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw, 'actual original first-pause gate proof', raw)
            if record['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw, 'actual original guest OS wait proof', raw)
        finally:
            (folder / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return base.main()
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'source', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--cgroup', required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--fixed-llvm-full', action='store_true', help='product has one fixed LLVM-full runtime; omit unavailable selector flags')
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    args = parser.parse_args()
    live.require(sys.platform == 'linux', 'Linux original cgroup runner only')
    live.require(Path('/proc/self/cgroup').read_text() == '0::' + args.cgroup + '\n', 'original test cgroup')
    cgroup = Path('/sys/fs/cgroup') / args.cgroup.lstrip('/')
    live.require((cgroup / 'memory.max').read_text().strip() == str(64 << 30), 'existing 64GiB memory limit')
    live.require((cgroup / 'memory.swap.max').read_text().strip() == '0', 'existing swap0')
    root = Path(__file__).resolve().parents[2]
    for name in ('uwvm', 'wasm', 'source'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    live.require(args.wasm.read_bytes()[:8] == b'\x00asm\x01\x00\x00\x00', 'unmodified compiler-produced core Wasm')
    paths = [Path(__file__), Path(base.__file__), Path(live.__file__), args.uwvm, args.wasm, args.source,
             root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py']
    pins = {str(path): live.sha(path) for path in paths}
    args.out.mkdir(mode=0o700)
    result = dict(passed=False, pins=pins, scope='Actual standard-library fixture guest stop, UnixBroker and DAP stdio; compiler/native qualification is separate.')
    try:
        record = session(args, args.out / 'session-0000', root)
        result.update(passed=record['passed'], summaries=len(record['summaries']), expansions=len(record['expansions']))
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['passed'] and result['pins_after'] == pins
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual standard-library pretty DAP acceptance', result)
    print('PASS real standard-library pretty DAP', result['summaries'], 'summaries')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
