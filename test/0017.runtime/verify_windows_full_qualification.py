#!/usr/bin/env python3
"""Cross-check the ordinary Windows LLVM-full PE, guest results and QEMU scope."""

import argparse
import hashlib
import json
from pathlib import Path


CPUS = sorted([0, 2, 4, 6, *range(16, 32)])
GC_STEMS = ('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
            'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
            'gc_struct_into_anyref_table', 'i31_table64')
BRANCH_STEMS = ('branch_semantics_br_if', 'branch_semantics_br_on_null',
                'branch_semantics_br_on_non_null')
GAP_STEMS = ('ref-test-exn-null', 'ref-cast-exn-null', 'br-on-cast-exn-null',
             'exn-cast-retained', 'ref-cast-exn-null-trap', 'exnref_call_retained',
             'eh-cross-function-catch-ref', 'eh-cross-function-win64-seh')
SOURCE_STEMS = ('source-c-dwarf4', 'source-c-dwarf5',
                'source-cpp-dwarf4', 'source-cpp-dwarf5', 'source-rust-dwarf5')


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path: Path) -> dict:
    result = json.loads(path.read_text())
    if not isinstance(result, dict):
        raise ValueError(f'expected JSON object: {path}')
    return result


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def required_core3_names() -> set[str]:
    names: set[str] = {'eh-threads-auto'}
    for policy in ('instruction', 'unwind'):
        for stem in GC_STEMS:
            names.add(f'gc-{stem}-{policy}')
            names.add(f'gc-{stem}-{policy}-gc-off')
        names.add(f'gc-i31_table64-{policy}-table64-off')
        names.add(f'gc-reference-types-{policy}-off')
        names.add(f'gc-table-instructions-{policy}-off')
        names.add(f'gc-const-expr-{policy}')
        names.add(f'gc-const-expr-{policy}-gc-off')
        names.add(f'exnref-struct-jit-{policy}')
        names.update(f'exnref-struct-jit-{policy}-{feature}-off'
                     for feature in ('exceptions', 'gc', 'reference-types'))
        names.add(f'ref-func-table-initializer-{policy}')
        names.add(f'ref-func-table-initializer-{policy}-extended-const-off')
        names.add(f'ref-func-table-initializer-{policy}-feature-off')
        names.add(f'eh-threads-{policy}')
        names.add(f'eh-threads-{policy}-exceptions-off')
        names.add(f'eh-threads-{policy}-threads-off')
        for stem in BRANCH_STEMS:
            names.add(f'{stem}-{policy}')
            names.add(f'{stem}-{policy}-function-references-off')
        for stem in GAP_STEMS:
            names.add(f'{stem}-{policy}')
            names.add(f'{stem}-{policy}-gc-off')
            names.add(f'{stem}-{policy}-exceptions-off')
    return names


