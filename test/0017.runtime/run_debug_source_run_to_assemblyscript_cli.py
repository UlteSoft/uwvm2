#!/usr/bin/env python3
"""Actual AssemblyScript source-map line break/display, with typed refusal.

Every displayed location is compared with the original sidecar at the actual
guest Code PC. A source map supplies neither DWARF locals nor inline identities.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess, sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for key in ('uwvm', 'wasm', 'source', 'map', 'out'):
        ap.add_argument('--' + key, type=Path, required=True)
    ap.add_argument('--build-record',type=Path,required=True)
    ap.add_argument('--source-root',type=Path,required=True)
    ap.add_argument('--run-to',choices=('until','advance'),required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--source-steps', action='store_true')
    ap.add_argument('--aliases', action='store_true', help='Use actual step/next/finish aliases for source steps')
    ap.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    a = ap.parse_args(); assert sys.platform == 'linux'
    sys.path.insert(0,str(a.source_root/'test/0017.runtime'))
    import run_debug_source_inline_metadata_cli as metadata
    import run_debug_source_step_cli as step
    import run_debug_language_experience_cli as language
    import run_debug_source_map_v3_oracle_cli as oracle
    root = a.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    paths = [a.uwvm, a.wasm, a.source, a.map, Path(__file__), Path(metadata.__file__),
             Path(step.__file__), Path(language.__file__), Path(oracle.__file__)]
    build=json.loads(a.build_record.read_text())
    assert build['passed'] and build['all_inputs_after_unchanged'] and build['new_objects_only'] and len(build['rows'])==4
    assert Path(build['source_root']).resolve()==root and Path(build['binary']).resolve()==a.uwvm.resolve() and build['binary_sha256']==sha(a.uwvm)
    before=dict(build['inputs']);before.update({str(p):sha(p) for p in paths+[a.build_record]})
    for p,h in before.items():assert sha(Path(p))==h,p

    row = dict(passed=False, full_native_parity=False, typed_language_values_PASS=False,
               source_steps_PASS=False, inputs_before=before, actions=[], positions=[])
    c = None
    def send(command):
        reply = language.clean(c.send(command))
        row['actions'].append(dict(command=command, reply=reply.decode()))
        assert len(c.transcript) < 8 * 1024 * 1024
        return reply
    try:
        data, (code_begin, code_size), url = oracle.layout(a.wasm)
        assert a.map.resolve() == (a.wasm.parent / url).resolve()
        customs = [metadata.named_custom(p).decode() for kind, p in metadata.sections(a.wasm) if kind == 0]
        assert 'name' in customs and not any(x.startswith('.debug_') for x in customs)
        mapping = json.loads(a.map.read_text()); assert mapping['version'] == 3
        rows = oracle.original_rows(mapping, code_begin, code_size, len(data))
        own = [i for i, name in enumerate(mapping['sources']) if Path(name).name == a.source.name]
        assert len(own) == 1
        source_index = own[0]; declared = mapping['sources'][source_index]
        if not os.path.isabs(declared):
            declared = os.path.join(mapping.get('sourceRoot', ''), declared)
            if not os.path.isabs(declared):
                declared = os.path.join(str(a.map.parent), declared)
        declared = os.path.normpath(declared)
        markers = [i for i, line in enumerate(a.source.read_text().splitlines(), 1) if '// PROBE_VALUES' in line]
        assert len(markers) == 1
        expressions = step.code_expressions(a.wasm)
        mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        argv = [str(a.uwvm), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', a.policy,
                '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)]
        row['argv'] = argv; c = metadata.Console(argv, a.out / 'console.log')
        assert b'prepared; no Wasm instruction executed' in send('status')
        reply = send(f'break-source 0 {declared}:{markers[0]}')
        bid = re.search(rb'breakpoint (\d+)', reply); assert bid is not None, reply
        table = send('info breakpoints')
        assert b'error:' in send(f'break-source 0 {declared}.absent:{markers[0]}')
        assert table == send('info breakpoints')
        send('continue')
        for _ in range(20):
            stopped = send('wait')
            if b'stopped: breakpoint' in stopped:
                break
        else:
            raise AssertionError('actual AssemblyScript source-map breakpoint did not stop')
        thread = re.search(rb'thread (\d+) module=0 function=', stopped); assert thread is not None
        thread = int(thread[1])
        def position():
            bt = send(f'bt {thread}')
            current = re.search(rb'^thread (\d+) module=0 function=(\d+) byte-offset=(\d+) generation=(\d+)\r?$', bt, re.M)
            stop = re.search(rb'^stop-id (\d+)\r?$', bt, re.M)
            assert current is not None and stop is not None and int(current[1]) == thread
            function = int(current[2]); offset = int(current[3]); assert function in expressions and int(current[4]) > 0
            pc = expressions[function] + offset
            matches = [r for r in rows if r[0] <= pc < r[1]]; assert len(matches) <= 1
            display = re.findall(rb'^  source (.+):(\d+):(\d+)\r?$', bt, re.M)
            if matches:
                actual = matches[0]
                path = mapping['sources'][actual[2]]
                if not os.path.isabs(path):
                    path = os.path.join(mapping.get('sourceRoot', ''), path)
                    if not os.path.isabs(path):
                        path = os.path.join(str(a.map.parent), path)
                assert display == [(os.path.normpath(path).encode(), str(actual[3]).encode(), str(actual[4]).encode())], (pc, matches, bt)
            else:
                assert not display, ('unmapped gap acquired a fabricated source location', pc, bt)
            value = dict(stop_id=int(stop[1]), function=function, offset=offset, code_pc=pc, oracle=matches, physical_functions=[int(v) for v in re.findall(rb'^  #\d+ module=0 function=(\d+) ',bt,re.M)])
            row['positions'].append(value)
            return value, bt
        first, bt = position()
        assert first['oracle'] and first['oracle'][0][2] == source_index and first['oracle'][0][3] >= markers[0]
        assert b'probe_outer' in bt
        send('delete ' + bid[1].decode()); table = send('info breakpoints')
        row['typed_and_scope_refusals'] = []
        for command in (f'print {thread} {first["stop_id"]} shadow', f'ptype {thread} {first["stop_id"]} packet'):
            reply = send(command)
            assert b'error:' in reply and b'source-value stop=' not in reply, (command, reply)
            assert table == send('info breakpoints')
            same, _ = position(); assert same['stop_id'] == first['stop_id'] and same['code_pc'] == first['code_pc']
            row['typed_and_scope_refusals'].append(dict(command=command, reply=reply.decode()))
        target_marker = '// PROBE_AFTER' if a.run_to=='until' else '// PROBE_LEAF_VALUES'
        target_lines=[i for i,line in enumerate(a.source.read_text().splitlines(),1) if target_marker in line]
        assert len(target_lines)==1
        wanted=target_lines[0]
        assert any(r[2]==source_index and r[3]==wanted for r in rows),'requested target must be an original compiler source-map coordinate'
        old=first
        bad=send(f'{a.run_to} {thread} {first["stop_id"]-1} {declared}:{wanted}')
        assert b'error:' in bad
        retained,_=position();assert retained['stop_id']==old['stop_id'] and retained['code_pc']==old['code_pc']
        reply=send(f'{a.run_to} {wanted}')
        assert b'stopped: selected participant step' in reply and b'error:' not in reply,reply
        after,_=position()
        assert after['stop_id']>old['stop_id'] and after['oracle'] and after['oracle'][0][2:4]==[source_index,wanted],(old,after)
        if a.run_to=='until':
            assert after['function']==old['function'] and after['physical_functions']==old['physical_functions'],(old,after)
        else:
            assert after['function']!=old['function'] and len(after['physical_functions'])==len(old['physical_functions'])+1,(old,after)
        row['run_to_result']=dict(policy=a.run_to,before=old,after=after,original_sidecar_target=wanted)
        row['run_to_PASS']=True
        row['source_step_aliases_requested'] = a.aliases
        already_exited = False
        if a.source_steps:
            row['source_step_results'] = []
            origin = first
            for policy in ('into', 'over'):
                reply = send(('step' if policy == 'into' else 'next') if a.aliases else f'step source {thread} {policy}')
                assert b'stopped: selected participant step' in reply and b'error:' not in reply, (policy, reply)
                after, _ = position()
                assert after['stop_id'] > origin['stop_id'] and after['oracle']
                assert after['code_pc'] != origin['code_pc']
                if policy == 'over':
                    assert len(after['physical_functions']) <= len(origin['physical_functions'])
                if after['physical_functions'] == origin['physical_functions']:
                    assert after['oracle'][0][2:5] != origin['oracle'][0][2:5], ('same mapped coordinate was a source stop',origin,after)
                row['source_step_results'].append(dict(policy=policy,before=origin,after=after))
                origin = after
            reply = send('finish' if a.aliases else f'step source {thread} out')
            assert b'error:' not in reply, reply
            if b'guest exited:' in reply:
                assert len(origin['physical_functions']) == 1 and b'guest exited: 0' in reply, (origin,reply)
                already_exited = True
                row['source_step_results'].append(dict(policy='out',before=origin,actual_guest_exit=True))
            else:
                assert b'stopped: selected participant step' in reply, reply
                after, _ = position()
                assert after['stop_id'] > origin['stop_id'] and after['oracle']
                assert 0 < len(after['physical_functions']) < len(origin['physical_functions']), (origin,after)
                row['source_step_results'].append(dict(policy='out',before=origin,after=after))
            row['source_steps_PASS'] = True
        else:
            reply = send(f'step wasm {thread}'); assert b'stopped: selected participant step' in reply
            second, _ = position(); assert second['stop_id'] > first['stop_id']
        if not already_exited:
            send('continue')
        for _ in range(20):
            result = b'guest exited: 0' if already_exited else send('wait')
            if b'guest exited:' in result:
                assert b'guest exited: 0' in result; break
        else:
            raise AssertionError('actual AssemblyScript workload exit missing')
        row['passed'] = True
    except BaseException as e:
        row['error'] = repr(e); raise
    finally:
        if c is not None:
            try:
                c.finish()
            except BaseException as e:
                row.update(passed=False, close_error=repr(e)); raise
            finally:
                row['quit_returncode'] = c.child.returncode
                row['managed_shutdown_complete'] = b'managed shutdown complete' in c.transcript
                if row['quit_returncode'] != 0 or not row['managed_shutdown_complete']:
                    row['passed'] = False
        row['inputs_after'] = {p: sha(Path(p)) for p in before}
        if row['inputs_after'] != before:
            row.update(passed=False, inputs_changed=True)
        (a.out / 'summary.json').write_text(json.dumps(row, indent=2) + '\n')
    assert row['passed']
    print('PASS original AssemblyScript source-map line break/display', 'and real into/over/out' if a.source_steps else '', '; typed values remain UNAVAILABLE')


if __name__ == '__main__':
    main()
