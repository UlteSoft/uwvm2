#!/usr/bin/env python3
"""Keeper Linux validates actual fresh Mach-O build provenance before Mac staging.

An original direct compile/link/MIG argv and its real logs/inputs/outputs are
required. The compact qualification transports verified hashes, not guest
debugger credentials, to the serial Mac executor. No Mach-O is executed here.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import stage_windows_debug_current_vm as build_helpers
import macos_debug_current_build_contract as contract


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def macho_arm64(path: Path) -> None:
    with path.open('rb') as stream:
        header = stream.read(32)
    require(len(header) == 32 and header[:4] == b'\xcf\xfa\xed\xfe' and
            int.from_bytes(header[4:8], 'little') == 0x0100000c and int.from_bytes(header[12:16], 'little') == 2,
            'actual thin arm64 Mach-O executable required; no PE/ELF/old other-architecture product')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'product', 'build-receipt', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--repository', choices=('ordinary', 'ros'), required=True)
    args = parser.parse_args()
    require(sys.platform == 'linux', 'keeper Linux qualification only')
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_macos_debug_current_product_rss.py' and
            Path(build_helpers.__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_vm.py' and
            Path(contract.__file__).resolve() == root / 'test/0017.runtime/macos_debug_current_build_contract.py', 'actual imported receipt checker path mismatch')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    product, receipt_path = args.product.resolve(strict=True), args.build_receipt.resolve(strict=True)
    receipt_digest = sha(receipt_path)
    product_digest = sha(product)
    build = build_helpers.read_json(receipt_path)
    require(build.get('schema') == 1 and build.get('purpose') == 'actual-current-macos-arm64-debug-full-rss-build' and
            build.get('repository') == args.repository and build.get('target') == 'arm64-apple-macos', 'actual current Mac arm64 build contract required')
    require(build.get('memory_max') == str(64 << 30) and build.get('memory_swap_max') == '0' and
            build.get('cpuset') == '0,2,4,6,16-31' and build.get('oom_before') == build.get('oom_after') == 0 and
            build.get('oom_kill_before') == build.get('oom_kill_after') == 0, 'actual original cross-build cgroup/OOM closure required')
    before, after = (Path(build[key]).resolve(strict=True) for key in ('source_before_file', 'source_after_file'))
    fingerprint_digest = sha(before)
    require(fingerprint_digest == sha(after), 'original build source/dependency fingerprints changed')
    fingerprint = build_helpers.read_json(before)
    entries = fingerprint.get('files')
    require(isinstance(entries, list) and bool(entries) and len({row['path'] for row in entries}) == len(entries), 'complete unique original source/dependency fingerprint required')
    source_id = 'sha256:' + hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    require(fingerprint.get('source_id') == build.get('source_id') == source_id and build_helpers.current_entries(root) == entries,
            'actual complete current source/dependency closure differs from original build')
    pins = ('src/uwvm2/uwvm/debugger/controller.h', 'src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_api.h',
            'src/uwvm2/runtime/lib/uwvm_runtime_debug_source_api.h',
            'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_debug_host_bridge.h',
            'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_native_eh_private_leaf_stage.h',
            'src/uwvm2/runtime/lib/native_eh_private_leaf_publication_impl.h')
    production_pins = {name: sha(root / name) for name in pins}
    require(build.get('production_pins') == production_pins and
            'llvm_jit_debug_query_source_activation_host_api' in (root / pins[1]).read_text(), 'current fused producer/joint API pins absent; old preloader build not admitted')
    linked, argv, cwd = build_helpers.verify_command(build['product_link'])
    require(linked == product and build_helpers.contains(product, source_id.encode()), 'actual original link output/source ID mismatch')
    macho_arm64(product)
    link_inputs = build_helpers.file_arguments(argv, cwd)
    commands = [build['product_link'], *build.get('product_compiles', [])]
    compile_inputs = set()
    for command in build.get('product_compiles', []):
        output, compile_argv, compile_cwd = build_helpers.verify_command(command)
        require('-c' in compile_argv and output in link_inputs, 'original fresh object not consumed by actual link')
        compile_inputs.update(build_helpers.file_arguments(compile_argv, compile_cwd))
    require({root / 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', root / 'src/uwvm2/uwvm/main.default.cpp'} <= compile_inputs,
            'fresh current runtime and main original compiles required')
    dwarf = Path(build['debug_info_dwarf_archive']['path']).resolve(strict=True)
    require('DebugInfoDWARF' in dwarf.name and dwarf in link_inputs and sha(dwarf) == build['debug_info_dwarf_archive']['sha256'], 'actual DebugInfoDWARF static closure missing')
    require(any(argv[index:index + 2] == ['-framework', 'Security'] for index in range(len(argv) - 1)) and
            any(argv[index:index + 2] == ['-framework', 'CoreFoundation'] for index in range(len(argv) - 1)), 'actual Security/CoreFoundation link closure missing')
    mig = build['protected_mig']
    mig_argv = mig.get('argv')
    require(mig.get('returncode') == 0 and isinstance(mig_argv, list) and mig_argv and
            Path(mig_argv[0]).name == 'mig' and '-DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1' in mig_argv,
            'actual original direct protected MIG command required; no reconstructed xcrun expansion')
    sdk = Path(mig['sdk_root']).resolve(strict=True)
    spec = sdk / 'usr/include/mach/mach_exc.defs'
    require(spec in build_helpers.file_arguments(mig_argv, Path(mig['cwd']).resolve(strict=True)), 'original MIG does not consume actual selected SDK spec')
    server, header = (Path(mig[key]).resolve(strict=True) for key in ('server', 'header'))
    require(server in compile_inputs and 'catch_mach_exception_raise_state_identity_protected' in server.read_text(), 'actual protected server was not freshly compiled into this link')
    for flag, path in (('-server', server), ('-header', header)):
        values = [mig_argv[index + 1] for index, value in enumerate(mig_argv[:-1]) if value == flag]
        require(len(values) == 1 and build_helpers.resolved_argument(values[0], Path(mig['cwd'])) == path, 'original generated MIG output identity differs')
    require(sha(Path(mig_argv[0]).resolve(strict=True)) == mig['tool_sha256'] and sha(Path(mig['log']).resolve(strict=True)) == mig['log_sha256'], 'original MIG tool/log changed')
    frameworks = build.get('framework_inputs')
    require(isinstance(frameworks, dict) and set(frameworks) == {'Security', 'CoreFoundation'}, 'actual framework stub closure missing')
    framework_paths = {Path(row['path']).resolve(strict=True) for row in frameworks.values()}
    require(all(path.is_relative_to(sdk) for path in framework_paths) and
            all(sha(Path(row['path'])) == row['sha256'] for row in frameworks.values()), 'actual selected SDK framework stubs changed')
    extra, extra_inputs = contract.qualify(build, root, product, argv, cwd, sdk, framework_paths)
    actual_inputs = build.get('actual_inputs')
    require(isinstance(actual_inputs, list) and bool(actual_inputs) and len({row['path'] for row in actual_inputs}) == len(actual_inputs), 'unique actual original input pins required')
    records = {Path(row['path']).resolve(strict=True): row['sha256'] for row in actual_inputs}
    require({spec, server, header, dwarf, *framework_paths, *compile_inputs, *link_inputs} <= records.keys(), 'MIG/compiler/link inputs lack actual original SHA records')
    require(all(sha(path) == value for path, value in records.items()), 'original consumed input changed')
    immutable = {root / name for name in pins} | extra_inputs | {Path(contract.__file__).resolve()} | set(records) | {guard, product, receipt_path, before, after, Path(__file__).resolve(), Path(build_helpers.__file__).resolve(),
                  Path(mig_argv[0]).resolve(strict=True), Path(mig['log']).resolve(strict=True)}
    for command in commands:
        immutable.update((build_helpers.resolved_argument(command['argv'][0], Path(command['cwd'])),
                          Path(command['log']).resolve(strict=True), Path(command['output']).resolve(strict=True)))
    inputs_before = {str(path): sha(path) for path in immutable}
    require(inputs_before[str(receipt_path)] == receipt_digest and inputs_before[str(product)] == product_digest and
            inputs_before[str(before)] == inputs_before[str(after)] == fingerprint_digest, 'record/product/fingerprint changed during original closure verification')
    for path, value in records.items():
        require(inputs_before[str(path)] == value, 'original input changed before snapshot')
    for command in commands:
        require(inputs_before[str(Path(command['output']).resolve(strict=True))] == command['output_sha256'] and
                inputs_before[str(Path(command['log']).resolve(strict=True))] == command['log_sha256'] and
                inputs_before[str(build_helpers.resolved_argument(command['argv'][0], Path(command['cwd'])))] == command['tool_sha256'], 'actual original command changed before final snapshot')
    # Recompare real original records: a later stable changed file is not proof.
    require(inputs_before[str(Path(extra['actual_linker']['path']).resolve(strict=True))] == extra['actual_linker']['sha256'],
            'actual original selected linker changed before snapshot')
    for row in (extra['macho_dylibs'], extra['macho_rpaths']):
        tool = build_helpers.resolved_argument(row['argv'][0], Path(row['cwd']).resolve(strict=True))
        log = Path(row['log']).resolve(strict=True)
        require(inputs_before[str(tool)] == row['tool_sha256'] and inputs_before[str(log)] == row['log_sha256'] and
                inputs_before[str(product)] == row['input_sha256'], 'actual original Mach-O oracle changed before snapshot')
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    qualification = {'schema': 1, 'purpose': 'actual-current-macos-arm64-debug-full-rss-qualified', 'passed': False,
                     'repository': args.repository, 'source_id': source_id, 'source_files': entries, 'production_pins': production_pins,
                     'product_name': product.name, 'product_sha256': sha(product), 'original_build_receipt_sha256': sha(receipt_path),
                     'original_build_resources': {key: build[key] for key in ('memory_max', 'memory_swap_max', 'cpuset')},
                     'sdk_root': str(sdk), 'sdk_mach_exc_spec_sha256': sha(spec), 'protected_server_sha256': sha(server),
                     'original_command_records': commands, 'protected_mig': mig, 'inputs_before': inputs_before}
    qualification.update(extra)
    subprocess.run(['bash', str(guard)], check=True)
    qualification['inputs_after'] = {str(path): sha(path) for path in immutable}
    require(qualification['inputs_after'] == inputs_before and build_helpers.current_entries(root) == entries, 'actual closure changed during qualification')
    qualification['passed'] = True
    (out / 'product.qualification.json').write_text(json.dumps(qualification, indent=2) + '\n')
    print('PASS actual fresh Mach-O cross-build provenance only; no Mac execution; retain all original remote records')


if __name__ == '__main__':
    main()
