#!/usr/bin/env python3
"""R5 preflight around the immutable r2 official fixture/DLL staging pipeline.

No PE is executed. Only the keeper may run this in the admitted Linux cgroup.
The separate r5-admission.json must pass before staging is transferred to VM.
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

R5_MANIFEST_SHA = '5cad7465e846b03e97b181e85a6149711be6a4a14e57f317a0553e1b29a9f4cd'


def r5_pins(root: Path, repository: str, manifest: dict) -> dict:
    prefix = ('uwvm2-ros' if repository == 'ros' else 'uwvm2') + '/'
    records = manifest.get('records')
    stage.require(type(records) is dict and len(records) == 194, 'exact R5 source packet required')
    pins = {key[len(prefix):]: row['sha256'] for key, row in records.items() if key.startswith(prefix)}
    stage.require(bool(pins), 'R5 repository pins absent')
    for name, expected in pins.items():
        path = root / name
        stage.require(path.is_file() and stage.sha(path) == expected, 'actual R5 source differs: ' + name)
    for name, expected in manifest['r5_thread_unchanged_baseline_closure'].items():
        stage.require(stage.sha(root / name) == expected, 'actual R5 thread module dependency differs')
        pins[name] = expected
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
    parser.add_argument('--r5-source-manifest', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    stage.require(sys.platform == 'linux', 'keeper Linux preflight only')
    root = args.source_root.resolve(strict=True)
    stage.require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_r5_vm.py' and
                  Path(stage.__file__).resolve() == root / 'test/0017.runtime/stage_windows_debug_current_vm.py' and
                  Path(layout.__file__).resolve() == root / 'test/0017.runtime/windows_debug_current_layout_contract.py', 'actual imported R5 preflight paths differ')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    manifest_path, build_path = args.r5_source_manifest.resolve(strict=True), args.build_receipt.resolve(strict=True)
    stage.require(stage.sha(manifest_path) == R5_MANIFEST_SHA, 'keeper R5 immutable manifest pin differs; not a live-source override')
    manifest, build = stage.read_json(manifest_path), stage.read_json(build_path)
    pins = r5_pins(root, args.repository, manifest)
    prefix = layout.main_runtime_contracts(root, build)
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
    stage.require(before[str(manifest_path)] == R5_MANIFEST_SHA and before[str(config_path)] == config['sha256'] and
                  before[str(dwarf)] == build['debug_info_dwarf_archive']['sha256'], 'original preflight inputs changed before snapshot')
    out = args.out.resolve()
    stage.require(not out.exists(), 'new output directory required')
    original_argv = sys.argv[:]
    admission = {'schema': 1, 'purpose': 'actual-r5-current-win64-debug-full-admission', 'passed': False,
                 'repository': args.repository, 'r5_manifest_sha256': R5_MANIFEST_SHA, 'r5_actual_pins': pins,
                 'actual_main_runtime_layout_contract': prefix, 'actual_llvm_config_header': config,
                 'actual_debug_info_dwarf_headers': build['debug_info_dwarf_headers'], 'inputs_before': before}
    try:
        # Retain the unchanged official r2 staging implementation and direct
        # actual build records. This wrapper executes no compiler or PE itself.
        sys.argv = [str(Path(stage.__file__)), '--source-root', str(root), '--build-receipt', str(build_path),
                    '--repository', args.repository, '--out', str(out), *remaining]
        stage.main()
        after = {str(path): stage.sha(path) for path in immutable}
        stage.require(after == before, 'R5 source/layout/SDK/oracle input changed during official staging')
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
            with (out / 'r5-admission.json').open('x') as stream:
                json.dump(admission, stream, indent=2); stream.write('\n')
    print(json.dumps({'r5_staging_only': True, 'r5_admission_sha256': stage.sha(out / 'r5-admission.json'),
                      'qualification_sha256': admission['qualification_sha256']}, sort_keys=True))


if __name__ == '__main__':
    main()
