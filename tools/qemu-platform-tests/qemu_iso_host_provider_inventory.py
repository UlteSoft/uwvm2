#!/usr/bin/env python3
"""Read only the exact QEMU/ISO ELF provider closure; run no native command.

Search roots describe the planned per-tool environment, not runtime acceptance.
The outer original guardian and actual QEMU mapping verifier remain required.
"""
from pathlib import Path
import hashlib
import json
import stat
import struct

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
Q = Path('/home/macromodel/.local/opt/qemu-10.2.1/usr')
ISO = B / 'builds/windows-current-r10-cofffix-iso-tool-20261002/genisoimage'
SYSTEM = [Path('/lib/x86_64-linux-gnu'), Path('/usr/lib/x86_64-linux-gnu'),
          Path('/lib64'), Path('/usr/lib64'), Path('/lib'), Path('/usr/lib')]
FIXED = [(Q / 'bin/qemu-system-x86_64', 'qemu'),
         (Q / 'bin/qemu-img', 'qemu'), (ISO, 'iso')]
ENV_SEARCH = {'qemu': [Q / 'lib/x86_64-linux-gnu', Q / 'lib'], 'iso': []}
MAX_FILES = 128

def elf_metadata(path):
    size = path.stat().st_size
    with path.open('rb') as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:7] != b'\x7fELF\x02\x01\x01':
            raise RuntimeError('not a Linux AMD64 ELF64 provider')
        fields = struct.unpack_from('<HHIQQQIHHHHHH', header, 16)
        if fields[1] != 62 or header[7] not in (0, 3):
            raise RuntimeError('unexpected Linux host provider ABI')
        offset, width, count = fields[4], fields[8], fields[9]
        if width != 56 or not 1 <= count <= 4096 or offset > size or count > (size - offset) // width:
            raise RuntimeError('host program table outside file')
        stream.seek(offset)
        raw = stream.read(width * count)
        if len(raw) != width * count:
            raise RuntimeError('truncated host program table')
        segments = [struct.unpack_from('<IIQQQQQQ', raw, index * width) for index in range(count)]
        for row in segments:
            if row[2] > size or row[5] > size - row[2]:
                raise RuntimeError('host segment outside file')
        dynamic = [row for row in segments if row[0] == 2]
        if len(dynamic) != 1 or dynamic[0][5] % 16 or not 16 <= dynamic[0][5] <= 128 << 10:
            raise RuntimeError('host dynamic table size invalid')
        stream.seek(dynamic[0][2])
        raw = stream.read(dynamic[0][5])
        if len(raw) != dynamic[0][5]:
            raise RuntimeError('truncated host dynamic table')
        tags, terminated = [], False
        for cursor in range(0, len(raw), 16):
            tag, value = struct.unpack_from('<qQ', raw, cursor)
            if tag == 0:
                terminated = True
                break
            tags.append((tag, value))
        addresses = [value for tag, value in tags if tag == 5]
        lengths = [value for tag, value in tags if tag == 10]
        # The pinned LLVM host DSOs export a much larger symbol string table
        # than the standalone target ELF. This separate metadata cap neither
        # changes the target parser's 2 MiB cap nor any execution/file budget.
        if not terminated or len(addresses) != 1 or len(lengths) != 1 or not 1 <= lengths[0] <= 64 << 20:
            raise RuntimeError('host dynamic string table invalid: ' + str(path))
        positions = [row[2] + addresses[0] - row[3] for row in segments
                     if row[0] == 1 and addresses[0] >= row[3] and addresses[0] - row[3] <= row[5]
                     and lengths[0] <= row[5] - (addresses[0] - row[3])]
        if len(positions) != 1:
            raise RuntimeError('ambiguous host dynamic string mapping')
        stream.seek(positions[0])
        strings = stream.read(lengths[0])
        if len(strings) != lengths[0]:
            raise RuntimeError('truncated host dynamic strings')

        def string(value):
            if value >= len(strings):
                raise RuntimeError('host dynamic string offset outside table')
            end = strings.find(b'\0', value)
            if end < value or end - value > 4096:
                raise RuntimeError('host dynamic string is not bounded')
            return strings[value:end].decode('ascii')

        interpreters = []
        for row in segments:
            if row[0] != 3:
                continue
            if not 2 <= row[5] <= 256:
                raise RuntimeError('host ELF interpreter size invalid')
            stream.seek(row[2])
            raw = stream.read(row[5])
            if len(raw) != row[5] or raw[-1] != 0 or b'\0' in raw[:-1]:
                raise RuntimeError('host ELF interpreter is not terminated')
            interpreters.append(raw[:-1].decode('ascii'))
        return {'DT_NEEDED': [string(value) for tag, value in tags if tag == 1],
                'RPATH': [string(value) for tag, value in tags if tag == 15],
                'RUNPATH': [string(value) for tag, value in tags if tag == 29],
                'interpreters': interpreters}

