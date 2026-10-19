#!/usr/bin/env python3
"""Qualify real thread-local interpreter VMs through existing production suites.

Build all three repositories for x86_64 and aarch64 from an immutable qualified
cohort. Reuse its exact compiler, OpenSSL and QEMU runtime inputs; write new
reports without changing the original qualification or its fixtures/state.
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
    parser.add_argument('--build-admission-mib', type=int,
                        help='Wait for this whole-cgroup usage before a build; bounded launcher ceilings remain unchanged')
    parser.add_argument('--working-set-limit-mib', type=int,
                        help='Use memory.current minus inactive_file for admission; preserve the actual 64GiB hard cap')
    parser.add_argument('--bounded-launcher', type=Path, help='Explicit immutable bounded driver snapshot')
    parser.add_argument('--all-qualified-targets', action='store_true', help='Rebuild all 27 qualified repository/target combinations with TLS enabled')
    parser.add_argument('--resume', type=Path, help='Reuse complete unchanged TLS products from a previous attempt')
    args = parser.parse_args()
    admission_ceiling = args.working_set_limit_mib or args.cg_limit_mib
    if args.working_set_limit_mib is not None and (args.cg_limit_mib != 65536 or not 128 < args.working_set_limit_mib <= args.cg_limit_mib-8192):
        raise RuntimeError('cache-aware admission requires the 64GiB hard cap and 8GiB working-set reserve')
    if args.build_admission_mib is None:
        args.build_admission_mib = min(45056, admission_ceiling-4096)
    if args.build_admission_mib <= 0 or args.build_admission_mib > admission_ceiling-4096:
        raise RuntimeError('build admission must retain at least 4 GiB below the bounded launcher ceiling')
    if os.uname().sysname != 'Linux' or not args.revision.isidentifier():
        raise RuntimeError('authorized SSH Linux and simple output revision required')
    root = args.workspace.resolve(strict=True)
    cohort = args.cohort.resolve(strict=True)
    support = args.support_source.resolve(strict=True)
    output = root / ('thread-local-' + args.revision)
    output.mkdir(mode=0o700, exist_ok=False)
    original = json.loads(args.qualification.read_text())
    if original.get('status') != 'PASS':
        raise RuntimeError('completed original qualification required')
    bounded = args.bounded_launcher.resolve(strict=True) if args.bounded_launcher else cohort / 'source/tests/security_wsea/run_bounded_v2.py'
    builder = support / 'tools/ci/build_wsea_security_cross.py'
    full_cosign = root / 'native/cosign'
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'driver_sha256': sha(__file__),
              'qualification': str(args.qualification), 'qualification_sha256': sha(args.qualification),
              'source_header_sha256': sha(cohort / 'source/src/uwvm2/utils/security/wsea.h'),
              'original_source_header_sha256': original['source_header_sha256'], 'builder_sha256': sha(builder),
              'build_admission_mib': args.build_admission_mib, 'bounded_ceiling_mib': args.cg_limit_mib,
              'working_set_limit_mib': args.working_set_limit_mib, 'bounded_driver': str(bounded), 'bounded_driver_sha256': sha(bounded),
              'full_cosign_sha256': sha(full_cosign), 'rows': []}
    environment = dict(os.environ, TMPDIR=str(root / 'tmp'))

    def save():
        temporary = output / 'report.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(output / 'report.json')

    def stage(name, command, timeout=1200):
        receipt = output / (name + '-guard.json')
        argv = ['python3', str(bounded), '--report', str(receipt), '--cwd', str(cohort / 'source'),
                '--disk-root', str(root), '--cg-limit-mib', str(args.cg_limit_mib),
                '--timeout', str(timeout)]
        if args.working_set_limit_mib is not None:
            argv += ['--working-set-limit-mib', str(args.working_set_limit_mib)]
        argv += ['--', *map(str, command)]
        print(json.dumps({'stage': name, 'state': 'START'}), flush=True)
        result = subprocess.run(argv, env=environment)
        data = json.loads(receipt.read_text())
        if result.returncode or data.get('status') != 'PASS' or not data.get('owned_chain_closed'):
            raise RuntimeError('retained failed/unclosed stage: ' + name)
        print(json.dumps({'stage': name, 'state': 'PASS'}), flush=True)
        return str(receipt)

    try:
        completed = {}
        if args.resume:
            previous_run = json.loads(args.resume.read_text())
            if previous_run.get('source_header_sha256') != report['source_header_sha256']:
                raise RuntimeError('resume security core differs')
            for row in previous_run['rows']:
                build = json.loads(Path(row['build']).read_text())
                if (build.get('status') != 'PASS' or build.get('thread_local_enabled') is not True or
                        build.get('wsea_enabled') is not True or
                        Path(build['plan']['source']).resolve() != (cohort / 'repositories' / row['repository']).resolve() or
                        any(sha(Path(row['build']).parent / name) != digest for name, digest in build['binary_sha256'].items()) or
                        any(sha(path) != digest for entry in build['stages'] for path, digest in entry.get('dependency_sha256', {}).items())):
                    raise RuntimeError('changed completed TLS build')
                if set(row['suites']) != {'matrix', 'interop', 'warm', 'https', 'multi', 'policy'}:
                    raise RuntimeError('incomplete resumed suites')
                for path in [row['build_guard'], *[suite['guard'] for suite in row['suites'].values()]]:
                    guard = json.loads(Path(path).read_text())
                    if guard.get('status') != 'PASS' or not guard.get('owned_chain_closed') or sha(Path(path).with_suffix('.log')) != guard['log_sha256']:
                        raise RuntimeError('unqualified resume guard')
                for suite in row['suites'].values():
                    data = json.loads(Path(suite['report']).read_text())
                    if data.get('status') != 'PASS' or len(data['records']) != suite['checks'] or not all(item['passed'] for item in data['records']):
                        raise RuntimeError('unqualified resume suite')
                completed[row['repository'], row['target']] = row
            report['resumed_from'] = {'path': str(args.resume), 'sha256': sha(args.resume)}
        targets = ['x86_64', 'i686', 'aarch64', 'arm', 'armhf', 'armeb', 'aarch64_be', 'ppc64', 'mips64', 'loongarch64'] if args.all_qualified_targets else ['x86_64', 'aarch64']
        for target in targets:
            first_product = True
            for repository in ['uwvm2-ros', 'uwvm2', 'uwvm2-stable']:
                previous = next((row for row in original['products'] if row['repository'] == repository and row['target'] == target), None)
                if previous is None:
                    if repository == 'uwvm2-ros' and target in ['ppc64', 'mips64', 'loongarch64']:
                        continue
                    raise RuntimeError('missing original product')
                if (repository, target) in completed:
                    report['rows'].append(completed[repository, target])
                    save()
                    first_product = False
                    print(json.dumps({'stage': repository + '-' + target, 'state': 'REUSE_PASS'}), flush=True)
                    continue
                old_build = json.loads(Path(previous['build']).read_text())
                plan = dict(old_build['plan'])
                source = (cohort / 'repositories' / repository).resolve(strict=True)
                if sha(source / 'src/uwvm2/utils/security/wsea.h') != report['source_header_sha256']:
                    raise RuntimeError('changed source cohort')
                for relative, digest in old_build['security_source_sha256'].items():
                    if relative != 'src/uwvm2/utils/security/wsea.h' and sha(source / relative) != digest:
                        raise RuntimeError('unexpected change outside the security core')
                plan['source'] = str(source)
                sdk = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
                hostlibs = [sdk / 'lib/x86_64-unknown-linux-gnu', sdk / 'lib',
                            root / 'providers/debian/usr/lib/x86_64-linux-gnu',
                            root / 'providers/mips-binutils/usr/lib/x86_64-linux-gnu']
                environment['LD_LIBRARY_PATH'] = ':'.join(str(p) for p in hostlibs if p.is_dir())
                name = repository + '-' + target
                destination = output / name
                destination.mkdir(mode=0o700)
                plan['output'] = str(destination / 'product')
                plan_path = destination / 'plan.json'
                plan_path.write_text(json.dumps(plan, indent=2) + '\n')
                product = destination / 'product'
                retained_product = args.resume.parent / name / 'product' if args.resume else None
                retained_guard = args.resume.parent / (name + '-build-guard.json') if args.resume else None
                reuse_build = retained_product is not None and (retained_product / 'build-report.json').is_file() and retained_guard.is_file()
                if reuse_build:
                    retained = json.loads((retained_product / 'build-report.json').read_text())
                    guarded = json.loads(retained_guard.read_text())
                    expected_plan = {k: v for k, v in plan.items() if k != 'output'}
                    actual_plan = {k: v for k, v in retained['plan'].items() if k != 'output'}
                    if (retained.get('status') != 'PASS' or retained.get('thread_local_enabled') is not True or
                            retained.get('wsea_enabled') is not True or expected_plan != actual_plan or
                            retained.get('source_header_sha256') != report['source_header_sha256'] or
                            guarded.get('status') != 'PASS' or not guarded.get('owned_chain_closed') or
                            sha(retained_guard.with_suffix('.log')) != guarded['log_sha256'] or
                            any(sha(retained_product / file) != digest for file, digest in retained['binary_sha256'].items()) or
                            any(entry['returncode'] != 0 for entry in retained['stages']) or
                            any(sha(path) != digest for entry in retained['stages'] for path, digest in entry.get('dependency_sha256', {}).items())):
                        reuse_build = False
                    else:
                        product = retained_product
                        build_guard = str(retained_guard)
                        report.setdefault('resumed_complete_builds', []).append({'repository': repository, 'target': target,
                            'report': str(retained_product / 'build-report.json'), 'guard': build_guard})
                        save()
                if not reuse_build:
                    with (root / 'build-admission.lock').open('a') as lock:
                        fcntl.flock(lock, fcntl.LOCK_EX)
                        memory = Path('/sys/fs/cgroup/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope/memory.current')
                        deadline = time.monotonic() + 600
                        def admitted_usage():
                            current = int(memory.read_text())
                            if args.working_set_limit_mib is not None:
                                stats = dict(line.split() for line in (memory.parent / 'memory.stat').read_text().splitlines())
                                return max(0, current-int(stats['inactive_file']))
                            return current
                        while admitted_usage() > args.build_admission_mib * 1048576:
                            if time.monotonic() > deadline:
                                raise TimeoutError('shared cgroup lacks build headroom')
                            time.sleep(5)
                        build_guard = stage(name + '-build', ['python3', builder, '--plan', plan_path, '--thread-local'], 2400)
                build_path = product / 'build-report.json'
                build = json.loads(build_path.read_text())
                if (build.get('status') != 'PASS' or build.get('thread_local_enabled') is not True or
                        build.get('wsea_enabled') is not True or not all('-DUWVM_USE_THREAD_LOCAL=1' in item['argv']
                        for item in build['stages'] if item['name'] in ['runtime', 'main', 'host'])):
                    raise RuntimeError('actual product is not the requested TLS/WSEA configuration')
                matrix = json.loads(Path(previous['matrix']).read_text())
                common = ['--fixtures', root / 'fixtures-final', '--runner', product / 'algorithm_matrix_runner',
                          '--vm', product / 'uwvm', '--target', target, '--execution', 'qemu',
                          '--exec-prefix-json', json.dumps(matrix['exec_prefix']),
                          '--target-sysroot', matrix['sysroot'], '--runtime-root', matrix['runtime_root'],
                          '--runtime-receipt', matrix['runtime_receipt']['path'], '--require-cgroup']
                row = {'repository': repository, 'target': target, 'build': str(build_path),
                       'binary_sha256': sha(product / 'uwvm'), 'verifier_sha256': sha(product / 'algorithm_matrix_runner'),
                       'build_guard': build_guard, 'suites': {}}
                profile = 'full' if first_product or not args.all_qualified_targets else 'product'
                row['case_profile'] = profile
                suites = [
                    ('matrix', 554 if profile == 'full' else 94, ['python3', cohort / 'source/tests/security_wsea/algorithm_matrix.py', *common, '--case-profile', profile]),
                    ('interop', 482, ['python3', root / 'wasm-cosign/test/wsea/algorithm_interop.py',
                     '--matrix-driver', cohort / 'source/tests/security_wsea/algorithm_matrix.py',
                     '--go-cli', full_cosign, '--go-subcommands-json', '["wasm","secure"]', *common]),
                ]
                for kind, count, command in suites:
                    directory = destination / kind
                    receipt = stage(name + '-' + kind, [*command, '--output', directory], 1800)
                    path = directory / ('interop-report.json' if kind == 'interop' else 'report.json')
                    data = json.loads(path.read_text())
                    if data.get('status') != 'PASS' or len(data['records']) != count or not all(x['passed'] for x in data['records']):
                        raise RuntimeError('incomplete ' + kind)
                    row['suites'][kind] = {'report': str(path), 'guard': receipt, 'checks': count}
                directory = destination / 'warm'
                receipt = stage(name + '-warm', ['python3', cohort / 'source/tests/security_wsea/warm_key_matrix.py',
                    '--fixtures', root / 'fixtures-final', '--runner', product / 'algorithm_matrix_runner',
                    '--exec-prefix-json', json.dumps(matrix['exec_prefix']), '--output', directory], 1800)
                data = json.loads((directory / 'report.json').read_text())
                if data.get('status') != 'PASS' or len(data['records']) != 277 or not all(x['passed'] for x in data['records']):
                    raise RuntimeError('incomplete warm-key suite')
                row['suites']['warm'] = {'report': str(directory / 'report.json'), 'guard': receipt, 'checks': 277}
                for kind, script, count in [('https', 'embedded_usage.py', 42),
                                            ('multi', 'multi_module_program.py', 44),
                                            ('policy', 'execution_policy.py', 18)]:
                    # Multi/policy use the lightweight entry point whose command
                    # implementation is identical; HTTPS exercises full cosign.
                    directory = destination / kind
                    upper = full_cosign if kind == 'https' else root / 'native/wsea-cli'
                    command = ['python3', cohort / 'source/tests/security_wsea' / script,
                               '--fixtures', root / 'fixtures-final', '--go-cli', upper,
                               '--vm', product / 'uwvm', '--exec-prefix-json', json.dumps(matrix['exec_prefix']),
                               '--output', directory]
                    if kind == 'https':
                        command += ['--go-subcommands-json', '["wasm","secure"]']
                    receipt = stage(name + '-' + kind, command, 1200)
                    path = directory / 'report.json'
                    data = json.loads(path.read_text())
                    if data.get('status') != 'PASS' or len(data['records']) != count or not all(x['passed'] for x in data['records']):
                        raise RuntimeError('incomplete ' + kind)
                    row['suites'][kind] = {'report': str(path), 'guard': receipt, 'checks': count}
                report['rows'].append(row)
                save()
                first_product = False
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        save()
    print(json.dumps({'status': report['status'], 'products': len(report['rows']),
                      'checks': sum(suite['checks'] for row in report['rows'] for suite in row['suites'].values())}))


if __name__ == '__main__':
    main()
