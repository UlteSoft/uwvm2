#!/usr/bin/env python3
"""Actual ROS ABI imports, execution and stopped-byte pretty qualification.

Use unchanged fixtures/pretty_ros_imports.cc with -DPRETTY_ROS1 or PRETTY_ROS2,
-O0 -g and the real SDK ABI header. Compile separately with an actual -MD
receipt. This runner parses the original Wasm import/type sections, observes the
actual imported abi_version result at the source marker, and separately tests
real copied payloads through UnixBroker/DAP. Import names alone never qualify
the runtime adapter or byte reads. Current products without the sealed ROS
debug-read integration must fail the read qualification; there is no xfail/PASS
conversion. Execute only in the designated existing Linux 64GiB cgroup.
"""
from pathlib import Path
import argparse
import json
import sys
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
import run_dap_pretty_producers as producers

sys.dont_write_bytecode = True


def original_imports(wasm, api):
    live.require(wasm.stat().st_size <= 64 * 1024 * 1024, 'bounded original compiler Wasm')
    data = wasm.read_bytes()
    live.require(data[:8] == b'\0asm\1\0\0\0', 'original core Wasm header')
    u32 = live.source_cli.metadata_cli.u32
    types, imports, seen, function_index = [], [], set(), 0

    def name(payload, at):
        count, at = u32(payload, at, len(payload))
        live.require(count <= 4096 and count <= len(payload) - at, 'bounded original import string')
        return payload[at:at + count].decode('utf-8', 'strict'), at + count

    def value_type(payload, at):
        live.require(at < len(payload) and payload[at] in (0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x70, 0x6f),
                     'actual scalar/vector/reference import value type')
        return payload[at], at + 1

    def limits(payload, at, table=False):
        flags, at = u32(payload, at, len(payload))
        live.require(flags in ((0, 1) if table else (0, 1, 3)), 'actual wasm32 import limits')
        minimum, at = u32(payload, at, len(payload))
        if flags & 1:
            maximum, at = u32(payload, at, len(payload))
            live.require(minimum <= maximum, 'complete original import limits')
        return at

    at = 8
    while at < len(data):
        section = data[at]
        count, at = u32(data, at + 1, len(data))
        live.require(count <= len(data) - at, 'bounded original Wasm section')
        payload = data[at:at + count];at += count
        if section not in (1, 2):
            continue
        live.require(section not in seen, 'unique original type/import section')
        seen.add(section)
        count, cursor = u32(payload, 0, len(payload))
        live.require(count <= 65536, 'bounded original type/import count')
        for _ in range(count):
            if section == 1:
                live.require(cursor < len(payload) and payload[cursor] == 0x60, 'actual compiler function type')
                cursor += 1
                parameters, cursor = u32(payload, cursor, len(payload))
                live.require(parameters <= 65536, 'finite original function parameters')
                arguments = []
                for _ in range(parameters):
                    kind, cursor = value_type(payload, cursor);arguments.append(kind)
                results, cursor = u32(payload, cursor, len(payload))
                live.require(results <= 16, 'finite original function results')
                returns = []
                for _ in range(results):
                    kind, cursor = value_type(payload, cursor);returns.append(kind)
                types.append((arguments, returns))
            else:
                module, cursor = name(payload, cursor)
                imported, cursor = name(payload, cursor)
                live.require(cursor < len(payload), 'complete original import descriptor')
                kind = payload[cursor];cursor += 1
                row = {'module': module, 'name': imported, 'kind': kind}
                if kind == 0:
                    type_index, cursor = u32(payload, cursor, len(payload))
                    row.update(type_index=type_index, function_index=function_index);function_index += 1
                elif kind == 1:
                    element, cursor = value_type(payload, cursor)
                    live.require(element in (0x70, 0x6f), 'original imported table reference type')
                    cursor = limits(payload, cursor, True)
                elif kind == 2:
                    cursor = limits(payload, cursor)
                elif kind == 3:
                    _, cursor = value_type(payload, cursor)
                    live.require(cursor < len(payload) and payload[cursor] in (0, 1), 'original global mutability')
                    cursor += 1
                elif kind == 4:
                    live.require(cursor < len(payload) and payload[cursor] == 0, 'original tag attribute')
                    _, cursor = u32(payload, cursor + 1, len(payload))
                else:
                    raise AssertionError(('unsupported original import descriptor', kind))
                imports.append(row)
        live.require(cursor == len(payload), 'complete original type/import section')
    expected = 'uwvm_' + api + '_v1'
    selected = [row for row in imports if row['module'].startswith('uwvm_ros')]
    live.require(len(selected) == 1 and selected[0]['module'] == expected and
                 selected[0]['name'] == 'abi_version' and selected[0]['kind'] == 0,
                 'one actual selected ROS ABI function import, not a string/namespace substitute', selected)
    live.require(selected[0]['type_index'] < len(types) and types[selected[0]['type_index']] == ([], [0x7f]),
                 'actual imported abi_version signature is () -> i32', selected[0])
    return {'selected': selected[0], 'signature': {'parameters': [], 'results': ['i32']}, 'original_imports': imports}


