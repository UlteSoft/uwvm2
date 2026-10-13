#!/usr/bin/env python3
"""Bind ROS Windows LLVM-full guest results to one source, PE and QEMU scope."""

import argparse
import hashlib
import json
from pathlib import Path
import re


CPUS = sorted([0, 2, 4, 6, *range(16, 32)])
GC_STEMS = ('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
            'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
            'gc_struct_into_anyref_table', 'i31_table64')
GAP_STEMS = ('ref-test-exn-null', 'ref-cast-exn-null', 'br-on-cast-exn-null',
             'exn-cast-retained', 'ref-cast-exn-null-trap', 'exnref_call_retained',
             'eh-cross-function-catch-ref', 'eh-cross-function-win64-seh')
SOURCE_STEMS = ('source-c-dwarf4', 'source-c-dwarf5',
                'source-cpp-dwarf4', 'source-cpp-dwarf5', 'source-rust-dwarf5')
GC_CONST_SHA256 = 'a25485ef0b598f6f7206fe6b7ae8c29c103d850e0e25d7d42ff58bb2805c9086'
EXNREF_STRUCT_SHA256 = '1cdc8e931df2783033c91ee557bdaad487fae9a744e8b207f40994af30cfc727'
INITIALIZER_SHA256 = '42a7d935a710b00789466f09ca1252edf000af4714dac28e4d2a792754df118a'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path: Path) -> dict:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError(f'expected JSON object: {path}')
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def debug_policies() -> set[str]:
    names = {'instruction', 'unwind', 'rejected-input-output-alias',
             'late-attach-fd-rejected', 'late-attach-guessed-handle-rejected'}
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
                 for operation in ('into', 'over', 'out', 'break-source'))
    return names


def core3_cases() -> set[str]:
    names = {'typed-ref-call-ref-full', 'typed-ref-call-ref-feature-off',
             'gc-i31_table64-table64-off',
             'gc-reference-types-off', 'gc-table-instructions-off',
             'eh-threads-auto'}
    for stem in GC_STEMS:
        names.update((f'gc-{stem}', f'gc-{stem}-gc-off'))
    for policy in ('instruction', 'unwind'):
        names.update((f'eh-threads-{policy}',
                      f'eh-threads-{policy}-exceptions-off',
                      f'eh-threads-{policy}-threads-off'))
        names.update((f'gc-const-expr-{policy}-extended-const-off',
                      f'gc-const-expr-{policy}-gc-off'))
        names.update((f'ref-func-table-initializer-{policy}',
                      f'ref-func-table-initializer-{policy}-extended-const-off',
                      f'ref-func-table-initializer-{policy}-feature-off'))
        for stem in ('core3-throw-ref-bottom', 'core3-br-on-null-bottom'):
            names.update((f'{stem}-{policy}',
                          f'{stem}-{policy}-exceptions-off'))
        names.add(f'core3-br-on-null-bottom-{policy}-function-references-off')
        for stem in ('branch_semantics_br_if', 'branch_semantics_br_on_null',
                     'branch_semantics_br_on_non_null'):
            names.update((f'{stem}-{policy}',
                          f'{stem}-{policy}-function-references-off'))
        for stem in GAP_STEMS:
            names.update((f'{stem}-{policy}', f'{stem}-{policy}-gc-off',
                          f'{stem}-{policy}-exceptions-off'))
    for mode in ('jit-instruction', 'jit-unwind'):
        names.update((f'exnref-struct-{mode}',
                      f'exnref-struct-{mode}-exceptions-off',
                      f'exnref-struct-{mode}-gc-off',
                      f'exnref-struct-{mode}-reference-types-off'))
    return names


def verify_stage(directory: Path, manifest: dict) -> None:
    for name, expected in manifest['files'].items():
        require(digest(directory / name).lower() == expected.lower(),
                f'staged Windows artifact changed: {directory / name}')


