#!/usr/bin/env python3
"""Exercise the real VM without OpenSSL: ordinary guests run, secure requests refuse.

Run only through the SSH Linux cgroup bounded launcher. This is a separate
build-configuration regression, not a cryptographic qualification.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures', 'vm', 'build-report', 'matrix-driver', 'runtime-receipt', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--exec-prefix-json', required=True)
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        raise RuntimeError('candidate execution requires authorized SSH Linux')
    spec = importlib.util.spec_from_file_location('wsea_disabled_matrix', args.matrix_driver)
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    before = matrix.cgroup_snapshot(True)
    os.umask(0o077)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    build = json.loads(args.build_report.read_text())
    fixture = json.loads((args.fixtures / 'fixtures.json').read_text())['guest_algorithms']['ES256']
    sealed = Path(fixture['module']).read_bytes()
    unsigned = args.output / 'unsigned.wasm'
    unsigned.write_bytes(matrix.embedded_parts(sealed)[0])
    changed = args.output / 'opaque-seal.wasm'
    changed.write_bytes(sealed[:-1] + bytes([sealed[-1] ^ 1]))
    missing_policy = args.output / 'absent.policy'
    missing_module = args.output / 'absent.wasm'
    prefix = json.loads(args.exec_prefix_json)
    if not isinstance(prefix, list) or not prefix or not all(isinstance(x, str) for x in prefix):
        raise ValueError('explicit actual target QEMU/loader argv required')
    vm = [*prefix, str(args.vm)]
    rows = []
    report = {'status': 'IN_PROGRESS', 'scope': __doc__, 'records': rows,
              'source_sha256': matrix.sha(__file__), 'matrix_driver_sha256': matrix.sha(args.matrix_driver),
              'fixtures_sha256': matrix.sha(args.fixtures / 'fixtures.json'),
              'build_report': str(args.build_report), 'build_report_sha256': matrix.sha(args.build_report),
              'vm_sha256': matrix.sha(args.vm), 'execution': 'qemu', 'exec_prefix': prefix,
              'runtime_receipt': str(args.runtime_receipt), 'runtime_receipt_sha256': matrix.sha(args.runtime_receipt),
              'execution_prefix_file_sha256': {p: matrix.sha(p) for p in prefix if Path(p).is_absolute() and Path(p).is_file()},
              'cgroup_before': before}

    def check(name, passed, **details):
        rows.append(dict(name=name, passed=passed, **details))
        print('PASS' if passed else 'FAIL', name, flush=True)
        if not passed:
            raise RuntimeError(name)

    def run(name, arguments, *, expected=(0,), contains=(), guest=False):
        argv = [*vm, *map(str, arguments)]
        result = subprocess.run(argv, capture_output=True, text=True, timeout=60)
        text = result.stdout + result.stderr
        passed = result.returncode in expected and all(token in text for token in contains)
        passed = passed and (fixture['guest_marker'] in result.stdout if guest else matrix.GUEST_PREFIX not in text)
        check(name, passed, argv=argv, expected_exit_codes=list(expected), returncode=result.returncode,
              stdout=result.stdout, stderr=result.stderr)

    try:
        report['elf'] = matrix.elf_identity(args.vm)
        matrix.target_elf(report['elf'], build['plan']['target'])
        check('actual ELF matches the claimed QEMU target', True)
        runtime = json.loads(args.runtime_receipt.read_text())
        check('qualified runtime files remain unchanged', runtime['target'] == build['plan']['target'] and
              all(matrix.sha(entry['path']) == entry['sha256'] for entry in runtime['runtime_files']))
        check('actual successful VM build explicitly excludes WSEA',
              build.get('status') == 'PASS' and build.get('wsea_enabled') is False and
              build['binary_sha256'].get('uwvm') == report['vm_sha256'])
        argv = [arg for stage in build['stages'] for arg in stage['argv']]
        check('no OpenSSL definition or archive enters compiler/linker commands',
              not any('UWVM_WSEA_OPENSSL' in arg or 'libcrypto' in arg or 'libssl' in arg or
                      arg in ('-lcrypto', '-lssl') for arg in argv))
        dependencies = {name: digest for stage in build['stages']
                        for name, digest in stage.get('dependency_sha256', {}).items()}
        check('actual compiled dependencies exclude OpenSSL headers',
              bool(dependencies) and not any('/openssl/' in name for name in dependencies))
        check('compiled dependency bytes and compiler remain unchanged',
              matrix.sha(build['plan']['cxx'][0]) == build['compiler_sha256'] and
              all(matrix.sha(name) == digest for name, digest in dependencies.items()))
        run('secure help remains available', ['--secure', 'help'], contains=('--secure', 'module'))
        run('help secure category remains available', ['--help', 'secure'], contains=('--secure',))
        run('status clearly reports unavailable verification', ['--secure', 'status'], contains=('disabled',))
        run('ordinary unsigned guest executes', ['--run', unsigned], guest=True)
        run('ordinary sealed guest remains explicitly unchecked', ['--run', fixture['module']], guest=True)
        run('ordinary opaque custom-section evidence remains unchecked', ['--run', changed], guest=True)
        run('tokens after run are guest arguments',
            ['--run', unsigned, '--secure', 'module', unsigned, missing_policy], guest=True)
        for name, module, named in [('unsigned', unsigned, False), ('sealed', fixture['module'], False),
                                    ('opaque-seal', changed, False), ('named', fixture['module'], True),
                                    ('missing-input', missing_module, False)]:
            selection = ['--secure', 'named-module' if named else 'module', module, missing_policy]
            if named:
                selection.append('matrix.app')
            run(name + ' secure selection fails closed before any guest',
                [*selection, '--run', module], expected=(126, 127), contains=('SIG_005',))
        run('secure prepass refuses before earlier preload callback can open input',
            ['--wasm-preload-library', missing_module, 'library', '--secure', 'module', unsigned,
             missing_policy, '--run', unsigned], expected=(126, 127), contains=('SIG_005',))
        run('unknown secure subcommand refuses', ['--secure', 'unknown', '--run', unsigned],
            expected=(126, 127), contains=('unknown --secure subcommand',))
        run('incomplete secure selection refuses', ['--secure', 'module', unsigned, '--run', unsigned],
            expected=(126, 127), contains=('requires',))
        report['status'] = 'PASS'
    except BaseException as error:
        report['status'] = 'FAIL'
        report['error'] = repr(error)
        raise
    finally:
        report['cgroup_after'] = matrix.cgroup_snapshot(True)
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
