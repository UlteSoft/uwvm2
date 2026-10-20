#!/usr/bin/env python3
"""Real stopped C++ reference parameters through UnixBroker commands and DAP Watch.

Uses unchanged -O0 -g fixtures/pretty_references.cc (builder references.wasm).
Requires genuine reference carrier metadata; no fabricated layout or VM stop.
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
    dap = base.load(root / 'tools/debug/dap_adapter.py', 'actual_reference_protocol')
    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['-m', 'run', *mode, '-Rct', '0', '-Rllvm-call-stack', 'instruction',
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / 'tools/debug/dap_adapter.py', vm, folder)
    record = {'passed': False, 'checks': [], 'queries': [], 'actual_dap_stdio': True}
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
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_REFERENCES_STOP' in text)
        points = client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'actual reference parameter source breakpoint', points)
        # The registered launch pause can precede every Wasm participant.
        # Setup/introspection can retire cached stop authority.
        # Refresh the actual stopped status before domain continuation.
        client.request('threads')
        client.request('continue', {'threadId': 0})
        for _ in range(64):
            stopped = client.evaluate('wait')
            if b'stopped: breakpoint' in stopped:
                break
            live.require(b'guest exited:' not in stopped, 'real reference source stop reached', stopped)
        else:
            raise AssertionError('real reference fixture stop timeout')
        position = live.actual_location(client)

        def check(label, good, evidence=None):
            live.require(good, label, evidence)
            record['checks'].append({'case': label, 'passed': True})

        def query(expression, raw=False):
            text = client.evaluate(f"print {'/r ' if raw else ''}{position['thread']} {position['stop']} {expression}").decode()
            record['queries'].append({'expression': expression, 'raw': raw, 'actual_packet': text})
            return text

        def tree(expression, raw=False):
            packet = query(expression, raw)
            nodes = dap.parse_source_object_display(packet, position['stop'], position['thread'])
            live.require(nodes is not None and nodes, 'actual copied reference/referent tree', packet)
            return nodes

        for name, values in (('ref', [7, 8, 9]), ('alias', [7, 8, 9]), ('mut', [10, 11, 12]), ('rvalue', [13, 14, 15])):
            nodes = tree(name)
            indexed = [nodes[i] for i in nodes[0]['children'] if nodes[i]['indexed']]
            check('transparent real ' + name + ' vector reference', nodes[0].get('pretty') is not None and
                  'size=3' in nodes[0]['pretty'] and [row['value'] for row in indexed] == list(map(str, values)), nodes)
            check('transparent real ' + name + '[1]', base.scalar(query(name + '[1]')) == values[1])
        nodes = tree('text')
        check('transparent string reference', nodes[0].get('pretty') is not None and 'reference text' in nodes[0]['pretty'], nodes)
        check('transparent static array reference index', base.scalar(query('fixed[1]')) == 17)
        check('transparent scalar reference', base.scalar(query('scalar')) == 19)
        check('scalar reference arithmetic takes one canonical operand capture', base.scalar(query('scalar_ref+1')) == 20)
        check('existing reference member path does not regress', base.scalar(query('object.value')) == 20)
        vector_bytes = base.scalar(query('vector_bytes'))
        scalar_bytes = base.scalar(query('scalar_bytes'))
        check('sizeof reference uses compiler referent extent', base.scalar(query('sizeof(ref)')) == vector_bytes)
        check('sizeof indexed reference uses metadata only', base.scalar(query('sizeof(ref[1])')) == scalar_bytes)
        check('sizeof scalar reference uses referent metadata', base.scalar(query('sizeof(scalar_ref)')) == scalar_bytes)
        indexed_type = client.evaluate(f"ptype {position['thread']} {position['stop']} ref[1]").decode()
        declared_type = client.evaluate(f"ptype {position['thread']} {position['stop']} ref").decode()
        record['queries'].extend([{'expression': 'ptype ref[1]', 'actual_packet': indexed_type},
                                  {'expression': 'ptype ref', 'actual_packet': declared_type}])
        def type_root(packet):
            rows = packet.splitlines()
            check('ptype packet belongs to actual stop', len(rows) >= 3 and
                  rows[0].startswith(f"source-type stop={position['stop']} name=") and rows[-1] == 'source-type end', packet)
            match = re.fullmatch(r'  type-node 0 parent=none depth=0 name=.*? type=(.+) kind=([a-z-]+) byte-offset=0 byte-size=([0-9]+)', rows[1])
            live.require(match is not None, 'complete actual type root', packet)
            return match.groups()
        indexed_name, indexed_kind, indexed_bytes = type_root(indexed_type)
        declared_name, declared_kind, _ = type_root(declared_type)
        check('ptype indexed reference transparently resolves int', indexed_name == 'int' and
              indexed_kind == 'scalar' and int(indexed_bytes) == scalar_bytes, indexed_type)
        check('bare ptype preserves reference declaration identity', declared_kind == 'pointer' and
              '&' in declared_name and 'vector' in declared_name, declared_type)
        raw = tree('ref', raw=True)
        check('raw keeps C++ transparency while bypassing printers', raw[0].get('pretty') is None and
              raw[0]['size'] == vector_bytes and not raw[0]['value'].startswith('guest:'), raw)
        pointer = tree('pointer')
        check('ordinary pointer still displays its guest offset', pointer[0].get('pretty') is None and
              pointer[0]['value'].startswith('guest:') and not pointer[0]['children'], pointer)
        explicit = tree('*pointer')
        check('explicit ordinary pointer dereference remains available', explicit[0].get('pretty') is not None and 'size=3' in explicit[0]['pretty'], explicit)
        frames = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames']
        frame = next(row for row in frames if row.get('source', {}).get('path') == str(args.source) and
                     row.get('line') == line and 'pretty_reference_probe' in row['name'])
        watched = client.request('evaluate', {'expression': 'ref', 'context': 'watch', 'frameId': frame['id']})['body']
        check('DAP Watch reference expands its actual copied referent', 'size=3' in watched['result'] and
              watched['variablesReference'] > 0 and watched.get('presentationHint', {}).get('attributes') == ['readOnly'] and
              'memoryReference' not in watched and 'evaluateName' not in watched, watched)
        for expression in ('ref[-1]', 'ref[3]'):
            rejected = client.request('evaluate', {'expression': f"print {position['thread']} {position['stop']} {expression}", 'context': 'repl'}, success=False)
            check('out-of-range reference index refused', not rejected['success'], rejected)
        check('all reference queries preserve the genuine stop', live.actual_location(client) == position)
        client.request('setBreakpoints', {'source': {'path': str(args.source)}, 'breakpoints': []})
        client.request('continue', {'threadId': position['thread']})
        for _ in range(64):
            exited = client.evaluate('wait')
            if b'guest exited:' in exited:
                check('original reference producer natural oracle', b'guest exited: 0' in exited, exited)
                break
        else:
            raise AssertionError('real reference fixture did not finish')
        server.child.wait(timeout=15)
        check('original reference broker OS wait', server.child.returncode == 0)
        record.update(passed=True, position=position)
    except BaseException as error:
        record['error'] = repr(error)
        raise
    finally:
        try:
            record['cleanup'] = server.close()
            raw_log = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw_log, 'real first-pause gate evidence', raw_log)
            if record['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw_log, 'original guest wait evidence', raw_log)
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
    parser.add_argument('--source', type=Path, default=root / 'test/0018.debugger/fixtures/pretty_references.cc')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--fixed-llvm-full', action='store_true', help='product has one fixed LLVM-full runtime; omit unavailable selector flags')
    args = parser.parse_args()
    for name in ('uwvm', 'wasm', 'source'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    admission = base.load(root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'references_admission')
    proof = admission.admit()
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    paths = (Path(__file__), args.uwvm, args.wasm, args.source, root / 'tools/debug/dap_adapter.py', root / 'tools/debug/secure_server.py', root / 'test/0018.debugger/run_dap_current_broker.py',
             root / 'test/0018.debugger/run_dap_tinygo_objects.py', root / 'test/0018.debugger/run_pretty_printer_matrix.py')
    pins = {str(path): live.sha(path) for path in paths}
    result = {'passed': False, 'admission': proof, 'pins_before': pins, 'scope': 'actual compiler-owned C++ references through authenticated broker and DAP Watch'}
    try:
        result['session'] = session(args, args.out / 'actual', root)
        result['passed'] = result['session']['passed']
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        if result['pins_after'] != pins:
            result['passed'] = False
        result['admission_after'] = admission.admit()
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'real C++ reference acceptance', result)
    print('PASS actual C++ references', len(result['session']['checks']), 'checks')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
