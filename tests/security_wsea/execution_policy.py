#!/usr/bin/env python3
"""Real VM import enforcement after the independent upper crypto gate accepts."""
import argparse
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures', 'go-cli', 'vm', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--exec-prefix-json', default='[]')
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('matrix', Path(__file__).with_name('algorithm_matrix.py'))
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    go = [str(args.go_cli)]
    vm = [*json.loads(args.exec_prefix_json), str(args.vm)]
    rows = []

    def run(name, argv, expected=(0,), codes=(), marker=None, forbid=False):
        command = list(map(str, argv))
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        passed = result.returncode in expected and (not codes or any(c in output for c in codes))
        if marker:
            passed = passed and marker in result.stdout
        if forbid:
            passed = passed and matrix.GUEST_PREFIX not in output
        rows.append({'name': name, 'argv': command, 'expected_exit_codes': list(expected),
                     'returncode': result.returncode, 'stdout': result.stdout,
                     'stderr': result.stderr, 'passed': passed})
        print('PASS' if passed else 'FAIL', name, flush=True)
        if not passed:
            raise RuntimeError('execution policy check failed: ' + name)

    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'source_sha256': matrix.sha(__file__),
              'vm_sha256': matrix.sha(args.vm), 'go_sha256': matrix.sha(args.go_cli), 'records': rows}
    try:
        for algorithm in ['ES256', 'Ed25519']:
            case = fixtures['guest_algorithms'][algorithm]
            directory = args.output / algorithm
            directory.mkdir(mode=0o700)
            state = directory / 'state'
            state.mkdir(mode=0o700)
            (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
            policy = directory / 'policy'
            policy.write_text(case['policy'].replace('@STATE_DIR@', str(state)))
            module = directory / 'guest.wasm'
            module.write_bytes(matrix.embedded_parts(Path(case['module']).read_bytes())[0])
            ca = case['pki']
            now = dt.datetime.now(dt.timezone.utc)
            stamp = lambda t: t.strftime('%Y-%m-%dT%H:%M:%SZ')
            sign = [*go, 'sign', module, '--policy', policy, '--key', ca['key'],
                    '--certificate', ca['certificate'], '--crl', ca['crl'], '--module-id', 'matrix.app',
                    '--module-version', '1', '--algorithm', algorithm,
                    '--not-before', stamp(now-dt.timedelta(minutes=1)),
                    '--not-after', stamp(now+dt.timedelta(hours=1))]
            run(algorithm + ' sign with no host capabilities', [*sign, '--capabilities', '0'])
            run(algorithm + ' upper crypto gate accepts authentic restricted manifest', [*go, 'verify', module, '--policy', policy])
            selected = [*vm, '--secure', 'module', module, policy, '--run', module]
            run(algorithm + ' VM blocks undeclared fd_write before entry', selected,
                expected=(126, 127), codes=('POL_003',), forbid=True)
            run(algorithm + ' ordinary mode remains explicitly unchecked', [*vm, '--run', module], marker=case['guest_marker'])
            alias = directory / 'alias.wasm'
            alias.symlink_to(module)
            run(algorithm + ' selected symlink cannot bypass gate',
                [*vm, '--secure', 'module', alias, policy, '--run', alias],
                expected=(126, 127), codes=('POL_003', 'SIG_001'), forbid=True)
            run(algorithm + ' explicitly re-sign with required permissions', [*sign, '--replace-signature', '--capabilities', '3'])
            run(algorithm + ' authorized imports execute real guest', selected, marker=case['guest_marker'])
            digest = matrix.sha(module)
            run(algorithm + ' publisher cannot exceed administrator capability mask',
                [*sign, '--replace-signature', '--capabilities', '32'], expected=(1,), codes=('POL_003',))
            unchanged = matrix.sha(module) == digest
            rows.append({'name': algorithm + ' refused signing preserves original file', 'passed': unchanged})
            if not unchanged:
                raise RuntimeError('refused sign changed input')
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
