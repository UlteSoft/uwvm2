#!/usr/bin/env python3
"""Real multi-module programs with independent identities and warm RSA keys."""
import argparse
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--go-cli', type=Path, required=True)
    parser.add_argument('--vm', type=Path, required=True)
    parser.add_argument('--exec-prefix-json', default='[]')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('authorized Linux only')
    spec = importlib.util.spec_from_file_location('multi_wsea_matrix', Path(__file__).with_name('algorithm_matrix.py'))
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    go = [str(args.go_cli)]
    vm = [*json.loads(args.exec_prefix_json), str(args.vm)]
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'source_sha256': matrix.sha(__file__),
              'vm_sha256': matrix.sha(args.vm), 'go_sha256': matrix.sha(args.go_cli), 'records': []}

    def record(name, command, expected=(0,), marker=None, forbidden=False, codes=()):
        start = time.perf_counter_ns()
        p = subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=120)
        output = p.stdout + p.stderr
        passed = p.returncode in expected
        if marker:
            passed = passed and marker in p.stdout
        if forbidden:
            passed = passed and matrix.GUEST_PREFIX not in output
        if codes:
            passed = passed and any(code in output for code in codes)
        report['records'].append({'name': name, 'argv': list(map(str, command)),
            'expected_exit_codes': list(expected), 'returncode': p.returncode,
            'stdout': p.stdout, 'stderr': p.stderr, 'elapsed_ns': time.perf_counter_ns() - start, 'passed': passed})
        print('PASS' if passed else 'FAIL', name, flush=True)
        if not passed:
            raise RuntimeError('multi-module case failed: ' + name)

    def scenario(name, originals):
        directory = args.output / name
        directory.mkdir(mode=0o700)
        roots = dict.fromkeys(item['pki']['root'] for _, item in originals)
        root_file = directory / 'roots.pem'
        root_file.write_bytes(b''.join(Path(path).read_bytes() for path in roots))
        state = directory / 'state'
        state.mkdir(mode=0o700)
        (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
        policy = directory / 'policy'
        text = ('UWVM-WSEA-POLICY-1\npolicy_version=1\ntrust_root=' + str(root_file) +
                '\nstate_dir=' + str(state) + '\nmax_crl_age_seconds=86400\nmax_module_bytes=1048576\n')
        for module_name, item in originals:
            text += ('module=program.' + module_name + '|uwvm2|' + item['pki']['pin'] +
                     '|1|0|3|' + item['algorithm'] + '\n')
        policy.write_text(text)
        stamp = lambda seconds: dt.datetime.fromtimestamp(seconds, dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
        now = int(time.time())
        modules = []
        for module_name, item in originals:
            core, _, _, _ = matrix.embedded_parts(Path(item['module']).read_bytes())
            path = directory / (module_name + '.wasm')
            path.write_bytes(core)
            ca = item['pki']
            record(name + ' sign ' + module_name, [*go, 'sign', path, '--policy', policy,
                '--key', ca['key'], '--certificate', ca['certificate'], '--crl', ca['crl'],
                '--module-id', 'program.' + module_name, '--module-version', '1', '--capabilities', '3',
                '--algorithm', item['algorithm'], '--not-before', stamp(now - 60), '--not-after', stamp(now + 3600)])
            record(name + ' upper gate ' + module_name, [*go, 'verify', path, '--policy', policy])
            modules.append((module_name, path, item))
        # Preloads appear before secure declarations to exercise parser prepass.
        command = list(vm)
        for module_name, path, item in modules[1:]:
            command += ['--wasm-preload-library', path, module_name]
        for module_name, path, item in modules:
            command += ['--secure', 'named-module', path, policy, 'program.' + module_name]
        command += ['--run', modules[0][1]]
        record(name + ' all independently selected modules execute', command, marker=modules[0][2]['guest_marker'])
        # Last RSA library shares an already validated key with preceding
        # libraries. Its own bad signature must still prevent all guest entry.
        bad = modules[-1][1]
        core, signature, certificates, crls = matrix.embedded_parts(bad.read_bytes())
        bad.write_bytes(matrix.pack_embedded(core, signature[:-1] + bytes([signature[-1] ^ 1]), certificates, crls))
        record(name + ' upper gate rejects changed library', [*go, 'verify', bad, '--policy', policy],
               expected=(1,), codes=('SIG_003',))
        record(name + ' warm key cannot authorize changed library', command,
               expected=(126, 127), forbidden=True, codes=('SIG_003',))

    try:
        scenario('all-13-profiles', list(fixtures['guest_algorithms'].items()))
        large = next(case for case in fixtures['cases'] if case['name'] == 'RSA8192 genuine RS256 boundary and guest witness')
        scenario('six-rsa8192-modules', [('rsa' + str(index), large) for index in range(6)])
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
