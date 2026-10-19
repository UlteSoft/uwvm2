#!/usr/bin/env python3
"""Native CPython and official Wasmtime oracles, including real input gates."""
import argparse
import json
import os
from pathlib import Path
import re

from linux_control import digest, guard, invoke, gated


def stable_output(text):
    lines = text.splitlines(keepends=True)
    clocks = [line for line in lines if re.fullmatch(r'Kernel ns: [0-9]+\n', line)]
    assert len(clocks) == 1
    assert int(clocks[0].split(':')[1]) > 0
    return ''.join(line for line in lines if line not in clocks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--rounds', default='1000,20000')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    before = guard(args.out)
    own = Path(__file__).resolve().parent
    build = json.loads((args.build / 'build-receipt.json').read_text())
    assert build['passed']
    for artifact in build['artifacts'].values():
        assert digest(artifact['path']) == artifact['sha256']
    native_python = Path(build['native_python']['path'])
    env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON') and k != 'UWVM_PY_TEST'}
    env['PYTHONHASHSEED'] = '0'
    report = {'passed': False, 'before': before, 'build_receipt': str(args.build / 'build-receipt.json'),
              'build_receipt_sha256': digest(args.build / 'build-receipt.json'),
              'native_python_sha256': digest(native_python), 'wasmtime_sha256': digest(args.wasmtime),
              'cases': [], 'independent_checksum_oracles': {}, 'source_language_debugger_claim': False}
    path = args.out / 'report.json'

    def save():
        path.write_text(json.dumps(report, indent=2) + '\n')

    save()
    try:
        rounds = [int(value) for value in args.rounds.split(',')]
        invoke([native_python, '-S', '-B', own / 'semantics.py', str(rounds[0])],
               args.out / 'native-source-cli', env=env)
        row = invoke([native_python, own / 'checksum_oracle.py', *map(str, rounds)],
                     args.out / 'independent-checksum', env=env)
        for line in (args.out / 'independent-checksum/stdout.log').read_text().splitlines():
            match = re.fullmatch(r'iterations=([0-9]+) checksum=([0-9]+)', line)
            assert match
            report['independent_checksum_oracles'][match[1]] = int(match[2])
        specs = [('rounds-' + str(value), [str(value)], {}) for value in rounds]
        specs += [('default', [], {}),
                  ('utf8-args', [str(rounds[0]), 'π', '你好', '', 'space value'], {}),
                  ('env-one', [str(rounds[0])], {'UWVM_PY_TEST': 'python-π'}),
                  ('env-many', [str(rounds[0])], {'UWVM_PY_AUX': 'two-你好',
                    'UWVM_PY_EMPTY': '', 'UWVM_PY_TEST': 'python-π'})]
        for name, arguments, guest_env in specs:
            count = arguments[0] if arguments else '1000'
            for entry in ['semantics', 'timed', 'profile']:
                outputs = []
                for engine in ['native', 'wasmtime']:
                    artifact = build['artifacts'][entry + ('-native' if engine == 'native' else '-wasi')]
                    argv = [artifact['path'], *arguments]
                    command_env = dict(env)
                    command_env.update(guest_env)
                    if engine == 'wasmtime':
                        argv = [str(args.wasmtime), 'run', '-C', 'cache=n,parallel-compilation=n',
                                '-W', 'max-wasm-stack=16777216',
                                *[token for pair in guest_env.items() for token in ['--env', '='.join(pair)]], *argv]
                    folder = args.out / name / entry / engine
                    result = (gated(argv, folder, env=command_env) if entry == 'profile'
                              else invoke(argv, folder, env=command_env))
                    text = (folder / 'stdout.log').read_text()
                    if entry != 'semantics':
                        text = stable_output(text)
                    checksum = report['independent_checksum_oracles'][count]
                    assert text.endswith(f'Iterations: {count}\nChecksum: {checksum}\n')
                    prefix = ('python classes=42 arrays=55 dicts=6 unicode=4 utf8=12 exceptions=3 '
                              'bytes=120 memory=369 gc=1 '
                              f'args={max(0, len(arguments) - 1)} env={int(bool(guest_env))}\n')
                    assert text.startswith(prefix)
                    row = {'name': name, 'entry': entry, 'engine': engine, 'args': arguments,
                           'environment': guest_env,
                           'passed': True, 'stable_stdout': text, 'artifact_sha256': artifact['sha256'],
                           'log': str(folder / 'stdout.log'), 'lifecycle': str(folder / 'receipt.json'),
                           'seconds': result['seconds']}
                    report['cases'].append(row)
                    outputs.append(text)
                    save()
                assert outputs[0] == outputs[1]
        for argument in ['0', '-1', '100000001', 'x', '9999999999999999999999999999999999999999']:
            for engine in ['native', 'wasmtime']:
                artifact = build['artifacts']['semantics-' + ('native' if engine == 'native' else 'wasi')]
                argv = [artifact['path'], argument]
                if engine == 'wasmtime':
                    argv = [str(args.wasmtime), 'run', '-C', 'cache=n,parallel-compilation=n',
                            '-W', 'max-wasm-stack=16777216', *argv]
                folder = args.out / ('invalid-' + argument) / engine
                invoke(argv, folder, env=env, expected_exit=2)
                assert (folder / 'stdout.log').read_bytes() == b''
                assert (folder / 'stderr.log').read_bytes() == b'python invalid iterations\n'
                report['cases'].append({'name': 'invalid-' + argument, 'entry': 'semantics',
                    'engine': engine, 'args': [argument], 'expected_exit': 2, 'passed': True,
                    'stable_stdout': '', 'expected_stderr': 'python invalid iterations\n',
                    'artifact_sha256': artifact['sha256'], 'lifecycle': str(folder / 'receipt.json')})
                save()
        report['passed'] = bool(report['cases']) and all(row['passed'] for row in report['cases'])
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        report['after'] = guard(args.out)
        save()


if __name__ == '__main__':
    main()
