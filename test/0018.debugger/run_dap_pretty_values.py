#!/usr/bin/env python3
"""Qualify unchanged C++23 value/ownership producers through real broker+DAP.

Compile fixtures/pretty_values_live.cc separately with -std=c++23 -O0 -g and
record its actual compiler/-MD closure. This runner never rebuilds a product or
rewrites Wasm/DWARF. ABI/profile failures are retained individually and fail the
qualification; raw fallback is not a successful pretty-printer result. Without
real C++ exceptions the valueless case is explicitly not_exercised and the full
qualification remains incomplete. Run only in the designated Linux cgroup.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
import run_dap_pretty_producers as producers

sys.dont_write_bytecode = True


def session(args, folder, root):
    folder.mkdir(mode=0o700)
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'actual_values_protocol')
    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'run', *mode, *(['--wasm-feature-enable-exceptions'] if args.wasm_exceptions else []), '-Rct', '0', '-Rllvm-call-stack', args.policy,
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = {'passed': False, 'qualifications': [], 'queries': [], 'actual_dap_stdio': True}
    try:
        gate = dap.UnixBroker(str(server.directory))
        try:
            first = gate.request('pause')
            live.require('error:' not in first and 'guest exited:' not in first, 'actual queued first pause', first)
        finally:
            gate.close()
        client_folder = folder / 'client';client_folder.mkdir(mode=0o700)
        client = base.TypedClient(root / 'tools/debug/dap_adapter.py', server.directory, client_folder, 'source')
        server.clients.append(client)
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_VALUES_LIVE_STOP' in text)
        function = producers.function_identity(args.wasm, 'pretty_values_probe')
        points = client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'actual C++23 value source breakpoint', points)
        client.request('continue', {'threadId': live.actual_location(client)['thread']})
        position = producers.wait_stop(client)
        live.require(position['function'] == function, 'actual stopped producer function from original Wasm names', position)
        stack = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
        frames = [row for row in stack if row.get('source', {}).get('path') == str(args.source) and
                  row.get('line') == line and 'pretty_values_probe' in row['name']]
        live.require(len(frames) == 1, 'actual current C++23 producer source frame', stack)
        frame = frames[0]
        record.update(position=position, frame=frame, breakpoint=points[0], actual_function=function)
        object_references = []

        def query(expression):
            packet = client.evaluate(f"print {position['thread']} {position['stop']} {expression}").decode()
            record['queries'].append({'expression': expression, 'actual_packet': packet})
            return packet

        def qualify(label, action):
            row = {'case': label, 'status': 'failed', 'passed': False}
            record['qualifications'].append(row)
            try:
                row['evidence'] = action()
            except AssertionError as error:
                row['error'] = repr(error)
            else:
                row.update(status='verified', passed=True)

        def members(reference, **page):
            rows = client.request('variables', dict(variablesReference=reference, **page))['body']['variables']
            for row in rows:
                producers.readonly(row)
            return rows

        def direct_copies(nodes, index=0):
            return [nodes[i] for i in nodes[index]['children'] if nodes[i]['detached_copy']]

        def profile(name, fragments, *, payload=None, empty_payload=False, sequence=None, heap=False, typedef=False, nested_scalar=None):
            packet = query(name)
            nodes = dap.parse_source_object_display(packet, position['stop'], position['thread'])
            live.require(nodes is not None and nodes and nodes[0].get('pretty') is not None,
                         'actual ABI profile must produce a pretty summary, never raw fallback', (name, packet))
            summary = nodes[0]['pretty']
            live.require(all(fragment in summary for fragment in fragments), 'actual value profile semantics', (name, summary, fragments))
            for counter, number in re.findall(r'(use_count|weak_count|index)=([0-9]+)', ' '.join(fragments)):
                live.require(re.search(r'\b' + counter + '=' + number + r'\b', summary), 'exact copied ownership counter', (name, summary))
            copied = direct_copies(nodes)
            if payload is not None:
                live.require(len(copied) == 1 and copied[0]['value'] == payload,
                             'actual selected payload has one copied value', (name, copied))
            if empty_payload:
                live.require(not copied, 'profile must not invent an active/copied pointee payload', (name, copied))
            if nested_scalar is not None:
                live.require(len(copied) == 1, 'time point has one typed copied duration', (name, copied))
                duration_index = next(i for i in nodes[0]['children'] if nodes[i]['detached_copy'])
                ticks = direct_copies(nodes, duration_index)
                live.require(len(ticks) == 1 and ticks[0]['value'] == nested_scalar, 'time point duration retains actual copied ticks', (name, ticks))
            responses = []
            for context in ('watch', 'hover', 'variables'):
                body = client.request('evaluate', {'expression': name, 'context': context, 'frameId': frame['id']})['body']
                producers.readonly(body)
                live.require(body['result'] == summary, 'DAP presents the exact current copied pretty summary', (name, context, body, summary))
                if typedef:
                    live.require('PrettyNumberList' in body.get('type', ''), 'real typedef declaration is preserved', body)
                children = members(body['variablesReference']) if body['variablesReference'] else []
                if body['variablesReference']:
                    object_references.append(body['variablesReference'])
                    live.require(members(body['variablesReference'], start=1024) == [], 'copied value pagination has a finite end')
                if payload is not None:
                    selected = [row for row in children if row['name'] == copied[0]['name'] and row['value'] == payload]
                    live.require(len(selected) == 1, 'DAP retains actual selected scalar payload', (name, children))
                if nested_scalar is not None:
                    durations = [row for row in children if row['name'] == 'value' and 'duration ticks' in row['value']]
                    live.require(len(durations) == 1 and durations[0]['variablesReference'] > 0, 'DAP time point retains duration expansion', (name, children))
                    tick_rows = members(durations[0]['variablesReference'])
                    live.require(len([row for row in tick_rows if row['name'] == 'value' and row['value'] == nested_scalar]) == 1,
                                 'DAP copied duration contains real scalar ticks', tick_rows)
                if sequence is not None:
                    container = body
                    if name != 'typedef_values':
                        containers = [row for row in children if row['name'] == 'value' and 'size=3' in row['value']]
                        live.require(len(containers) == 1 and containers[0]['variablesReference'] > 0,
                                     'adaptor retains its typed copied underlying container', (name, children))
                        container = containers[0]
                    indexed = members(container['variablesReference'], filter='indexed')
                    actual = [producers.numeric(row['value']) for row in indexed]
                    live.require([row['name'] for row in indexed] == ['[0]', '[1]', '[2]'], 'actual adaptor/container copied indices', (name, indexed))
                    live.require(sorted(actual) == sequence if heap else actual == sequence, 'actual underlying container values', (name, indexed))
                    if heap:
                        live.require(actual[0] == max(sequence), 'actual heap root; remaining heap order is not assumed', (name, indexed))
                    live.require(members(container['variablesReference'], filter='indexed', start=1, count=1) == indexed[1:2],
                                 'copied underlying sequence indexed pagination')
                responses.append({'context': context, 'response': body, 'children': children})
            return {'summary': summary, 'actual_packet': packet, 'responses': responses}

        cases = [
            ('expected_value', ('value', '23'), {'payload': '23'}),
            ('expected_error', ('error', '29'), {'payload': '29'}),
            ('expected_void_value', ('value (void)',), {'empty_payload': True}),
            ('expected_void_error', ('error', '31'), {'payload': '31'}),
            ('present', ('engaged', '37'), {'payload': '37'}),
            ('absent', ('nullopt',), {'empty_payload': True}),
            ('active', ('index=1', 'true'), {'payload': 'true'}),
            ('shared', ('shared_ptr', 'use_count=2', 'weak_count=1', '41'), {'payload': '41'}),
            ('weak_live', ('weak_ptr', 'use_count=2', 'weak_count=1', 'pointee not read'), {'empty_payload': True}),
            ('weak_expired', ('weak_ptr', 'use_count=0', 'weak_count=1', 'expired'), {'empty_payload': True}),
            ('alias_void', ('shared_ptr', 'use_count=2', 'weak_count=1', 'stored pointer to void'), {'empty_payload': True}),
            ('array_owned', ('shared_ptr', 'use_count=1', 'weak_count=0', 'array extent unavailable'), {'empty_payload': True}),
            ('ticks', ('duration ticks', '1234'), {'payload': '1234'}),
            ('point', ('time_point since epoch', 'duration ticks', '1234'), {'nested_scalar': '1234'}),
            ('stack_vector', ('stack', 'size=3'), {'sequence': [61, 62, 63]}),
            ('stack_deque', ('stack', 'size=3'), {'sequence': [64, 65, 66]}),
            ('queue_deque', ('queue', 'size=3'), {'sequence': [71, 72, 73]}),
            ('priority_vector', ('priority_queue', 'size=3'), {'sequence': [81, 82, 83], 'heap': True}),
            ('priority_deque', ('priority_queue', 'size=3'), {'sequence': [84, 85, 86], 'heap': True}),
            ('typedef_values', ('size=3',), {'sequence': [91, 92, 93], 'typedef': True}),
        ]
        for name, fragments, options in cases:
            qualify(name, lambda name=name, fragments=fragments, options=options: profile(name, fragments, **options))
        exceptions = base.scalar(query('valueless_exceptions'))
        live.require(exceptions in (0, 1), 'actual compiler exception-mode oracle', exceptions)
        record['exceptions_enabled'] = bool(exceptions)
        if exceptions:
            qualify('valueless_by_exception', lambda: profile('valueless', ('valueless_by_exception',), empty_payload=True))
            def actual_caught():
                caught = base.scalar(query('valueless_caught'))
                live.require(caught == 1, 'actual throwing-move exception must be caught', caught)
                return {'caught': caught}
            qualify('real throwing-move exception caught', actual_caught)
        else:
            record['qualifications'].append({'case': 'valueless_by_exception', 'status': 'not_exercised', 'passed': False,
                'reason': 'actual compiler exceptions disabled; no variant object layout was fabricated'})

        scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
        source_scope = next(row['variablesReference'] for row in scopes if row['name'] == 'Source variables')

        def actual_scope():
            rows = members(source_scope, filter='named')
            selected = [row for row in rows if row['name'] == 'typedef_values']
            live.require(len(selected) == 1 and 'size=3' in selected[0]['value'] and selected[0]['variablesReference'] > 0,
                         'actual Source variables scope retains typedef pretty expansion', rows)
            indexed = members(selected[0]['variablesReference'], filter='indexed')
            live.require([producers.numeric(row['value']) for row in indexed] == [91, 92, 93], 'actual Source variables copied values', indexed)
            return {'scope': source_scope, 'value': selected[0], 'indexed': indexed}
        qualify('actual source scope variables', actual_scope)
        live.require(live.actual_location(client) == position, 'all value queries retain actual stopped activation')
        client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        live.require(object_references, 'an actual retained copied object is required for retirement qualification')
        commands = [('stepIn', {'threadId': position['thread'], 'granularity': 'statement'}),
                    ('variables', {'variablesReference': source_scope}),
                    ('evaluate', {'expression': 'expected_value', 'context': 'watch', 'frameId': frame['id']})]
        if object_references:
            commands.append(('variables', {'variablesReference': object_references[-1]}))
        replies = [client.response(number) for number in client.queue(commands)]
        live.require(replies[0]['success'] and all(not row['success'] for row in replies[1:]),
                     'real step retires copied value, source scope and source frame references', replies)
        after = live.actual_location(client)
        live.require(after['stop'] > position['stop'], 'actual new stop after source statement step', after)
        record['retirement'] = replies
        client.request('continue', {'threadId': after['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                live.require(b'guest exited: 0' in exited, 'unchanged C++23 producer natural oracle exit', exited)
                break
        else:
            raise AssertionError('actual C++23 value producer did not finish')
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original value broker OS wait')
        record.update(natural_guest_exit=exited.decode(), actual_runtime_oracles_passed=True,
                      passed=all(row['passed'] for row in record['qualifications']))
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw, 'actual first-pause gate proof', raw)
            if record.get('actual_runtime_oracles_passed'):
                live.require('TinyGo DAP original guest wait: 0' in raw, 'actual original guest OS wait proof', raw)
        finally:
            (folder / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return base.main()
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out', 'producer-receipt'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source', type=Path, default=root / 'test/0018.debugger/fixtures/pretty_values_live.cc')
    parser.add_argument('--compiler-id', required=True, help='actual compiler/version/target/flags, including -std=c++23')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--fixed-llvm-full', action='store_true', help='product has one fixed LLVM-full runtime; omit unavailable selector flags')
    parser.add_argument('--wasm-exceptions', action='store_true', help='enable the VM exceptions proposal for a genuine exception-enabled Wasm producer')
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    args = parser.parse_args()
    live.require(0 < len(args.compiler_id.encode()) <= 4096 and 'c++23' in args.compiler_id,
                 'finite actual C++23 compiler/version/target/flags identity')
    for name in ('uwvm', 'wasm', 'source', 'producer_receipt'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    live.require('uwvm-dbg-pretty-values-live-v1' in args.source.read_text(), 'actual versioned C++23 producer source')
    admission = base.load(root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'values_live_admission')
    proof = admission.admit()
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    paths = [Path(__file__), Path(base.__file__), Path(live.__file__), Path(producers.__file__), args.uwvm, args.wasm,
             args.source, args.producer_receipt, root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py',
             root / 'test/0018.debugger/run_pretty_printer_matrix.py',
             root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', root / 'test/0017.runtime/run_debug_source_step_cli.py']
    pins = {str(path): live.sha(path) for path in paths}
    result = {'passed': False, 'admission': proof, 'pins_before': pins, 'compiler_id': args.compiler_id,
              'producer_receipt': str(args.producer_receipt), 'fixture_identity': 'uwvm-dbg-pretty-values-live-v1',
              'scope': 'actual compiler-owned C++23 values through authenticated broker/DAP; separate producer/-MD and product source receipts are pinned, never relabeled'}
    try:
        result['session'] = session(args, args.out / 'actual', root)
        result['passed'] = result['session']['passed']
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['passed'] and result['pins_after'] == pins
        result['admission_after'] = admission.admit()
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'complete real C++23 value qualification; inspect individual ABI/not_exercised failures',
                 result.get('session', {}).get('qualifications'))
    print('PASS actual C++23 value producer', len(result['session']['qualifications']), 'qualifications')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
