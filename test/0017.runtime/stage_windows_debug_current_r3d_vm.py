#!/usr/bin/env python3
"""R3d preflight around the immutable r2 official fixture/DLL staging pipeline.

R3c ordinary / R3d ROS fixed actual source IDs; old R5 is not admission.
No PE is executed. Only the keeper may run this in the admitted Linux cgroup.
The separate r3d-admission.json must pass before staging is transferred to VM.
This pins SOURCE content only; actual Win64 PE/SDK/link/DLL proof is still required.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import stage_windows_debug_current_vm as stage
import windows_debug_current_layout_contract as layout

R3C_MANIFEST_SHA = '2970cba891f4741b4b8bee3d11d2c4076c5ffbd2c922040fcc6adb9c0c3d5daf'
SOURCE_IDS = {
    'ordinary': 'sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d',
    'ros': 'sha256:ce797300723ff12b4a3253fc0287504dc20103824b54b97eb0f7e5f4abc6985a',
}
ROS_NATIVE_CODE_PATH = 'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_code_api.h'
ROS_NATIVE_CODE_BEFORE_SHA = '888670c0eda866829024d61e8bb8221826fa6d95bd929cbb22b6ff935f5889ec'
ROS_NATIVE_CODE_AFTER_SHA = 'a35f0ac277a550b6f85c7945377f9948ad620542bb1489eaa238192e9fe1003a'
HELPER_PINS = {
    'stage_windows_debug_current_vm.py': '3d0eed667c1c05eb49905add4520f544e6d4922c805332407e0fad7a39746235',
    'windows_debug_current_layout_contract.py': 'e914efc75fe0273316fec3d2f28a36816b4767387db1e2d08c4e1c7de7dc335b',
    'run_debug_source_step_cli.py': '5f23172f673ec598189b7c9a81643169cb2c7cb157431c0ad192f6230df9705b',
    'run_debug_source_inline_metadata_cli.py': 'bac9075590e9d12117ca1ecc4cb9bb62fc178a466891496f6c2bd78f93ab9f31',
    'windows_debug_current_launcher.cc': 'e2a855bf0724705148d1db934ec1d8d22391770ec5f1770d88cef893040356ba',
    'windows_debug_current_owned_process.ps1': '05d73c0d84fb085bc1b7576e06a307ac436635b0bb031dbc25d7a7b90ffab4f3',
    'run_windows_debug_acceptance_current_vm.ps1': '0276680ec43b71e61d171eb5ff7231e8a85875d7277b860e0a98b60f4b355474',
    'fixtures/debug_source_step_c.c': '2f4a2451bd138042b72fe2faab8185ade0af27419816c8fc894f8dd5968bee7e',
}


def r3d_pins(root: Path, repository: str, manifest: dict) -> dict:
    prefix = ('uwvm2-ros' if repository == 'ros' else 'uwvm2') + '/'
    pins = {}
    # These immutable ancestor/direct sets are DISJOINT. Never replay an old
    # R5 payload over a newer native-code API/controller/runtime declaration.
    for name, count in (('records', 60), ('unchanged_R2_records', 32),
                        ('unchanged_R5_dependencies', 156)):
        records = manifest.get(name)
        stage.require(type(records) is dict and len(records) == count,
                      'exact immutable R3c source record set required: ' + name)
        for key, row in records.items():
            if key.startswith(prefix):
                path = key[len(prefix):]
                stage.require(path not in pins and type(row.get('sha256')) is str,
                              'R3c ancestor/direct pin overlap')
                pins[path] = row['sha256']
    stage.require(bool(pins), 'R3c repository pins absent')
    stage.require(pins.get(ROS_NATIVE_CODE_PATH) == ROS_NATIVE_CODE_BEFORE_SHA,
                  'immutable native-code API preimage differs')
    if repository == 'ros':
        # Root approved the sole ROS resolver-contract fix. Its published
        # pointer is borrowed only through lease -> ONE domain -> publication.
        # The ordinary repository keeps its original found/value-copy contract.
        pins[ROS_NATIVE_CODE_PATH] = ROS_NATIVE_CODE_AFTER_SHA
    for name, expected in pins.items():
        path = root / name
        stage.require(path.is_file() and stage.sha(path) == expected,
                      'actual current R3 source differs: ' + name)
    for name, expected in HELPER_PINS.items():
        path = root / 'test/0017.runtime' / name
        stage.require(path.is_file() and stage.sha(path) == expected,
                      'actual current Windows qualifier helper differs: ' + name)
        pins['test/0017.runtime/' + name] = expected
    return pins


def dwarf_coff_oracle(build: dict, dwarf: Path) -> set[Path]:
    row = build['debug_info_dwarf_headers']
    argv, cwd = row.get('argv'), Path(row['cwd']).resolve(strict=True)
    stage.require(row.get('returncode') == 0 and type(argv) is list and len(argv) == 3 and argv[1] == '--file-headers' and
                  stage.resolved_argument(argv[2], cwd) == dwarf, 'actual original DWARF archive COFF oracle argv required')
    tool, log = stage.resolved_argument(argv[0], cwd), Path(row['log']).resolve(strict=True)
    stage.require(re.fullmatch(r'llvm-readobj(?:-[0-9]+(?:\.[0-9]+)*)?', tool.name) is not None and
                  stage.sha(tool) == row['tool_sha256'] and stage.sha(log) == row['log_sha256'] and
                  log.stat().st_size <= 16 << 20 and stage.sha(dwarf) == row['input_sha256'], 'actual original DWARF COFF tool/log/archive changed')
    layout.coff_amd64_headers(log.read_text())
    return {tool, log}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, add_help=False)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build-receipt', type=Path, required=True)
    parser.add_argument('--repository', choices=('ordinary', 'ros'), required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--r3c-source-manifest', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    stage.require(sys.platform == 'linux', 'keeper Linux preflight only')
    root = args.source_root.resolve(strict=True)
    stage.require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_r3d_vm.py' and
                  Path(stage.__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_vm.py' and
                  Path(layout.__file__).resolve() == root / 'test/0017.runtime/windows_debug_current_layout_contract.py', 'actual imported R3d preflight paths differ')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    manifest_path, build_path = args.r3c_source_manifest.resolve(strict=True), args.build_receipt.resolve(strict=True)
    stage.require(stage.sha(manifest_path) == R3C_MANIFEST_SHA, 'keeper R3c immutable ancestor manifest pin differs; not a live-source override')
    manifest, build = stage.read_json(manifest_path), stage.read_json(build_path)
    pins = r3d_pins(root, args.repository, manifest)
    stage.require(build.get('source_id') == SOURCE_IDS[args.repository],
                  'actual fresh current source ID differs; no historical/live override')
    prefix = layout.main_runtime_contracts(root, build)
    # Preserve the existing exact main/RT contract and also check every actual
    # product consumer TU (including a host provider, if this build has one).
    # The unchanged stage later proves each output was consumed by the PE link.
    for row in build.get('product_compiles', []):
        _, argv, cwd = stage.verify_command(row)
        stage.require('-c' in argv and layout.layout_contract(argv, cwd) == prefix,
                      'actual current product consumer TU layout differs')
    config = build['llvm_config_header']
    config_path = Path(config['path']).resolve(strict=True)
    stage.require(config_path.name == 'llvm-config.h' and config_path.parts[-3:] == ('llvm', 'Config', 'llvm-config.h') and
                  stage.sha(config_path) == config['sha256'] and
                  re.findall(r'^\s*#\s*define\s+LLVM_VERSION_MAJOR\s+(\d+)\s*$', config_path.read_text(), re.MULTILINE) == ['23'],
                  'actual selected LLVM23 generated config header required')
    searches = {Path(path) for kind, path in prefix[1] if kind in ('-I', '-isystem')}
    stage.require(any(config_path == path / 'llvm/Config/llvm-config.h' for path in searches), 'LLVM23 config is not in the actual common TU header search')
    records = {Path(row['path']).resolve(strict=True): row['sha256'] for row in build['actual_inputs']}
    stage.require(records.get(config_path) == config['sha256'], 'actual original consumed LLVM23 config pin absent')
    dwarf = Path(build['debug_info_dwarf_archive']['path']).resolve(strict=True)
    stage.require(records.get(dwarf) == stage.sha(dwarf), 'actual original linked DWARF archive pin absent')
    oracle_inputs = dwarf_coff_oracle(build, dwarf)
    immutable = {manifest_path, build_path, config_path, dwarf, guard, Path(__file__).resolve(), Path(stage.__file__).resolve(),
                 Path(layout.__file__).resolve(), *oracle_inputs, *(root / name for name in pins)}
    before = {str(path): stage.sha(path) for path in immutable}
    stage.require(before[str(manifest_path)] == R3C_MANIFEST_SHA and before[str(config_path)] == config['sha256'] and
                  before[str(dwarf)] == build['debug_info_dwarf_archive']['sha256'], 'original preflight inputs changed before snapshot')
    out = args.out.resolve()
    stage.require(not out.exists(), 'new output directory required')
    original_argv = sys.argv[:]
    admission = {'schema': 1, 'purpose': 'actual-r3d-current-win64-debug-full-admission', 'passed': False,
                 'repository': args.repository, 'r3c_manifest_sha256': R3C_MANIFEST_SHA, 'r3d_actual_pins': pins,
                 'expected_current_source_id': SOURCE_IDS[args.repository],
                 'actual_main_runtime_layout_contract': prefix, 'actual_llvm_config_header': config,
                 'actual_debug_info_dwarf_headers': build['debug_info_dwarf_headers'], 'inputs_before': before}
    try:
        # Retain the unchanged official r2 staging implementation and direct
        # actual build records. This wrapper executes no compiler or PE itself.
        sys.argv = [str(Path(stage.__file__)), '--source-root', str(root), '--build-receipt', str(build_path),
                    '--repository', args.repository, '--out', str(out), *remaining]
        stage.main()
        after = {str(path): stage.sha(path) for path in immutable}
        stage.require(after == before, 'R3d source/layout/SDK/oracle input changed during official staging')
        qualification = out / 'qualification.json'
        staged = stage.read_json(qualification)
        stage.require(staged['repository'] == args.repository and staged['source_before_sha256'] == staged['source_after_sha256'], 'actual official qualifier differs')
        admission.update(passed=True, inputs_after=after, qualification_sha256=stage.sha(qualification), source_id=staged['source_id'])
    except BaseException as error:
        admission['error'] = repr(error)
        raise
    finally:
        sys.argv = original_argv
        if out.exists():
            with (out / 'r3d-admission.json').open('x') as stream:
                json.dump(admission, stream, indent=2); stream.write('\n')
    print(json.dumps({'r3d_staging_only': True, 'r3d_admission_sha256': stage.sha(out / 'r3d-admission.json'),
                      'qualification_sha256': admission['qualification_sha256']}, sort_keys=True))


if __name__ == '__main__':
    main()
