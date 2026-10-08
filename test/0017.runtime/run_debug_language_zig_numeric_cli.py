#!/usr/bin/env python3
"""Original Zig DWARF, finite @as values, rejection and stop authentication.

Runs only in the designated Linux cgroup. Compiler output and its actual LLVM
line oracle are immutable inputs; unavailable required values fail the case.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
import run_debug_source_step_cli as step
import run_debug_language_experience_cli as language


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for key in ('uwvm', 'wasm', 'source', 'oracle', 'out'):
        ap.add_argument('--' + key, type=Path, required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    a = ap.parse_args()
    assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    paths = [a.uwvm, a.wasm, a.source, a.oracle, Path(__file__), Path(step.__file__), Path(language.__file__)]
    before = {str(p): sha(p) for p in paths}
    record = dict(passed=False, language='zig', full_native_parity=False,
                  inputs_before=before, values=[], refusals=[])
    s = None
    try:
        mode = ['-Raot'] if a.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        argv = [str(a.uwvm), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', a.policy,
                '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)]
        s = step.Session(argv, a.out / 'console.log', step.code_expressions(a.wasm),
                         step.line_sequences(a.oracle.read_text()), a.source)
        record.update(argv=argv, actions=s.actions, positions=s.positions)
        target, _ = step.metadata_cli.function(a.wasm, 'numeric_probe')
        s.begin(target)
        markers = [i for i, line in enumerate(a.source.read_text().splitlines(), 1) if '// ZIG_NUMERIC_READY' in line]
        assert len(markers) == 1
        position = language.own_seek(s, lambda p: p['function'] == target and p['line'] == markers[0]
                                     and p['is_statement'], 'real initialized Zig numeric values')
        for expression, expected, width in [('value', 3, 4), ('wide', 3, 8), ('decimal', 1.25, 4),
                                           ('decimal64', 1.25, 8), ('@as(i64, value)', 3, 8),
                                           ('@as(f64, decimal)', 1.25, 8), ('@as(i32, value)', 3, 4),
                                           ('@as(isize, value)', 3, 4), ('@as(u8, 255)', 255, 1),
                                           ('@as(f32, 1.25)', 1.25, 4), ('@as(i64, @as(i32, value))', 3, 8)]:
            reply = language.command_value(s, position, expression)
            values = re.findall(rb'(?:, value=| = f(?:32|64)=| = [iu]\d+=)([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)(?: \([^\r\n]*\))?\r?$', reply, re.M)
            assert len(values) == 1 and float(values[0]) == expected, (expression, reply)
            if expression.startswith('@as'):
                assert b', bytes=' + str(width).encode() + b',' in reply, (expression, reply)
            record['values'].append(dict(expression=expression, expected=expected, width_bytes=width, reply=reply.decode()))
        table = language.query(s, 'info breakpoints')
        for expression in ('@as(i16, value)', '@as(u32, value)', '@as(i32, wide)', '@as(f32, decimal64)',
                           '@as(f64, value)', '@as(i32, decimal)', '@as(i32, true)', '@as(u8, 256)',
                           '@as(f32, 1e100)', '@as(i64, value + 2)', '@as(double, decimal)',
                           '@intCast(value)', '@as(i64, numeric_probe())', '@as(i64, value = 9)'):
            reply = language.query(s, f'print {s.thread} {position["stop_id"]} {expression}')
            assert b'error:' in reply and b'source-value stop=' not in reply, (expression, reply)
            assert table == language.query(s, 'info breakpoints')
            record['refusals'].append(dict(expression=expression, reply=reply.decode()))
        language.scalar(s, position, 'value', 3)
        s.send(f'step wasm {s.thread}')
        stale = language.query(s, f'print {s.thread} {position["stop_id"]} @as(u8, 255)')
        assert b'error:' in stale and b'source-value stop=' not in stale, stale
        s.finish_guest()
        record['passed'] = True
    except BaseException as e:
        record['error'] = repr(e)
        raise
    finally:
        if s is not None:
            try:
                s.console.finish()
            except BaseException as e:
                record.update(passed=False, close_error=repr(e))
                raise
            finally:
                record['quit_returncode'] = s.console.child.returncode
                record['managed_shutdown_complete'] = b'managed shutdown complete' in s.console.transcript
                if record['quit_returncode'] != 0 or not record['managed_shutdown_complete']:
                    record['passed'] = False
        record['inputs_after'] = {str(p): sha(p) for p in paths}
        if record['inputs_after'] != before:
            record.update(passed=False, inputs_changed=True)
        (a.out / 'summary.json').write_text(json.dumps(record, indent=2) + '\n')
    assert record['passed']
    print('PASS original Zig typed @as subset, refusals, stale stop and real guest result')


if __name__ == '__main__':
    main()
