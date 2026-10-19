#!/usr/bin/env python3
"""Native startup scaling and final-prefix tampering for real embedded modules."""
import argparse
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--vm', type=Path, required=True)
    parser.add_argument('--go-cli', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('authorized native Linux only')
    spec = importlib.util.spec_from_file_location('size_matrix', Path(__file__).with_name('algorithm_matrix.py'))
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fixtures = json.loads((args.fixtures / 'fixtures.json').read_text())
    go = [str(args.go_cli), 'wasm', 'secure']
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'source_sha256': matrix.sha(__file__),
              'vm_sha256': matrix.sha(args.vm), 'go_sha256': matrix.sha(args.go_cli),
              'fixtures_sha256': matrix.sha(args.fixtures / 'fixtures.json'),
              'affinity': sorted(os.sched_getaffinity(0)), 'ambient_load': os.getloadavg(),
              'filesystem': 'private tmpfs; warm page cache, not durable disk latency',
              'warmups': 2, 'samples_per_mode': 5, 'rows': [], 'records': []}

    def command(name, argv, expected=0, marker=None, code=None):
        start = time.perf_counter_ns()
        result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=60)
        elapsed = (time.perf_counter_ns() - start) / 1e6
        passed = result.returncode in ((expected,) if isinstance(expected, int) else expected)
        if marker:
            passed = passed and marker in result.stdout
        if code:
            passed = passed and code in result.stderr + result.stdout and matrix.GUEST_PREFIX not in result.stdout
        report['records'].append({'name': name, 'argv': list(map(str, argv)), 'returncode': result.returncode,
                                  'stdout': result.stdout, 'stderr': result.stderr, 'elapsed_ms': elapsed, 'passed': passed})
        if not passed:
            raise RuntimeError('size scenario failed: ' + name)
        return elapsed

    try:
        for algorithm in ['ES256', 'Ed25519']:
            original = fixtures['guest_algorithms'][algorithm]
            pki = original['pki']
            core, _, _, _ = matrix.embedded_parts(Path(original['module']).read_bytes())
            for size in [1, 16, 64]:
                directory = args.output / (algorithm + '-' + str(size))
                directory.mkdir(mode=0o700)
                state = directory / 'state'
                state.mkdir(mode=0o700)
                (state / 'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
                policy = directory / 'policy'
                identity = 'benchmark.' + algorithm + '.' + str(size)
                policy.write_text('UWVM-WSEA-POLICY-1\npolicy_version=1\ntrust_root=' + pki['root'] +
                    '\nstate_dir=' + str(state) + '\nmax_crl_age_seconds=86400\nmax_module_bytes=104857600\nmodule=' +
                    identity + '|uwvm2|' + pki['pin'] + '|1|0|3|' + algorithm + '\n')
                module = directory / 'module.wasm'
                name = b'wsea.benchmark.padding'
                padding = size * 1024 * 1024
                with module.open('wb') as stream:
                    stream.write(core)
                    stream.write(b'\0' + matrix.uleb(len(matrix.uleb(len(name))) + len(name) + padding))
                    stream.write(matrix.uleb(len(name)) + name)
                    for _ in range(size):
                        stream.write(bytes(1024 * 1024))
                prefix_size = module.stat().st_size
                now = int(time.time())
                stamp = lambda seconds: dt.datetime.fromtimestamp(seconds, dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
                command('sign ' + identity, [*go, 'sign', module, '--policy', policy, '--key', pki['key'],
                    '--certificate', pki['certificate'], '--crl', pki['crl'], '--module-id', identity,
                    '--module-version', '1', '--algorithm', algorithm, '--capabilities', '3',
                    '--not-before', stamp(now - 60), '--not-after', stamp(now + 3600)])
                command('upper gate ' + identity, [*go, 'verify', module, '--policy', policy])
                samples = {'ordinary': [], 'secure': []}
                for iteration in range(7):
                    for secure in [False, True]:
                        argv = [str(args.vm)]
                        if secure:
                            argv += ['--secure', 'module', module, policy]
                        argv += ['--run', module]
                        elapsed = command(identity + (' secure' if secure else ' ordinary'), argv,
                                          marker=original['guest_marker'])
                        if iteration >= 2:
                            samples['secure' if secure else 'ordinary'].append(elapsed)
                medians = {mode: statistics.median(values) for mode, values in samples.items()}
                report['rows'].append({'algorithm': algorithm, 'padding_mib': size,
                    'prefix_bytes': prefix_size, 'signed_bytes': module.stat().st_size,
                    'signed_sha256': matrix.sha(module), 'samples_ms': samples, 'p50_ms': medians,
                    'p50_added_ms': medians['secure'] - medians['ordinary']})
                with module.open('r+b') as stream:
                    stream.seek(prefix_size - 1)
                    stream.write(b'\1')
                command('upper gate final-prefix tamper ' + identity, [*go, 'verify', module, '--policy', policy],
                        expected=1, code='SIG_004')
                command('VM final-prefix tamper ' + identity, [args.vm, '--secure', 'module', module, policy,
                        '--run', module], expected=(126, 127), code='SIG_004')
                print(json.dumps(report['rows'][-1]), flush=True)
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