def required_debug_policies() -> set[str]:
    names = {'debug-jit-lazy-mode-rejected',
             'core3-typed-reference-full-and-feature-gate',
             'core3-throw-ref-bottom-exceptions-feature-off',
             'core3-br-on-null-bottom-exceptions-feature-off',
             'core3-br-on-null-bottom-function-references-off',
             'instruction', 'unwind', 'rejected-input-output-alias',
             'late-attach-fd-rejected', 'late-attach-guessed-handle-rejected'}
    names.update(f'core3-throw-ref-bottom-{policy}'
                 for policy in ('instruction', 'unwind'))
    names.update(f'core3-br-on-null-bottom-{policy}'
                 for policy in ('instruction', 'unwind'))
    names.update(f'{policy}-function-replacement'
                 for policy in ('instruction', 'unwind'))
    names.update(f'{policy}-active-frame-replacement-rejected'
                 for policy in ('instruction', 'unwind'))
    names.update(f'{policy}-cross-function-eh-replacement'
                 for policy in ('instruction', 'unwind'))
    names.update(f'{policy}-{case}' for policy in ('instruction', 'unwind')
                 for case in ('non-executable-offset-rejected',
                              'nonzero-executable-breakpoint'))
    names.update(f'{stem}-{operation}' for stem in SOURCE_STEMS
                 for operation in ('into', 'over', 'out'))
    names.update(f'{stem}-break-source' for stem in SOURCE_STEMS)
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'main-stage', 'core3-stage', 'broker-stage', 'main-result',
                 'core3-result', 'broker-result', 'vm-cgroup', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    build = read(args.build / 'build.json')
    main_stage = read(args.main_stage / 'stage.json')
    core3_stage = read(args.core3_stage / 'stage.json')
    broker_stage = read(args.broker_stage / 'stage.json')
    main_result = read(args.main_result)
    core3_result = read(args.core3_result)
    broker_result = read(args.broker_result)
    vm = read(args.vm_cgroup)
    source_id = build['source_id']
    product_sha = build['products']['uwvm.exe']['sha256'].lower()
    require(build['status'] == 'cross-built-awaiting-real-windows-vm',
            'ordinary Windows PE build did not complete')
    require(main_stage['source_id'] == core3_stage['source_id'] ==
            broker_stage['source_id'] == source_id,
            'main, Core 3 or broker staging source ID differs from the PE')
    require(main_stage['files']['uwvm.exe'] == core3_stage['files']['uwvm.exe'] ==
            broker_stage['files']['uwvm.exe'] == product_sha and
            broker_stage['files']['uwvm-debug-server.exe'] ==
            build['products']['uwvm-debug-server.exe']['sha256'],
            'staged Windows PE hashes differ from the build')
    for directory, manifest in ((args.main_stage, main_stage),
                                (args.core3_stage, core3_stage),
                                (args.broker_stage, broker_stage)):
        for name, expected in manifest['files'].items():
            require(sha256(directory / name) == expected,
                    f'staged Windows input changed: {directory / name}')
    main_qualification_path = args.main_stage / 'qualification.json'
    main_qualification = read(main_qualification_path)
    require(main_stage['files'].get('qualification.json') ==
            sha256(main_qualification_path) and
            main_qualification.get('schema') == 1 and
            main_qualification.get('source_id') == source_id and
            main_qualification.get('product_sha256', '').lower() == product_sha and
            main_qualification.get('llvm_certificate_sha256') ==
            build['llvm_certificate_sha256'] and
            main_qualification.get('fixture_summary_sha256') ==
            main_stage['fixture_summary_sha256'] and
            main_qualification.get('eh_replacement_oracle_sha256') ==
            main_stage.get('eh_replacement_oracle_sha256') and
            len(main_stage.get('eh_replacement_oracle_sha256', '')) == 64,
            'main guest trust anchor differs from the frozen PE or fixtures')
    require(main_qualification.get('files_sha256') == {
                name: expected for name, expected in main_stage['files'].items()
                if name != 'qualification.json'},
            'main guest trust anchor does not cover every staged input')
    qualification = read(args.core3_stage / 'qualification.json')
    require(core3_stage['files'].get('qualification.json') ==
            sha256(args.core3_stage / 'qualification.json'),
            'Core 3 guest trust anchor differs from its staged manifest')
    require(qualification['source_id'] == source_id and
            qualification['product_sha256'] == product_sha,
            'supplemental reference qualification is for a different PE')
    require(qualification['llvm_certificate_sha256'] ==
            build['llvm_certificate_sha256'] == core3_stage['llvm_certificate_sha256'] ==
            broker_stage['llvm_certificate_sha256'],
            'supplemental LLVM certificate changed')
    require(broker_stage['main_stage_sha256'] == sha256(args.main_stage / 'stage.json') and
            broker_stage['product_sha256'] == product_sha and
            broker_stage['broker_sha256'] ==
            build['products']['uwvm-debug-server.exe']['sha256'],
            'broker stage is not bound to this main stage and PE pair')
    require(len(qualification['fixture_sha256']) == 23 and
            len(qualification['reference_runs']) == 59 and
            all(record['exit_code'] == 0 or
                (name == 'ref-cast-exn-null-trap-wasmtime' and
                 record['exit_code'] != 0 and
                 record.get('expected_success') is False and
                 record.get('diagnostic') == 'wasm trap: cast failure')
                for name, record in qualification['reference_runs'].items()),
            'Core 3 oracle parse, validation or expected cast trap is incomplete for 23 Wasm files')
    require(qualification.get('gap_oracle_sha256', '') ==
            core3_stage.get('gap_oracle_sha256', '') and
            len(qualification.get('gap_oracle_sha256', '')) == 64,
            'Core 3 exception-reference cast oracle changed during staging')
    require(qualification['reference_runs'] == core3_stage['reference_runs'],
            'Core 3 staged reference records changed')
    for name, expected in qualification['fixture_sha256'].items():
        require(core3_stage['files'].get(name) == expected,
                f'Core 3 reference fixture hash changed: {name}')
    for name, record in qualification['reference_runs'].items():
        require(sha256(args.core3_stage / (name + '.log')) == record['log_sha256'],
                f'Core 3 reference log changed: {name}')

    require(main_result.get('passed') is True and
            main_result.get('source_id') == source_id and
            main_result.get('qualification_sha256', '').lower() ==
            sha256(main_qualification_path) and
            main_result.get('product_sha256', '').lower() == product_sha,
            'native/source debugger guest result failed or used another PE or trust anchor')
    require(main_result.get('launcher_sha256', '').lower() ==
            build['products']['windows_debug_product_launcher.exe']['sha256'].lower() and
            main_result.get('fixture_sha256', '').lower() ==
            main_stage['files']['native-step-fixture.wasm'],
            'native debugger guest used a different Windows launcher or Wasm fixture')
    require('Windows' in main_result.get('os', '') and
            main_result.get('architecture') == 'AMD64' and
            bool(main_result.get('os_version')),
            'native debugger result is not from the Windows x64 guest')
    debug_policies = {record.get('policy') for record in main_result.get('runs', [])}
    require(required_debug_policies() <= debug_policies,
            f'missing debugger/ABI/security result: {sorted(required_debug_policies() - debug_policies)}')
    for policy in ('instruction', 'unwind'):
        rows = [record for record in main_result['runs']
                if record.get('policy') == f'{policy}-cross-function-eh-replacement']
        require(len(rows) == 1 and rows[0].get('baseline_exit_code') == 0 and
                rows[0].get('feature_off_exit_code') != 0 and
                rows[0].get('replacement_exit_code') == 0 and
                rows[0].get('wasm_sha256', '').lower() ==
                main_stage['files']['eh-hot-replace-cross-function.wasm'] and
                rows[0].get('body_sha256', '').lower() ==
                main_stage['files']['eh-hot-replace-cross-function-30.bin'],
                f'{policy} cross-function EH replacement differs from staged Wasm/body')
    require(core3_result.get('passed') is True and
            core3_result.get('source_id') == source_id and
            core3_result.get('product_sha256', '').lower() == product_sha,
            'Core 3 guest result failed or used another PE/source')
    require(core3_result.get('qualification_sha256', '').lower() ==
            sha256(args.core3_stage / 'qualification.json') and
            core3_result.get('llvm_certificate_sha256') ==
            build['llvm_certificate_sha256'] and
            core3_result.get('reference_wasmtime_sha256') ==
            qualification['wasmtime_sha256'] and
            core3_result.get('gap_oracle_sha256') ==
            qualification['gap_oracle_sha256'],
            'Core 3 guest result differs from its staged oracle or LLVM certificate')
    require('Windows' in core3_result.get('os', '') and
            core3_result.get('architecture') == 'AMD64' and
            bool(core3_result.get('os_version')),
            'Core 3 result is not from the Windows x64 guest')
    cases = core3_result.get('cases', [])
    expected_cases = required_core3_names()
    require(len(cases) == len(expected_cases) and
            {case.get('name') for case in cases} == expected_cases and
            all(case.get('passed') is True for case in cases),
            'Core 3 guest execution or individual feature-gate matrix incomplete')
    require(broker_result.get('passed') is True,
            'secure Windows late-attach broker result failed')
    require(broker_result.get('product_sha256', '').lower() == product_sha and
            broker_result.get('broker_sha256', '').lower() ==
            build['products']['uwvm-debug-server.exe']['sha256'].lower(),
            'late-attach broker result used a different ordinary Windows PE')
    require('Windows' in broker_result.get('os', '') and
            broker_result.get('architecture') == 'AMD64' and
            bool(broker_result.get('os_version')),
            'late-attach broker result is not from the Windows x64 guest')

    require(vm.get('source_id') == source_id and
            vm.get('product_sha256', '').lower() == product_sha,
            'QEMU cgroup record is not bound to this source and PE')
    require(vm.get('memory_max') == '68719476736' and
            vm.get('memory_swap_max') == '0' and
            vm.get('cpuset_cpus_effective') == CPUS and
            bool(vm.get('qemu_tids')),
            'QEMU was not recorded in the qualified 64 GiB/swap0/20-CPU scope')
    evidence = {'passed': True, 'source_id': source_id,
                'product_sha256': product_sha,
                'llvm_certificate_sha256': build['llvm_certificate_sha256'],
                'main_debugger_cases': len(main_result['runs']),
                'core3_guest_cases': len(cases),
                'core3_reference_checks': len(qualification['reference_runs']),
                'main_qualification_sha256': sha256(main_qualification_path),
                'core3_qualification_sha256': sha256(args.core3_stage / 'qualification.json'),
                'qemu_pid': vm['qemu_pid'],
                'qemu_tids': vm['qemu_tids'],
                'evidence_sha256': {
                    name: sha256(path) for name, path in (
                        ('build', args.build / 'build.json'),
                        ('main_stage', args.main_stage / 'stage.json'),
                        ('core3_stage', args.core3_stage / 'stage.json'),
                        ('broker_stage', args.broker_stage / 'stage.json'),
                        ('main_guest', args.main_result),
                        ('core3_guest', args.core3_result),
                        ('broker_guest', args.broker_result),
                        ('qemu_cgroup', args.vm_cgroup))}}
    args.output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': True, 'source_id': source_id,
                      'product_sha256': product_sha,
                      'main_cases': evidence['main_debugger_cases'],
                      'core3_cases': len(cases)}, sort_keys=True))


if __name__ == '__main__':
    main()
