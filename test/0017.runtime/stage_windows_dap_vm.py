#!/usr/bin/env python3
"""Make a separate source- and PE-bound Win11 DAP stdio artifact stage."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


SHARED = ('uwvm.exe', 'uwvm-debug-server.exe', 'native-step-fixture.wasm',
          'source-c-dwarf4.wasm', 'dap_adapter.py', 'run_dap_windows_guest.py')
RUNNER = 'run_windows_dap_host_vm.ps1'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--main-stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    build = json.loads((args.build / 'build.json').read_text())
    main = json.loads((args.main_stage / 'stage.json').read_text())
    require(build['status'] == 'cross-built-awaiting-real-windows-vm',
            'Win64 LLVM-full product has not linked')
    require(main['source_id'] == build['source_id'] and
            main['llvm_certificate_sha256'] == build['llvm_certificate_sha256'],
            'main stage differs from the product source or LLVM certificate')
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or output.is_relative_to(Path('/dev/shm')),
            'DAP stage must be ephemeral')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    current = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-current.json')],
        text=True).strip()
    require(current == build['source_id'], 'DAP test source differs from linked PE')
    files = {}
    for name in SHARED:
        origin = args.main_stage / name
        require(origin.is_file() and not origin.is_symlink() and
                digest(origin) == main['files'][name],
                f'main stage DAP input changed: {name}')
        target = output / name
        with origin.open('rb') as incoming, target.open('xb') as outgoing:
            shutil.copyfileobj(incoming, outgoing)
        files[name] = digest(target)
        require(files[name] == main['files'][name], f'DAP copy changed: {name}')
    require(files['uwvm.exe'] == build['products']['uwvm.exe']['sha256'] and
            files['uwvm-debug-server.exe'] ==
            build['products']['uwvm-debug-server.exe']['sha256'],
            'DAP stage PE differs from product certificate')
    runner = source / 'test/0017.runtime' / RUNNER
    require(runner.is_file() and not runner.is_symlink(), 'missing trusted DAP host runner')
    runner_sha = digest(runner)
    with runner.open('rb') as incoming, (output / RUNNER).open('xb') as outgoing:
        shutil.copyfileobj(incoming, outgoing)
    require(digest(output / RUNNER) == runner_sha, 'DAP host runner changed during copy')
    files[RUNNER] = runner_sha
    after = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-after.json')], text=True).strip()
    require(after == current, 'DAP test source changed during staging')
    result = {'schema': 1, 'source_id': current,
              'llvm_certificate_sha256': build['llvm_certificate_sha256'],
              'main_stage_sha256': digest(args.main_stage / 'stage.json'),
              'files': files}
    (output / 'stage.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': current, 'files': len(files),
                      'output': str(output)}, sort_keys=True))


if __name__ == '__main__':
    main()
