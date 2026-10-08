#!/usr/bin/env python3
"""Compare real TinyGo len/cap stops with original compiler-produced arguments.

Only the designated Linux cgroup may execute this runner. Fixed arrays/pointer-arrays are this finite subset; maps/channels and full
Go expression semantics remain unqualified.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
sys.dont_write_bytecode = True
from run_debug_source_tinygo_cli import TinyGoConsole


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'build-receipt', 'wasm', 'source', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--ros', action='store_true')
    ap.add_argument('--policy', choices=('instruction', 'unwind'), default='instruction')
    ap.add_argument('--expect-unavailable', action='store_true')
    a = ap.parse_args()
    assert sys.platform == 'linux'
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    a.out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
    build = json.loads(a.build_receipt.read_text())
    assert build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after']
    assert sha(a.uwvm) == build['binary_sha256']
    paths = [a.uwvm, a.build_receipt, a.wasm, a.source, Path(__file__), root / 'test/0017.runtime/run_debug_source_tinygo_cli.py', root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py']
    pins = {str(p.resolve()): sha(p) for p in paths}
    record = dict(passed=False, product_source_id=build['source_id'], inputs=pins,
                  expected_old_refusals=a.expect_unavailable, policy=a.policy,
                  values=[], actions=[], all_language_parity=False)
    save = lambda: (a.out / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    console = None
    save()
    try:
        prefix = [str(a.uwvm), '-Rdbg']
        if not a.ros:
            prefix += ['-Rcc', 'jit', '-Rcm', 'full']
        prefix += ['-Rct', '0', '-Rllvm-call-stack', a.policy, '-Rllvm-cache-path', 'disable', '--run', str(a.wasm)]
        record['argv'] = prefix
        console = TinyGoConsole(prefix, a.out / 'console.log')
        def ask(command):
            reply = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', console.send(command)).decode(errors='replace')
            record['actions'].append(dict(command=command, reply=reply))
            save()
            return reply
        def number(reply):
            assert 'error:' not in reply and 'unavailable' not in reply, reply
            values = re.findall(r'(?m)^(?!source-value\b)[^\r\n]*\b(?:value=|[ui](?:8|16|32|64)=)(-?\d+)\b', reply)
            assert len(values) == 1, reply
            return int(values[0])
        line = next(i for i, text in enumerate(a.source.read_text().splitlines(), 1) if 'GO_ARRAY_READY' in text)
        br = ask(f'break-source 0 {a.source}:{line}')
        bid = int(re.search(r'breakpoint (\d+)', br)[1])
        ask('continue')
        for _ in range(64):
            stop = ask('wait')
            if 'stopped: breakpoint' in stop:
                break
            assert 'guest exited:' not in stop, stop
        else:
            raise AssertionError('No real TinyGo source breakpoint')
        thread = int(re.search(r'thread (\d+) module=0 function=', stop)[1])
        sid = int(re.search(r'(?m)^stop-id (\d+)', ask(f'bt {thread}'))[1])
        oracle = {name: number(ask(f'print {thread} {sid} {name}')) for name in ('arrayLen', 'outerLen', 'innerLen', 'zeroLen', 'zeroSizedLen')}
        assert oracle == dict(arrayLen=4, outerLen=2, innerLen=3, zeroLen=0, zeroSizedLen=6), oracle
        cases = [
            ('len(box.Array)', oracle['arrayLen']),
            ('cap(box.Array)', oracle['arrayLen']),
            ('len(box.Named)', oracle['arrayLen']),
            ('cap(box.Named)', oracle['arrayLen']),
            ('len(box.Matrix)', oracle['outerLen']),
            ('cap(box.Matrix)', oracle['outerLen']),
            ('len(box.Matrix[0])', oracle['innerLen']),
            ('cap(box.Matrix[1])', oracle['innerLen']),
            ('len(box.Zero)', oracle['zeroLen']),
            ('cap(box.Zero)', oracle['zeroLen']),
            ('len(box.ZeroSized)', oracle['zeroSizedLen']),
            ('cap(box.ZeroSized)', oracle['zeroSizedLen']),
            ('len(box.Pointer)', oracle['arrayLen']),
            ('cap(box.Pointer)', oracle['arrayLen']),
            ('len(box.NilPointer)', oracle['arrayLen']),
            ('cap(box.NilPointer)', oracle['arrayLen']),
            ('len(nilArray)', oracle['arrayLen']),
            ('cap(nilArray)', oracle['arrayLen']),
            ('len(*nilArray)', oracle['arrayLen']),
            ('cap(*nilArray)', oracle['arrayLen']),
            ('len(nilBox.Array)', oracle['arrayLen']),
            ('cap(nilBox.NilPointer)', oracle['arrayLen']),
            ('len(nilBox.Zero)', oracle['zeroLen']),
            ('len(*box.NilPointer)', oracle['arrayLen']),
            ('len(box.ZeroPointer)', oracle['zeroLen']),
            ('cap(box.ZeroPointer)', oracle['zeroLen']),
            ('len(*box.ZeroPointer)', oracle['zeroLen']),
            ('cap(*box.ZeroPointer)', oracle['zeroLen']),
            ('len(box.Array)-1', oracle['arrayLen']-1),
            ('len(box.Array)+cap(box.Matrix)', oracle['arrayLen']+oracle['outerLen'])]
        for expression, expected in cases:
            reply = ask(f'print {thread} {sid} {expression}')
            if a.expect_unavailable:
                assert 'error:' in reply or 'unavailable' in reply, (expression, reply)
                assert 'source-value stop=' not in reply, reply
            else:
                assert number(reply) == expected, (expression, expected, reply)
                assert number(ask(f'print-frame {thread} {sid} 0 {expression}')) == expected
                assert reply == ask(f'print {thread} {sid} {expression}'), 'Same-stop value changed'
            record['values'].append(dict(expression=expression, compiler_oracle=expected, reply=reply, passed=True))
        if not a.expect_unavailable:
            for expression in ('len(box.Array)', 'cap(nilArray)', 'len(*box.ZeroPointer)'):
                reply = ask(f'ptype {thread} {sid} {expression}')
                assert 'error:' not in reply and reply.startswith(f'source-type stop={sid} name={expression}\n') and 'source-type end\n' in reply, reply
                assert re.search(r'(?m)^  type-node 0 parent=none depth=0 name=\$(?:len|cap) type=int kind=scalar byte-offset=0 byte-size=4$', reply), reply
            for expression in ('cap(box.Message)', 'len(box.Array[0])', 'len(box.Matrix[2])', 'len(box.Array,box.Matrix)', 'len(box.Array+1)', 'len(box.Array)[0]', 'len(box.Array=1)', 'cap(len(box.Array))'):
                reply = ask(f'print {thread} {sid} {expression}')
                assert ('error:' in reply or 'unavailable' in reply) and 'source-value stop=' not in reply, reply
            assert 'error:' in ask(f'print {thread} {sid+1000000} len(box.Array)')
        ask(f'delete {bid}')
        stepped = ask(f'step wasm {thread}')
        assert 'guest exited:' not in stepped, stepped
        stale = ask(f'print {thread} {sid} len(box.Array)')
        assert 'error:' in stale and 'source-value stop=' not in stale, stale
        ask('continue')
        for _ in range(64):
            exited = ask('wait')
            if 'guest exited:' in exited:
                assert 'guest exited: 0' in exited, exited
                break
        else:
            raise AssertionError('Guest did not finish its compiler result check')
        record.update(passed=True, guest_completed_without_trap=True, compiler_oracles=oracle)
    except BaseException as error:
        record.update(passed=False, error=repr(error))
        raise
    finally:
        if console:
            try:
                console.finish()
                record['quit_returncode'] = console.child.returncode
                record['managed_shutdown_complete'] = b'managed shutdown complete' in console.transcript
                assert record['quit_returncode'] == 0 and record['managed_shutdown_complete']
            except BaseException as error:
                record.update(passed=False, shutdown_error=repr(error))
                save()
                raise
        for p, h in pins.items():
            if sha(p) != h:
                record.update(passed=False, changed_input=p)
        save()
    assert record['passed']
    print('PASS real TinyGo array compiler-oracle stop', a.policy, len(record['values']), 'old refusals' if a.expect_unavailable else 'current values')


if __name__ == '__main__':
    main()
