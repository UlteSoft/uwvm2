#!/usr/bin/env python3
"""Check the ROS Win11 runner trust chain with synthetic immutable stages."""

import base64
from contextlib import redirect_stdout
from io import StringIO
import json
from pathlib import Path
import sys
import tempfile

import prepare_windows_guest_bootstrap as bootstrap


def write(path: Path, payload: bytes) -> str:
    path.write_bytes(payload)
    return bootstrap.sha256(path)


def payload(name: str) -> bytes:
    if name == 'uwvm.exe':
        return b'qualified-ros-product'
    if name == 'uwvm-debug-server.exe':
        return b'qualified-ros-broker'
    return name.encode()


def stage(directory: Path, names: tuple[str, ...], manifest: str,
          source_id: str, llvm_sha: str) -> dict:
    directory.mkdir()
    files = {name: write(directory / name, payload(name)) for name in names}
    record = {'schema': 1, 'source_id': source_id,
              'llvm_certificate_sha256': llvm_sha, 'files': files}
    (directory / manifest).write_text(json.dumps(record))
    return record


def main() -> None:
    source_id = 'sha256:' + 'a' * 64
    llvm_sha = 'bw' * 64
    main_names = ('uwvm.exe', 'uwvm-debug-server.exe',
                  'windows_control_broker_child.exe', 'native-step-fixture.wasm',
                  'run_windows_control_broker_vm.ps1',
                  'run_windows_broker_host_vm.ps1',
                  'run_native_step_windows_product_vm.ps1', 'qualification.json')
    core3_names = ('uwvm.exe', 'run_core3_windows_vm.ps1', 'qualification.json')
    broker_names = main_names[:6]
    with tempfile.TemporaryDirectory(prefix='uwvm-ros-windows-anchor-') as temporary:
        root = Path(temporary)
        build = root / 'build'
        build.mkdir()
        product_sha = bootstrap.sha256(_write_build_input(root / 'product.bin', payload('uwvm.exe')))
        broker_sha = bootstrap.sha256(_write_build_input(root / 'broker.bin', payload('uwvm-debug-server.exe')))
        (build / 'build.json').write_text(json.dumps({
            'source_id': source_id, 'llvm_certificate_sha256': llvm_sha,
            'products': {'uwvm.exe': {'sha256': product_sha},
                         'uwvm-debug-server.exe': {'sha256': broker_sha}},
        }))
        main_stage, core3_stage, broker_stage = root / 'main', root / 'core3', root / 'broker'
        stage(main_stage, main_names, 'stage.json', source_id, llvm_sha)
        stage(core3_stage, core3_names, 'manifest.json', source_id, llvm_sha)
        stage(broker_stage, broker_names, 'stage.json', source_id, llvm_sha)

        def run(output: Path) -> None:
            sys.argv = [__file__, '--build', str(build), '--main-stage', str(main_stage),
                        '--core3-stage', str(core3_stage), '--broker-stage', str(broker_stage),
                        '--output', str(output)]
            with redirect_stdout(StringIO()):
                bootstrap.main()

        output = root / 'commands'
        run(output)
        for kind, directory in (('main', main_stage), ('core3', core3_stage),
                                ('broker', broker_stage)):
            command = (output / f'{kind}.command.txt').read_text().strip()
            assert len(command) < 8192, (kind, len(command))
            script = base64.b64decode(command.rsplit(' ', 1)[-1]).decode('utf-16le')
            assert script.index('Get-FileHash') < script.index('& $p -BaseUrl')
            assert 'bootstrap-runner-verification-failed' in script
            if kind != 'broker':
                expected = bootstrap.sha256(directory / 'qualification.json')
                assert f"-QualificationSha256 '{expected}'" in script
            else:
                assert f"-BrokerSha256 '{broker_sha}'" in script
        (core3_stage / 'qualification.json').write_bytes(b'tampered')
        try:
            run(root / 'tampered-output')
        except ValueError as error:
            assert 'stage file changed: qualification.json' in str(error), error
        else:
            raise AssertionError('tampered ROS qualification reached guest bootstrap')
        print(json.dumps({'passed': True, 'source_id': source_id,
                          'checked_stages': 3, 'tamper_rejected': True}, sort_keys=True))


def _write_build_input(path: Path, data: bytes) -> Path:
    path.write_bytes(data)
    return path


if __name__ == '__main__':
    main()
