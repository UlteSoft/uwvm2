#!/usr/bin/env python3
"""Make a separate one-result Windows broker stage from the qualified PE stage."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


FILES = ('uwvm.exe', 'uwvm-debug-server.exe',
         'windows_control_broker_child.exe', 'native-step-fixture.wasm',
         'run_windows_control_broker_vm.ps1', 'run_windows_broker_host_vm.ps1',
         'dap_adapter.py', 'run_dap_windows_guest.py')


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--main-stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build = json.loads((args.build / 'build.json').read_text())
    main_stage = json.loads((args.main_stage / 'stage.json').read_text())
    require(build['status'] == 'cross-built-awaiting-real-windows-vm',
            'Windows LLVM-full product has not linked')
    require(main_stage['source_id'] == build['source_id'] and
            main_stage['llvm_certificate_sha256'] == build['llvm_certificate_sha256'],
            'broker staging source/LLVM differs from the qualified main stage')
    for name in ('uwvm.exe', 'uwvm-debug-server.exe'):
        require(main_stage['files'][name].lower() ==
                build['products'][name]['sha256'].lower(),
                f'broker staging product differs from the build certificate: {name}')
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or
            output.is_relative_to(Path('/dev/shm')),
            'broker VM stage must remain on temporary storage')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    files: dict[str, str] = {}
    for name in FILES:
        origin = args.main_stage / name
        require(origin.is_file() and not origin.is_symlink() and
                digest(origin).lower() == main_stage['files'][name].lower(),
                f'changed or missing main-stage broker input: {name}')
        target = output / name
        with origin.open('rb') as source, target.open('xb') as destination:
            shutil.copyfileobj(source, destination)
        files[name] = digest(target)
        require(files[name].lower() == main_stage['files'][name].lower(),
                f'broker copy differs from the qualified main stage: {name}')
    result = {'schema': 1, 'source_id': build['source_id'],
              'llvm_certificate_sha256': build['llvm_certificate_sha256'],
              'files': files}
    with (output / 'stage.json').open('x') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps({'source_id': result['source_id'], 'files': len(files),
                      'output': str(output)}, sort_keys=True))


if __name__ == '__main__':
    main()
