#!/usr/bin/env python3
"""New-count reference evidence for the unchanged O2 timed/profile Wasm."""
import argparse
import json
import os
from pathlib import Path
import re

from linux_control import digest, guard, invoke, gated
from references import stable_output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--previous-manifests', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--manifests', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=200000)
    parser.add_argument('--wasmtime', type=Path, required=True)
    args = parser.parse_args()
    assert 1 <= args.rounds <= 100000000
    args.out.mkdir(parents=True, exist_ok=False)
    guard(args.out)
    os.sched_setaffinity(0, {4})
    own = Path(__file__).resolve().parent
    build_path = args.build / 'build-receipt.json'
    build = json.loads(build_path.read_text())
    assert build['passed']
    native = Path(build['native_python']['path'])
    assert digest(native) == build['native_python']['sha256']
    before = guard(args.out)
    report = {'passed': False, 'before': before, 'rounds': args.rounds, 'cases': [],
              'build_receipt': str(build_path), 'build_receipt_sha256': digest(build_path),
              'native_python': str(native), 'native_python_sha256': digest(native),
              'wasmtime': str(args.wasmtime), 'wasmtime_sha256': digest(args.wasmtime),
              'helper_sha256': digest(Path(__file__)),
              'purpose': 'correctness/calibration references, not formal performance ratios'}
    report_path = args.out / 'report.json'
    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    save()
    try:
        env = {k: v for k, v in os.environ.items() if not k.startswith('PYTHON')
               and k not in {'UWVM_PY_TEST', 'UWVM_PY_AUX', 'UWVM_PY_EMPTY'}}
        env['PYTHONHASHSEED'] = '0'
        invoke([native, own / 'checksum_oracle.py', str(args.rounds)],
               args.out / 'independent-checksum', env=env)
        scalar = (args.out / 'independent-checksum/stdout.log').read_text()
        match = re.fullmatch(r'iterations=([0-9]+) checksum=([0-9]+)\n', scalar)
        assert match and int(match[1]) == args.rounds
        checksum = int(match[2])
        expected = ('python classes=42 arrays=55 dicts=6 unicode=4 utf8=12 exceptions=3 '
                    'bytes=120 memory=369 gc=1 args=0 env=0\n'
                    f'Iterations: {args.rounds}\nChecksum: {checksum}\n')
        report['independent_checksum'] = checksum
        report['expected_stdout'] = expected
        invoke([native, '-S', '-B', own / 'semantics.py', str(args.rounds)],
               args.out / 'native-source-cli', env=env)
        assert (args.out / 'native-source-cli/stdout.log').read_text() == expected
        assert (args.out / 'native-source-cli/stderr.log').read_bytes() == b''
        for entry in ['timed', 'profile']:
            for engine in ['native', 'wasmtime']:
                artifact = build['artifacts'][entry + ('-native' if engine == 'native' else '-wasi')]
                assert digest(artifact['path']) == artifact['sha256']
                argv = [artifact['path'], str(args.rounds)]
                if engine == 'wasmtime':
                    argv = [str(args.wasmtime), 'run', '-C', 'cache=n,parallel-compilation=n',
                            '-W', 'max-wasm-stack=16777216', *argv]
                folder = args.out / entry / engine
                result = (gated(argv, folder, env=env) if entry == 'profile'
                          else invoke(argv, folder, env=env))
                text = (folder / 'stdout.log').read_text()
                assert stable_output(text) == expected
                if entry == 'timed':
                    assert (folder / 'stderr.log').read_bytes() == b''
                clock = re.search(r'^Kernel ns: ([0-9]+)$', text, re.MULTILINE)
                assert clock and int(clock[1]) > 0
                assert result['retired'] and result['ownership']['closed']
                assert digest(artifact['path']) == artifact['sha256']
                report['cases'].append({'entry': entry, 'engine': engine, 'passed': True,
                    'artifact_sha256': artifact['sha256'], 'stable_stdout': expected,
                    'kernel_ns': int(clock[1]), 'lifecycle': str(folder / 'receipt.json')})
                save()
        report['passed'] = True
        report['after'] = guard(args.out)
        save()
        args.manifests.mkdir(parents=True, exist_ok=False)
        for filename in ['perf-timed-cases.json', 'perf-profile-cases.json']:
            old = json.loads((args.previous_manifests / filename).read_text())
            assert len(old) == 1
            row = dict(old[0])
            assert digest(row['wasm']) == row['wasm_sha256']
            row.update(name=re.sub(r'-[0-9]+$', '-' + str(args.rounds), row['name']),
                       rounds=args.rounds, iterations=args.rounds, args=[str(args.rounds)],
                       expected_checksum=checksum, checks=expected.splitlines(),
                       checksum_line=expected.splitlines()[-1],
                       oracle=str(report_path), oracle_sha256=digest(report_path),
                       load_args=['--wasip1-noinherit-system-environment'])
            (args.manifests / filename).write_text(json.dumps([row], indent=2) + '\n')
    except BaseException as error:
        report['error'] = repr(error)
        report['after'] = guard(args.out)
        save()
        raise


if __name__ == '__main__':
    main()
