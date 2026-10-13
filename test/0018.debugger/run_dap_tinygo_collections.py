#!/usr/bin/env python3
"""Real Linux TinyGo collection DAP acceptance, not an in-process adapter mock.

The original R35 fresh-product receipt and original producer files are required.
A host-only launch gate queues an authenticated pause before the unchanged VM
executes; it neither reads the packet nor modifies the Wasm/DWARF. Only execute
under the designated cgroup supervisor. Actual VS Code UI is outside this test.
"""
from __future__ import annotations
import argparse, importlib.util, json, os, re, select, sys, time
from pathlib import Path
import run_dap_current_broker as live

sys.dont_write_bytecode = True


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def scalar(text):
    values = re.findall(r'(?m)^(?!source-value\b)[^\r\n]*\b(?:value=|[ui](?:8|16|32|64)=)(-?\d+)\b', text)
    live.require(len(values) == 1 and 'unavailable' not in text and 'error:' not in text,
                 'one actual finite source scalar', text)
    return int(values[0])


class TypedClient(live.Client):
    """Real stdio framing with the IDE's type-display capability negotiated."""
    def request(self, command, args=None, success=True):
        if command == 'initialize':
            args = dict(args or {}, supportsVariableType=True)
        reply = super().request(command, args, success)
        if command == 'initialize':
            live.require(reply['body'].get('supportsEvaluateForHovers') is True, 'real hover capability advertised')
        return reply


