#!/usr/bin/env python3
"""Prepare a hash-bound, capability-free Win11 DAP QMP bootstrap command."""

import argparse
import base64
import hashlib
import json
from pathlib import Path


URLS = {
    'ros': 'http://10.0.2.4:18022/uwvm-dap-windows-x64',
    'ordinary': 'http://10.0.2.4:18022/uwvm-dap-ordinary-x64',
}
RUNNER = 'run_windows_dap_host_vm.ps1'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def quoted(value: str) -> str:
    require("'" not in value and '\n' not in value and '\r' not in value,
            'unsafe PowerShell literal')
    return "'" + value + "'"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', choices=tuple(URLS), required=True)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--main-stage', required=True, type=Path)
    parser.add_argument('--dap-stage', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    build = json.loads((args.build / 'build.json').read_text())
    main = json.loads((args.main_stage / 'stage.json').read_text())
    stage = json.loads((args.dap_stage / 'stage.json').read_text())
    require(build['status'] == 'cross-built-awaiting-real-windows-vm' and
            stage['source_id'] == main['source_id'] == build['source_id'] and
            stage['llvm_certificate_sha256'] == main['llvm_certificate_sha256'] ==
            build['llvm_certificate_sha256'],
            'DAP bootstrap source or LLVM certificate mismatch')
    require(stage['main_stage_sha256'] == digest(args.main_stage / 'stage.json'),
            'DAP stage is not bound to the main stage')
    files = stage['files']
    for name, expected in files.items():
        path = args.dap_stage / name
        require(path.is_file() and not path.is_symlink() and digest(path) == expected,
                f'DAP stage input changed: {name}')
    require(files['uwvm.exe'] == build['products']['uwvm.exe']['sha256'] and
            files['uwvm-debug-server.exe'] ==
            build['products']['uwvm-debug-server.exe']['sha256'],
            'DAP stage PE differs from build certificate')
    url = URLS[args.engine]
    statements = [
        "$ErrorActionPreference='Stop'",
        f'$u={quoted(url)}',
        "$p=Join-Path $env:USERPROFILE 'Documents\\uwvm-dap-runner.ps1'",
        'try {',
        f'Invoke-WebRequest -UseBasicParsing "$u/{RUNNER}" -OutFile $p',
        f"if ((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne {quoted(files[RUNNER])}) {{ throw 'DAP host runner SHA-256 changed' }}",
        '} catch {',
        "$j='{" + '"passed":false,"error":"dap-bootstrap-runner-verification-failed"' + "}'",
        'try { Invoke-WebRequest -UseBasicParsing -Method Post -ContentType "application/json" -Body $j "$u/result" | Out-Null } catch {}',
        'exit 1',
        '}',
    ]
    invoke = f'& $p -BaseUrl $u -Engine {quoted(args.engine)}'
    for parameter, name in (
        ('ProductSha256', 'uwvm.exe'),
        ('BrokerSha256', 'uwvm-debug-server.exe'),
        ('WasmSha256', 'native-step-fixture.wasm'),
        ('SourceWasmSha256', 'source-c-dwarf4.wasm'),
        ('AdapterSha256', 'dap_adapter.py'),
        ('ProbeSha256', 'run_dap_windows_guest.py'),
    ):
        invoke += f' -{parameter} {quoted(files[name])}'
    statements.append(invoke)
    command = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand ' + \
        base64.b64encode(('; '.join(statements)).encode('utf-16le')).decode('ascii')
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or output.is_relative_to(Path('/dev/shm')),
            'DAP bootstrap must remain ephemeral')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    (output / 'dap.command.txt').write_text(command + '\n')
    record = {'schema': 1, 'engine': args.engine, 'source_id': build['source_id'],
              'product_sha256': files['uwvm.exe'],
              'broker_sha256': files['uwvm-debug-server.exe'],
              'dap_stage_sha256': digest(args.dap_stage / 'stage.json'),
              'runner_sha256': files[RUNNER],
              'command_sha256': hashlib.sha256(command.encode()).hexdigest(),
              'command_file': 'dap.command.txt',
              'capability_in_command': False}
    (output / 'bootstrap.json').write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': build['source_id'], 'engine': args.engine,
                      'output': str(output)}, sort_keys=True))


if __name__ == '__main__':
    main()
