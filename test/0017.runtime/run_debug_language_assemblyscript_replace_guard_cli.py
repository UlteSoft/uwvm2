#!/usr/bin/env python3
"""Original AS body replacement must retire its old source-map authority.

Uses an unchanged real compiler-emitted function body, including local decls.
No typed locations, replacement source bindings or native parity are invented.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
import run_debug_source_inline_metadata_cli as metadata
import run_debug_source_step_cli as step
import run_debug_language_experience_cli as language


def original_body(wasm):
    target = None
    expressions = step.code_expressions(wasm)
    for tag, payload in metadata.sections(wasm):
        if tag == 7:
            count, at = metadata.u32(payload, 0, len(payload))
            for _ in range(count):
                size, at = metadata.u32(payload, at, len(payload))
                assert size < len(payload) - at
                name = payload[at:at + size]; at += size
                kind = payload[at]
                index, at = metadata.u32(payload, at + 1, len(payload))
                if name == b'probe_outer' and kind == 0:
                    assert target is None
                    target = index
            assert at == len(payload)
        elif tag == 10:
            code = payload
    assert target in expressions
    imported = min(expressions)
    count, at = metadata.u32(code, 0, len(code)); body = None
    for index in range(count):
        size, at = metadata.u32(code, at, len(code))
        assert size <= len(code) - at
        if index + imported == target:
            body = code[at:at + size]
        at += size
    assert at == len(code) and body
    return target, body


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for key in ('uwvm', 'wasm', 'map', 'out'):
        ap.add_argument('--' + key, type=Path, required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--policy', choices=('instruction', 'unwind'), required=True)
    a = ap.parse_args(); assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    paths = [a.uwvm, a.wasm, a.map, Path(__file__), Path(metadata.__file__),
             Path(step.__file__), Path(language.__file__)]
    before = {str(p): sha(p) for p in paths}
    row = dict(passed=False, full_native_parity=False, replacement_source_binding=False,
               inputs_before=before, actions=[], positions=[])
    c = None
    def send(command):
        reply = language.clean(c.send(command))
        row['actions'].append(dict(command=command, reply=reply.decode()))
        assert len(c.transcript) < 8*1024*1024
        return reply
    try:
        target, body = original_body(a.wasm)
        replacement = a.out/'original-probe-outer-body.bin'
        replacement.write_bytes(body)
        row.update(function=target, replacement_body_sha256=sha(replacement))
        mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        argv = [str(a.uwvm), '-Rdbg', *mode, '-Rct', '0', '-Rllvm-call-stack', a.policy,
                '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)]
        row['argv'] = argv; c = metadata.Console(argv, a.out/'console.log')
        assert b'prepared; no Wasm instruction executed' in send('status')
        reply = send(f'replace 0 {target} 1 {replacement}')
        assert b'function replaced; generation 2' in reply, reply
        reply = send(f'break 0 {target} 0')
        breakpoint = re.search(rb'breakpoint (\d+)', reply); assert breakpoint, reply
        send('continue')
        for _ in range(20):
            reply = send('wait')
            if b'stopped: breakpoint' in reply:
                break
        else:
            raise AssertionError('actual replaced guest function did not stop')
        thread = re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=', reply)
        assert thread and int(thread[2]) == target
        participant = int(thread[1]); trace = send(f'bt {participant}')
        stop = re.search(rb'^stop-id (\d+)\r?$', trace, re.M); assert stop
        assert b'probe_outer' in trace and b'\n  source ' not in trace, trace
        row['positions'].append(dict(thread=participant, function=target, stop=int(stop[1])))
        for command in (f'print {participant} {stop[1].decode()} shadow',
                        f'locals source {participant}', f'step source {participant} into',
                        f'step source {participant} over', f'step source {participant} out'):
            rejected = send(command)
            assert b'error:' in rejected or b'unavailable' in rejected, (command, rejected)
            assert b'source-value stop=' not in rejected
            after = send(f'bt {participant}')
            assert after == trace, ('rejected stale-source request moved the real stop', command)
        send('delete ' + breakpoint[1].decode()); send('continue')
        for _ in range(20):
            reply = send('wait')
            if b'guest exited:' in reply:
                assert b'guest exited: 0' in reply, reply
                break
        else:
            raise AssertionError('actual unchanged replacement workload did not exit')
        row['passed'] = True
    except BaseException as error:
        row['error'] = repr(error); raise
    finally:
        if c is not None:
            try:
                c.finish()
            except BaseException as error:
                row.update(passed=False, close_error=repr(error)); raise
            finally:
                row.update(quit_returncode=c.child.returncode,
                           managed_shutdown_complete=b'managed shutdown complete' in c.transcript)
                if row['quit_returncode'] != 0 or not row['managed_shutdown_complete']:
                    row['passed'] = False
        row['inputs_after'] = {str(p): sha(p) for p in paths}
        if before != row['inputs_after']:
            row.update(passed=False, inputs_changed=True)
        (a.out/'summary.json').write_text(json.dumps(row, indent=2)+'\n')
    assert row['passed']
    print('PASS real AS function body replacement; old source authority retired; actual guest exit/join')


if __name__ == '__main__':
    main()