def session(args, folder, root, imports):
    folder.mkdir(mode=0o700)
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'actual_ros_import_protocol')
    vm = ['-m', 'run', '-Rct', '0', '-Rllvm-call-stack', args.policy,
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = {'passed': False, 'actual_ros_read_qualified': False, 'qualifications': [], 'queries': [],
              'actual_dap_stdio': True, 'original_imports': imports}
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
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_ROS_IMPORTS_STOP' in text)
        function = producers.function_identity(args.wasm, 'pretty_ros_import_probe')
        points = client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'actual ROS import source breakpoint', points)
        client.request('continue', {'threadId': live.actual_location(client)['thread']})
        position = producers.wait_stop(client)
        live.require(position['function'] == function, 'actual producer function reached after the real ROS import call', position)
        stack = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
        frames = [row for row in stack if row.get('source', {}).get('path') == str(args.source) and
                  row.get('line') == line and 'pretty_ros_import_probe' in row['name']]
        live.require(len(frames) == 1, 'actual ROS imported guest source frame', stack)
        frame = frames[0]
        record.update(position=position, frame=frame, breakpoint=points[0], actual_function=function)
        retained = []

        def qualify(label, action):
            row = {'case': label, 'passed': False, 'status': 'failed'}
            record['qualifications'].append(row)
            try:
                row['evidence'] = action()
            except AssertionError as error:
                row['error'] = repr(error)
            else:
                row.update(passed=True, status='verified')

        def query(expression):
            packet = client.evaluate(f"print {position['thread']} {position['stop']} {expression}").decode()
            record['queries'].append({'expression': expression, 'actual_packet': packet})
            return packet

        def version():
            actual = base.scalar(query('version'))
            live.require(actual == 1, 'actual imported abi_version result materialized in the stopped parameter', actual)
            return {'actual_parameter_version': actual}
        qualify('actual ROS import return value at marker', version)

        def members(reference, **page):
            rows = client.request('variables', dict(variablesReference=reference, **page))['body']['variables']
            for row in rows:
                producers.readonly(row)
            return rows

        def profile(name, fragments, payload=None):
            packet = query(name)
            nodes = dap.parse_source_object_display(packet, position['stop'], position['thread'])
            live.require(nodes is not None and nodes and nodes[0].get('pretty') is not None,
                         'actual ROS guest bytes must yield a pretty profile; raw/unavailable is a failure', (name, packet))
            summary = nodes[0]['pretty']
            live.require(all(fragment in summary for fragment in fragments), 'actual ROS guest profile summary', (name, summary))
            copied = [nodes[i] for i in nodes[0]['children'] if nodes[i]['detached_copy']]
            if name == 'values':
                live.require([row['name'] for row in copied] == ['[0]', '[1]', '[2]'] and
                             [row['value'] for row in copied] == ['7', '8', '9'], 'actual copied ROS guest vector payload', copied)
            elif payload is not None:
                live.require(len(copied) == 1 and copied[0]['value'] == payload, 'actual active optional payload copied from ROS guest bytes', copied)
            responses = []
            for context in ('watch', 'hover', 'variables'):
                body = client.request('evaluate', {'expression': name, 'context': context, 'frameId': frame['id']})['body']
                producers.readonly(body)
                live.require(body['result'] == summary and body['variablesReference'] > 0,
                             'actual ROS source pretty value remains an expandable copied DAP view', (name, context, body))
                retained.append(body['variablesReference'])
                rows = members(body['variablesReference'])
                if name == 'values':
                    indexed = members(body['variablesReference'], filter='indexed')
                    live.require([row['name'] for row in indexed] == ['[0]', '[1]', '[2]'] and
                                 [row['value'] for row in indexed] == ['7', '8', '9'], 'actual ROS vector DAP expansion', indexed)
                    live.require(members(body['variablesReference'], filter='indexed', start=1, count=1) == indexed[1:2], 'actual copied ROS vector pagination')
                elif payload is not None:
                    live.require(len([row for row in rows if row['name'] == 'value' and row['value'] == payload]) == 1,
                                 'actual optional DAP payload retained', rows)
                live.require(members(body['variablesReference'], start=1024) == [], 'finite actual ROS copied object page')
                responses.append({'context': context, 'response': body, 'children': rows})
            return {'actual_packet': packet, 'responses': responses}

        for name, fragments, payload in (
            ('values', ('size=3',), None),
            ('short_text', ('length=5', 'small'), None),
            ('long_text', ('length=52', 'this is a string longer than the short string buffer'), None),
            ('present', ('engaged', '7'), '7'),
        ):
            qualify('actual ROS byte reads ' + name, lambda name=name, fragments=fragments, payload=payload: profile(name, fragments, payload))
        scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
        source_scope = next(row['variablesReference'] for row in scopes if row['name'] == 'Source variables')
        def actual_scope():
            rows = members(source_scope, filter='named')
            selected = [row for row in rows if row['name'] == 'values']
            live.require(len(selected) == 1 and 'size=3' in selected[0]['value'] and selected[0]['variablesReference'] > 0,
                         'actual ROS Source variables scope retains pretty copied vector', rows)
            indexed = members(selected[0]['variablesReference'], filter='indexed')
            live.require([row['name'] for row in indexed] == ['[0]', '[1]', '[2]'] and
                         [row['value'] for row in indexed] == ['7', '8', '9'], 'actual ROS source scope vector payload', indexed)
            retained.append(selected[0]['variablesReference'])
            return {'scope': source_scope, 'value': selected[0], 'indexed': indexed}
        qualify('actual ROS Source variables copied bytes', actual_scope)
        live.require(live.actual_location(client) == position, 'actual ROS print probes preserve the genuine stop')
        client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        commands = [('stepIn', {'threadId': position['thread'], 'granularity': 'instruction'}),
                    ('variables', {'variablesReference': source_scope}),
                    ('evaluate', {'expression': 'version', 'context': 'watch', 'frameId': frame['id']})]
        if retained:
            commands.append(('variables', {'variablesReference': retained[-1]}))
        replies = [client.response(number) for number in client.queue(commands)]
        live.require(replies[0]['success'] and all(not row['success'] for row in replies[1:]), 'real guest step retires old ROS source views', replies)
        record['retirement'] = {'actual_responses': replies, 'copied_object_exercised': bool(retained)}
        record['qualifications'].append({'case': 'actual copied ROS object retirement', 'passed': bool(retained),
            'status': 'verified' if retained else 'not_exercised',
            'reason': 'real step rejected copied object reference' if retained else 'no copied ROS pretty object was available; no retirement PASS claimed'})
        after = live.actual_location(client)
        live.require(after['stop'] > position['stop'], 'actual new ROS guest stop after instruction step', after)
        client.request('continue', {'threadId': after['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                live.require(b'guest exited: 0' in exited, 'original ROS import fixture natural runtime oracle', exited)
                break
        else:
            raise AssertionError('actual ROS imported guest did not finish')
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original ROS broker OS wait')
        qualified = all(row['passed'] for row in record['qualifications'])
        record.update(passed=qualified, actual_ros_read_qualified=qualified, natural_guest_exit=exited.decode(), actual_runtime_oracles_passed=True)
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw, 'actual original first-pause gate proof', raw)
            if record.get('actual_runtime_oracles_passed'):
                live.require('TinyGo DAP original guest wait: 0' in raw, 'original ROS guest OS wait proof', raw)
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
    parser.add_argument('--source', type=Path, default=root / 'test/0018.debugger/fixtures/pretty_ros_imports.cc')
    parser.add_argument('--api', choices=('ros1', 'ros2'), required=True)
    parser.add_argument('--compiler-id', required=True, help='actual compiler/version/target/flags including PRETTY_ROS1 or PRETTY_ROS2')
    parser.add_argument('--adapter-receipt', type=Path, help='separate source/census integration evidence, if available; import names never replace it')
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    args = parser.parse_args()
    live.require(0 < len(args.compiler_id.encode()) <= 4096 and 'PRETTY_' + args.api.upper() in args.compiler_id,
                 'finite actual compiler/version/target and selected real SDK macro identity')
    for name in ('uwvm', 'wasm', 'source', 'producer_receipt'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    live.require('uwvm-dbg-pretty-ros-imports-v1' in args.source.read_text(), 'actual versioned ROS import producer source')
    admission = base.load(root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'ros_import_admission')
    proof = admission.admit()
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    abi = (args.source.parent / '../../../sdk' / args.api / 'abi.h').resolve(strict=True)
    common = (abi.parent / '../common/abi.h').resolve(strict=True)
    paths = [Path(__file__), Path(base.__file__), Path(live.__file__), Path(producers.__file__), args.uwvm, args.wasm,
             args.source, args.producer_receipt, abi, common, root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py',
             root / 'test/0018.debugger/run_pretty_printer_matrix.py',
             root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', root / 'test/0017.runtime/run_debug_source_step_cli.py']
    if args.adapter_receipt is not None:
        args.adapter_receipt = args.adapter_receipt.resolve(strict=True);paths.append(args.adapter_receipt)
    pins = {str(path): live.sha(path) for path in paths}
    result = {'passed': False, 'actual_ros_read_qualified': False, 'admission': proof, 'api': args.api,
              'pins_before': pins, 'compiler_id': args.compiler_id, 'producer_receipt': str(args.producer_receipt),
              'adapter_receipt': str(args.adapter_receipt) if args.adapter_receipt is not None else None,
              'scope': 'separate original import/signature, actual import return/execution and real ROS copied-byte/DAP qualification; never namespace-only adapter evidence'}
    try:
        result['original_imports'] = original_imports(args.wasm, args.api)
        result['original_imports_verified'] = True
        result['session'] = session(args, args.out / 'actual', root, result['original_imports'])
        result['passed'] = result['actual_ros_read_qualified'] = result['session']['passed']
    except BaseException as error:
        result['error'] = repr(error)
        raise
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['actual_ros_read_qualified'] = result['passed'] and result['pins_after'] == pins
        result['admission_after'] = admission.admit()
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual ROS imported guest byte reads must qualify; inspect recorded failures',
                 result.get('session', {}).get('qualifications'))
    print('PASS actual ROS import execution and stopped-byte pretty qualification', args.api)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
