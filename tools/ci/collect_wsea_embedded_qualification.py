#!/usr/bin/env python3
"""Collect only complete, hash-bound embedded-v2 SSH/QEMU qualification evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import time

TARGETS = ['x86_64', 'i686', 'aarch64', 'arm', 'armhf', 'armeb', 'aarch64_be',
           'ppc64', 'mips64', 'loongarch64']
GO_TARGETS = set(TARGETS) - {'armeb', 'aarch64_be'}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--cohort', type=Path, required=True)
    parser.add_argument('--go-report', type=Path, required=True, action='append',
                        help='Repeat for continued campaigns; only fully qualified target rows count')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.workspace.resolve(strict=True)
    cohort = args.cohort.resolve(strict=True)
    output = args.output.absolute()
    header = sha(cohort / 'source/src/uwvm2/utils/security/wsea.h')
    expected = {(repo, target) for repo in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']
                for target in TARGETS if repo != 'uwvm2-ros' or target in TARGETS[:7]}
    files = set()

    def evidence(path):
        path = Path(path).resolve(strict=True)
        require(path.is_relative_to(root), 'evidence outside private qualification workspace')
        files.add(path)
        return read(path)

    def guard(path):
        data = evidence(path)
        require(data.get('status') == 'PASS' and data.get('owned_chain_closed'), 'failed/unclosed guard: ' + str(path))
        log = Path(path).with_suffix('.log')
        require(sha(log) == data['log_sha256'], 'changed guarded log: ' + str(log))
        files.add(log)
        return data

    def checks(path, count):
        data = evidence(path)
        require(data.get('status') == 'PASS' and len(data.get('records', [])) == count and
                all(x.get('passed') for x in data['records']), 'incomplete cases: ' + str(path))
        return data

    products = {}
    for inventory in sorted(root.glob('qualification-products-*.json')):
        data = read(inventory)
        if data.get('source_header_sha256') != header:
            continue
        files.add(inventory.resolve())
        for row in data.get('rows', []):
            key = row['repository'], row['target']
            if key not in expected:
                continue
            build = evidence(row['build'])
            if Path(build['plan']['source']).resolve() != (cohort / 'repositories' / row['repository']).resolve():
                continue
            security = build.get('security_source_sha256', {})
            require(set(security) == {'src/uwvm2/utils/security/wsea.h', 'src/uwvm2/uwvm/runtime/initializer/init.h', 'src/uwvm2/uwvm/runtime/initializer/init.cppm', 'src/uwvm2/uwvm/wasm/loader/wasm_file.h', 'src/uwvm2/uwvm/wasm/type/file.h', 'src/uwvm2/uwvm/cmdline/parser.h'} and all(sha(Path(build['plan']['source']) / name) == digest for name, digest in security.items()), 'changed/missing execution gate sources')
            for name in security:
                files.add((Path(build['plan']['source']) / name).resolve())
            for path, digest in build['plan'].get('linker_file_sha256', {}).items():
                require(sha(path) == digest, 'changed linker')
            if row['target'] == 'mips64':
                provider = Path(build['plan']['linker_provider_receipt'])
                require(sha(provider) == build['plan']['linker_provider_sha256'], 'changed MIPS linker provider')
                files.add(provider.resolve())
            require(build['status'] == 'PASS' and build['source_header_sha256'] == header, 'wrong build cohort')
            binary_dir = Path(row['build']).parent
            for name, digest in build['binary_sha256'].items():
                require(sha(binary_dir / name) == digest, 'changed product binary')
            for stage in build['stages']:
                for path, digest in stage.get('dependency_sha256', {}).items():
                    require(sha(path) == digest, 'changed compiled dependency: ' + path)
                log = binary_dir / (stage['name'] + '.log')
                require(sha(log) == stage['log_sha256'], 'changed compiler log')
                files.add(log.resolve())
            if row['target'] == 'arm':
                overlay = root / 'arm-boost-final/include/boost/unordered/detail/foa/core.hpp'
                require(any(stage.get('dependency_sha256', {}).get(str(overlay)) == sha(overlay)
                            for stage in build['stages']), 'ARM build did not consume the fixed sentinel')
            require(sha(build['plan']['cxx'][0]) == build['compiler_sha256'], 'changed compiler')
            guard(root / 'results' / ('product-' + row['target'] + '-' + binary_dir.name + '.json'))
            full = row.get('case_profile', 'full') == 'full'
            matrix = checks(row['matrix'], 554 if full else 94)
            require(matrix['execution'] == 'qemu' and matrix['target'] == row['target'] and
                    matrix.get('execution_oracle_revision') == 'r4' and
                    matrix['fixtures_sha256'] == sha(root / 'fixtures-final/fixtures.json') and
                    matrix['vm']['sha256'] == row['binary_sha256'] and
                    matrix['runner_sha256'] == row['verifier_sha256'], 'wrong QEMU product identity')
            for kind, path, count in [('matrix', row['matrix'], 554 if full else 94),
                                      ('interop', row['interop'], 482), ('warm', row['warm'], 277)]:
                checks(path, count)
                guard(root / 'results' / (kind + '-' + row['target'] + '-' + Path(path).parent.name + '.json'))
            require(read(row['warm'])['fixtures_sha256'] == matrix['fixtures_sha256'], 'wrong warm-process fixture cohort')
            receipt = Path(matrix['runtime_receipt']['path'])
            require(sha(receipt) == matrix['runtime_receipt']['sha256'], 'changed runtime receipt')
            runtime = evidence(receipt)
            for entry in runtime['runtime_files']:
                require(sha(entry['path']) == entry['sha256'], 'changed runtime file')
            crypto = Path(build['plan']['openssl_receipt'])
            require(sha(crypto) == build['openssl_receipt_sha256'], 'changed OpenSSL receipt')
            files.add(crypto.resolve())
            products[key] = dict(row, matrix_checks=554 if full else 94, interop_checks=482, warm_checks=277)
    require(set(products) == expected, 'missing products: ' + repr(sorted(expected - products.keys())))
    for suite, prefix, count in [('https', 'usage-products-', 42), ('multi', 'multi-products-', 44),
                                 ('policy', 'policy-products-', 18)]:
        covered = {}
        for index in sorted(root.glob(prefix + '*/report.json')):
            data = read(index)
            if data.get('source_header_sha256') != header:
                continue
            files.add(index.resolve())
            for row in data.get('rows', []):
                key = row['repository'], row['target']
                if key not in products or row['vm_sha256'] != products[key]['binary_sha256']:
                    continue
                usage = checks(row['usage'], count)
                require(usage.get('vm_sha256', usage.get('vm_binary_sha256')) == row['vm_sha256'], 'wrong usage product')
                guard(Path(row['usage']).parent.parent / (row['repository'] + '-' + row['target'] + '-guard.json'))
                covered[key] = row['usage']
        require(set(covered) == expected, 'missing ' + suite + ' products: ' + repr(sorted(expected - covered.keys())))
        for key, path in covered.items():
            products[key][suite] = path
            products[key][suite + '_checks'] = count
    go_rows = {}
    go_campaigns = []
    for campaign in args.go_report:
        go = evidence(campaign)
        require(go['status'] in ('PASS', 'FAIL') and 'full static cosign' in go['scope'],
                'unclosed or wrong Go campaign scope')
        for name, digest in go['source_sha256'].items():
            require(sha(root / 'wasm-cosign-full' / name) == digest, 'changed Go campaign source')
        require(sha('/home/macromodel/Documents/uwvm-test-261018/wsea/go-toolchain/go/bin/go') ==
                go['go_toolchain_binary_sha256'], 'changed Go compiler')
        go_campaigns.append({'path': str(campaign), 'status': go['status'],
                             'completed_targets': [row['target'] for row in go['rows']]})
        for row in go['rows']:
            target = row['target']
            require(target in GO_TARGETS and target not in go_rows, 'unexpected/duplicate Go target')
            interop = checks(row['interop'], 482)
            usage = checks(row['usage'], 42)
            require(interop['go_cli_sha256'] == usage['go_binary_sha256'] == row['binary_sha256'],
                    'wrong full cosign executable in target tests')
            execution = interop['go_cli_execution']
            require(interop['execution'] == 'qemu' and interop['target'] == target and
                    execution['exec_prefix'] and not execution['elf'].get('interpreter') and
                    all(execution['elf'][key] == interop['elf'][key] for key in ['machine', 'bits', 'byte_order']),
                    'full cosign did not execute as the actual static target')
            product = products[(row['product']['repository'], target)]
            require(usage['vm_binary_sha256'] == row['product']['binary_sha256'] == product['binary_sha256'],
                    'Go target used a different lower-gate product')
            for name in ['build-', 'execute-', 'usage-']:
                guard(campaign.parent / (name + target + '.json'))
            binary = campaign.parent / ('cosign-' + target)
            require(sha(binary) == row['binary_sha256'], 'changed full cosign binary')
            go_rows[target] = dict(row, campaign=str(campaign), binary=str(binary))
        # Preserve interrupted/failed stages too, with their original outcome.
        files.update(path.resolve() for path in campaign.parent.iterdir()
                     if path.is_file() and path.suffix in ('.json', '.log'))
    require(set(go_rows) == GO_TARGETS, 'incomplete full cosign target qualification')
    native = {
        'receipt': ('receipt-native-cache1/report.json', 40),
        'full_cosign_interop': ('interop-full-cosign-native-gate1/interop-report.json', 482),
        'full_cosign_https': ('usage-full-cosign-native-gate1/report.json', 42),
        'warm_process': ('warm-key-native-r1/report.json', 277),
        'asan_ubsan_warm_process': ('warm-key-sanitizer-cache1/report.json', 277),
        'elf_audit': ('elf-audit-padding-r1/report.json', 7),
        'execution_policy_native': ('policy-native-gate1/report.json', 18),
        'startup_sizes_and_tail_tamper': ('startup-sizes-native-gate1/report.json', 108),
    }
    for name, (path, count) in native.items():
        checks(root / path, count)
    arm_sentinel = checks(root / 'arm-sentinel-regression-gate6/report.json', 2)
    require(arm_sentinel.get('qemu_cpu') == 'arm926', 'missing explicit ARMv5 CPU regression')
    be_sdk = checks(root / 'be-sdk-kat-gate1/report.json', 2)
    for name in ['go-final-core-cli-r1.json', 'embedded-go-fuzz-30s-r1.json',
                 'native-final-receipt-build-cache1.json', 'native-final-receipt-bounds-cache1.json',
                 'native-full-cosign-usage-gate1.json', 'native-full-cosign-interop-gate1.json', 'policy-native-gate1.json',
                 'native-algorithm-startup-gate1.json',
                 'sanitizer-build-cache1.json', 'sanitizer-warm-matrix-cache1.json',
                 'native-full-cosign-startup-cache1.json', 'mips-kat-fixed-build-gate1.json',
                 'mips-kat-fixed-execute-gate1.json', 'arm-sentinel-regression-gate6.json',
                 'native-size-startup-gate1.json', 'be-sdk-kat-gate1.json']:
        guard(root / 'results' / name)
    require(evidence(root / 'results/xmake-provider-selection-r2.json')['status'] == 'PASS', 'failed provider integration')
    startup = evidence(root / 'startup-algorithms-gate1/report.json')
    require(startup['status'] == 'PASS' and len(startup['rows']) == 17, 'incomplete startup observations')
    kat_log = root / 'results/mips-kat-fixed-execute-gate1.log'
    require(kat_log.read_text().count('PASS ') == 3 and 'FAIL' not in kat_log.read_text(), 'MIPS digest/context-copy known answers failed')
    cosign_startup = evidence(root / 'startup-cosign-native-cache1/report.json')
    require(cosign_startup['status'] == 'PASS' and len(cosign_startup['rows']) == 17, 'incomplete full CLI startup observations')
    size_startup = evidence(root / 'startup-sizes-native-gate1/report.json')
    require(len(size_startup['rows']) == 6 and size_startup['vm_sha256'] == startup['vm_sha256'] and
            size_startup['go_sha256'] == cosign_startup['binary_sha256'], 'wrong native size observations')
    source_audits = {}
    for name in ['wsea-embedded-source-sync-current.json',
                 'wsea-embedded-final-gate-sources.json',
                 'wsea-embedded-upstream-source-audit.json',
                 'go-full-build-source-audit.json']:
        audit = evidence(root / 'final-source-audits' / name)
        require(audit['status'] == 'PASS', 'failed final source-only audit: ' + name)
        source_audits[name] = audit
    build_audit = next(item for item in source_audits['wsea-embedded-source-sync-current.json']['files']
                       if item['path'] == 'xmake.lua')
    for public_root, entry in build_audit['copies'].items():
        path = root / 'final-source-audits/project-build-files' / Path(public_root).name / 'xmake.lua'
        require(sha(path) == entry['sha256'], 'changed public build configuration snapshot')
        files.add(path.resolve())
    go_source_manifest = root / 'final-source-audits/go_embedded_v2_sources.sha256'
    for line in go_source_manifest.read_text().splitlines():
        digest, name = line.split('  ', 1)
        require(sha(root / 'wasm-cosign-full' / name) == digest, 'changed full Go source: ' + name)
    files.add(go_source_manifest.resolve())
    evidence(root / 'fixtures-final/fixtures.json')
    # Retain attempts as historical evidence, including failed guarded runs.
    for path in (root / 'results').iterdir():
        if path.is_file() and path.suffix in ('.json', '.log'):
            files.add(path.resolve())
    for base in ['source', 'cohort-rsa-cache/source']:
        for relative in ['src/uwvm2/utils/security/wsea.h', 'tests/security_wsea/algorithm_matrix.py']:
            files.add((root / base / relative).resolve())
    files.add((cohort / 'build_openssl_mips_context.py').resolve())
    files.add((cohort / 'go-full-target-driver.py').resolve())
    files.add((cohort / 'source/tools/dependencies/build_openssl.py').resolve())
    files.add((cohort / 'run_wsea_embedded_targets_armfix.py').resolve())
    files.add((cohort / 'run_wsea_embedded_targets_admit.py').resolve())
    files.add((cohort / 'run_wsea_embedded_targets_admit52.py').resolve())
    files.add((cohort / 'run_wsea_embedded_targets_beheaders.py').resolve())
    files.add((cohort / 'run_wsea_embedded_targets_beconstexpr.py').resolve())
    files.add((root / 'be-sdk-kat-gate1.py').resolve())
    files.add((root / 'arm-sentinel-regression-gate6.py').resolve())
    arm_overlay = root / 'arm-boost-final/include/boost/unordered/detail/foa/core.hpp'
    require(sha(arm_overlay) == read(root / 'arm-sentinel-regression-gate6/report.json')['fixed_header_sha256'],
            'changed ARM sentinel overlay')
    files.update(path.resolve() for path in (root / 'arm-boost-final/include').rglob('*') if path.is_file())
    for repo in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        files.add((cohort / 'repositories' / repo / 'third-parties/boost_unordered/include/boost/unordered/detail/foa/core.hpp').resolve())
    for directory in ['arm-diagnostic-gate1', 'arm-sentinel-regression-gate6', 'be-sdk-kat-gate1']:
        files.update(path.resolve() for path in (root / directory).glob('*.log'))
    for directory in [cohort / 'helper-history', cohort / 'source/tests/security_wsea', cohort / 'source/tools/ci']:
        for path in directory.glob('*'):
            if path.is_file() and path.suffix in ('.py', '.cpp', '.md'):
                files.add(path.resolve())
    for relative in ['go.mod', 'go.sum', 'cmd/cosign/cli/wasm.go', 'test/wsea/cli_main.go',
                     'test/wsea/README.md']:
        files.add((root / 'wasm-cosign-full' / relative).resolve())
    for folder in ['pkg/wasmsecure', 'cmd/cosign/cli/wasmsecure']:
        files.update(path.resolve() for path in (root / 'wasm-cosign-full' / folder).glob('*.go'))
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    def archive_name(path):
        return str(path.relative_to(root)) if path.is_relative_to(root) else 'external/' + sha(path) + '-' + path.name
    manifest = {archive_name(path): {'sha256': sha(path), 'original_path': str(path)} for path in sorted(files)}
    summary = {'status': 'PASS', 'schema': 'embedded-wsea-v2-qualification', 'collected_utc_seconds': time.time(),
        'source_header_sha256': header, 'code_cohort': str(cohort), 'products': [products[key] for key in sorted(products)],
        'full_cosign_targets': sorted(go_rows), 'full_cosign_campaigns': go_campaigns,
        'full_cosign_products': [go_rows[key] for key in sorted(go_rows)],
        'go_big_endian_arm': 'No standard Go Linux ports; host Go/full cosign interoperates with actual big-endian target VMs.',
        'mips_runtime_stack': 'The Debian glibc SDK retains executable-stack compatibility for pre-4.8 kernels; the recorded loader actually requests PROT_EXEC on the stack. The main ELF is linked with -z noexecstack, but this does not establish process-wide W^X. Crypto/authorization qualification does not claim that hardening property.',
        'scope': 'Ordinary interpreter, WASI, OpenSSL; LLVM/JIT and thread-local execution disabled. C++ language-module (UWVM_MODULE) builds were not qualified; the CLI --secure named-module command is covered.',
        'native': native, 'startup': startup, 'cosign_startup': cosign_startup, 'size_startup': size_startup,
        'source_only_audits': source_audits, 'arm_sentinel_regression': arm_sentinel, 'big_endian_sdk': be_sdk,
        'evidence_sha256': manifest}
    (output / 'qualification.json').write_text(json.dumps(summary, indent=2) + '\n')
    with tarfile.open(output / 'evidence.tar.gz', 'w:gz') as archive:
        for path in sorted(files):
            archive.add(path, arcname=archive_name(path), recursive=False)
    lines = ['# Embedded WSEA v2 qualification', '',
             'Status: **PASS**. Candidate builds and execution ran in the designated SSH Linux cgroup.', '',
             'The VM enables verification only for explicitly selected `--secure module` or',
             '`--secure named-module` paths. Each signed WASM carries one final `uwvm.wsea.v2`',
             'custom section. The complete cosign CLI provides the independent first gate;',
             'the VM verifies local evidence again before parsing and checks authorization before guest entry.', '',
             'Profiles: ES256/384/512, ESP256/384/512, Ed25519, PS256/384/512, RS256/384/512.',
             'RSA requires 3072–8192 bits and exponent 65537. Unsupported and weak profiles fail closed.', '',
             '| Actual QEMU target | uwvm2 | uwvm2-stable | uwvm2-ros | Full target cosign |',
             '| --- | --- | --- | --- | --- |']
    for target in TARGETS:
        cells = [target] + ['PASS' if (repo, target) in products else 'Outside requested ROS scope'
                            for repo in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']]
        cells.append('PASS' if target in GO_TARGETS else 'No standard Go Linux port; host-Go/target-VM interop passed')
        lines.append('| ' + ' | '.join(cells) + ' |')
    lines += ['', 'Each target has one full 554-check verifier/VM matrix; the other products on that',
              'target run a 94-check product matrix. Every one of the 27 VM products also passed',
              '482 interoperability checks, 277 same-process adversarial checks, 42 HTTPS/deployment',
              'checks, 44 multi-module checks and 18 execution-policy checks. These are executed',
              'assertions, not counts of independent security properties. Every full target cosign',
              'binary passed 482 interoperability and 42 HTTPS checks under its actual QEMU.', '',
              'Native checks include 40 execution-receipt cases, the full-CLI 482/42 suites,',
              '277 ordinary and 277 ASan/UBSan verifier cases, 18 execution-policy cases,',
              'core/CLI Go unit tests and 30 seconds of parser fuzzing. The linked OpenSSL archive',
              'was not itself sanitizer-instrumented. MIPS digest/context-copy known-answer tests passed.', '',
              '## Native startup observations', '',
              'Small fixture modules, 2 warmups and 7 samples per mode, including process startup.',
              'Recorded host load and tmpfs storage are part of these observations; these are not latency guarantees.', '',
              '| Signing case | VM median added startup (ms) |', '| --- | --- |']
    for row in startup['rows']:
        lines.append('| ' + row['case'] + ' | ' + format(row['p50_added_ms'], '.2f') + ' |')
    cli_times = [row['p50_ms'] for row in cosign_startup['rows']]
    lines += ['', '## Module size scaling', '',
              'Native, 2 warmups and 5 samples per mode, warm tmpfs page cache. All six modules',
              'also passed the real full-cosign first gate. Changing their final signed-prefix byte',
              'then caused both gates to refuse them. There are 108 command checks in this suite.', '',
              '| Algorithm | Padding (MiB) | Ordinary p50 (ms) | Secure p50 (ms) | Added p50 (ms) |',
              '| --- | --- | --- | --- | --- |']
    for row in size_startup['rows']:
        lines.append('| ' + row['algorithm'] + ' | ' + str(row['padding_mib']) + ' | '
            + format(row['p50_ms']['ordinary'], '.2f') + ' | ' + format(row['p50_ms']['secure'], '.2f')
            + ' | ' + format(row['p50_added_ms'], '.2f') + ' |')
    lines += ['', 'The complete native cosign process measured median verification times of '
              + format(min(cli_times), '.2f') + '–' + format(max(cli_times), '.2f')
              + ' ms across the seventeen profile/key samples under the separately recorded load.', '',
              'Only successful RSA public-key mathematical checks are memoized in a bounded process-local cache.',
              'Every new module still requires its signature, PKI, revocation, policy, time and rollback checks.', '',
              '## Qualification boundaries', '', summary['scope'], '', summary['mips_runtime_stack'], '',
              'Offline revocation requires fresh authenticated evidence and a protected host clock/state.',
              'Signing does not establish guest correctness or sandbox isolation. The historical failed attempts',
              'are retained with their original outcomes in the evidence archive; they are not successful qualifications.', '',
              'ARMv5 compilation uses an explicit include overlay containing the current public Boost header.',
              'This preserves earlier immutable build cohorts while fixing the pre-ARMv7 read-only empty-table',
              'sentinel: its 64-bit atomic load can use a write-capable compare-and-swap helper. The original',
              'small reproducer crashes and the fixed concurrent/moved-from regression succeeds. The overlay',
              'and exact compiler dependency hashes are retained. The conditional change is inactive on the',
              'other qualified architectures.', '',
              'All three complete OpenSSL source copies and the final public execution-gate/Go sources passed',
              'the separately labeled source-only hash audits. OpenSSL release provenance is retained in each repository.', '',
              'Security core SHA-256: `' + header + '`.', '',
              '`qualification.json` binds reports, binaries, runtimes, compiler inputs and guarded process cleanup.',
              '`evidence.tar.gz` contains reports, logs and selected sources; it contains no generated signing keys.', '']
    (output / 'SUMMARY.md').write_text('\n'.join(lines))
    (output / 'SHA256SUMS').write_text(''.join(sha(output / name) + '  ' + name + '\n'
        for name in ['qualification.json', 'evidence.tar.gz', 'SUMMARY.md']))
    print(json.dumps({'status': 'PASS', 'products': len(products), 'full_cosign_targets': len(GO_TARGETS),
                      'evidence_files': len(files), 'archive_sha256': sha(output / 'evidence.tar.gz')}))


if __name__ == '__main__':
    main()
