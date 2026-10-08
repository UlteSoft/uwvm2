#!/usr/bin/env python3
"""Actual language-typed conditional breakpoints in the designated Linux cgroup.

Original C++/Rust DWARF4/5 producer bytes and actual guest exits are required.
A false predicate must skip the real hit, a true predicate must stop, and an
unavailable predicate must preserve the guest. This does not claim IDE parity.
"""
from pathlib import Path
import argparse,json,re,subprocess,sys,time
import run_dap_tinygo_objects as base
import run_dap_current_broker as live
from run_dap_enum_objects import retain_completed_protocol
import run_debug_language_experience_cli as language
sys.dont_write_bytecode=True

def predicate_cases(profile, broken):
    rust=profile['name'].startswith('rust-')
    point="leaf_point == '\\u{1f641}'" if rust else "leaf_point == U'\\U0001F641'"
    if broken:
        return [('false',False),(point,False)]
    common=[('false',False),('true',True),('leaf_boolean',True),
            ('!leaf_boolean',False),(point,False),('leaf_negative == -31',True),
            ('leaf_positive == 0',False),('leaf_fraction > 1.0',True),
            ('leaf_zero == 0.0',True),('leaf_double < -3.0',False),
            ('true && leaf_boolean',True),('false && (1 / 0 == 0)',False),
            ('missing_condition_variable == 0',None),('1 / 0 == 0',None),
            ('false && missing_condition_variable',None)]
    if rust:
        return [('leaf_positive == 23u32', True), ('leaf_positive == 0u32', False), ('leaf_fraction > 1.0f32', True), ('leaf_fraction < 1.0f32', False), ('255u8 - 1 == 254u8', True), ('255u8 - 1 == 255u8', False), ('-128i8 == -128', True), ('1.5f32 + 2.0 == 3.5f32', True), ('1u8 == 1', True), ('true && (255u8 == 255)', True), ('1u8', None), ('255u8 + 1u8 == 0u8', None), ('1u8 == 1u16', None), ('false && (1u8 == 256)', None), ('true || (1u8 == 1u16)', None), ('false && (1u8 / 0u8 == 0u8)', False)]
    return common+[('23',True),('leaf_positive',True),('leaf_negative',True),
                   ('leaf_zero',False),('leaf_fraction',True),('-0.0',False),
                   ('0.0',False)]

