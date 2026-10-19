#!/usr/bin/env python3
"""Build and QEMU-test all three real x86_64 VMs with OpenSSL/WSEA disabled.

Consumes a completed embedded-v2 qualification for exact compilers, source
cohorts and QEMU runtimes. The original qualification remains immutable.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['workspace', 'cohort', 'qualification', 'support-source']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--cg-limit-mib', type=int, default=32768)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux' or not args.revision.isidentifier():
        raise RuntimeError('authorized SSH Linux and simple output revision required')
    root = args.workspace.resolve(strict=True)
    cohort = args.cohort.resolve(strict=True)
    support = args.support_source.resolve(strict=True)
    output = root / ('backend-disabled-' + args.revision)
    output.mkdir(mode=0o700, exist_ok=False)
    qualification = json.loads(args.qualification.read_text())
    if qualification.get('status') != 'PASS':
        raise RuntimeError('a complete original qualification is required')
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'driver_sha256': sha(__file__),
              'qualification': str(args.qualification), 'qualification_sha256': sha(args.qualification),
              'source_header_sha256': qualification['source_header_sha256'], 'rows': []}
    bounded = cohort / 'source/tests/security_wsea/run_bounded_v2.py'
    builder = support / 'tools/ci/build_wsea_security_cross.py'
    harness = support / 'tests/security_wsea/backend_disabled.py'
    report['support_source_sha256'] = {str(p): sha(p) for p in [builder, harness]}

    def save():
        temporary = output / 'report.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(output / 'report.json')

    def run(name, command, timeout, environment):
        receipt = output / (name + '-guard.json')
        print(json.dumps({'stage': name, 'state': 'START'}), flush=True)
        argv = ['python3', str(bounded), '--report', str(receipt), '--cwd', str(cohort / 'source'),
                '--disk-root', str(root), '--cg-limit-mib', str(args.cg_limit_mib),
                '--timeout', str(timeout), '--', *map(str, command)]
        result = subprocess.run(argv, env=environment)
        guard = json.loads(receipt.read_text())
        if result.returncode or guard.get('status') != 'PASS' or not guard.get('owned_chain_closed'):
            raise RuntimeError('retained failed/unclosed stage: ' + name)
        print(json.dumps({'stage': name, 'state': 'PASS'}), flush=True)
        return str(receipt)

    try:
        for repository in ['uwvm2-ros', 'uwvm2', 'uwvm2-stable']:
            original = next(row for row in qualification['products']
                            if row['repository'] == repository and row['target'] == 'x86_64')
            build = json.loads(Path(original['build']).read_text())
            plan = dict(build['plan'])
            sdk = Path(plan['cxx'][0]).parent.parent
            hostlibs = [sdk / 'lib/x86_64-unknown-linux-gnu', sdk / 'lib',
                        root / 'providers/debian/usr/lib/x86_64-linux-gnu',
                        root / 'providers/mips-binutils/usr/lib/x86_64-linux-gnu']
            environment = dict(os.environ, TMPDIR=str(root / 'tmp'),
                               LD_LIBRARY_PATH=':'.join(str(p) for p in hostlibs if p.is_dir()))
            report['host_compiler_library_dirs'] = environment['LD_LIBRARY_PATH']
            source = Path(plan['source']).resolve(strict=True)
            if (source != (cohort / 'repositories' / repository).resolve(strict=True) or
                    sha(source / 'src/uwvm2/utils/security/wsea.h') != report['source_header_sha256']):
                raise RuntimeError('source cohort differs from qualified production gate')
            destination = output / repository
            destination.mkdir(mode=0o700)
            plan['output'] = str(destination / 'product')
            plan_path = destination / 'plan.json'
            plan_path.write_text(json.dumps(plan, indent=2) + '\n')
            # Serialize only our builds. The bounded launcher independently
            # observes actual whole-cgroup memory, RSS, OOM and process ownership.
            with (root / 'build-admission.lock').open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                deadline = time.monotonic() + 600
                current = Path('/sys/fs/cgroup/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope/memory.current')
                while int(current.read_text()) > min(45056, args.cg_limit_mib - 4096) * 1048576:
                    if time.monotonic() > deadline:
                        raise TimeoutError('shared cgroup lacks build admission headroom')
                    time.sleep(5)
                build_guard = run(repository + '-build',
                                  ['python3', builder, '--plan', plan_path, '--disable-wsea'], 2400, environment)
            matrix = json.loads(Path(original['matrix']).read_text())
            product_report = destination / 'product/build-report.json'
            usage = destination / 'usage'
            test_guard = run(repository + '-qemu',
                ['python3', harness, '--fixtures', root / 'fixtures-final',
                 '--vm', destination / 'product/uwvm', '--build-report', product_report,
                 '--matrix-driver', cohort / 'source/tests/security_wsea/algorithm_matrix.py',
                 '--runtime-receipt', matrix['runtime_receipt']['path'],
                 '--exec-prefix-json', json.dumps(matrix['exec_prefix']), '--output', usage], 300, environment)
            data = json.loads((usage / 'report.json').read_text())
            if data.get('status') != 'PASS' or len(data['records']) != 21 or not all(x['passed'] for x in data['records']):
                raise RuntimeError('incomplete disabled-backend behavior checks')
            report['rows'].append({'repository': repository, 'target': 'x86_64', 'checks': 21,
                'binary_sha256': data['vm_sha256'], 'build': str(product_report),
                'usage': str(usage / 'report.json'), 'build_guard': build_guard, 'test_guard': test_guard})
            save()
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        save()
    print(json.dumps({'status': report['status'], 'products': len(report['rows']), 'checks': 63}))


if __name__ == '__main__':
    main()