def session(a, folder, root):
    folder.mkdir(mode=0o700)
    dap = load(root / 'tools/debug/dap_adapter.py', 'actual_tinygo_dap')
    mode = [] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'run', *mode, '-Rct', '0', '-Rllvm-call-stack', a.policy,
          '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)]
    server = live.BrokerSession(a.uwvm, Path(__file__).resolve(),
                              root / 'tools/debug/dap_adapter.py', vm, folder)
    record = dict(passed=False, policy=a.policy, values=[], rejections=[], dap_transport='real stdio')
    client = None
    try:
        # An actual authenticated host connection sends pause as its FIRST
        # packet. The launch gate observes POLLIN without consuming credentials.
        initial = dap.UnixBroker(str(server.directory))
        try:
            pause_started = time.monotonic()
            first = initial.request('pause')
            live.require('error:' not in first and 'guest exited:' not in first,
                         'actual first queued pause refused', first)
            record['first_pause'] = first
            record['first_pause_seconds'] = time.monotonic() - pause_started
        finally:
            initial.close()
        client_folder = folder / 'session-0'; client_folder.mkdir(mode=0o700)
        client = TypedClient(root / 'tools/debug/dap_adapter.py', server.directory, client_folder, 'source')
        server.clients.append(client)
        line = next(i for i, s in enumerate(a.source.read_text().splitlines(), 1)
                    if 'GO_COLLECTION_READY' in s)
        point = client.request('setBreakpoints', {'source': {'path': str(a.source)},
                                                'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(point) == 1 and point[0]['verified'], 'real source breakpoint', point)
        record['breakpoint'] = point[0]
        client.request('continue', {'threadId': live.actual_location(client)['thread']})
        for _ in range(64):
            stopped = client.evaluate('wait')
            if b'stopped: breakpoint' in stopped:
                break
            live.require(b'guest exited:' not in stopped, 'finite guest exited before source stop', stopped)
        else:
            raise AssertionError('actual TinyGo source stop timeout')
        position = live.actual_location(client)
        stack = client.request('stackTrace', {'threadId': position['thread']})['body']
        frames = stack['stackFrames']
        rows = [f for f in frames if f.get('source', {}).get('path') == str(a.source)
                and f.get('line') == line and 'collectionProbe' in f['name']]
        live.require(len(rows) == 1, 'original TinyGo DAP source frame/line', frames)
        frame = rows[0]
        live.require(frame.get('canRestart') is False and
                     frame.get('instructionPointerReference') ==
                     f"wasm:0:{position['function']}:{position['offset']}",
                     'actual guest physical frame only', frame)
        scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
        source = [s for s in scopes if s['name'] == 'Source variables']
        live.require(len(source) == 1, 'actual source scope', scopes)
        ref = source[0]['variablesReference']
        variables = client.request('variables', {'variablesReference': ref})['body']['variables']
        names = ('mapLen', 'namedLen', 'bufferedLen', 'bufferedCap', 'closedLen', 'closedCap')
        oracle = {}
        for name, expected in zip(names, (3, 2, 2, 4, 1, 3)):
            matches = [v for v in variables if v['name'] == name]
            live.require(len(matches) == 1 and matches[0]['value'] == f'i32={expected}' and
                         matches[0]['variablesReference'] == 0 and 'memoryReference' not in matches[0],
                         'actual compiler argument through DAP variables', (name, variables))
            oracle[name] = expected
        page = client.request('variables', {'variablesReference': ref, 'start': 1, 'count': 2})['body']['variables']
        live.require(page == variables[1:3], 'actual source pagination')
        live.require(client.request('variables', {'variablesReference': ref, 'filter': 'indexed'})['body']['variables'] == [],
                     'source names are not invented indexed entries')
        cases = [('len(box.Numbers)', oracle['mapLen']), ('len(box.Named)', oracle['namedLen']),
                 ('len(box.Empty)', 0), ('len(box.NilMap)', 0),
                 ('len(box.Buffered)', oracle['bufferedLen']), ('cap(box.Buffered)', oracle['bufferedCap']),
                 ('len(box.NamedChan)', 1), ('cap(box.NamedChan)', 5),
                 ('len(box.Closed)', oracle['closedLen']), ('cap(box.Closed)', oracle['closedCap']),
                 ('len(box.EmptyChan)', 0), ('cap(box.EmptyChan)', 0),
                 ('len(box.NilChan)', 0), ('cap(box.NilChan)', 0),
                 ('len(box.Receive)', 2), ('cap(box.Receive)', 4), ('len(box.Send)', 2), ('cap(box.Send)', 4),
                 ('len(box.Numbers)+cap(box.Buffered)', 7), ('cap(box.Buffered)-len(box.Buffered)', 2),
                 ('len(box.NilMap)+cap(box.NilChan)', 0), ('(cap(box.Buffered))-1', 3)]
        for context in ('watch', 'hover', 'variables'):
            for expression, expected in cases:
                response = client.request('evaluate', {'expression': expression, 'context': context, 'frameId': frame['id']})['body']
                if a.expect_display_packet:
                    actual = scalar(response['result'])
                else:
                    actual = int(response['result'])
                    expected_type = 'signed integer' if expression in dict(cases[18:]) else 'int'
                    live.require(response.get('type') == expected_type and
                                 response.get('presentationHint', {}).get('attributes') == ['readOnly'],
                                 'native-style finite Go builtin scalar metadata', response)
                live.require(actual == expected and response['variablesReference'] == 0 and
                             'memoryReference' not in response,
                             'actual TinyGo DAP builtin value', (expression, response))
                record['values'].append(dict(context=context, expression=expression, expected=expected, response=response))
        for expression in ('cap(box.Numbers)', 'cap(box.NilMap)', 'len(box)',
                           'len(box.Numbers,box.Buffered)', 'len(box.Numbers=1)',
                           'len(box.Numbers);continue', 'continue', 'len(box.Numbers)\ncontinue'):
            negative_frame = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames'][0]
            reply = client.request('evaluate', {'expression': expression, 'context': 'watch',
                                               'frameId': negative_frame['id']}, success=False)
            record['rejections'].append(dict(expression=expression, message=reply.get('message')))
        # A refused backend expression retires copied frames. Refresh from the
        # still-real stop instead of assuming an old opaque ID has survived.
        fresh = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames'][0]
        live.require(fresh['id'] != frame['id'], 'opaque reference was not reused')
        fresh_scopes = client.request('scopes', {'frameId': fresh['id']})['body']['scopes']
        fresh_ref = next(s['variablesReference'] for s in fresh_scopes if s['name'] == 'Source variables')
        client.request('setBreakpoints', {'source': {'path': str(a.source)}, 'breakpoints': []})
        queued = client.queue([('stepIn', {'threadId': position['thread'], 'granularity': 'instruction'}),
                               ('evaluate', {'expression': 'len(box.Numbers)', 'context': 'watch', 'frameId': fresh['id']}),
                               ('variables', {'variablesReference': fresh_ref}),
                               ('scopes', {'frameId': fresh['id']})])
        responses = [client.response(n) for n in queued]
        live.require(responses[0]['success'] and all(not r['success'] for r in responses[1:]),
                     'queued step retired real frame and scope before next requests', responses)
        after = live.actual_location(client)
        live.require(after['stop'] > position['stop'], 'actual step produced a new stop')
        client.request('continue', {'threadId': after['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                live.require(b'guest exited: 0' in exited, 'original compiler self-check failed', exited)
                break
        else:
            raise AssertionError('original guest did not exit')
        # The external endpoint retires when the guest naturally exits. Wait
        # the original launcher; a post-exit quit would race that closed socket.
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original broker/guest normal exit and wait')
        record.update(passed=True, position=position, stack=stack, scopes=scopes,
                      variables=variables, compiler_oracles=oracle, queued_retirement=responses,
                      natural_guest_exit=exited.decode())
    except BaseException as error:
        record.update(passed=False, error=repr(error))
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw = (folder / 'broker.raw').read_text()
            record['queued_before_exec'] = 'TinyGo DAP gate: authenticated first command queued before VM exec' in raw
            live.require(record['queued_before_exec'], 'original launch gate evidence absent', raw)
            if record['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw, 'actual original guest OS wait missing', raw)
                record['original_guest_wait_returncode'] = 0
        except BaseException as error:
            record.update(passed=False, cleanup_error=repr(error))
            raise
        finally:
            (folder / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


def main():
    root = Path(__file__).resolve().parents[2]
    if len(sys.argv) > 1 and sys.argv[1] == '__vm_gate':
        argv = sys.argv[2:]
        fd = int(argv[argv.index('--debug-jit-control-fd') + 1])
        poll = select.poll(); poll.register(fd, select.POLLIN)
        live.require(any(flags & select.POLLIN for _, flags in poll.poll(8000)), 'real first pause was not queued')
        print('TinyGo DAP gate: authenticated first command queued before VM exec', flush=True)
        os.execv(argv[0], argv)
    if len(sys.argv) > 1 and sys.argv[1] == 'serve':
        server = load(root / 'tools/debug/secure_server.py', 'actual_tinygo_secure_server')
        original = server.subprocess.Popen
        owned = []
        def launch(argv, *args, **kwargs):
            child = original([sys.executable, str(Path(__file__).resolve()), '__vm_gate', *argv], *args, **kwargs)
            owned.append(child)
            return child
        server.subprocess.Popen = launch
        result = server.main()
        live.require(len(owned) == 1, 'one actual finite-fixture launch owner')
        exited = owned[0].wait(timeout=15)
        print(f'TinyGo DAP original guest wait: {exited}', flush=True)
        live.require(result != 0 or exited == 0, 'normal broker return requires original guest exit 0', exited)
        return result
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'build-receipt', 'wasm', 'source', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--expect-display-packet', action='store_true')
    ap.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    ap.add_argument('--seconds', type=int, default=0)
    a = ap.parse_args()
    live.require(sys.platform == 'linux' and 0 <= a.seconds <= 900, 'bounded Linux runner required')
    import subprocess
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    build = json.loads(a.build_receipt.read_text())
    live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after']
                 and live.sha(a.uwvm) == build['binary_sha256'], 'original R35 fresh full-product qualification')
    a.out.mkdir(mode=0o700)
    paths = [Path(__file__), a.uwvm, a.build_receipt, a.wasm, a.source,
             root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py',
             Path(live.__file__), Path(live.source_cli.__file__),
             Path(live.source_cli.metadata_cli.__file__), root / 'tools/ci/require_wasm3_test_cgroup.sh']
    pins = {str(p): live.sha(p) for p in paths}
    result = dict(passed=False, product_source_id=build['source_id'], pins=pins,
                  actual_vscode_ui=False, full_go_parity=False, sessions=[])
    begin = time.monotonic()
    try:
        while True:
            record = session(a, a.out / f'session-{len(result["sessions"]):04}', root)
            result['sessions'].append(dict(passed=record['passed'], policy=record['policy'], comparisons=len(record['values'])))
            if time.monotonic() - begin >= a.seconds:
                break
            a.policy = 'unwind' if a.policy == 'instruction' else 'instruction'
            live.require(len(result['sessions']) < 128, 'bounded session count')
        result.update(passed=True, elapsed_seconds=time.monotonic()-begin)
    finally:
        result['pins_after'] = {p: live.sha(Path(p)) for p in pins}
        if result['pins_after'] != pins:
            result['passed'] = False
        (a.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'real TinyGo DAP qualification failed', result)
    print('PASS real TinyGo DAP', len(result['sessions']), 'sessions', sum(s['comparisons'] for s in result['sessions']), 'values')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
