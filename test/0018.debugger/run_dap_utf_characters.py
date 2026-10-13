#!/usr/bin/env python3
"""C++ UTF character Watch expressions and compiler-original direct constants through the real Linux broker and DAP.

Requires a fresh complete product receipt and the actual language CLI producer
receipt. The original Wasm/DWARF is never rewritten. Run only in the designated
test cgroup; this qualifies these scalar paths, not full language/IDE parity.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import time

import run_dap_tinygo_objects as base
import run_dap_current_broker as live
from run_dap_enum_objects import retain_completed_protocol
import run_debug_language_experience_cli as language

sys.dont_write_bytecode = True


def session(args, root, folder, profile, oracle):
    folder.mkdir(mode=0o700)
    adapter = base.load(root / 'tools/debug/dap_adapter.py', 'utf_characters_dap')
    mode = [] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    command = ['-Rdbg', *mode, '-Rct', '0', '-Rllvm-call-stack', args.policy,
               '-Rllvm-cache-path', 'disable', '--run', str(profile['wasm'])]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(),
                                root / 'tools/debug/dap_adapter.py', command, folder)
    result = dict(passed=False, profile=profile['name'], policy=args.policy,
                  evaluations=[], direct_constant_origins=[])
    try:
        first = adapter.UnixBroker(str(server.directory))
        try:
            live.require('prepared; no Wasm instruction executed' in first.request('status'),
                         'original prepared guest')
        finally:
            first.close()
        client_folder = folder / 'session-0'
        client_folder.mkdir(mode=0o700)
        client = base.TypedClient(root / 'tools/debug/dap_adapter.py',
                                  server.directory, client_folder, 'source')
        server.clients.append(client)
        source = profile['source']
        line = next(i for i, text in enumerate(source.read_text().splitlines(), 1)
                    if 'LANG_LEAF_READY' in text)
        points = client.request('setBreakpoints', {'source': {'path': str(source)},
                                'breakpoints': [{'line': line}]})['body']['breakpoints']
        live.require(len(points) == 1 and points[0]['verified'], 'original source breakpoint', points)
        client.request('continue')
        for _ in range(64):
            stop = client.evaluate('wait')
            if b'stopped: breakpoint' in stop:
                break
            live.require(b'guest exited:' not in stop, 'actual leaf source stop required', stop)
        else:
            raise AssertionError('leaf source breakpoint timeout')
        expected = {'leaf_negative': ('-31', 'const int'),
                    'leaf_positive': ('23', 'const unsigned int'),
                    'leaf_boolean': ('true', 'const bool' if profile['name'].startswith('cpp-') else 'const _Bool'),
                    'leaf_fraction': ('1.25', 'const float'), 'leaf_double': ('-2.5', 'const double'),
                    'leaf_zero': ('-0', 'const float')}
        if profile['name'].startswith('rust-'):
            expected = {'leaf_negative': ('-31', 'i32'), 'leaf_positive': ('23', 'u32'),
                        'leaf_boolean': ('true', 'bool'), 'leaf_fraction': ('1.25', 'f32'),
                        'leaf_double': ('-2.5', 'f64'), 'leaf_zero': ('-0', 'f32'),
                        'leaf_point': ('128578', 'char')}
        else:
            expected.update(leaf_octet=('128', 'const char8_t'),
                            leaf_unit=('955', 'const char16_t'),
                            leaf_point=('128578', 'const char32_t'))
        expressions = live.source_cli.code_expressions(profile['wasm'])

        def inspect():
            position = live.actual_location(client)
            position['code_offset'] = expressions[position['function']] + position['offset']
            frame = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames'][0]
            live.require('language_leaf_' in frame['name'] and frame['moduleId'] == '0',
                         'actual original leaf activation', frame)
            scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
            refs = [row['variablesReference'] for row in scopes if row['name'] == 'Source variables']
            live.require(len(refs) == 1, 'actual Source variables scope', scopes)
            ref = refs[0]
            variables = client.request('variables', {'variablesReference': ref})['body']['variables']
            for name, (value, kind) in expected.items():
                selected = [row for row in variables if row['name'] == name]
                live.require(len(selected) == 1, 'one concrete source constant', (name, variables))
                row = selected[0]
                live.require(row['value'] == value and row['type'] == kind and row['variablesReference'] == 0
                             and row['presentationHint']['attributes'] == ['readOnly'] and 'memoryReference' not in row,
                             'original scalar constant value/type', (name, row))
                raw = client.evaluate(f"print {position['thread']} {position['stop']} {name}")
                origin = language.direct_constant_receipt(oracle, raw,
                         {'stop_id': position['stop'], 'code_offset': position['code_offset']},
                         position['thread'], name)
                live.require(origin is not None, 'actual DW_AT_const_value path required', raw)
                result['direct_constant_origins'].append(origin)
                for context in ('watch', 'hover', 'variables'):
                    response = client.request('evaluate', {'expression': name, 'context': context,
                                              'frameId': frame['id']})['body']
                    live.require(response['result'] == value and response['type'] == kind
                                 and response['variablesReference'] == 0 and 'memoryReference' not in response
                                 and response['presentationHint']['attributes'] == ['readOnly'],
                                 'actual constant Source/Watch/Hover agreement', (name, context, response))
                    result['evaluations'].append(dict(name=name, context=context, response=response))
            utf_cases = [
                ("u8'\\x80'", "128", "char8_t"), ("u8'\\xff'", "255", "char8_t"),
                ("u'\\u03bb'", "955", "char16_t"), ("u'\\xD800'", "55296", "char16_t"),
                ("U'\\U0001F642'", "128578", "char32_t"), ("U'\\xffffffff'", "4294967295", "char32_t"),
                ("+u8'\\xff'", "255", "int"), ("+u'\\uffff'", "65535", "int"),
                ("+U'\\U0001F642'", "128578", "unsigned int"),
                ("sizeof(u'\\u03bb')", "2", "unsigned long"),
                ("sizeof(char32_t)", "4", "unsigned long"),
                ("1 ? u'\\u03bb' : u'a'", "955", "char16_t"),
                ("0 ? U'a' : U'\\U0001F642'", "128578", "char32_t"),
                ("1 ? u'\\u03bb' : (unsigned short)955", "955", "int"),
                ("1 ? U'\\U0001F642' : 42u", "128578", "unsigned int"),
                ("static_cast<char16_t>(955)", "955", "char16_t"),
                ("char32_t(128578)", "128578", "char32_t"),
                ("leaf_unit == u'\\u03bb'", "true", "bool"),
                ("leaf_point == U'\\U0001F642'", "true", "bool"),
                ("sizeof(U'\\U0001F642' + 1/0)", "4", "unsigned long"),
            ]
            live.require(client.request('variables', {'variablesReference': ref, 'start': 1, 'count': 2})['body']['variables']
                         == variables[1:3], 'actual source pagination')
            def refuse(expression):
                # Failed source evaluations deliberately retire cached opaque
                # frames/scopes. Acquire a genuine fresh frame for EVERY trial.
                current = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames'][0]
                live.require(current['moduleId'] == '0' and 'language_leaf_' in current['name'],
                             'original leaf frame for each expected refusal', current)
                reply = client.request('evaluate', {'expression': expression, 'context': 'watch', 'frameId': current['id']},
                                       success=False)
                live.require('actual current opaque source frame' not in reply.get('message', ''),
                             'refusal must not use a retired source frame', reply)
                actual = live.actual_location(client)
                live.require(actual['thread'] == position['thread'] and actual['stop'] == position['stop'],
                             'expected refusal must preserve original paused guest', actual)
                return reply
            if profile['name'].startswith('cpp-') and args.expect_unsupported:
                for expression, _, _ in utf_cases:
                    refused = refuse(expression)
                    live.require('actual current opaque source frame' not in refused.get('message', ''),
                                 'baseline must reach the original current frame, not reuse a retired reference', refused)
                    result.setdefault('baseline_refusals', []).append(dict(expression=expression, response=refused))
            elif profile['name'].startswith('cpp-'):
                for expression, value, kind in utf_cases:
                    for context in ('watch', 'hover', 'variables'):
                        response = client.request('evaluate', {'expression': expression, 'context': context,
                                      'frameId': frame['id']})
                        body = response['body']
                        live.require(body['result'] == value and body['type'] == kind and body['variablesReference'] == 0
                                     and 'memoryReference' not in body, 'actual UTF expression value and primitive type',
                                     (expression, body))
                        result['evaluations'].append(dict(expression=expression, context=context, value=value, type=kind))
                for expression in ("u8'\\u0080'", "u'\\U0001F642'", "U'\\U00110000'", "u'\\uD800'",
                                   "u8'\\x100'", "u'ab'", "1 ? 7 : u8'\\u0080'"):
                    refused = refuse(expression)
                    result.setdefault('refusals', []).append(dict(expression=expression, response=refused))
            else:
                for expression in ("u8'\\x80'", "u'\\u03bb'", "U'\\U0001F642'", "0 && u8'a'"):
                    refused = refuse(expression)
                    result.setdefault('refusals', []).append(dict(expression=expression, response=refused))

            frame = client.request('stackTrace', {'threadId': position['thread']})['body']['stackFrames'][0]
            scopes = client.request('scopes', {'frameId': frame['id']})['body']['scopes']
            refs = [row['variablesReference'] for row in scopes if row['name'] == 'Source variables']
            live.require(len(refs) == 1, 'fresh scope after deliberate expected refusal')
            ref = refs[0]
            return position, frame, ref

        before, frame, ref = inspect()
        client.request('setBreakpoints', {'source': {'path': str(source)}, 'breakpoints': []})
        replies = [client.response(number) for number in client.queue([
            ('evaluate', {'expression': 'step wasm ' + str(before['thread']), 'context': 'repl'}),
            ('evaluate', {'expression': 'leaf_negative', 'context': 'watch', 'frameId': frame['id']}),
            ('scopes', {'frameId': frame['id']}), ('variables', {'variablesReference': ref})])]
        live.require(replies[0]['success'] and all(not row['success'] for row in replies[1:]),
                     'actual step retires constant frame and scope references', replies)
        result['queued_retirement'] = replies
        after, _, _ = inspect()
        live.require(after['stop'] > before['stop'] and after['function'] == before['function'],
                     'fresh constant provenance belongs to a new real stop', (before, after))
        fresh = client.request('stackTrace', {'threadId': after['thread']})['body']['stackFrames'][0]
        fresh_scopes = client.request('scopes', {'frameId': fresh['id']})['body']['scopes']
        fresh_refs = [row['variablesReference'] for row in fresh_scopes if row['name'] == 'Source variables']
        live.require(len(fresh_refs) == 1, 'fresh real scope before unsupported write control')
        client.request('setVariable', {'variablesReference': fresh_refs[0], 'name': 'leaf_negative', 'value': '99'}, success=False)
        client.request('continue', {'threadId': after['thread']})
        for _ in range(64):
            exit_reply = client.evaluate('wait')
            if b'guest exited:' in exit_reply:
                live.require(b'guest exited: 0' in exit_reply, 'original guest self-check', exit_reply)
                break
        else:
            raise AssertionError('original guest terminal timeout')
        server.child.wait(timeout=15)
        live.require(server.child.returncode == 0, 'original broker OS wait')
        result['passed'] = True
    except BaseException as error:
        result['error'] = repr(error)
        raise
    finally:
        try:
            result['cleanup'] = server.close()
            raw = (folder / 'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw,
                         'original authenticated prepared gate', raw)
            if result['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw, 'original guest OS wait', raw)
                result['original_guest_wait_returncode'] = 0
        finally:
            (folder / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    root = Path(__file__).resolve().parents[2]
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return base.main()
    parser = argparse.ArgumentParser()
    for name in ('uwvm', 'build-receipt', 'producer-summary', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--expect-unsupported', action='store_true', help='Original pre-fix full product must reject the new finite C++ UTF expressions')
    parser.add_argument('--seconds', type=int, default=0)
    parser.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    args = parser.parse_args()
    live.require(sys.platform == 'linux' and 0 <= args.seconds <= 900, 'bounded Linux original cgroup driver')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    build = json.loads(args.build_receipt.read_text())
    producer = json.loads(args.producer_summary.read_text())
    live.require(build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after']
                 and live.sha(args.uwvm) == build['binary_sha256'], 'closed fresh full product')
    live.require(producer['passed'] and producer['inputs_before'] == producer['inputs_after']
                 and len(producer['fixtures']) == 4 and len(producer['sessions']) == 8
                 and all(row['passed'] for row in producer['sessions']),
                 'closed actual compiler-original UTF/floating producer evidence')
    profiles = []
    paths = [Path(__file__), Path(base.__file__), Path(live.__file__), Path(language.__file__),
             Path(live.source_cli.__file__), Path(live.source_cli.metadata_cli.__file__),
             root / 'test/0018.debugger/run_dap_enum_objects.py', root / 'tools/debug/dap_adapter.py',
             root / 'tools/debug/secure_server.py', root / 'tools/ci/require_wasm3_test_cgroup.sh',
             args.uwvm, args.build_receipt, args.producer_summary]
    for command in producer['commands']:
        log = Path(command['log'])
        live.require(command['returncode'] == 0 and live.sha(log) == command['log_sha256'],
                     'original compiler/validator/DWARF command evidence', command['argv'])
        paths.append(log)
    for fixture in producer['fixtures']:
        name = fixture['name']
        cases = [row for row in producer['sessions'] if row['policy'] == args.policy and row['profile'] == name]
        live.require(len(cases) == 1 and cases[0]['passed'], 'unique original qualified CLI case', name)
        case = cases[0]
        live.require(live.sha(Path(case['log'])) == case['log_sha256'], 'original CLI case log', name)
        wasm, source, oracle = map(Path, (fixture['wasm'], fixture['source'], fixture['oracle']))
        live.require(live.sha(wasm) == fixture['wasm_sha256'] and live.sha(source) == fixture['source_sha256']
                     and live.sha(oracle) == fixture['oracle_sha256'], 'original compiler bytes unchanged', name)
        paths.extend([wasm, source, oracle])
        profiles.append(dict(name=name, wasm=wasm, source=source, oracle=oracle))
    pins = {str(path): live.sha(path) for path in paths}
    args.out.mkdir(mode=0o700)
    result = dict(passed=False, pins=pins, sessions=[], actual_ide_ui=False, full_language_parity=False)
    try:
        started = time.monotonic()
        while True:
            for profile in profiles:
                folder = args.out / (f"session-{len(result['sessions']):04}-" + profile['name'])
                row = session(args, root, folder, profile, profile['oracle'].read_text())
                retention = retain_completed_protocol(folder)
                result['sessions'].append(dict(passed=row['passed'], profile=profile['name'], policy=args.policy,
                         evaluations=len(row['evaluations']), direct_constant_origins=row['direct_constant_origins'],
                         protocol_retention=retention))
                if args.seconds:
                    time.sleep(0.25)
            if time.monotonic() - started >= args.seconds:
                break
            live.require(len(result['sessions']) < 1024, 'bounded completed session retention')
            args.policy = 'unwind' if args.policy == 'instruction' else 'instruction'
        result.update(passed=True, elapsed_seconds=time.monotonic() - started)
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['passed'] and pins == result['pins_after']
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual direct constants DAP qualification')
    print('PASS genuine UTF characters and direct constants through DAP', len(result['sessions']), 'sessions', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
