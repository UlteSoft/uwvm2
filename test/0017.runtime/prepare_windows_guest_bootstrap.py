#!/usr/bin/env python3
"""Prepare hash-bound, secret-free QMP commands for the qualified Win11 guest.

Run after the ordinary PE and all three immutable artifact stages exist. The
commands only download a staged PowerShell runner, verify its SHA-256 before
execution, and pass broker input hashes. The broker capability is generated
later inside the trusted Windows console and never appears in these files.
"""

import argparse
import base64
import hashlib
import json
from pathlib import Path


URLS = {
    'main': 'http://10.0.2.4:18018/uwvm-native-step-windows-x64',
    'core3': 'http://10.0.2.4:18021/uwvm-core3-ordinary-x64',
    'broker': 'http://10.0.2.4:18020/uwvm-broker-ordinary-x64',
}
RUNNERS = {
    'main': 'run_native_step_windows_product_vm.ps1',
    'core3': 'run_core3_windows_full_vm.ps1',
    'broker': 'run_windows_broker_host_vm.ps1',
}


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def load(path: Path) -> dict:
    result = json.loads(path.read_text())
    require(isinstance(result, dict), f'expected JSON object: {path}')
    return result


def quoted(value: str) -> str:
    require("'" not in value and '\n' not in value and '\r' not in value,
            'unsafe PowerShell literal')
    return "'" + value + "'"


def command_for(kind: str, files: dict[str, str]) -> str:
    runner = RUNNERS[kind]
    url = URLS[kind]
    expected = files[runner]
    runner_path = 'Documents\\uwvm-' + kind + '-runner.ps1'
    statements = [
        "$ErrorActionPreference='Stop'",
        f'$u={quoted(url)}',
        f'$p=Join-Path $env:USERPROFILE {quoted(runner_path)}',
        'try {',
        f'Invoke-WebRequest -UseBasicParsing "$u/{runner}" -OutFile $p',
        f"if ((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne {quoted(expected)}) {{ throw 'staged runner SHA-256 changed' }}",
        '} catch {',
        "$j='{\"passed\":false,\"error\":\"bootstrap-runner-verification-failed\"}'",
        'try { [System.IO.File]::WriteAllText($p + ".failure.json", $j, [System.Text.Encoding]::UTF8) } catch {}',
        'try { Invoke-WebRequest -UseBasicParsing -Method Post -ContentType "application/json" -Body $j "$u/result" | Out-Null } catch {}',
        'exit 1',
        '}',
    ]
    invoke = '& $p -BaseUrl $u'
    if kind in ('main', 'core3'):
        require('qualification.json' in files,
                f'{kind} stage lacks the manifest needed before guest execution')
        invoke += f" -QualificationSha256 {quoted(files['qualification.json'])}"
    if kind == 'broker':
        for parameter, name in (
            ('ProductSha256', 'uwvm.exe'),
            ('BrokerSha256', 'uwvm-debug-server.exe'),
            ('ChildSha256', 'windows_control_broker_child.exe'),
            ('WasmSha256', 'native-step-fixture.wasm'),
            ('ProbeSha256', 'run_windows_control_broker_vm.ps1'),
        ):
            invoke += f' -{parameter} {quoted(files[name])}'
    statements.append(invoke)
    encoded = base64.b64encode(('; '.join(statements)).encode('utf-16le')).decode('ascii')
    return 'powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand ' + encoded


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'main-stage', 'core3-stage', 'broker-stage', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    build = load(args.build / 'build.json')
    source_id = build['source_id']
    product_sha = build['products']['uwvm.exe']['sha256'].lower()
    llvm_sha = build['llvm_certificate_sha256']
    stages = {
        'main': args.main_stage,
        'core3': args.core3_stage,
        'broker': args.broker_stage,
    }
    records: dict[str, dict] = {}
    for kind, directory in stages.items():
        manifest_path = directory / 'stage.json'
        stage = load(manifest_path)
        require(stage['source_id'] == source_id and
                stage['llvm_certificate_sha256'] == llvm_sha and
                stage['files']['uwvm.exe'] == product_sha,
                f'{kind} stage differs from the frozen ordinary PE')
        files = stage['files']
        require(RUNNERS[kind] in files, f'{kind} stage lacks its guest runner')
        for name, expected in files.items():
            path = directory / name
            require(path.is_file() and not path.is_symlink() and
                    sha256(path) == expected,
                    f'{kind} stage file changed: {name}')
        records[kind] = {'directory': str(directory),
                         'stage_sha256': sha256(manifest_path), 'files': files}
    require(records['broker']['files']['uwvm-debug-server.exe'] ==
            build['products']['uwvm-debug-server.exe']['sha256'],
            'broker PE differs from frozen build')
    broker = load(args.broker_stage / 'stage.json')
    require(broker['main_stage_sha256'] == records['main']['stage_sha256'],
            'broker stage is not bound to the main stage')

    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    commands = {}
    for kind, record in records.items():
        command = command_for(kind, record['files'])
        (output / (kind + '.command.txt')).write_text(command + '\n')
        commands[kind] = {'stage_sha256': record['stage_sha256'],
                          'runner_sha256': record['files'][RUNNERS[kind]],
                          'qualification_sha256': record['files'].get('qualification.json'),
                          'command_sha256': hashlib.sha256(command.encode()).hexdigest(),
                          'command_file': kind + '.command.txt'}
    result = {'schema': 1, 'source_id': source_id,
              'product_sha256': product_sha,
              'llvm_certificate_sha256': llvm_sha,
              'commands': commands,
              'operator_order': ['main', 'core3', 'broker'],
              'broker_console_rule': 'Do not screenshot, OCR, redirect, or persist broker console output.'}
    (output / 'bootstrap.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': source_id, 'output': str(output),
                      'commands': list(commands)}, sort_keys=True))


if __name__ == '__main__':
    main()
