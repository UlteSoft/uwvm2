#!/usr/bin/env python3
"""Stage the exact ordinary Windows PE pair for a separate broker result POST."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from prepare_windows_debug_fixtures import check_cgroup


FILES = ('uwvm.exe', 'uwvm-debug-server.exe',
         'windows_control_broker_child.exe', 'native-step-fixture.wasm',
         'run_windows_control_broker_vm.ps1', 'run_windows_broker_host_vm.ps1',
         'dap_adapter.py', 'run_dap_windows_guest.py')
PRODUCTS = ('uwvm.exe', 'uwvm-debug-server.exe',
            'windows_control_broker_child.exe')


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'build', 'main-stage', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    cgroup = check_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True)
    build = args.build.resolve(strict=True)
    main_stage = args.main_stage.resolve(strict=True)
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or
            output.is_relative_to(Path('/dev/shm')),
            'broker VM stage must stay on temporary storage')
    built = json.loads((build / 'build.json').read_text())
    prior = json.loads((main_stage / 'stage.json').read_text())
    require(built.get('status') == 'cross-built-awaiting-real-windows-vm',
            'ordinary Windows PE build has not completed')
    require({key: built.get('build_cgroup', {}).get(key) for key in cgroup} == cgroup and
            prior.get('cgroup') == cgroup,
            'broker staging differs from the qualified build/main-stage cgroup')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    source_id = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-current.json')],
        text=True).strip()
    require(source_id == built['source_id'] == prior['source_id'],
            'broker stage, main stage and PE source IDs differ')
    for relative, expected in built['build_recipe_files_sha256'].items():
        require(sha256(source / relative) == expected,
                f'ordinary Windows build recipe changed: {relative}')
    files: dict[str, str] = {}
    for name in FILES:
        origin = main_stage / name
        require(origin.is_file() and not origin.is_symlink(),
                f'missing regular main-stage input: {name}')
        expected = prior['files'].get(name)
        require(expected == sha256(origin), f'main-stage input changed: {name}')
        if name in PRODUCTS:
            require(expected == built['products'][name]['sha256'],
                    f'broker stage would use a different PE: {name}')
        target = output / name
        with origin.open('rb') as stream, target.open('xb') as destination:
            shutil.copyfileobj(stream, destination)
        files[name] = sha256(target)
        require(files[name] == expected, f'broker staging changed {name}')
    result = {'schema': 1, 'source_id': source_id, 'cgroup': cgroup,
              'llvm_certificate_sha256': built['llvm_certificate_sha256'],
              'product_sha256': files['uwvm.exe'],
              'broker_sha256': files['uwvm-debug-server.exe'],
              'main_stage_sha256': sha256(main_stage / 'stage.json'),
              'files': files}
    (output / 'stage.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': source_id, 'output': str(output),
                      'files': len(files)}, sort_keys=True))


if __name__ == '__main__':
    main()