def session(args,root,folder,profile,expression,expected):
    folder.mkdir(mode=0o700)
    adapter=base.load(root/'tools/debug/dap_adapter.py','conditional_truth_dap')
    mode=[] if args.ros else ['-Rcc','jit','-Rcm','full']
    argv=['-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',args.policy,
          '-Rllvm-cache-path','disable','--run',str(profile['wasm'])]
    server=live.BrokerSession(args.uwvm,Path(__file__).resolve(),
                             root/'tools/debug/dap_adapter.py',argv,folder)
    result=dict(passed=False,profile=profile['name'],policy=args.policy,
                expression=expression,expected=expected,observations=[])
    try:
        first=adapter.UnixBroker(str(server.directory))
        try:
            live.require('prepared; no Wasm instruction executed' in first.request('status'),
                         'authenticated genuine prepared gate')
        finally:first.close()
        cf=folder/'session-0';cf.mkdir(mode=0o700)
        client=base.TypedClient(root/'tools/debug/dap_adapter.py',server.directory,cf,'source')
        server.clients.append(client)
        source=profile['source']
        line=next(i for i,text in enumerate(source.read_text().splitlines(),1)
                  if 'LANG_LEAF_READY' in text)
        after_line=next(i for i,text in enumerate(source.read_text().splitlines(),1)
                        if 'LANG_AFTER_CALL' in text)
        points=client.request('setBreakpoints',{'source':{'path':str(source)},
                     'breakpoints':[{'line':line,'condition':'false' if args.update_condition else expression},{'line':after_line}]})['body']['breakpoints']
        live.require(len(points)==2 and all(p['verified'] for p in points),
                     'actual conditional and post-call witness registration',points)
        bid=points[0]['id'];result['breakpoint']=points[0];result['sentinel']=points[1]
        if args.update_condition:
            update=client.evaluate(f'condition {bid} {expression}')
            live.require(b'breakpoint policy updated' in update,'actual CLI condition replacement',update)
            result['condition_update']=update.decode()
        client.request('continue')
        terminal=False
        for _ in range(64):
            raw=client.evaluate('wait');result['observations'].append(raw.decode())
            if b'stopped: breakpoint' in raw or b'guest exited:' in raw:break
        else:raise AssertionError('actual condition observation timeout')
        unavailable=b'breakpoint-condition ' in raw and b' unavailable code=' in raw
        stopped=b'stopped: breakpoint' in raw
        if args.expect_broken:
            live.require(stopped and unavailable,'original false Boolean condition incorrectly stops unavailable',raw)
            result['baseline_false_boolean_failure']=True
        elif expected is False:
            live.require(stopped and not unavailable,
                         'false predicate must skip the leaf and reach the post-call witness',raw)
            terminal=False
        else:
            live.require(stopped and unavailable is (expected is None),
                         'true stops; unavailable preserves the actual paused guest',raw)
        if stopped and (args.expect_broken or expected is not False):
            pos=live.actual_location(client)
            frame=client.request('stackTrace',{'threadId':pos['thread']})['body']['stackFrames'][0]
            live.require('language_leaf_' in frame['name'] and frame['moduleId']=='0'
                         and frame['source']['path']==str(source) and frame['line']==line,
                         'genuine original leaf source stop',frame)
            result['position']=pos;result['frame']=frame
            # Independent Watch result on this exact fresh opaque frame.
            if expected is not None:
                body=client.request('evaluate',{'expression':expression,'context':'watch',
                                    'frameId':frame['id']})['body']
                result['watch']=body
                if expected is False:live.require(body['result']=='false','original predicate really false',body)
            else:
                # Some unavailable conditions are valid values of the wrong
                # condition type. Others fail arithmetic/reference evaluation.
                number=client.queue([('evaluate',{'expression':expression,'context':'watch',
                                                  'frameId':frame['id']})])[0]
                reply=client.response(number)
                live.require('actual current opaque source frame' not in reply.get('message',''),
                             'unavailability must not be a stale frame refusal',reply)
                result['watch']=reply
            raw=client.evaluate('status')
            live.require(b'stopped: breakpoint' in raw,'inspection preserves real pause',raw)
            client.evaluate(f'disable {bid}')
            client.request('continue',{'threadId':pos['thread']})
            for _ in range(64):
                raw=client.evaluate('wait')
                if b'stopped: breakpoint' in raw:break
                live.require(b'guest exited:' not in raw,'post-call witness still required',raw)
            else:raise AssertionError('post-call witness timeout')
        witness=live.actual_location(client)
        wf=client.request('stackTrace',{'threadId':witness['thread']})['body']['stackFrames'][0]
        live.require('language_outer_' in wf['name'] and wf['moduleId']=='0'
                     and wf['source']['path']==str(source) and wf['line']>=after_line
                     and b'breakpoint-condition ' not in raw,
                     'real caller post-call witness proves the false condition was skipped',wf)
        result['post_call_witness']=dict(position=witness,frame=wf)
        policies=client.evaluate('info breakpoints')
        match=re.search(rb'breakpoint '+str(bid).encode()+rb'\b[^\n]*hits=([0-9]+)',policies)
        live.require(match is not None and int(match[1])>0,'condition tested on a real counted hit',policies)
        result['breakpoint_policy_listing']=policies.decode();result['hits']=int(match[1])
        client.evaluate('disable')
        client.request('continue',{'threadId':witness['thread']})
        for _ in range(64):
            raw=client.evaluate('wait')
            if b'guest exited:' in raw:
                live.require(b'guest exited: 0' in raw,'original guest self-check',raw);break
        else:raise AssertionError('actual original guest terminal timeout')
        client.close()
        receipt=json.loads((cf/'source-dap.json').read_text())
        live.require(receipt['returncode']==0 and receipt['actual_eof_drained'],'actual adapter EOF and wait')
        server.child.wait(timeout=15)
        live.require(server.child.returncode==0,'original broker OS wait')
        result['passed']=True
    except BaseException as e:result['error']=repr(e);raise
    finally:
        try:
            result['cleanup']=server.close()
            raw=(folder/'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw,
                         'original authenticated gate')
            if result['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw,'actual original guest OS wait')
                result['original_guest_wait_returncode']=0
        finally:(folder/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

def main():
    root = Path(__file__).resolve().parents[2]
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return base.main()
    parser = argparse.ArgumentParser()
    for name in ('uwvm', 'build-receipt', 'producer-summary', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--expect-broken', action='store_true', help='Original full product must stop on false Boolean predicates')
    parser.add_argument('--seconds', type=int, default=0)
    parser.add_argument('--live-values', action='store_true', help='Actual parameter/local values, not only original DWARF constants')
    parser.add_argument('--update-condition', action='store_true', help='Replace initially false condition through CLI before real execution')
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
                selected=[('value == 5',True),('value != 5',False),
                          ('leaf_cookie == 16',True),('leaf_cookie == 0',False)] if args.live_values else predicate_cases(profile,args.expect_broken)
                for expression,expected in selected:
                    folder=args.out/(f"session-{len(result['sessions']):04}-"+profile['name'])
                    row=session(args,root,folder,profile,expression,expected)
                    retention=retain_completed_protocol(folder)
                    result['sessions'].append(dict(passed=row['passed'],profile=profile['name'],policy=args.policy,
                        expression=expression,expected=expected,hits=row['hits'],
                        baseline_false_boolean_failure=row.get('baseline_false_boolean_failure',False),
                        original_guest_wait_returncode=row['original_guest_wait_returncode'],
                        protocol_retention=retention))
            if time.monotonic() - started >= args.seconds:
                break
            live.require(len(result['sessions']) < 2048, 'bounded completed session retention')
            args.policy = 'unwind' if args.policy == 'instruction' else 'instruction'
        result.update(passed=True, elapsed_seconds=time.monotonic() - started)
    finally:
        result['pins_after'] = {path: live.sha(Path(path)) for path in pins}
        result['passed'] = result['passed'] and pins == result['pins_after']
        (args.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    live.require(result['passed'], 'actual direct constants DAP qualification')
    print('PASS actual language-typed conditional source breakpoints', len(result['sessions']), 'sessions', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
