#!/usr/bin/env python3
"""Qualify an explicit real Win64 compiler-rt archive before adding link input.

This does not select a C++ EH runtime or change Clang's -rtlib policy. It only
supplies compiler integer helpers required by the chosen LLVM static closure.
"""

import hashlib
import json
from pathlib import Path
import re
import subprocess


REQUIRED_SYMBOLS = (
    '__ashldi3', '__ashrdi3', '__cmpdi2', '__divdi3', '__fixdfdi',
    '__fixsfdi', '__floatdidf', '__lshrdi3', '__moddi3', '__udivdi3', '__umoddi3',
)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_headers(members: list[str], headers: str) -> None:
    formats = re.findall(r'^Format: (\S+)\s*$', headers, re.MULTILINE)
    machines = re.findall(r'^\s*Machine: (\S+)', headers, re.MULTILINE)
    require(bool(members) and len(formats) == len(machines) == len(members),
            'builtins archive has incomplete member/header evidence')
    require(all(value == 'COFF-x86-64' for value in formats) and
            all(value == 'IMAGE_FILE_MACHINE_AMD64' for value in machines),
            'builtins archive contains a non-AMD64 COFF member')


def verify_symbols(symbols: str) -> list[str]:
    # llvm-nm --format=posix --defined-only prints name/type before any numeric
    # fields. Require real externally defined code, never an undefined import.
    defined = set(re.findall(r'^(\S+)\s+[TW]\s', symbols, re.MULTILINE))
    require(set(REQUIRED_SYMBOLS) <= defined,
            'builtins archive lacks integer helpers: ' +
            ', '.join(sorted(set(REQUIRED_SYMBOLS) - defined)))
    return list(REQUIRED_SYMBOLS)


def verify_unchanged(certificate: dict[str, object]) -> None:
    require(digest(Path(certificate['archive'])) == certificate['sha256'] and
            all(digest(Path(record['path']).resolve(strict=True)) == record['sha256']
                for record in certificate['tools'].values()) and
            digest(Path(certificate['certificate_path'])) == certificate['certificate_sha256'],
            'qualified builtins archive, tools or certificate changed during build')


def qualify(archive: Path, ar: Path, readobj: Path, nm: Path,
            output: Path, environment: dict[str, str]) -> dict[str, object]:
    archive = archive.resolve(strict=True)
    require(archive.is_file() and archive.suffix == '.a',
            'builtins must be an existing explicit static archive')
    before = digest(archive)
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    tools = {'ar': ar.absolute(), 'readobj': readobj.absolute(), 'nm': nm.absolute()}
    records = {name: {'path': str(path),
                      'realpath': str(path.resolve(strict=True)),
                      'sha256': digest(path.resolve(strict=True))}
               for name, path in tools.items()}
    commands = []

    def invoke(name: str, arguments: list[str]) -> str:
        command = [str(tools[name]), *arguments, str(archive)]
        stdout = output / (name + '.stdout')
        stderr = output / (name + '.stderr')
        with stdout.open('xb') as out, stderr.open('xb') as err:
            result = subprocess.run(command, env=environment, stdout=out,
                                    stderr=err, timeout=60, check=False)
        commands.append({'argv': command, 'exit': result.returncode,
                         'stdout_sha256': digest(stdout),
                         'stderr_sha256': digest(stderr)})
        require(result.returncode == 0, 'builtins qualification tool failed: ' + name)
        return stdout.read_text()

    members = invoke('ar', ['t']).splitlines()
    verify_headers(members, invoke('readobj', ['--file-headers']))
    required = verify_symbols(invoke('nm', ['--defined-only', '--extern-only', '--format=posix']))
    require(digest(archive) == before and all(
                digest(Path(record['path']).resolve(strict=True)) == record['sha256']
                for record in records.values()),
            'builtins archive or inspection tool changed during qualification')
    certificate = {'schema': 1, 'archive': str(archive), 'sha256': before,
                   'size': archive.stat().st_size, 'members': len(members),
                   'target': 'x86_64-w64-windows-gnu', 'format': 'COFF-x86-64',
                   'required_defined_code_symbols': required, 'tools': records,
                   'commands': commands,
                   'scope': 'actual static builtins link input; C++ EH runtime selection unchanged'}
    certificate_path = output / 'qualification.json'
    certificate_path.write_text(json.dumps(certificate, indent=2, sort_keys=True) + '\n')
    certificate['certificate_path'] = str(certificate_path)
    certificate['certificate_sha256'] = digest(certificate_path)
    return certificate
