#!/usr/bin/env python3
"""Exercise the Windows guest bootstrap trust chain without starting a VM."""

import base64
from contextlib import redirect_stdout
from io import StringIO
import json
from pathlib import Path
import sys
import tempfile

import prepare_windows_guest_bootstrap as bootstrap


def write(path: Path, data: bytes) -> str:
    path.write_bytes(data)
    return bootstrap.sha256(path)


def stage(directory: Path, names: tuple[str, ...], source_id: str,
          llvm_sha: str, product_sha: str, main_stage_sha: str = '') -> dict:
    directory.mkdir()
    files = {name: write(directory / name, name.encode()) for name in names}
    write(directory / 'uwvm.exe', b'qualified-ordinary-product')
    files['uwvm.exe'] = product_sha
    record = {'schema': 1, 'source_id': source_id,
              'llvm_certificate_sha256': llvm_sha, 'files': files}
    if main_stage_sha:
        record['main_stage_sha256'] = main_stage_sha
    (directory / 'stage.json').write_text(json.dumps(record))
    return record


def main() -> None:
    source_id = 'sha256:' + 'a' * 64
    llvm_sha = 'b' * 64
    with tempfile.TemporaryDirectory(prefix='uwvm-windows-anchor-') as temporary:
        root = Path(temporary)
        build = root / 'build'
        build.mkdir()
        product_path = root / 'product.bin'
        product_sha = write(product_path, b'qualified-ordinary-product')
        broker_path = root / 'broker.bin'
        broker_sha = write(broker_path, b'qualified-broker-product')
        (build / 'build.json').write_text(json.dumps({
            'source_id': source_id, 'llvm_certificate_sha256': llvm_sha,
            'products': {'uwvm.exe': {'sha256': product_sha},
                         'uwvm-debug-server.exe': {'sha256': broker_sha}},
        }))
        main_stage = root / 'main'
        stage(main_stage, ('run_native_step_windows_product_vm.ps1',
                           'qualification.json'), source_id, llvm_sha, product_sha)
        core3_stage = root / 'core3'
        stage(core3_stage, ('run_core3_windows_full_vm.ps1',
                            'qualification.json'), source_id, llvm_sha, product_sha)
        broker_stage = root / 'broker'
        record = stage(broker_stage, ('run_windows_broker_host_vm.ps1',
                                      'windows_control_broker_child.exe',
                                      'native-step-fixture.wasm',
                                      'run_windows_control_broker_vm.ps1'),
                       source_id, llvm_sha, product_sha,
                       bootstrap.sha256(main_stage / 'stage.json'))
        write(broker_stage / 'uwvm-debug-server.exe', b'qualified-broker-product')
        record['files']['uwvm-debug-server.exe'] = broker_sha
        (broker_stage / 'stage.json').write_text(json.dumps(record))

        def run(output: Path) -> None:
            sys.argv = [__file__, '--build', str(build), '--main-stage', str(main_stage),
                        '--core3-stage', str(core3_stage),
                        '--broker-stage', str(broker_stage), '--output', str(output)]
            with redirect_stdout(StringIO()):
                bootstrap.main()

        output = root / 'commands'
        run(output)
        for kind, stage_path in (('main', main_stage), ('core3', core3_stage),
                                 ('broker', broker_stage)):
            command = (output / f'{kind}.command.txt').read_text().strip()
            assert len(command) < 8192, (kind, len(command))
            script = base64.b64decode(command.rsplit(' ', 1)[-1]).decode('utf-16le')
            assert script.index('Get-FileHash') < script.index('& $p -BaseUrl')
            assert 'bootstrap-runner-verification-failed' in script
            if kind != 'broker':
                expected = bootstrap.sha256(stage_path / 'qualification.json')
                assert f"-QualificationSha256 '{expected}'" in script
            else:
                assert f"-BrokerSha256 '{broker_sha}'" in script
        (main_stage / 'qualification.json').write_bytes(b'tampered')
        try:
            run(root / 'tampered-output')
        except ValueError as error:
            assert 'stage file changed: qualification.json' in str(error), error
        else:
            raise AssertionError('tampered qualification unexpectedly reached guest bootstrap')
        print(json.dumps({'passed': True, 'source_id': source_id,
                          'checked_stages': 3, 'tamper_rejected': True}, sort_keys=True))


if __name__ == '__main__':
    main()
