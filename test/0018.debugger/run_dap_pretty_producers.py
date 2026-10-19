#!/usr/bin/env python3
"""Actual -g sequence/stateful/coroutine producer acceptance through broker+DAP.

Run only on SSH Linux inside the existing 64 GiB/no-swap cgroup. Inputs are
unchanged compiler products and matching source; --compiler-id records the
builder's exact compiler/version/target/flags, and --producer-receipt pins its
separate build qualification. No Wasm or DWARF is rewritten. Function entry
breakpoints resolve the actual Wasm name/export section and are verified against
the actual stopped function, rather than guessing a numeric function index.

Coroutine handles must keep the erased ABI boundary. Typed compiler-frame
qualification is reported separately; --require-typed-frame makes it mandatory.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import run_dap_current_broker as live
import run_dap_tinygo_objects as base

sys.dont_write_bytecode = True


def function_identity(wasm, wanted):
    data = wasm.read_bytes()
    live.require(len(data) <= 64 * 1024 * 1024 and data[:8] == b'\0asm\1\0\0\0', 'bounded original compiler Wasm')
    u32 = live.source_cli.metadata_cli.u32
    found = set()
    at = 8

    def string(payload, cursor, end):
        size, cursor = u32(payload, cursor, end)
        live.require(size <= end - cursor, 'bounded original Wasm string')
        return payload[cursor:cursor + size], cursor + size

    while at < len(data):
        kind = data[at]
        size, at = u32(data, at + 1, len(data))
        live.require(size <= len(data) - at, 'bounded original Wasm section')
        payload = data[at:at + size]
        at += size
        if kind == 7:
            count, cursor = u32(payload, 0, len(payload))
            live.require(count <= 65536, 'bounded original export count')
            for _ in range(count):
                name, cursor = string(payload, cursor, len(payload))
                live.require(cursor < len(payload), 'complete original export')
                tag = payload[cursor]
                index, cursor = u32(payload, cursor + 1, len(payload))
                if tag == 0 and name == wanted.encode():
                    found.add(index)
            live.require(cursor == len(payload), 'complete original export section')
        elif kind == 0:
            name, cursor = string(payload, 0, len(payload))
            if name != b'name':
                continue
            while cursor < len(payload):
                subsection = payload[cursor]
                size, cursor = u32(payload, cursor + 1, len(payload))
                live.require(size <= len(payload) - cursor, 'bounded original name subsection')
                end = cursor + size
                if subsection == 1:
                    count, cursor = u32(payload, cursor, end)
                    live.require(count <= 65536, 'bounded original function-name count')
                    for _ in range(count):
                        index, cursor = u32(payload, cursor, end)
                        name, cursor = string(payload, cursor, end)
                        if name == wanted.encode():
                            found.add(index)
                    live.require(cursor == end, 'complete original function-name subsection')
                cursor = end
    live.require(len(found) == 1, 'unique actual producer marker function identity', (wanted, sorted(found)))
    return found.pop()


def readonly(row):
    live.require(row.get('presentationHint', {}).get('attributes') == ['readOnly'] and
                 'evaluateName' not in row and 'memoryReference' not in row, 'opaque copied producer value', row)


def numeric(value):
    if value in ('true', 'false'):
        return int(value == 'true')
    match = re.fullmatch(r'(?:[ui](?:8|16|32|64)=)?(-?\d+)', value)
    live.require(match is not None, 'actual copied integer scalar', value)
    return int(match[1])


def wait_stop(client):
    for _ in range(64):
        reply = client.evaluate('wait')
        if b'stopped: breakpoint' in reply:
            return live.actual_location(client)
        live.require(b'guest exited:' not in reply, 'producer must reach its next marker', reply)
    raise AssertionError('producer marker timeout')


def frames(client, position, source, marker=None):
    rows = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
    selected = [row for row in rows if row.get('source', {}).get('path') == str(source)
                and (marker is None or marker in row['name'])]
    return selected, rows


def evaluate(client, frame, expression, context='watch'):
    body = client.request('evaluate', {'expression': expression, 'context': context, 'frameId': frame['id']})['body']
    readonly(body)
    return body


def members(client, reference, **page):
    rows = client.request('variables', dict(variablesReference=reference, **page))['body']['variables']
    for row in rows:
        readonly(row)
    return rows


def has_size(text, count):
    return re.search(r'\bsize=' + str(count) + r'(?=\s|$)', text) is not None


def sequences(client, frame, record, stateful):
    def sequence(name, count, expected):
        for context in ('watch', 'hover', 'variables'):
            body = evaluate(client, frame, name, context)
            live.require(has_size(body['result'], count) and body['variablesReference'] > 0,
                         'actual producer sequence summary', (name, body))
            indexed = members(client, body['variablesReference'], filter='indexed')
            live.require([row['name'] for row in indexed] == [f'[{i}]' for i in range(len(expected))] and
                         [numeric(row['value']) for row in indexed] == expected, 'actual copied producer sequence values', (name, indexed))
            live.require(members(client, body['variablesReference'], filter='indexed', start=1, count=2) == indexed[1:3],
                         'producer indexed pagination')
            record['values'].append(dict(expression=name, context=context, response=body, indexed=indexed))

    def mapping(name, ordered):
        body = evaluate(client, frame, name)
        live.require(has_size(body['result'], 3) and body['variablesReference'] > 0, 'actual map producer summary', (name, body))
        rows = members(client, body['variablesReference'], filter='indexed')
        live.require([row['name'] for row in rows] == ['[0]', '[1]', '[2]'], 'actual map has three ordered copied indices', rows)
        pairs = []
        for row in rows:
            fields = members(client, row['variablesReference'], filter='named')
            values = {field['name']: field['value'] for field in fields}
            live.require(sum(field['name'] == 'first' for field in fields) == 1 and
                         sum(field['name'] == 'second' for field in fields) == 1, 'actual copied pair has unique key and mapped value', fields)
            pairs.append((numeric(values['first']), numeric(values['second'])))
        expected = [(1, 10), (2, 20), (3, 30)]
        live.require((pairs if ordered else sorted(pairs)) == expected, 'actual ordered traversal or unordered key/value set', (name, ordered, pairs))
        record['values'].append(dict(expression=name, response=body, pairs=pairs, indexed=rows))

    def selected_scalar(expression, expected):
        for context in ('watch', 'hover', 'variables'):
            body = evaluate(client, frame, expression, context)
            live.require(numeric(body['result']) == expected and body['variablesReference'] == 0,
                         'actual producer selected scalar code unit/element', (expression, body))
            record['values'].append(dict(expression=expression, context=context, response=body))

    def string(name, expected):
        for context in ('watch', 'hover', 'variables'):
            body = evaluate(client, frame, name, context)
            live.require(body['result'] == f'string length={len(expected)} "{expected}"' and body['variablesReference'] > 0,
                         'actual stateful string code units and exact length', (name, body))
            record['values'].append(dict(expression=name, context=context, response=body))

    if stateful:
        sequence('pretty_stateful_list', 3, [11, 22, 33])
        sequence('pretty_stateful_packed', 137, [int(i % 3 == 1) for i in range(137)])
        sequence('pretty_stateful_vector', 3, [41, 42, 43])
        selected = evaluate(client, frame, 'pretty_stateful_vector[1]')
        live.require(numeric(selected['result']) == 42, 'actual stateful vector selected-element expression', selected)
        record['values'].append(dict(expression='pretty_stateful_vector[1]', context='watch', response=selected))
        string('pretty_stateful_short_text', 'stateful')
        string('pretty_stateful_long_text', 'x' * 48)
        selected_scalar('pretty_stateful_short_text[0]', ord('s'))
        selected_scalar('pretty_stateful_short_text[7]', ord('l'))
        selected_scalar('pretty_stateful_long_text[0]', ord('x'))
        selected_scalar('pretty_stateful_long_text[47]', ord('x'))
        mapping('pretty_stateful_tree', True)
        mapping('pretty_stateful_hash_table', False)
    else:
        sequence('pretty_deque', 1280, list(range(1020, 1220)))
        sequence('pretty_forward', 3, [11, 22, 33])
        sequence('pretty_list', 3, [11, 22, 33])
        sequence('pretty_packed', 137, [int(i % 3 == 1) for i in range(137)])
        sequence('pretty_bitset', 130, [int(i in (0, 64, 129)) for i in range(130)])
        sequence('pretty_static_span', 4, [7, 8, 9, 10])
        selected_scalar('pretty_static_span[0]', 7)
        selected_scalar('pretty_static_span[3]', 10)
        mapping('pretty_tree', True)
        mapping('pretty_hash', False)


def coroutine_handle_marker(client, args, function, phase, empty=False):
    attempts = []
    for _ in range(128):
        position = live.actual_location(client)
        live.require(position['function'] == function, 'handle inspection remains inside actual marker', position)
        selected, stack = frames(client, position, args.source, 'pretty_coroutine_empty_ready' if empty else 'pretty_coroutine_ready')
        if len(selected) == 1:
            frame = selected[0]
            expressions = ['erased'] if empty else ['typed', 'erased', 'phase']
            replies = [client.response(number) for number in client.queue([
                ('evaluate', {'expression': name, 'context': 'watch', 'frameId': frame['id']}) for name in expressions])]
            attempts.append(dict(position=position, replies=replies))
            if all(row.get('success') for row in replies):
                bodies = [row['body'] for row in replies]
                for body in bodies:
                    readonly(body)
                addresses = [re.search(r'frame=guest:0x([0-9a-fA-F]+)', body['result']) for body in bodies[:1 if empty else 2]]
                if all(addresses) and all(('frame ABI unavailable' in body['result'] or ('ABI='+args.handle_abi) in body['result']) for body in bodies[:1 if empty else 2]) and (
                        empty or re.fullmatch(r'(?:[ui](?:8|16|32|64)=)?-?\d+', bodies[2]['result'])):
                    values = [int(match[1], 16) for match in addresses]
                    # Function-entry DWARF may describe stack slots before the
                    # producer prologue has initialized them. Retain evidence
                    # and advance until the actual arguments have materialized.
                    address_ready = values == [0] if empty else values[0] == values[1] and values[0] != 0
                    phase_ready = empty or numeric(bodies[2]['result']) == phase
                    if address_ready and phase_ready:
                        abi = all(('ABI='+args.handle_abi) in body['result'] for body in bodies[:1 if empty else 2])
                        live.require(not args.require_handle_abi or abi, 'requested actual versioned handle ABI qualification', bodies)
                        inspected = {}
                        if abi:
                            if empty:
                                live.require('state=empty' in bodies[0]['result'], 'null handle does not read a frame', bodies[0])
                            else:
                                words = [re.search(r'resume-table-index=(\d+), destroy-table-index=(\d+)', body['result']) for body in bodies[:2]]
                                live.require(all(words), 'both actual handles expose ABI table words', bodies)
                                headers = [tuple(map(int, word.groups())) for word in words]
                                live.require(headers[0] == headers[1] and headers[0][1] > 0, 'typed/erased copied header agreement', headers)
                                live.require((headers[0][0] == 0) == (phase == 3), 'original guest final-suspend marker agrees with frame header', (phase, headers))
                                for body in bodies[:2]:
                                    live.require(('state=done (final-suspend marker; lifetime not proven)' if phase == 3 else
                                                  'state=unknown (resumable; suspended/running not proven)') in body['result'],
                                                 'ABI marker retains lifecycle uncertainty', body)
                                for kind, body in zip(('typed', 'erased'), bodies[:2]):
                                    page = client.request('variables', {'variablesReference': body['variablesReference']})['body']['variables']
                                    for item in page:
                                        readonly(item)
                                    promise = [item for item in page if item['name'] == '[promise]']
                                    if kind == 'erased':
                                        live.require(not promise and 'promise=erased' in body['result'], 'erased handle never invents promise type', page)
                                    else:
                                        live.require(len(promise) == 1 and promise[0]['variablesReference'] > 0, 'typed handle has owned copied promise child', page)
                                        payload = client.request('variables', {'variablesReference': promise[0]['variablesReference']})['body']['variables']
                                        for item in payload:
                                            readonly(item)
                                        selected_value = [item for item in payload if item['name'] == 'current_value']
                                        expected_value = 40 if phase < 2 else 41
                                        live.require(len(selected_value) == 1 and numeric(selected_value[0]['value']) == expected_value,
                                                     'original C++20 constructor/yield promise payload', (phase, payload))
                                        inspected['promise_value'] = expected_value
                                    inspected[kind] = page
                                inspected['header_table_indices'] = headers[0]
                        return dict(phase='empty' if empty else phase, position=position, responses=bodies, attempts=attempts,
                                    handle_abi_qualified=abi, abi_copied_children=inspected)
        # At a marker entry, source arguments may precede their prologue.
        # Request the real Wasm opcode policy explicitly; DAP instruction at
        # the source level means native stepping and requires a native trap.
        client.evaluate(f"step wasm {position['thread']}")
    raise AssertionError('real coroutine arguments did not materialize before marker left or step budget expired')


def coroutine_body(client, args, position, dap, body_phase):
    # The marker helper owns its own locals. Return through genuine Wasm
    # instruction stops until the compiler coroutine body is the CURRENT
    # activation; every subsequent frame/value query uses that new stop.
    entry = dict(position)
    returns = []
    for _ in range(128):
        if position['function'] != entry['function']:
            break
        before = dict(position)
        reply = client.evaluate(f"step wasm {position['thread']}")
        position = live.actual_location(client)
        returns.append(dict(before=before, after=position, reply=reply.decode(errors='replace')))
    live.require(position['function'] != entry['function'],
                 'actual marker return reaches a fresh current Wasm activation', returns)
    selected, stack = frames(client, position, args.source, 'pretty_coroutine_sequence')
    record = dict(position=position, marker_entry=entry, actual_return_stops=returns, stack=stack, probes=[], body_phase=body_phase,
                  typed_frame_layout_qualified=False, typed_frame_fields_qualified=False,
                  typed_frame_promise_qualified=False, typed_frame_locals_qualified=False,
                  state_boundary_verified=False, lifecycle_state_qualified=False,
                  typed_frame_qualified=False)
    expected_previous, expected_current = 39 + body_phase, 40 + body_phase
    # C++20 aggregate promise construction receives the coroutine seed (40).
    # Both original native body-entry probes independently read 40; the first
    # yield also stores 40. The unchanged source fixture is not rewritten.
    expected_promise = 40
    for expression in ('__coro_frame', '*__coro_frame', '__promise', '*__promise'):
        current, _ = frames(client, live.actual_location(client), args.source, 'pretty_coroutine_sequence')
        for frame in current:
            number = client.queue([('evaluate', {'expression': expression, 'context': 'watch', 'frameId': frame['id']})])[0]
            response = client.response(number)
            probe = dict(frame=frame, expression=expression, response=response)
            record['probes'].append(probe)
            if not response.get('success'):
                continue
            readonly(response['body'])
            text = response['body']['result']
            if not re.match(r'(?:Clang|GCC) coroutine frame, state=', text):
                continue
            # Reselect the real canonical caller ordinal; copied labels/pointers
            # are never converted into memory selectors or frame credentials.
            actual = live.actual_location(client)
            page_text = client.evaluate(f"frames {actual['thread']} {actual['stop']}").decode()
            page = dap.parse_source_frames(page_text, actual['thread'], actual['stop'], with_page=True)
            candidates = [row for row in page['rows'] if row['current'] and 'pretty_coroutine_sequence' in row['name']]
            probe['actual_frame_page'] = page_text
            if len(candidates) != 1:
                probe['typed_boundary'] = 'actual current compiler caller ordinal is unavailable or ambiguous'
                continue
            ordinal = candidates[0]['ordinal']
            packet = client.evaluate(f"print-frame {actual['thread']} {actual['stop']} {ordinal} {expression}").decode()
            nodes = dap.parse_source_object_display(packet, actual['stop'], actual['thread'])
            probe['actual_packet'] = packet
            if not nodes or nodes[0].get('pretty') != text:
                continue
            clang = text.startswith('Clang ')
            type_matches = '.coro_frame_ty' in nodes[0]['type'] if clang else 'Frame' in nodes[0]['type']
            if not type_matches:
                continue
            probe['layout_qualified'] = True
            record['typed_frame_layout_qualified'] = True

            def child(index, name):
                matches = [i for i in nodes[index]['children'] if nodes[i]['name'] == name]
                live.require(len(matches) <= 1, 'actual compiler field has unique copied ancestry', (name, packet))
                return nodes[matches[0]] if matches else None

            resume = child(0, '__resume_fn' if clang else '_Coro_resume_fn')
            destroy = child(0, '__destroy_fn' if clang else '_Coro_destroy_fn')
            index = child(0, '__coro_index' if clang else '_Coro_resume_index')
            pointers = []
            for node in (resume, destroy):
                match = re.fullmatch(r'guest:0x([0-9a-fA-F]+)', node['value']) if node and node['copied_value'] else None
                pointers.append(int(match[1], 16) if match else None)
            fields_ready = all(value is not None for value in pointers) and index and index['copied_value']
            if fields_ready:
                width = resume['size']
                live.require(width in (4, 8) and destroy['size'] == width and resume['offset'] == nodes[0]['offset'] and
                             destroy['offset'] == nodes[0]['offset'] + width and index['offset'] >= nodes[0]['offset'] + 2 * width and
                             index['size'] in ((1, 2, 4, 8) if clang else (2,)), 'actual copied compiler frame widths/offsets', packet)
                index_value = numeric(index['value'])
                live.require(0 <= index_value < 1 << (8 * index['size']) and
                             f', suspension-index={index_value},' in text, 'copied suspension-index agrees with summary', (index_value, text))
                live.require(all(value > 0 for value in pointers), 'active coroutine caller has nonnull resume/destroy entries', pointers)
                live.require(re.match(r'(?:Clang|GCC) coroutine frame, state=unknown \(', text),
                             'active body retains conservative lifecycle boundary', text)
                probe['copied_fields'] = dict(resume=resume, destroy=destroy, index=index)
                probe['fields_qualified'] = True
                record['typed_frame_fields_qualified'] = True
                record['state_boundary_verified'] = True
            promise_name = '__promise' if clang else '_Coro_promise'
            promise_indices = [i for i in nodes[0]['children'] if nodes[i]['name'] == promise_name]
            live.require(len(promise_indices) <= 1, 'unique actual promise child', packet)
            promise_ok = False
            if promise_indices:
                value = child(promise_indices[0], 'current_value')
                if value and value['copied_value']:
                    live.require(value['size'] == 4 and numeric(value['value']) == expected_promise,
                                 'actual promise payload at this body phase', (body_phase, value))
                    probe['promise_value'] = value
                    promise_ok = True
                    record['typed_frame_promise_qualified'] = True
            local_values = {}
            for name, expected in (('previous', expected_previous), ('current', expected_current)):
                reply = client.evaluate(f"print-frame {actual['thread']} {actual['stop']} {ordinal} {name}").decode()
                copied = dap.parse_source_object_display(reply, actual['stop'], actual['thread'])
                probe.setdefault('local_packets', {})[name] = reply
                if copied and len(copied) == 1 and copied[0]['copied_value']:
                    value = numeric(copied[0]['value'])
                    live.require(value == expected, 'actual saved coroutine local at this body phase', (body_phase, name, value))
                    local_values[name] = value
            locals_ok = len(local_values) == 2
            record['typed_frame_locals_qualified'] |= locals_ok
            complete = bool(fields_ready and promise_ok and locals_ok)
            probe['typed_frame_qualified'] = complete
            record['typed_frame_qualified'] |= complete
    if not record['typed_frame_qualified']:
        record['typed_frame_boundary'] = 'Actual typed frame fields, promise or locals incomplete; ABI/state qualification is not synthesized.'
    return record


def rust(client, frame, record):
    def copied(name):
        body=evaluate(client,frame,name)
        record['values'].append(dict(expression=name,context='watch',response=body))
        return body
    def vector(name, expected):
        for context in ('watch','hover','variables'):
            body=evaluate(client,frame,name,context)
            record['values'].append(dict(expression=name,context=context,response=body))
            live.require(has_size(body['result'],len(expected)) and body['variablesReference']>0,
                         'real Rust Vec has its logical element count',(name,body))
            rows=members(client,body['variablesReference'],filter='indexed')
            live.require([r['name'] for r in rows]==[f'[{i}]' for i in range(len(expected))],
                         'real Rust Vec indexed children',(name,rows))
            live.require([numeric(r['value']) for r in rows]==expected,
                         'real Rust Vec uses owned template element type rather than erased storage bytes',(name,rows))
            live.require(members(client,body['variablesReference'],filter='indexed',start=1,count=2)==rows[1:3],
                         'real Rust Vec indexed pagination')
    vector('*values',list(range(1,47,3)))
    vector('slice',[7,10,13,16])
    for name in ('*title','text'):
        body=copied(name)
        live.require('string length=22' in body['result'] and 'native Rust ' in body['result'] and
                     r'\xce\xbb' in body['result'] and ' printer' in body['result'],
                     'real Rust UTF8 string/slice preserves copied bytes',(name,body))
    body=copied('*words')
    live.require(has_size(body['result'],3) and body['variablesReference']>0,'real nested Rust Vec<String>',body)
    rows=members(client,body['variablesReference'],filter='indexed')
    live.require(len(rows)==3,'three real nested Rust Strings',rows)
    for row,length,text in zip(rows,[5,6,2],['first','second',r'\xce\xbb']):
        live.require('string length='+str(length) in row['value'] and text in row['value'],
                     'nested Rust String uses owned String type rather than erased u8',(row,length,text))
    body=copied('*units')
    live.require(has_size(body['result'],7) and 'capacity=4294967295' in body['result'] and body['variablesReference']>0,
                 'real Rust zero-sized Vec has logical count and Rust effective capacity',body)
    rows=members(client,body['variablesReference'],filter='indexed')
    live.require([r['name'] for r in rows]==[f'[{i}]' for i in range(7)] and
                 all(r['type']=='()' for r in rows),'zero-sized Rust elements retain their unit type',rows)

def session(args, folder, root):
    folder.mkdir(mode=0o700)
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'actual_pretty_producer_dap')
    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'debug' if args.case in ('coroutine','rust') else 'run', *mode, *(['--wasm-feature-enable-exceptions'] if args.wasm_exceptions else []), '-Rct', '0', '-Rllvm-call-stack', args.policy, '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = dict(passed=False, case=args.case, compiler_id=args.compiler_id, policy=args.policy, values=[], markers=[], actual_dap_stdio=True)
    try:
        gate = dap.UnixBroker(str(server.directory))
        try:
            first = gate.request('status' if args.case in ('coroutine','rust') else 'pause')
            live.require('error:' not in first and 'guest exited:' not in first, 'actual queued first pause', first)
        finally:
            gate.close()
        client_folder = folder / 'client'; client_folder.mkdir(mode=0o700)
        client = base.TypedClient(root / 'tools/debug/dap_adapter.py', server.directory, client_folder, 'source')
        server.clients.append(client)
        client.evaluate('set print elements 200')
        names = (['pretty_rust_ready'] if args.case == 'rust' else ['pretty_coroutine_ready', 'pretty_coroutine_body_ready', 'pretty_coroutine_empty_ready'] if args.case == 'coroutine' else
                 ['pretty_stateful_ready'] if args.case == 'stateful' else ['pretty_sequences_ready'])
        functions = {name: function_identity(args.wasm, name) for name in names}
        points = []
        if args.case == 'rust':
            # A source breakpoint is after the compiler's prologue. A raw Wasm
            # function-entry stop intentionally precedes argument stack stores.
            line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1)
                        if 'std::hint::black_box((values.len()' in text)
            bps = client.request('setBreakpoints', {'source': {'path': str(args.source)},
                                 'breakpoints': [{'line': line}]})['body']['breakpoints']
            live.require(len(bps)==1 and bps[0]['verified'] and bps[0]['line']==line,
                         'actual Rust compiler source line breakpoint after prologue', bps)
            record['source_breakpoint'] = bps[0]
        else:
            for name, function in functions.items():
                reply = client.evaluate(f'breakwasm 0 {function} 0')
                match = re.search(rb'breakpoint ([0-9]+)', reply)
                live.require(match is not None and b'registered' in reply, 'actual compiler marker function breakpoint', (name, reply))
                points.append(int(match[1]))
        record['function_identities'] = functions
        if args.case in ('coroutine','rust'):
            # The late run-mode endpoint may pause after the initial phase.
            # The explicit debug launch arms the existing controller before
            # starting Guest; this is a prepared state, without a fabricated PC.
            live.require('prepared; no Wasm instruction executed' in first,
                         'actual explicit debug launch prepared before first Wasm instruction', first)
            record['prepared_first_pause'] = first
            client.evaluate('continue')
        else:
            client.request('continue', {'threadId': live.actual_location(client)['thread']})
        if args.case == 'coroutine':
            expected = [('pretty_coroutine_ready', 0), ('pretty_coroutine_body_ready', None), ('pretty_coroutine_ready', 1),
                        ('pretty_coroutine_body_ready', None), ('pretty_coroutine_ready', 2), ('pretty_coroutine_ready', 3),
                        ('pretty_coroutine_empty_ready', None)]
            body_qualifications = []
            body_phase = 0
            for name, phase in expected:
                position = wait_stop(client)
                live.require(position['function'] == functions[name], 'actual coroutine phase function order', (name, position))
                if name == 'pretty_coroutine_body_ready':
                    body_phase += 1
                    marker = coroutine_body(client, args, position, dap, body_phase)
                    body_qualifications.append(marker['typed_frame_qualified'])
                else:
                    marker = coroutine_handle_marker(client, args, functions[name], phase, name == 'pretty_coroutine_empty_ready')
                record['markers'].append(marker)
                if name != expected[-1][0]:
                    client.request('continue', {'threadId': live.actual_location(client)['thread']})
            qualified = len(body_qualifications) == 2 and all(body_qualifications)
            record['typed_frame_qualified'] = qualified
            record['typed_lifecycle_state_qualified'] = False
            live.require(not args.require_typed_frame or qualified, 'requested actual compiler-frame qualification', record['markers'])
        else:
            position = wait_stop(client)
            live.require(position['function'] == functions[names[0]], 'actual sequence marker function stop', position)
            for _ in range(128):
                selected, stack = frames(client, position, args.source, names[0])
                if len(selected) == 1:
                    break
                client.evaluate(f"step wasm {position['thread']}")
                position = live.actual_location(client)
                live.require(position['function'] == functions[names[0]], 'source frame materializes inside marker', position)
            else:
                raise AssertionError('actual sequence marker source frame unavailable')
            record['markers'].append(dict(position=position, frame=selected[0], stack=stack))
            rust(client, selected[0], record) if args.case == 'rust' else sequences(client, selected[0], record, args.case == 'stateful')
        if args.case == 'rust':
            client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        for point in points:
            client.evaluate(f'delete {point}')
        continuation = {'threadId': live.actual_location(client)['thread']}
        if record['values']:
            old = record['values'][0]['response']['variablesReference']
            replies = [client.response(number) for number in client.queue([
                ('continue', continuation), ('variables', {'variablesReference': old})])]
            live.require(replies[0].get('success') and not replies[1].get('success'),
                         'real producer execution retires copied sequence reference', replies)
            record['reference_retirement'] = replies
        else:
            client.request('continue', continuation)
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                live.require(b'guest exited: 0' in exited, 'actual producer natural oracle exit', exited)
                break
        else:
            raise AssertionError('actual producer did not finish')
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original producer broker OS wait')
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
    parser.add_argument('--case', choices=('sequences', 'stateful', 'coroutine', 'rust'), required=True)
    parser.add_argument('--compiler-id', required=True)
    parser.add_argument('--producer-receipt', type=Path)
    parser.add_argument('--require-typed-frame', action='store_true')
    parser.add_argument('--require-handle-abi', action='store_true')
    parser.add_argument('--handle-abi', choices=('LLVM18.1.2/libc++18.1/Wasm32','LLVM23.1.0/libc++23.1/Wasm32'), default='LLVM18.1.2/libc++18.1/Wasm32')
    parser.add_argument('--wasm-exceptions', action='store_true', help='enable genuine Wasm exception producer support')
    parser.add_argument('--cgroup', required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--fixed-llvm-full', action='store_true', help='product has one fixed LLVM-full runtime; omit unavailable selector flags')
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    args = parser.parse_args()
    live.require(0 < len(args.compiler_id.encode()) <= 4096, 'finite exact compiler/version/target/flags identity')
    live.require(sys.platform == 'linux', 'Linux existing-cgroup runner only')
    live.require(Path('/proc/self/cgroup').read_text() == '0::' + args.cgroup + '\n', 'actual original test cgroup')
    cgroup = Path('/sys/fs/cgroup') / args.cgroup.lstrip('/')
    live.require((cgroup / 'memory.max').read_text().strip() == str(64 << 30), 'existing 64GiB memory limit')
    live.require((cgroup / 'memory.swap.max').read_text().strip() == '0', 'existing swap0')
    root = Path(__file__).resolve().parents[2]
    for name in ('uwvm', 'wasm', 'source'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    identity = {'sequences': 'uwvm-dbg-pretty-sequences-v3', 'stateful': 'uwvm-dbg-pretty-stateful-v2', 'coroutine': 'uwvm-dbg-pretty-coroutine-v1', 'rust': 'uwvm-dbg-pretty-rust-native-v2'}[args.case]
    live.require(identity in args.source.read_text(), 'actual versioned producer fixture source', identity)
    paths = [Path(__file__), Path(base.__file__), Path(live.__file__), args.uwvm, args.wasm, args.source,
             root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py',
             root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', root / 'test/0017.runtime/run_debug_source_step_cli.py']
    if args.producer_receipt:
        paths.append(args.producer_receipt.resolve(strict=True))
    pins = {str(path): live.sha(path) for path in paths}
    args.out.mkdir(mode=0o700)
    result = dict(passed=False, pins=pins, fixture_identity=identity, compiler_id=args.compiler_id,
                  producer_receipt=str(args.producer_receipt) if args.producer_receipt else None,
                  scope='Actual unchanged producer guest stop, broker and DAP stdio. Compiler/native qualification is separate; typed coroutine frame qualification is explicit.')
    try:
        record = session(args, args.out / 'session-0000', root)
        result.update(passed=record['passed'], values=len(record['values']), markers=len(record['markers']),
                      typed_frame_qualified=record.get('typed_frame_qualified'))
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['passed'] and result['pins_after'] == pins
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual versioned pretty producer acceptance', result)
    print('PASS actual pretty producer', args.case, result['values'], 'values', result['markers'], 'markers')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
