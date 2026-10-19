#!/usr/bin/env python3
"""Actual compiler-owned custom containers, broker configuration and DAP retirement.

Compile fixtures/source_print_configuration.cc unchanged with -O0 -g (the
full-product builder's configuration.wasm is suitable). This is a real guest
stop/stdio DAP test, not a copied-DATA protocol or throughput control.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import run_dap_current_broker as live
import run_dap_tinygo_objects as base

sys.dont_write_bytecode = True


def session(args, folder, root):
    folder.mkdir(mode=0o700)
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'configuration_dap_protocol')
    mode = [] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'run', *mode, '-Rct', '0', '-Rllvm-call-stack', 'instruction',
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = {'passed': False, 'checks': [], 'actual_dap_stdio': True}
    try:
        gate = dap.UnixBroker(str(server.directory))
        try:
            first = gate.request('pause')
            live.require('error:' not in first and 'guest exited:' not in first, 'actual queued initial pause', first)
        finally:
            gate.close()
        client_folder = folder / 'client';client_folder.mkdir(mode=0o700)
        client = base.TypedClient(root / 'tools/debug/dap_adapter.py', server.directory, client_folder, 'source')
        server.clients.append(client)
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_CONFIGURATION_STOP' in text)
        points = client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'actual configuration fixture source breakpoint', points)
        client.request('continue', {'threadId': live.actual_location(client)['thread']})
        for _ in range(64):
            stopped = client.evaluate('wait')
            if b'stopped: breakpoint' in stopped:
                break
            live.require(b'guest exited:' not in stopped, 'guest reached configuration breakpoint', stopped)
        else:
            raise AssertionError('configuration source stop timeout')
        position = live.actual_location(client)

        def check(label, good, evidence=None):
            live.require(good, label, evidence)
            record['checks'].append({'case': label, 'passed': True})

        def frame():
            frames = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
            selected = [row for row in frames if row.get('source', {}).get('path') == str(args.source)
                        and row.get('line') == line and 'pretty_configuration_probe' in row['name']]
            live.require(len(selected) == 1, 'real current configuration source frame', frames)
            return selected[0]

        def value(name):
            current = frame()
            response = client.request('evaluate', {'expression': name, 'context': 'watch', 'frameId': current['id']})['body']
            check('read-only current ' + name, response.get('presentationHint', {}).get('attributes') == ['readOnly'] and
                  'evaluateName' not in response and 'memoryReference' not in response, response)
            return current, response

        def configure(command):
            response = client.request('evaluate', {'expression': command, 'context': 'repl'})['body']['result']
            check('short successful mutation acknowledgement', len(response.encode()) < 256 and not response.startswith('error:'), response)
            return response

        def indexed(reference):
            return client.request('variables', {'variablesReference': reference, 'filter': 'indexed'})['body']['variables']

        old_frame, raw = value('values')
        check('unregistered actual user container retains raw fields', 'size=3' not in raw['result'] and raw['variablesReference'] > 0, raw)
        configure('pretty-printer sequence values_printer "*Sequence<*>" data length capacity')
        check('configuration retires old source object reference', not client.request('variables', {'variablesReference': raw['variablesReference']}, success=False)['success'])
        check('configuration retires old source frame', not client.request('evaluate', {'expression': 'values', 'context': 'watch', 'frameId': old_frame['id']}, success=False)['success'])
        _, full = value('values')
        check('registered actual typed sequence and guest copies', 'size=3 capacity=3' in full['result'] and
              [row['value'] for row in indexed(full['variablesReference'])] == ['7', '8', '9'], full)
        configure('pretty-printer string text_printer "*Text" data length')
        configure('pretty-printer pointer owner_printer "*Owner" ptr')
        configure('pretty-printer optional maybe_printer "*Maybe" engaged value')
        for name, fragments in (('text', ('length=12', 'bounded text')), ('owner', ('stored pointer', '8')),
                                ('present', ('engaged', '9')), ('absent', ('none',))):
            _, body = value(name)
            check('actual custom ' + name, all(fragment in body['result'] for fragment in fragments), body)
        configure('pretty-printer disable builtin')
        _, body = value('values');check('custom profile independent from builtin group', 'size=3' in body['result'], body)
        configure('set print elements 0')
        _, body = value('values');check('summary-only configuration copies no synthetic elements', 'size=3' in body['result'] and indexed(body['variablesReference']) == [], body)
        configure('set print elements 1')
        _, body = value('values');check('one-element configuration shares actual source read budget', [row['value'] for row in indexed(body['variablesReference'])] == ['7'], body)
        shown = client.request('evaluate', {'expression': 'show print elements', 'context': 'repl'})['body']['result']
        check('show preserves existing copied reference', 'limit is 1' in shown and len(indexed(body['variablesReference'])) == 1, shown)
        configure('pretty-printer disable values_printer')
        _, body = value('values');check('disable retains raw fallback', 'size=3' not in body['result'], body)
        configure('pretty-printer enable values_printer')
        configure('set print elements 200')
        _, body = value('values');check('enable and restore original budget', len(indexed(body['variablesReference'])) == 3, body)
        raw_packet = client.request('evaluate', {'expression': f"print /r {position['thread']} {position['stop']} values", 'context': 'repl'})['body']['result']
        nodes = dap.parse_source_object_display(raw_packet + '\n', position['stop'], position['thread'])
        check('raw override bypasses configured printer', nodes is not None and nodes[0].get('pretty') is None, raw_packet)
        # A complete legal 64-profile registry can exceed a transport packet.
        # Successful writes stay short; list preserves complete escaped rows.
        quoted_pattern = '"' + '\\"' * 512 + '"'
        for i in range(60):
            name = 'long_' + str(i) + 'n' * 110
            configure('pretty-printer sequence ' + name + ' ' + quoted_pattern + ' ' + 'd' * 128 + ' ' + 's' * 128 + ' ' + 'c' * 128)
        listing = client.request('evaluate', {'expression': 'pretty-printer list', 'context': 'repl'})['body']['result']
        trailer = re.search(r'(?m)^pretty-printers shown=([0-9]+) total=64 truncated=true$', listing)
        check('actual 64-profile list is bounded with explicit complete prefix', len(listing.encode()) <= 61440 and trailer is not None and 0 < int(trailer[1]) < 64, listing[-300:])
        configure('pretty-printer delete ' + 'long_0' + 'n' * 110)
        check('broker configuration never advances the actual guest', live.actual_location(client) == position, position)
        client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        client.request('continue', {'threadId': position['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                check('original custom-container guest natural oracle', b'guest exited: 0' in exited, exited)
                break
        else:
            raise AssertionError('configuration fixture did not finish')
        server.child.wait(timeout=15)
        check('original broker OS wait', server.child.returncode == 0, server.child.returncode)
        record.update(passed=True, position=position)
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw_log = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw_log,
                         'original first-pause gate proof', raw_log)
            if record['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw_log, 'original guest OS wait proof', raw_log)
        finally:
            (folder / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return base.main()
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source', type=Path, default=root / 'test/0018.debugger/fixtures/source_print_configuration.cc')
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    for name in ('uwvm', 'wasm', 'source'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    admission = base.load(root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'configuration_admission')
    proof = admission.admit()
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    pins = {str(path): live.sha(path) for path in (Path(__file__), args.uwvm, args.wasm, args.source,
            root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py')}
    result = {'passed': False, 'admission': proof, 'pins_before': pins, 'scope': 'actual custom-container stop, HOST registry through UnixBroker and stdio DAP'}
    try:
        result['session'] = session(args, args.out / 'actual', root)
        result['passed'] = result['session']['passed']
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        if result['pins_after'] != pins:
            result['passed'] = False
        result['admission_after'] = admission.admit()
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual printer configuration acceptance', result)
    print('PASS actual printer configuration', len(result['session']['checks']), 'checks')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
