#!/usr/bin/env python3
"""Stage exact-source Windows debugger PE and Wasm inputs for the VM test."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from prepare_windows_debug_fixtures import check_cgroup


SOURCE_STEMS = ('source-c-dwarf4', 'source-c-dwarf5',
                'source-cpp-dwarf4', 'source-cpp-dwarf5', 'source-rust-dwarf5')
FIXTURES = ('native-step-fixture.wasm', 'core3-typed-ref.wasm',
            'core3-throw-ref-bottom.wasm', 'core3-br-on-null-bottom.wasm', 'replace.wasm',
            'good4.bin', 'good5.bin', 'malformed.bin', 'wrong_result.bin') + tuple(
                name + suffix for name in SOURCE_STEMS for suffix in ('.wasm', '.meta.json'))
PRODUCTS = ('uwvm.exe', 'uwvm-debug-server.exe',
            'windows_debug_product_launcher.exe', 'windows_control_broker_child.exe')
SCRIPTS = ('run_native_step_windows_product_vm.ps1',
           'run_windows_control_broker_vm.ps1',
           'run_windows_broker_host_vm.ps1')
EH_REPLACEMENT = ('eh-hot-replace-cross-function.wasm',
                  'eh-hot-replace-cross-function-30.bin')
EH_WASM_SHA256 = '32f313178b8487228eec154ffb3903a9e963355d6baf369e9fdaf61aa1fb4675'
EH_BODY_SHA256 = '84ecfe6f0f1578e9ca75d7dc4566a83006cbd457365d8cb6def403d7336059d2'
WASM_TOOLS_SHA256 = '115d5986a8a1aeb112a5f2d98209c144f70c26188a5d41e266698a1e20de4ed1'
WASMTIME_SHA256 = '9f3f3e1b1b048f802c1d35a17607c064425a9c746f00bda3c2466b92ddcb5c92'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--eh-replace-fixtures', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    cgroup = check_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True)
    build = args.build.resolve(strict=True)
    fixtures = args.fixtures.resolve(strict=True)
    eh_fixtures = args.eh_replace_fixtures.resolve(strict=True)
    output = args.output.resolve()
    if not output.is_relative_to(Path('/tmp')) and not output.is_relative_to(Path('/dev/shm')):
        raise ValueError('VM evidence staging must stay on tmpfs')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    built = json.loads((build / 'build.json').read_text())
    if built.get('status') != 'cross-built-awaiting-real-windows-vm':
        raise ValueError('Windows PE build has not completed')
    if {key: built.get('build_cgroup', {}).get(key) for key in cgroup} != cgroup:
        raise ValueError('Windows PE was not built in the staging cgroup and CPU set')
    fixture_summary = json.loads((fixtures / 'fixture-summary.json').read_text())
    qualified = fixture_summary.get('cgroup', {})
    if qualified != cgroup:
        raise ValueError('fixtures were not produced in the staging cgroup and CPU set')
    if set(fixture_summary.get('wasmtime_reference_passed', [])) != {
            'core3-typed-ref.wasm', 'core3-throw-ref-bottom.wasm',
            'core3-br-on-null-bottom.wasm'}:
        raise ValueError('Core 3 fixtures lack complete Wasmtime reference execution')
    current = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-current.json')], text=True).strip()
    if current != built['source_id']:
        raise ValueError('Windows PE build source differs from staged test source')
    for relative, expected in built.get('build_recipe_files_sha256', {}).items():
        if digest(source / relative) != expected:
            raise ValueError(f'Windows build recipe changed before VM staging: {relative}')
    files: dict[str, str] = {}
    origins: dict[str, str] = {}
    for name in PRODUCTS:
        record = built['products'].get(name)
        if not isinstance(record, dict):
            raise ValueError(f'missing cross-built product: {name}')
        origin = Path(record['path']).resolve(strict=True)
        if digest(origin) != record['sha256']:
            raise ValueError(f'cross-built product changed: {name}')
        origins[name] = str(origin)
    # Xmake can reuse a compiled module carrying an older build-source-id
    # even when this run's source tree and command line are current. Require
    # the linked PE itself to contain this exact cache provenance string.
    product_bytes = Path(origins['uwvm.exe']).read_bytes()
    if current.encode('ascii') not in product_bytes:
        raise ValueError('linked Windows PE does not embed its source ID')
    for stale in (
        b'sha256:0cbd743e73dc72dd9ac597accf7b1d894e838ccfe58f067fa9f44dc3c37ea09b',
        b'sha256:102d90e4ce911d70a81d466588fb50a6f914fcf8e1ffba0fd1ea1ff7718fd3da',
    ):
        if stale in product_bytes:
            raise ValueError('linked Windows PE retained a stale ordinary source ID')
    for name in FIXTURES:
        origin = fixtures / name
        if not origin.is_file() or origin.is_symlink():
            raise ValueError(f'missing regular Wasm fixture: {name}')
        if fixture_summary['files_sha256'].get(name) != digest(origin):
            raise ValueError(f'fixture changed after validation: {name}')
        origins[name] = str(origin)
    eh_manifest_path = eh_fixtures / 'manifest.json'
    eh_manifest = json.loads(eh_manifest_path.read_text())
    eh_group = eh_manifest.get('cgroup', {})
    if (eh_manifest.get('schema') != 1 or
            eh_manifest.get('status') != 'wasm-tools-and-wasmtime-oracle-passed' or
            any(eh_group.get(key) != cgroup.get(key)
                for key in ('scope', 'memory_max', 'swap_max', 'cpus')) or
            eh_manifest.get('target_function_index') != 1 or
            eh_manifest.get('entry_function_index') != 3 or
            eh_manifest.get('replacement_generation') != 1 or
            eh_manifest.get('wasm_tools_sha256') != WASM_TOOLS_SHA256 or
            eh_manifest.get('wasmtime_sha256') != WASMTIME_SHA256 or
            len(eh_manifest.get('checks', [])) != 6 or
            any(check.get('exit_code') != 0 for check in eh_manifest['checks'])):
        raise ValueError('cross-function EH replacement lacks its Wasmtime oracle')
    wat = source / 'test/0017.runtime/fixtures/eh-hot-replace-cross-function.wat'
    if digest(wat) != eh_manifest.get('source_wat_sha256'):
        raise ValueError('cross-function EH replacement WAT changed')
    for name, expected in eh_manifest.get('files_sha256', {}).items():
        origin = eh_fixtures / name
        if (Path(name).name != name or not origin.is_file() or origin.is_symlink() or
                digest(origin) != expected):
            raise ValueError(f'cross-function EH oracle file changed: {name}')
    if (eh_manifest['files_sha256'].get(EH_REPLACEMENT[0]) != EH_WASM_SHA256 or
            eh_manifest['files_sha256'].get(EH_REPLACEMENT[1]) != EH_BODY_SHA256 or
            (eh_fixtures / 'wasmtime-original.stdout').read_bytes() !=
            b'eh-replaced-value=29\n' or
            (eh_fixtures / 'wasmtime-replacement.stdout').read_bytes() !=
            b'eh-replaced-value=30\n'):
        raise ValueError('cross-function EH oracle payloads differ from Wasmtime 48')
    for name in EH_REPLACEMENT:
        if name not in eh_manifest.get('files_sha256', {}):
            raise ValueError(f'missing cross-function EH replacement fixture: {name}')
        origins[name] = str(eh_fixtures / name)
    for name in SCRIPTS:
        origin = source / 'test/0017.runtime' / name
        if not origin.is_file() or origin.is_symlink():
            raise ValueError(f'missing Windows VM runner: {name}')
        origins[name] = str(origin)
    dap_adapter = source / 'tools/debug/dap_adapter.py'
    if not dap_adapter.is_file() or dap_adapter.is_symlink():
        raise ValueError('missing source-bound Windows DAP adapter')
    origins['dap_adapter.py'] = str(dap_adapter)
    dap_probe = source / 'test/0018.debugger/run_dap_windows_guest.py'
    if not dap_probe.is_file() or dap_probe.is_symlink():
        raise ValueError('missing Windows DAP stdio guest probe')
    origins['run_dap_windows_guest.py'] = str(dap_probe)
    # This non-self-referential manifest is the guest's trust anchor. The
    # bootstrap passes its hash out of band; the guest verifies it before any
    # downloaded PE, Wasm fixture, or secondary script is executed.
    qualification_path = output / 'qualification.json'
    qualification = {
        'schema': 1,
        'source_id': current,
        'product_sha256': built['products']['uwvm.exe']['sha256'],
        'llvm_certificate_sha256': built['llvm_certificate_sha256'],
        'fixture_summary_sha256': digest(fixtures / 'fixture-summary.json'),
        'eh_replacement_oracle_sha256': digest(eh_manifest_path),
        'files_sha256': {name: digest(Path(pathname)) for name, pathname in origins.items()},
    }
    qualification_path.write_text(json.dumps(qualification, indent=2, sort_keys=True) + '\n')
    origins['qualification.json'] = str(qualification_path)
    for name, pathname in origins.items():
        target = output / name
        if target != Path(pathname):
            with target.open('xb') as destination, Path(pathname).open('rb') as origin:
                shutil.copyfileobj(origin, destination)
        files[name] = digest(target)
    if (current != subprocess.check_output(
            [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
             str(source), str(output / 'source-after-staging.json')], text=True).strip()):
        raise ValueError('source changed during Windows VM staging')
    result = {'schema': 1, 'source_id': current, 'cgroup': cgroup,
              'llvm_certificate_sha256': built['llvm_certificate_sha256'],
              'fixture_summary_sha256': digest(fixtures / 'fixture-summary.json'),
              'eh_replacement_oracle_sha256': digest(eh_manifest_path),
              'files': files}
    with (output / 'stage.json').open('x') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps({'source_id': current, 'files': len(files),
                      'output': str(output)}, sort_keys=True))


if __name__ == '__main__':
    main()