def verify_guest_anchor(directory: Path, manifest: dict, source_id: str,
                        product_sha: str, llvm_sha: str) -> dict:
    path = directory / 'qualification.json'
    qualification = read(path)
    require(manifest['files'].get('qualification.json', '').lower() == digest(path),
            f'guest qualification hash differs from stage: {path}')
    require(qualification.get('schema') == 1 and
            qualification.get('source_id') == source_id and
            qualification.get('product_sha256', '').lower() == product_sha and
            qualification.get('llvm_certificate_sha256') == llvm_sha,
            f'guest qualification differs from frozen source/PE/LLVM: {path}')
    require(qualification.get('files_sha256') == {
                name: expected for name, expected in manifest['files'].items()
                if name != 'qualification.json'},
            f'guest qualification does not cover every staged input: {path}')
    return qualification


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'main-stage', 'core3-stage', 'broker-stage', 'main-result',
                 'core3-result', 'broker-result', 'vm-cgroup', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    build = read(args.build / 'build.json')
    main_stage = read(args.main_stage / 'stage.json')
    core3_stage = read(args.core3_stage / 'manifest.json')
    broker_stage = read(args.broker_stage / 'stage.json')
    main_result = read(args.main_result)
    core3_result = read(args.core3_result)
    broker_result = read(args.broker_result)
    vm = read(args.vm_cgroup)
    source_id = build['source_id']
    product_sha = build['products']['uwvm.exe']['sha256'].lower()
    broker_sha = build['products']['uwvm-debug-server.exe']['sha256'].lower()
    require(build['status'] == 'cross-built-awaiting-real-windows-vm',
            'ROS Windows LLVM-full PE build did not complete')
    require(main_stage['source_id'] == core3_stage['source_id'] ==
            broker_stage['source_id'] == source_id,
            'main/Core 3/broker stage source IDs differ from the PE')
    require(main_stage['files']['uwvm.exe'].lower() ==
            core3_stage['files']['uwvm.exe'].lower() == product_sha,
            'main/Core 3 staged PE hashes differ')
    require(main_stage['files']['uwvm-debug-server.exe'].lower() == broker_sha,
            'staged Windows debug broker differs from the PE build')
    require(broker_stage['files']['uwvm.exe'].lower() == product_sha and
            broker_stage['files']['uwvm-debug-server.exe'].lower() == broker_sha,
            'broker stage has different Windows executables')
    require(main_stage['llvm_certificate_sha256'] ==
            core3_stage['llvm_certificate_sha256'] ==
            broker_stage['llvm_certificate_sha256'] ==
            build['llvm_certificate_sha256'],
            'qualified ROS LLVM23 certificate differs between stages')
    verify_stage(args.main_stage, main_stage)
    verify_stage(args.core3_stage, core3_stage)
    verify_stage(args.broker_stage, broker_stage)
    main_qualification = verify_guest_anchor(
        args.main_stage, main_stage, source_id, product_sha,
        build['llvm_certificate_sha256'])
    core3_qualification = verify_guest_anchor(
        args.core3_stage, core3_stage, source_id, product_sha,
        build['llvm_certificate_sha256'])
    require(main_qualification.get('fixture_summary_sha256') ==
            main_stage['fixture_summary_sha256'],
            'main guest qualification differs from independently validated fixtures')
    require(re.fullmatch(r'[0-9a-f]{64}',
                         main_stage.get('eh_replacement_oracle_sha256', '')) is not None and
            main_qualification.get('eh_replacement_oracle_sha256') ==
            main_stage['eh_replacement_oracle_sha256'],
            'main guest qualification lacks the cross-function EH replacement oracle')
    require(re.fullmatch(r'[0-9a-f]{64}',
                         core3_qualification.get('branch_reference_sha256', '')) is not None,
            'Core 3 qualification lacks the branch oracle fingerprint')
    require(core3_qualification['branch_reference_sha256'] ==
            core3_stage.get('branch_reference_sha256'),
            'Core 3 staged branch oracle differs from the guest trust anchor')
    require(re.fullmatch(r'[0-9a-f]{64}',
                         core3_qualification.get('gap_oracle_sha256', '')) is not None and
            core3_qualification['gap_oracle_sha256'] ==
            core3_stage.get('gap_oracle_sha256'),
            'Core 3 staged exception-reference cast oracle differs from the guest trust anchor')
    require(core3_stage['files']['gc_const_expr_execution.wasm'].lower() ==
            GC_CONST_SHA256,
            'single-op Core 3 GC constant fixture differs from Wasmtime reference bytes')
    require(core3_stage['files']['exnref_struct_payload_execution.wasm'].lower() ==
            EXNREF_STRUCT_SHA256,
            'Core 3 GC exception payload differs from Wasmtime reference bytes')
    require(core3_stage['files']['wasm3_initializers.wasm'].lower() ==
            INITIALIZER_SHA256,
            'Core 3 ref.func table initializer differs from Wasmtime reference bytes')

    require(main_result.get('passed') is True and
            main_result.get('source_id') == source_id and
            main_result.get('qualification_sha256', '').lower() ==
            digest(args.main_stage / 'qualification.json') and
            main_result.get('llvm_certificate_sha256') ==
            build['llvm_certificate_sha256'] and
            main_result.get('product_sha256', '').lower() == product_sha and
            main_result.get('launcher_sha256', '').lower() ==
            main_stage['files']['windows_debug_product_launcher.exe'].lower() and
            main_result.get('fixture_sha256', '').lower() ==
            main_stage['files']['native-step-fixture.wasm'].lower(),
            'Windows debugger guest result failed or used another product/fixture')
    require('Windows' in main_result.get('os', '') and
            main_result.get('architecture') == 'AMD64' and
            bool(main_result.get('os_version')),
            'debugger guest result lacks real Windows x64 OS identity')
    observed_debug = {item.get('policy') for item in main_result.get('runs', [])}
    require(debug_policies() <= observed_debug,
            f'missing debugger/security cases: {sorted(debug_policies() - observed_debug)}')
    for policy in ('instruction', 'unwind'):
        rows = [item for item in main_result['runs']
                if item.get('policy') == f'{policy}-cross-function-eh-replacement']
        require(len(rows) == 1 and rows[0].get('baseline_exit_code') == 0 and
                rows[0].get('feature_off_exit_code') != 0 and
                rows[0].get('replacement_exit_code') == 0 and
                rows[0].get('wasm_sha256', '').lower() ==
                main_stage['files']['eh-hot-replace-cross-function.wasm'].lower() and
                rows[0].get('body_sha256', '').lower() ==
                main_stage['files']['eh-hot-replace-cross-function-30.bin'].lower(),
                f'{policy} cross-function EH replacement differs from staged Wasm/body')
    require(core3_result.get('passed') is True and
            core3_result.get('source_id') == source_id and
            core3_result.get('product_sha256', '').lower() == product_sha and
            core3_result.get('qualification_sha256', '').lower() ==
            digest(args.core3_stage / 'qualification.json') and
            core3_result.get('llvm_certificate_sha256') ==
            build['llvm_certificate_sha256'] and
            core3_result.get('branch_reference_sha256') ==
            core3_qualification['branch_reference_sha256'] and
            core3_result.get('gap_oracle_sha256') ==
            core3_qualification['gap_oracle_sha256'],
            'Windows Core 3 result failed or used another source/PE')
    require('Windows' in core3_result.get('os', '') and
            core3_result.get('architecture') == 'AMD64' and
            bool(core3_result.get('os_version')),
            'Core 3 guest result lacks real Windows x64 OS identity')
    observed_core3 = core3_result.get('cases', [])
    expected_core3 = core3_cases()
    require(len(observed_core3) == len(expected_core3) and
            {item.get('name') for item in observed_core3} == expected_core3 and
            all(item.get('passed') is True for item in observed_core3),
            'Windows Core 3 execution or precise feature-gate matrix incomplete')
    require(broker_result.get('passed') is True and
            broker_result.get('product_sha256', '').lower() == product_sha and
            broker_result.get('broker_sha256', '').lower() == broker_sha and
            'pipe' not in broker_result and 'token' not in broker_result,
            'secure late-attach result failed, used another PE or persisted a capability')
    require('Windows' in broker_result.get('os', '') and
            broker_result.get('architecture') == 'AMD64' and
            bool(broker_result.get('os_version')),
            'broker guest result lacks real Windows x64 OS identity')
    require(vm.get('source_id') == source_id and
            vm.get('product_sha256', '').lower() == product_sha and
            vm.get('memory_max') == '68719476736' and
            vm.get('memory_swap_max') == '0' and
            vm.get('cpuset_cpus_effective') == CPUS and
            bool(vm.get('qemu_tids')),
            'Windows QEMU was not recorded in the shared 64 GiB/swap0/20-CPU scope')
    inputs = {
        'build': args.build / 'build.json',
        'main_stage': args.main_stage / 'stage.json',
        'core3_stage': args.core3_stage / 'manifest.json',
        'broker_stage': args.broker_stage / 'stage.json',
        'main_result': args.main_result,
        'core3_result': args.core3_result,
        'broker_result': args.broker_result,
        'vm_cgroup': args.vm_cgroup,
    }
    evidence = {'passed': True, 'source_id': source_id,
                'product_sha256': product_sha,
                'llvm_certificate_sha256': build['llvm_certificate_sha256'],
                'debugger_cases': len(main_result['runs']),
                'core3_cases': len(observed_core3),
                'main_qualification_sha256': digest(args.main_stage / 'qualification.json'),
                'core3_qualification_sha256': digest(args.core3_stage / 'qualification.json'),
                'qemu_pid': vm['qemu_pid'],
                'qemu_tids': vm['qemu_tids'],
                'inputs_sha256': {name: digest(path) for name, path in inputs.items()}}
    args.output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': True, 'source_id': source_id,
                      'debugger_cases': evidence['debugger_cases'],
                      'core3_cases': evidence['core3_cases']}, sort_keys=True))


if __name__ == '__main__':
    main()