def main():
    queue = list(FIXED)
    rows, seen = [], set()
    while queue:
        candidate, environment = queue.pop(0)
        path = candidate.resolve(strict=True)
        if (path, environment) in seen:
            continue
        seen.add((path, environment))
        if len(seen) > MAX_FILES:
            raise RuntimeError('fixed QEMU/ISO closure exceeds 128 provider contexts')
        before = path.stat()
        if (not stat.S_ISREG(before.st_mode) or not 64 <= before.st_size <= 2 << 30
                or before.st_uid not in (0, 1000)):
            raise RuntimeError('host provider ownership/byte budget invalid')
        metadata = elf_metadata(path)
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        after = path.stat()
        identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                                  value.st_mtime_ns, value.st_ctime_ns)
        if identity(before) != identity(after):
            raise RuntimeError('host provider changed during static inventory')
        origins = [Path(value.replace('$ORIGIN', str(path.parent)))
                   for field in ('RPATH', 'RUNPATH') for text in metadata[field]
                   for value in text.split(':')]
        if any(not origin.is_absolute() or '$' in str(origin) for origin in origins):
            raise RuntimeError('unknown host loader path expansion')
        # This records a finite proposed resolution for later real-loader checks.
        # It does not claim that RPATH/RUNPATH priority or dlopen was executed.
        search = [*ENV_SEARCH[environment], *origins, *SYSTEM]
        dependencies = []
        for name in metadata['DT_NEEDED']:
            if '/' in name or not name:
                raise RuntimeError('host DT_NEEDED is not a library basename')
            candidates = [directory / name for directory in search
                          if (directory / name).is_file()]
            if not candidates:
                raise RuntimeError('missing fixed host ELF provider: ' + name)
            selected = candidates[0].resolve(strict=True)
            dependencies.append({'name': name, 'resolved': str(selected)})
            queue.append((selected, environment))
        for interpreter in metadata['interpreters']:
            if interpreter != '/lib64/ld-linux-x86-64.so.2':
                raise RuntimeError('unexpected host tool interpreter')
            queue.append((Path(interpreter), environment))
        rows.append({'requested_path': str(candidate), 'path': str(path),
                     'planned_environment': environment, 'bytes': before.st_size,
                     'device': before.st_dev, 'inode': before.st_ino, 'uid': before.st_uid,
                     'sha256': digest, **metadata,
                     'resolved_DT_NEEDED': dependencies})
    firmware = []
    for path in (Q / 'share/seabios/bios-256k.bin',):
        before = path.stat()
        if not stat.S_ISREG(before.st_mode) or not 1 <= before.st_size <= 4 << 20:
            raise RuntimeError('fixed firmware must be bounded regular input')
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        after = path.stat()
        if identity(before) != identity(after):
            raise RuntimeError('fixed firmware changed during static inventory')
        firmware.append({'path': str(path.resolve(strict=True)), 'bytes': before.st_size,
                         'sha256': digest, 'device': before.st_dev, 'inode': before.st_ino,
                         'uid': before.st_uid})
    print(json.dumps({'schema': 'uwvm-qemu-iso-host-providers-source-static-v1',
          'kind': 'ELF-source-metadata-only', 'compiler_or_native_tool_executed': False,
          'actual_guest_qualified': False, 'runtime_loader_execution_qualified': False,
          'planned_environment_search': {name: list(map(str, paths))
                                         for name, paths in ENV_SEARCH.items()},
          'system_search': list(map(str, SYSTEM)), 'files': rows, 'firmware': firmware},
          indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
