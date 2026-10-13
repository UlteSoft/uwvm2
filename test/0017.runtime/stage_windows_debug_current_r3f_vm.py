#!/usr/bin/env python3
"""R3f preflight around the immutable r2 official fixture/DLL staging pipeline.

R3c ordinary / R3f ROS fixed actual source IDs; old R5 is not admission.
No PE is executed. Only the keeper may run this in the admitted Linux cgroup.
The separate r3f-admission.json must pass before staging is transferred to VM.
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
    'ros': 'sha256:bf7328bcebd44d7dea6d97ce38525ad3f72ed3ddd4c2382bfe9149be37133845',
}
R3E_DERIVATION_SHA = 'e955d1773c450a1cb6276b5e427e14066ee8ed376894ca4fa675df6e217363bb'
R3F_DERIVATION_SHA = 'd74657abc7d09bbeba834a9c4757289f7666699678b3b722126e03fdd69a9bae'
ROS_ROUTING_PATH = 'src/uwvm2/uwvm/run/run.h'
ROS_REVIEWED_OVERRIDES = {
    'src/uwvm2/runtime/lib/uwvm_runtime_debug_native_code_api.h': 'a35f0ac277a550b6f85c7945377f9948ad620542bb1489eaa238192e9fe1003a',
    'src/uwvm2/uwvm/debugger/linux_control_fd.h': 'ca79f9268d099c7112409ef90cefbb578696b824845400c9226679d4e3cc5df5',
    'src/uwvm2/utils/control/protocol.h': '24fd50222a6da190f0b1cccb1691bf866c58a144d13a36b396d85f8b0c488700',
    'src/uwvm2/utils/control/protocol.cppm': '396e9cc0f689249def95955059f0a7b891c7a10d609d3e2d4a94727ebcb03c84',
    'src/uwvm2/utils/control/session.h': 'abba9a9c8f1dd5220634d0cedb1b98d1a6bcde114787bdf9f1ca70bf35310f14',
    'src/uwvm2/utils/control/session.cppm': '8d7481f309ce97b5bcf86d9b729638e1c119723d8f24ec33b7a6bde50ac3ab74',
    'src/uwvm2/utils/control/linux_launch_channel.h': '56d675fe5a98ec98e93331cc651cc061fd5aa64887d4be22610ac15440e1ccc7',
    'src/uwvm2/utils/control/linux_launch_channel.cppm': '037c90fe8e033b41ad7b946dbe02d0cf20b375869b851d8d5e8fe84707714e04',
    ROS_ROUTING_PATH: 'fef37d85849006edfc72c677358ec15fcc43af6e61710b207cf53f0152a9407f',
}
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


def r3f_pins(root: Path, repository: str, manifest: dict, r3e: dict, r3f: dict) -> dict:
    ancestor = {}
    # The immutable ancestor/direct sets are DISJOINT. The ONLY descendants
    # admitted here are the reviewed ROS resolver, control and routing leaves.
    # stored_original_path metadata is never opened or treated as authority.
    for name, count in (('records', 60), ('unchanged_R2_records', 32),
                        ('unchanged_R5_dependencies', 156)):
        records = manifest.get(name)
        stage.require(type(records) is dict and len(records) == count,
                      'exact immutable R3c source record set required: ' + name)
        for key, row in records.items():
            stage.require(key not in ancestor and type(row.get('sha256')) is str,
                          'R3c ancestor/direct pin overlap')
            ancestor[key] = row['sha256']
    expected = dict(ancestor)
    expected.update({'uwvm2-ros/' + name: digest for name, digest in ROS_REVIEWED_OVERRIDES.items()})
    for data, count in ((r3e, 254), (r3f, 255)):
        rows = data.get('overlays')
        stage.require(data.get('origin_manifest_sha256') == R3C_MANIFEST_SHA and
                      type(rows) is dict and len(rows) == count,
                      'actual fixed source derivation ancestor/row count differs')
    run_key = 'uwvm2-ros/' + ROS_ROUTING_PATH
    stage.require(set(r3f['overlays']) - set(r3e['overlays']) == {run_key} and
                  all(r3f['overlays'].get(key) == row for key, row in r3e['overlays'].items()) and
                  r3f.get('ancestor_derivation_sha256') == R3E_DERIVATION_SHA and
                  r3f.get('ancestor_frozen_overlay_count') == 254 and
                  r3f.get('additional_overlay_count') == 1,
                  'R3f must preserve all 254 actual R3e rows and add only reviewed routing leaf')
    actual = {key: row.get('sha256') for key, row in r3f['overlays'].items()}
    stage.require(actual == expected and len(expected) == 255,
                  'fixed R3f source overlays differ; no live or extra platform override')
    for repository_name, choice in (('uwvm2', 'ordinary'), ('uwvm2-ros', 'ros')):
        stage.require(r3f.get('repositories', {}).get(repository_name, {}).get('source_id') == SOURCE_IDS[choice],
                      'actual R3f canonical source ID differs')
    prefix = ('uwvm2-ros' if repository == 'ros' else 'uwvm2') + '/'
    pins = {}
    for key, expected_sha in expected.items():
        if not key.startswith(prefix):
            continue
        name = key[len(prefix):]
        stage.require(Path(name).parts and Path(name).parts[0] in ('src', 'test', 'tools') and '..' not in Path(name).parts and not Path(name).is_absolute(),
                      'source pin is not a normalized repository-owned path')
        path = root / name
        row = r3f['overlays'][key]
        stage.require(type(row.get('bytes')) is int and row['bytes'] > 0 and
                      path.is_file() and path.stat().st_size == row['bytes'] and stage.sha(path) == expected_sha,
                      'actual current R3f source differs: ' + name)
        pins[name] = expected_sha
    stage.require(len(pins) == (131 if repository == 'ros' else 124), 'actual selected R3f source pin count differs')
    for name, expected_sha in HELPER_PINS.items():
        path = root / 'test/0017.runtime' / name
        stage.require(path.is_file() and stage.sha(path) == expected_sha,
                      'actual current Windows qualifier helper differs: ' + name)
        pins['test/0017.runtime/' + name] = expected_sha
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
    parser.add_argument('--r3e-source-derivation', type=Path, required=True)
    parser.add_argument('--r3f-source-derivation', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    stage.require(sys.platform == 'linux', 'keeper Linux preflight only')
    root = args.source_root.resolve(strict=True)
    stage.require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_r3f_vm.py' and
                  Path(stage.__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_vm.py' and
                  Path(layout.__file__).resolve() == root / 'test/0017.runtime/windows_debug_current_layout_contract.py', 'actual imported R3f preflight paths differ')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    manifest_path, build_path = args.r3c_source_manifest.resolve(strict=True), args.build_receipt.resolve(strict=True)
    stage.require(stage.sha(manifest_path) == R3C_MANIFEST_SHA, 'keeper R3c immutable ancestor manifest pin differs; not a live-source override')
    r3e_path, r3f_path = (path.resolve(strict=True) for path in (args.r3e_source_derivation, args.r3f_source_derivation))
    stage.require(stage.sha(r3e_path) == R3E_DERIVATION_SHA and stage.sha(r3f_path) == R3F_DERIVATION_SHA,
                  'actual reviewed R3e/R3f source derivation pins differ')
    manifest, build = stage.read_json(manifest_path), stage.read_json(build_path)
    pins = r3f_pins(root, args.repository, manifest, stage.read_json(r3e_path), stage.read_json(r3f_path))
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
    immutable = {manifest_path, r3e_path, r3f_path, build_path, config_path, dwarf, guard, Path(__file__).resolve(), Path(stage.__file__).resolve(),
                 Path(layout.__file__).resolve(), *oracle_inputs, *(root / name for name in pins)}
    before = {str(path): stage.sha(path) for path in immutable}
    stage.require(before[str(manifest_path)] == R3C_MANIFEST_SHA and before[str(r3e_path)] == R3E_DERIVATION_SHA and
                  before[str(r3f_path)] == R3F_DERIVATION_SHA and before[str(config_path)] == config['sha256'] and
                  before[str(dwarf)] == build['debug_info_dwarf_archive']['sha256'], 'original preflight inputs changed before snapshot')
    out = args.out.resolve()
    stage.require(not out.exists(), 'new output directory required')
    original_argv = sys.argv[:]
    admission = {'schema': 1, 'purpose': 'actual-r3f-current-win64-debug-full-admission', 'passed': False,
                 'repository': args.repository, 'r3c_manifest_sha256': R3C_MANIFEST_SHA, 'r3f_actual_pins': pins,
                 'r3e_derivation_sha256': R3E_DERIVATION_SHA, 'r3f_derivation_sha256': R3F_DERIVATION_SHA,
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
        stage.require(after == before, 'R3f source/layout/SDK/oracle input changed during official staging')
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
            with (out / 'r3f-admission.json').open('x') as stream:
                json.dump(admission, stream, indent=2); stream.write('\n')
    print(json.dumps({'r3f_staging_only': True, 'r3f_admission_sha256': stage.sha(out / 'r3f-admission.json'),
                      'qualification_sha256': admission['qualification_sha256']}, sort_keys=True))


if __name__ == '__main__':
    main()
