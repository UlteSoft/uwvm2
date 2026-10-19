#!/usr/bin/env python3
"""Run real private HTTPS deployment and opt-in VM checks for every product.

Consumes completed, hash-bound QEMU qualification rows. The upper Go deployment
gate runs on the host; each lower VM gate executes its actual target binary.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


TARGETS = ['x86_64', 'i686', 'aarch64', 'arm', 'armhf', 'armeb', 'aarch64_be',
           'ppc64', 'mips64', 'loongarch64']


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--cohort', type=Path)
    parser.add_argument('--suite', choices=['https', 'multi', 'policy'], default='https')
    parser.add_argument('--cg-limit-mib', type=int, default=32768)
    parser.add_argument('--resume', type=Path, help='Reuse completed, immutable matching product receipts')
    args = parser.parse_args()
    if os.uname().sysname != 'Linux' or not args.revision.isidentifier():
        raise RuntimeError('authorized Linux and simple output revision required')
    root = args.workspace.resolve(strict=True)
    cohort = (args.cohort or root).resolve(strict=True)
    prefix = {'https': 'usage-products-', 'multi': 'multi-products-', 'policy': 'policy-products-'}[args.suite]
    suite_script = {'https': 'embedded_usage.py', 'multi': 'multi_module_program.py', 'policy': 'execution_policy.py'}[args.suite]
    suite_checks = {'https': 42, 'multi': 44, 'policy': 18}[args.suite]
    output = root / (prefix + args.revision)
    output.mkdir(mode=0o700, exist_ok=False)
    expected = {(repo, target) for repo in ['uwvm2-ros', 'uwvm2', 'uwvm2-stable']
                for target in TARGETS if repo != 'uwvm2-ros' or target in TARGETS[:7]}
    header = sha(cohort / 'source/src/uwvm2/utils/security/wsea.h')
    report = {'status': 'IN_PROGRESS', 'scope': args.suite, 'driver_sha256': sha(__file__),
              'source_header_sha256': header, 'expected_products': len(expected), 'rows': []}
    deadline = time.monotonic() + 28800

    def save():
        temporary = output / 'report.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(output / 'report.json')

    try:
        if args.resume:
            previous = json.loads(args.resume.read_text())
            legacy = cohort / 'previous-run_wsea_usage_targets.py-1bd3441dba6e'
            legacy_https = (args.suite == 'https' and previous.get('scope') == __doc__ and
                            legacy.is_file() and sha(legacy) == previous.get('driver_sha256'))
            if (previous.get('source_header_sha256') != header or
                    (previous.get('scope') != args.suite and not legacy_https)):
                raise ValueError('resume source/suite differs')
            script = cohort / 'source/tests/security_wsea' / suite_script
            for row in previous['rows']:
                key = row['repository'], row['target']
                data = json.loads(Path(row['usage']).read_text())
                receipt = Path(row['usage']).parent.parent / (row['repository'] + '-' + row['target'] + '-guard.json')
                bounded = json.loads(receipt.read_text())
                matrix = json.loads(Path(row['matrix']).read_text())
                if (key not in expected or data.get('status') != 'PASS' or
                        data.get('source_sha256') != sha(script) or
                        data.get('go_sha256', data.get('go_binary_sha256')) != sha(root / 'native/wsea-cli') or
                        data.get('vm_sha256', data.get('vm_binary_sha256')) != row['vm_sha256'] or
                        sha(matrix['vm']['path']) != row['vm_sha256'] or
                        bounded.get('status') != 'PASS' or not bounded.get('owned_chain_closed') or
                        len(data.get('records', [])) != suite_checks or
                        not all(item.get('passed') for item in data['records'])):
                    raise ValueError('unqualified resume row: ' + repr(key))
                report['rows'].append(row)
                expected.remove(key)
            report['resumed_from'] = {'path': str(args.resume), 'sha256': sha(args.resume)}
        save()
        while expected:
            ready = {}
            for inventory in sorted(root.glob('qualification-products-*.json')):
                raw = inventory.read_bytes()
                try:
                    data = json.loads(raw)
                except json.JSONDecodeError:
                    continue
                if data.get('source_header_sha256') != header:
                    continue
                for row in data.get('rows', []):
                    key = row['repository'], row['target']
                    if key not in expected:
                        continue
                    build = json.loads(Path(row['build']).read_text())
                    if Path(build['plan']['source']).resolve() != (cohort / 'repositories' / row['repository']).resolve():
                        continue
                    if (row['target'] == 'x86_64' and
                            '--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/15' not in build['plan']['cxx']):
                        continue
                    matrix = json.loads(Path(row['matrix']).read_text())
                    interop = json.loads(Path(row['interop']).read_text())
                    if (build.get('status') != 'PASS' or matrix.get('status') != 'PASS' or
                            interop.get('status') != 'PASS' or matrix.get('execution') != 'qemu' or
                            matrix['vm']['sha256'] != row['binary_sha256'] or
                            sha(matrix['vm']['path']) != row['binary_sha256']):
                        continue
                    ready[key] = row, matrix, raw
            if not ready:
                if time.monotonic() >= deadline:
                    raise TimeoutError('products still unavailable: ' + repr(sorted(expected)))
                time.sleep(10)
                continue
            for key, (row, matrix, raw) in sorted(ready.items()):
                repo, target = key
                name = repo + '-' + target
                (output / (name + '-product.json')).write_bytes(raw)
                destination = output / name
                command = ['python3', str(cohort / 'source/tests/security_wsea/run_bounded_v2.py'),
                    '--disk-root', str(root), '--cg-limit-mib', str(args.cg_limit_mib), '--report',
                    str(output / (name + '-guard.json')), '--cwd', str(cohort / 'source'),
                    '--timeout', '900' if args.suite=='multi' else '300', '--', 'python3',
                    str(cohort / 'source/tests/security_wsea' / suite_script),
                    '--fixtures', str(root / 'fixtures-final'), '--go-cli', str(root / 'native/wsea-cli'),
                    '--vm', matrix['vm']['path'], '--exec-prefix-json', json.dumps(matrix['exec_prefix']),
                    '--output', str(destination)]
                print(json.dumps({'stage': name, 'state': 'START'}), flush=True)
                env = dict(os.environ, TMPDIR=str(root / 'tmp'))
                code = subprocess.run(command, env=env).returncode
                if code:
                    raise RuntimeError('retained failing usage stage: ' + name)
                report['rows'].append({'repository': repo, 'target': target,
                    'vm_sha256': row['binary_sha256'], 'matrix': row['matrix'],
                    'upper_gate': 'native host Go', 'lower_gate': 'actual target VM under QEMU',
                    'usage': str(destination / 'report.json')})
                expected.remove(key)
                save()
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        save()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
