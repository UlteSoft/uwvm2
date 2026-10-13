#!/usr/bin/env python3
"""Stage source-, PE-, and oracle-bound Core 3 syntax inputs for Win11."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


STEMS = ('memory64', 'relaxed-simd', 'exnref-table64', 'atomic-fence')
WASM_TOOLS_SHA256 = '115d5986a8a1aeb112a5f2d98209c144f70c26188a5d41e266698a1e20de4ed1'
WASMTIME_SHA256 = '9f3f3e1b1b048f802c1d35a17607c064425a9c746f00bda3c2466b92ddcb5c92'
RUNNER = 'run_core3_newsyntax_windows_vm.ps1'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def quote(value: str) -> str:
    require("'" not in value and '\r' not in value and '\n' not in value,
            'unsafe PowerShell argument')
    return "'" + value + "'"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--oracle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--port', type=int, default=18024)
    parser.add_argument('--prefix', default='uwvm-newsyntax-windows-x64')
    args = parser.parse_args()
    require(1024 <= args.port <= 65535 and
            re.fullmatch(r'[A-Za-z0-9_-]{12,80}', args.prefix) is not None,
            'invalid guest artifact endpoint')
    source = args.source_root.resolve(strict=True)
    build_file = (args.build / 'build.json').resolve(strict=True)
    build = json.loads(build_file.read_text())
    oracle_dir = args.oracle.resolve(strict=True)
    oracle_file = oracle_dir / 'summary.json'
    oracle = json.loads(oracle_file.read_text())
    require(build.get('status') == 'cross-built-awaiting-real-windows-vm',
            'Windows PE has not linked')
    require(oracle.get('schema') == 1 and oracle.get('status') == 'official-oracle-passed' and
            oracle.get('source_id') == build.get('source_id') and
            oracle.get('wasm_tools_sha256') == WASM_TOOLS_SHA256 and
            oracle.get('wasmtime_sha256') == WASMTIME_SHA256 and
            oracle.get('cgroup_memory_max') == str(64 << 30) and
            oracle.get('cgroup_swap_max') == '0' and
            oracle.get('cgroup_cpuset') == '0,2,4,6,16-31',
            'Core 3 official oracle differs from the qualified source/cgroup')
    rows = {row.get('stem'): row for row in oracle.get('fixtures', [])}
    require(set(rows) == set(STEMS) and len(oracle['fixtures']) == len(STEMS),
            'Core 3 syntax witness set differs')
    for stem in STEMS:
        row = rows[stem]
        wat = source / row['source_relative']
        require(digest(wat) == row['wat_sha256'] and
                digest(oracle_dir / (stem + '.wat')) == row['wat_sha256'] and
                digest(oracle_dir / (stem + '.wasm')) == row['wasm_sha256'] and
                len(row.get('checks', [])) == 3 and
                all(check.get('exit_code') == 0 and
                    digest(oracle_dir / f'{stem}-step{index}.log') == check.get('log_sha256')
                    for index, check in enumerate(row['checks'])),
                f'Core 3 syntax oracle changed: {stem}')
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or
            output.is_relative_to(Path('/dev/shm')),
            'Win11 staging must remain on ephemeral host storage')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fingerprint = source / 'tools/ci/wasm3_source_fingerprint.py'
    current = subprocess.check_output(
        [sys.executable, str(fingerprint), str(source), str(output / 'source-before.json')],
        text=True).strip()
    require(current == build['source_id'], 'linked PE differs from current source')
    for relative, expected in build.get('build_recipe_files_sha256', {}).items():
        require(digest(source / relative) == expected,
                f'Windows build recipe changed before staging: {relative}')
    product = Path(build['products']['uwvm.exe']['path']).resolve(strict=True)
    require(digest(product) == build['products']['uwvm.exe']['sha256'] and
            current.encode('ascii') in product.read_bytes(),
            'linked Windows PE lacks its exact embedded source ID')
    origins = {'uwvm.exe': product,
               RUNNER: source / 'test/0017.runtime' / RUNNER}
    origins.update({stem + '.wasm': oracle_dir / (stem + '.wasm') for stem in STEMS})
    files = {}
    for name, origin in origins.items():
        require(origin.is_file() and not origin.is_symlink(), f'missing stage input: {name}')
        destination = output / name
        with origin.open('rb') as incoming, destination.open('xb') as outgoing:
            shutil.copyfileobj(incoming, outgoing)
        files[name] = digest(destination)
        require(files[name] == digest(origin), f'changed stage input: {name}')
    qualification = {'schema': 1, 'source_id': current,
                     'product_sha256': files['uwvm.exe'],
                     'build_sha256': digest(build_file),
                     'oracle_sha256': digest(oracle_file),
                     'files_sha256': files.copy()}
    qualification_file = output / 'qualification.json'
    qualification_file.write_text(json.dumps(qualification, indent=2, sort_keys=True) + '\n')
    files['qualification.json'] = digest(qualification_file)
    url = f'http://10.0.2.4:{args.port}/{args.prefix}'
    guest_runner = 'Documents\\uwvm-core3-newsyntax-runner.ps1'
    statements = ["$ErrorActionPreference='Stop'", f'$u={quote(url)}',
                  f'$p=Join-Path $env:USERPROFILE {quote(guest_runner)}',
                  f'Invoke-WebRequest -UseBasicParsing "$u/{RUNNER}" -OutFile $p',
                  f'if ((Get-FileHash $p -Algorithm SHA256).Hash.ToLowerInvariant() -ne '
                  f'{quote(files[RUNNER])}) {{ throw "staged new-syntax runner changed" }}',
                  f'& $p -BaseUrl $u -QualificationSha256 {quote(files["qualification.json"])}']
    command = ('powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand ' +
               base64.b64encode(('; '.join(statements)).encode('utf-16le')).decode('ascii'))
    (output / 'bootstrap-command.txt').write_text(command + '\n')
    after = subprocess.check_output(
        [sys.executable, str(fingerprint), str(source), str(output / 'source-after.json')],
        text=True).strip()
    require(after == current, 'source changed during Core 3 syntax staging')
    manifest = {'schema': 1, 'source_id': current,
                'product_sha256': files['uwvm.exe'],
                'oracle_sha256': qualification['oracle_sha256'],
                'files': files, 'port': args.port, 'prefix': args.prefix,
                'bootstrap_command_sha256': digest(output / 'bootstrap-command.txt')}
    (output / 'stage.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': current, 'product_sha256': files['uwvm.exe'],
                      'stage_sha256': digest(output / 'stage.json'),
                      'bootstrap_sha256': manifest['bootstrap_command_sha256']}, sort_keys=True))


if __name__ == '__main__':
    main()
