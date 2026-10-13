#!/usr/bin/env python3
"""Serve one deliberately altered staged input for a real Win11 fail-closed test.

This separate test transport never changes the qualified stage or the normal
artifact server. Run a fresh instance and result directory for each case.
"""

import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import re
import stat
import time

import prepare_windows_guest_bootstrap as bootstrap


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--manifest', choices=('stage.json', 'manifest.json'),
                        default='stage.json')
    parser.add_argument('--expected-stage-sha256', required=True)
    parser.add_argument('--kind', choices=('main', 'core3', 'broker'), required=True)
    parser.add_argument('--tamper-file', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--port', type=int, default=18022)
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()
    require(1024 <= args.port <= 65535 and 1 <= args.timeout <= 600,
            'invalid bounded test server port or timeout')
    stage = args.stage.resolve(strict=True)
    record_path = stage / args.manifest
    record_bytes = record_path.read_bytes()
    require(re.fullmatch(r'[0-9a-f]{64}', args.expected_stage_sha256) is not None and
            sha256(record_bytes) == args.expected_stage_sha256,
            'adversarial test stage differs from the previously qualified manifest')
    record = json.loads(record_bytes)
    files = record['files']
    require(args.tamper_file in files and
            re.fullmatch(r'[A-Za-z0-9_.-]{1,120}', args.tamper_file) is not None,
            'tamper target is not an exact staged basename')
    require(args.tamper_file in ('qualification.json',
                                bootstrap.RUNNERS[args.kind],
                                'uwvm.exe', 'uwvm-debug-server.exe',
                                'native-step-fixture.wasm'),
            'tamper target is outside the execution-before-hash cases')
    require((args.tamper_file != 'qualification.json' or args.kind != 'broker') and
            (args.tamper_file != 'uwvm-debug-server.exe' or args.kind == 'broker') and
            (args.tamper_file != 'native-step-fixture.wasm' or args.kind != 'core3'),
            'this runner does not consume the selected tamper target')
    originals: dict[str, Path] = {}
    for name, expected in files.items():
        path = stage / name
        require(not path.is_symlink() and path.is_file() and
                re.fullmatch(r'[A-Za-z0-9_.-]{1,120}', name) is not None and
                re.fullmatch(r'[0-9a-f]{64}', expected) is not None,
                f'unsafe or absent staged artifact: {name}')
        with path.open('rb') as stream:
            require(hashlib.file_digest(stream, 'sha256').hexdigest() == expected,
                    f'staged artifact changed: {name}')
        originals[name] = path
    require(originals[args.tamper_file].stat().st_size > 0,
            'cannot tamper an empty artifact')
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    prefix = f'uwvm-{args.kind}-tamper-windows-x64'
    url = f'http://10.0.2.4:{args.port}/{prefix}'
    bootstrap.URLS[args.kind] = url
    command = bootstrap.command_for(args.kind, files)
    require(len(command) < 8192, 'QMP type-file cannot carry this encoded command')
    (output / 'guest.command.txt').write_text(command + '\n')
    requests: list[dict[str, object]] = []
    result: dict | None = None

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            name = self.path.removeprefix('/' + prefix + '/')
            if self.path != '/' + prefix + '/' + name or name not in originals:
                self.send_error(404)
                return
            altered = name == args.tamper_file
            try:
                descriptor = os.open(originals[name], os.O_RDONLY |
                                     getattr(os, 'O_NOFOLLOW', 0))
                with os.fdopen(descriptor, 'rb') as stream:
                    info = os.fstat(stream.fileno())
                    require(stat.S_ISREG(info.st_mode), 'not a regular staged file')
                    require(hashlib.file_digest(stream, 'sha256').hexdigest() == files[name],
                            'stage changed after adversarial server startup')
                    stream.seek(0)
                    delivered = hashlib.sha256()
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/octet-stream')
                    self.send_header('Content-Length', str(info.st_size))
                    self.end_headers()
                    first = True
                    while block := stream.read(65536):
                        if first and altered:
                            block = bytes((block[0] ^ 1,)) + block[1:]
                        first = False
                        delivered.update(block)
                        self.wfile.write(block)
                    requests.append({'name': name, 'altered': altered,
                                     'delivered_sha256': delivered.hexdigest()})
            except (OSError, ValueError):
                self.close_connection = True

        def do_POST(self) -> None:
            nonlocal result
            if self.path != '/' + prefix + '/result' or result is not None:
                self.send_error(409)
                return
            try:
                size = int(self.headers.get('Content-Length', ''))
                require(0 < size <= 2 * 1024 * 1024, 'invalid result size')
                raw = self.rfile.read(size)
                require(len(raw) == size, 'truncated guest result')
                parsed = json.loads(raw.decode('utf-8-sig'))
                require(isinstance(parsed, dict) and parsed.get('passed') is False,
                        'tampered guest unexpectedly passed')
                if args.kind == 'broker':
                    require(parsed == {'passed': False,
                                       'error': 'broker-input-verification-failed'},
                            'broker failure result is not the bounded pre-capability record')
                result = parsed
                (output / 'guest-result.json').write_bytes(raw)
            except (ValueError, UnicodeError, OSError):
                self.send_error(400)
                return
            self.send_response(204)
            self.end_headers()

        def log_message(self, format: str, *values: object) -> None:
            pass

    server = HTTPServer(('127.0.0.1', args.port), Handler)
    server.timeout = 0.5
    print(json.dumps({'stage_sha256': sha256(record_bytes), 'url': url,
                      'command_file': str(output / 'guest.command.txt'),
                      'tamper_file': args.tamper_file}, sort_keys=True), flush=True)
    deadline = time.monotonic() + args.timeout
    try:
        while result is None and time.monotonic() < deadline:
            server.handle_request()
    finally:
        server.server_close()
    error = str((result or {}).get('error', ''))
    tampered_served = any(item['name'] == args.tamper_file and item['altered']
                          for item in requests)
    if args.tamper_file == bootstrap.RUNNERS[args.kind]:
        diagnostic = 'bootstrap-runner-verification-failed' in error
    elif args.tamper_file == 'qualification.json':
        diagnostic = 'qualification changed before guest execution' in error
    elif args.kind == 'broker':
        diagnostic = error == 'broker-input-verification-failed'
    else:
        diagnostic = 'changed before guest execution' in error or \
                     'changed before execution' in error or 'hash mismatch' in error
    passed = result is not None and tampered_served and diagnostic
    summary = {'passed': passed, 'source_id': record['source_id'],
               'stage_sha256': sha256(record_bytes),
               'qualification_sha256': files.get('qualification.json'),
               'kind': args.kind, 'tamper_file': args.tamper_file,
               'original_sha256': files[args.tamper_file],
               'requests': requests, 'guest_error': error,
               'guest_result_sha256': (sha256((output / 'guest-result.json').read_bytes())
                                       if result is not None else None)}
    (output / 'summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': passed, 'summary': str(output / 'summary.json')},
                     sort_keys=True), flush=True)
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
