#!/usr/bin/env python3
"""Compare real TinyGo len/cap stops with original compiler-produced arguments.

Only the designated Linux cgroup may execute this runner. String/slice
descriptors are the finite subset; maps, channels and arrays are unqualified.
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
        line = next(i for i, text in enumerate(a.source.read_text().splitlines(), 1) if 'GO_BUILTIN_READY' in text)
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
        oracle = {name: number(ask(f'print {thread} {sid} {name}')) for name in ('length', 'capacity', 'textBytes', 'nilLength', 'nilCapacity')}
        assert oracle == dict(length=3, capacity=5, textBytes=7, nilLength=0, nilCapacity=0), oracle
        cases = [('len(box.Numbers)', oracle['length']), ('cap(box.Numbers)', oracle['capacity']),
                 ('len(box.Text)', oracle['textBytes']), ('len(box.Nil)', oracle['nilLength']),
                 ('cap(box.Nil)', oracle['nilCapacity']), ('len(box.Empty)', oracle['nilLength']),
                 ('len(box.Numbers) + cap(box.Numbers)', oracle['length'] + oracle['capacity']),
                 ('len(box.Numbers)-1', oracle['length']-1), ('(cap(box.Numbers))-1', oracle['capacity']-1)]
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
            for expression in ('len(box.Numbers)', 'cap(box.Numbers)', 'len(box.Text)'):
                reply = ask(f'ptype {thread} {sid} {expression}')
                assert 'error:' not in reply and reply.startswith(f'source-type stop={sid} name={expression}\n') and 'source-type end\n' in reply, reply
                assert re.search(r'(?m)^  type-node 0 parent=none depth=0 name=\$(?:len|cap) type=int kind=scalar byte-offset=0 byte-size=4$', reply), reply
            for expression in ('cap(box.Text)', 'cap(box.Empty)', 'len(box.Text,box.Numbers)', 'len(box.Numbers+1)', 'len(box.Numbers)[0]', 'len(box.Numbers=1)', 'cap(len(box.Numbers))'):
                reply = ask(f'print {thread} {sid} {expression}')
                assert ('error:' in reply or 'unavailable' in reply) and 'source-value stop=' not in reply, reply
            assert 'error:' in ask(f'print {thread} {sid+1000000} len(box.Numbers)')
        ask(f'delete {bid}')
        stepped = ask(f'step wasm {thread}')
        assert 'guest exited:' not in stepped, stepped
        stale = ask(f'print {thread} {sid} len(box.Numbers)')
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
    print('PASS real TinyGo builtin compiler-oracle stop', a.policy, len(record['values']), 'old refusals' if a.expect_unavailable else 'current values')


if __name__ == '__main__':
    main()
