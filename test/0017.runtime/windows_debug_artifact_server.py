#!/usr/bin/env python3
"""Serve immutable Windows debugger PE/fixture evidence to the local QEMU VM.

The server binds only the QEMU host loopback. A manifest lists exact basenames
and SHA-256 values; a guest cannot request arbitrary host paths or overwrite
prior results. This is a test transport, not the debugger control endpoint.
"""

import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import re
import stat


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--prefix', default='uwvm-native-step-windows-x64')
    parser.add_argument('--port', type=int, default=18018)
    args = parser.parse_args()
    directory = args.directory.resolve(strict=True)
    manifest = json.loads(args.manifest.read_text())
    files = manifest.get('files')
    if not isinstance(files, dict) or not files:
        raise ValueError('manifest needs nonempty files mapping')
    if not re.fullmatch(r'[A-Za-z0-9_-]{12,80}', args.prefix):
        raise ValueError('invalid URL prefix')
    for name, expected in files.items():
        if not re.fullmatch(r'[A-Za-z0-9_.-]{1,120}', name) or not re.fullmatch(r'[0-9a-f]{64}', expected):
            raise ValueError(f'invalid manifest entry: {name!r}')
        path = directory / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'missing regular staging input: {name}')
        with path.open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
                raise ValueError(f'changed staging input: {name}')

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            route = '/' + args.prefix + '/'
            if not self.path.startswith(route):
                self.send_error(404)
                return
            name = self.path[len(route):]
            if name not in files:
                self.send_error(404)
                return
            try:
                descriptor = os.open(directory / name, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0))
                with os.fdopen(descriptor, 'rb') as stream:
                    info = os.fstat(stream.fileno())
                    if not stat.S_ISREG(info.st_mode):
                        raise ValueError('not a regular file')
                    if hashlib.file_digest(stream, 'sha256').hexdigest() != files[name]:
                        raise ValueError('input changed since staging')
                    stream.seek(0)
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/octet-stream')
                    self.send_header('Content-Length', str(info.st_size))
                    self.end_headers()
                    while block := stream.read(65536):
                        self.wfile.write(block)
            except (OSError, ValueError):
                if not self.wfile.closed:
                    self.close_connection = True

        def do_POST(self) -> None:
            if self.path != '/' + args.prefix + '/result':
                self.send_error(404)
                return
            try:
                size = int(self.headers.get('Content-Length', ''))
            except ValueError:
                self.send_error(411)
                return
            if not 0 < size <= 2 * 1024 * 1024:
                self.send_error(413)
                return
            raw = self.rfile.read(size)
            if len(raw) != size:
                self.send_error(400)
                return
            try:
                result = json.loads(raw.decode('utf-8-sig'))
                if not isinstance(result, dict) or not isinstance(result.get('passed'), bool):
                    raise ValueError('missing pass/fail result')
                descriptor = os.open(directory / 'result.json',
                                     os.O_WRONLY | os.O_CREAT | os.O_EXCL |
                                     getattr(os, 'O_NOFOLLOW', 0), 0o600)
                with os.fdopen(descriptor, 'wb') as stream:
                    stream.write(raw)
            except (OSError, ValueError, UnicodeError):
                self.send_error(409)
                return
            self.send_response(204)
            self.end_headers()

        def log_message(self, format: str, *values: object) -> None:
            print('%s %s' % (self.address_string(), format % values), flush=True)

    server = HTTPServer(('127.0.0.1', args.port), Handler)
    print(json.dumps({'url': f'http://10.0.2.4:{args.port}/{args.prefix}',
                      'files': len(files), 'directory': str(directory)}, sort_keys=True), flush=True)
    server.serve_forever()


if __name__ == '__main__':
    main()
