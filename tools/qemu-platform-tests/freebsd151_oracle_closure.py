#!/usr/bin/env python3
"""Bind fixed FreeBSD SDK/compiler/artifact data without executing native tools.

Every compiler, linker and inspector is separately admitted by the original
PIDFD guardian. An ELF/header result is not a guest or product qualification.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import struct


SOURCE_SHA = '58b066d3454eed5b972036bc14f081e9d8284408ab9f568e66507a445e42f8b3'
SDK_RECORD_SHA = '250aa737639c465c746a3332ffa59c8503d1cb399ba8b7c110b8d4ced31cf93a'
HOST_RECORD_SHA = 'd941b7c9a349dddfda5dbd6059006768ecabe3a0d5825c1d0eb0af0712fdcbe9'
TOOLS = {
    'clang++': '0e800e4decf0de00b5ebeefed814b2cf6551d17291a220ee107b5198cd7ca23b',
    'ld.lld': '2c67e1532947128d789d0c4696f75ff26d30400cd658361604b46806fd61b105',
    'llvm-readobj': 'cc4a1825e38b09d9522508b50c7b5bafe99af3451ad23b7bc40da4a6d9c9a7b7',
    'llvm-objdump': '0925143d4d994e24f2875b9dd6a7778a880fd8372a60d7053092b59dda4fb7cd',
}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def record(path):
    before = path.stat()
    if not stat.S_ISREG(before.st_mode) or before.st_size > 2 << 30:
        raise RuntimeError('provider is not a bounded regular file: ' + str(path))
    row = {'path': str(path), 'resolved': str(path.resolve(strict=True)),
           'bytes': before.st_size, 'device': before.st_dev, 'inode': before.st_ino,
           'uid': before.st_uid, 'mode': stat.S_IMODE(before.st_mode), 'sha256': sha(path)}
    after = path.stat()
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns,
            before.st_mode, before.st_uid) != (after.st_dev, after.st_ino, after.st_size,
            after.st_mtime_ns, after.st_ctime_ns, after.st_mode, after.st_uid):
        raise RuntimeError('provider changed while hashing: ' + str(path))
    return row


def write_new(path, value):
    data = (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()
    if len(data) > 16 << 20:
        raise RuntimeError('bounded closure record exceeds 16 MiB')
    with path.open('xb') as stream:
        stream.write(data)
    return hashlib.sha256(data).hexdigest()


def pinned_record(path, pinned):
    actual = record(path)
    candidates = ([pinned[str(path)]] if str(path) in pinned else
                  [row for row in pinned.values() if row['resolved'] == actual['resolved']])
    identity = {key: value for key, value in actual.items() if key != 'path'}
    if not any({key: value for key, value in row.items() if key != 'path'} == identity
               for row in candidates):
        raise RuntimeError('unpinned or changed compiler/linker input: ' + str(path))
    return actual


def elf(path, executable):
    # This parser reads only bounded regular artifacts/providers. Each header,
    # program table, PT_DYNAMIC entry and string is proved within the real file.
    size = path.stat().st_size
    if not 64 <= size <= 32 << 20:
        raise RuntimeError('ELF outside cold component byte budget')
    data = path.read_bytes()
    if len(data) != size or data[:7] != b'\x7fELF\x02\x01\x01':
        raise RuntimeError('not the actual ELF64 little-endian target')
    header = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = header
    if machine != 62 or version != 1 or ehsize != 64 or data[7] not in (0, 9):
        raise RuntimeError('ELF does not name expected AMD64/FreeBSD ABI')
    if (executable and kind not in (2, 3)) or (not executable and kind != 1):
        raise RuntimeError('wrong ELF object/executable kind')
    result = {'ELF_class': 64, 'ELF_data': 'little', 'ELF_machine': 62,
              'ELF_OSABI': data[7], 'ELF_type': kind}
    if not executable:
        return result
    if not 1 <= phnum <= 4096 or phsize != 56 or phoff > size or phnum > (size - phoff) // phsize:
        raise RuntimeError('ELF program table outside file')
    loads, dynamics, interpreters = [], [], []
    for index in range(phnum):
        ptype, pflags, offset, address, physical, filesz, memsz, alignment = struct.unpack_from(
            '<IIQQQQQQ', data, phoff + index * phsize)
        if offset > size or filesz > size - offset or filesz > memsz:
            raise RuntimeError('ELF segment outside file')
        if ptype == 1:
            loads.append((address, filesz, offset))
        elif ptype == 2:
            dynamics.append((offset, filesz))
        elif ptype == 3:
            if not 2 <= filesz <= 256 or data[offset + filesz - 1] != 0:
                raise RuntimeError('ELF interpreter is not bounded and terminated')
            value = data[offset:offset + filesz - 1]
            if b'\0' in value:
                raise RuntimeError('ELF interpreter has embedded terminator')
            interpreters.append(value.decode('ascii'))
    if len(dynamics) != 1:
        raise RuntimeError('ELF requires one real dynamic provider table')
    offset, count = dynamics[0]
    if not 16 <= count <= 128 << 10 or count % 16:
        raise RuntimeError('ELF dynamic table size invalid')
    tags, ended = [], False
    for cursor in range(offset, offset + count, 16):
        tag, value = struct.unpack_from('<qQ', data, cursor)
        if tag == 0:
            ended = True
            break
        tags.append((tag, value))
    if not ended:
        raise RuntimeError('unterminated ELF dynamic table')
    tables = [value for tag, value in tags if tag == 5]
    lengths = [value for tag, value in tags if tag == 10]
    if len(tables) != 1 or len(lengths) != 1 or not 1 <= lengths[0] <= 2 << 20:
        raise RuntimeError('ELF dynamic strings lack exact bounded table')
    positions = [raw + tables[0] - address for address, length, raw in loads
                 if tables[0] >= address and tables[0] - address <= length
                 and lengths[0] <= length - (tables[0] - address)]
    if len(positions) != 1 or positions[0] > size or lengths[0] > size - positions[0]:
        raise RuntimeError('ELF dynamic strings have ambiguous/outside mapping')
    strings = data[positions[0]:positions[0] + lengths[0]]

    def string(value):
        if value >= len(strings):
            raise RuntimeError('ELF dynamic string offset outside table')
        end = strings.find(b'\0', value)
        if end < 0 or end - value > 256:
            raise RuntimeError('ELF dynamic string is not bounded')
        return strings[value:end].decode('ascii')

    needed = [string(value) for tag, value in tags if tag == 1]
    sonames = [string(value) for tag, value in tags if tag == 14]
    if (len(set(needed)) != len(needed) or len(sonames) > 1
            or any(not re.fullmatch(r'lib[A-Za-z0-9_+.-]+\.so(?:\.[0-9]+)*', name) for name in needed)):
        raise RuntimeError('ELF dependency/SONAME is not a fixed library name')
    result.update(interpreters=interpreters, DT_NEEDED=needed, DT_SONAME=sonames)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('phase', choices=('before', 'object', 'binary', 'after'))
    parser.add_argument('--label')
    args = parser.parse_args()
    plan_path = args.plan.resolve(strict=True)
    plan = json.loads(plan_path.read_text())
    source, sdk, tools, output = map(Path, (plan['source'], plan['sdk'], plan['toolchain'], plan['output']))
    manifest = source.parent / 'manifest.json'
    sdk_record = sdk.parent / 'provider-actual.json'
    host_record = Path(plan['host_provider_record'])
    if (sha(manifest) != SOURCE_SHA or plan['source_manifest_sha256'] != SOURCE_SHA
            or sha(sdk_record) != SDK_RECORD_SHA or plan['SDK_provider_record_sha256'] != SDK_RECORD_SHA
            or sha(host_record) != HOST_RECORD_SHA or plan['host_provider_record_sha256'] != HOST_RECORD_SHA):
        raise RuntimeError('immutable actual FreeBSD source/SDK record changed')
    if args.phase == 'before':
        paths = {plan_path, output / 'commands.json', Path(__file__).resolve(), manifest, sdk_record, host_record}
        host = json.loads(host_record.read_text())
        if host['schema'] != 'uwvm-freebsd-cross-tool-host-provider-static-v1' or len(host['files']) != 21:
            raise RuntimeError('unexpected fixed host cross-tool provider set')
        for row in host['files']:
            path = Path(row['path'])
            actual = record(path)
            if any(actual[key] != row[key] for key in ('path', 'bytes', 'device', 'inode', 'uid', 'sha256')):
                raise RuntimeError('fixed Linux-host cross-tool provider changed')
            paths.add(path)
        for row in json.loads(manifest.read_text())['files']:
            path = source / row['path']
            if path.is_symlink() or path.stat().st_size != row['bytes'] or sha(path) != row['sha256']:
                raise RuntimeError('frozen FreeBSD source leaf changed')
            paths.add(path)
        provider = json.loads(sdk_record.read_text())
        if (provider['archive_sha256'] != '3768988b151c20f965679062b065c63a977d6bbb9f47fd83695ec2c40790c18f'
                or len(provider['excluded_non_SDK_test_aliases']) != 1 or not provider['existing_archive_after_match']):
            raise RuntimeError('actual SDK extraction provenance incomplete')
        for row in provider['selected_files']:
            path = sdk / row['path']
            if not path.resolve(strict=True).is_relative_to(sdk.resolve(strict=True)):
                raise RuntimeError('SDK link now escapes its pinned private subset')
            if path.stat().st_size != row['bytes'] or sha(path) != row['sha256']:
                raise RuntimeError('actual selected SDK provider changed: ' + str(path))
            paths.add(path)
        paths.update(path for path in (tools / 'lib/clang/23/include').rglob('*') if path.is_file())
        paths.update(path for path in (tools / 'lib').rglob('*.so*') if path.is_file())
        for name, expected in TOOLS.items():
            path = tools / 'bin' / name
            if sha(path) != expected:
                raise RuntimeError('exact native compiler/inspector changed')
            paths.add(path)
        config = (sdk / 'usr/include/c++/v1/__config_site').read_text()
        if '#define _LIBCPP_ABI_VERSION 1' not in config or '#define _LIBCPP_ABI_NAMESPACE __1' not in config:
            raise RuntimeError('actual FreeBSD SDK libc++ ABI1 required')
        for variant in plan['variants']:
            for key in ('object', 'binary', 'depfile', 'link_map'):
                if Path(variant[key]).exists():
                    raise RuntimeError('cold FreeBSD artifact already exists')
        print(write_new(output / 'input-before.json', {'phase': 'before', 'plan_sha256': sha(plan_path),
            'files': [record(path) for path in sorted(paths)], 'actual_target_libcpp_abi': '__1',
            'actual_guest_qualified': False, 'ROS_paired_LLVM23_product_qualified': False}))
        return
    before = json.loads((output / 'input-before.json').read_text())
    if before['plan_sha256'] != sha(plan_path):
        raise RuntimeError('fixed FreeBSD plan changed after original admission')
    pinned = {row['path']: row for row in before['files']}
    if args.phase == 'after':
        for name, row in pinned.items():
            if record(Path(name)) != row:
                raise RuntimeError('FreeBSD source/SDK/compiler closure changed')
        for variant in plan['variants']:
            saved = json.loads((output / (variant['label'] + '-binary.json')).read_text())
            if record(Path(variant['binary'])) != saved['binary']:
                raise RuntimeError('bound FreeBSD artifact changed')
        print(write_new(output / 'input-after.json', {'phase': 'after', 'input_files': len(pinned),
              'input_before_sha256': sha(output / 'input-before.json'), 'actual_guest_qualified': False,
              'ROS_paired_LLVM23_product_qualified': False}))
        return
    variants = [row for row in plan['variants'] if row['label'] == args.label]
    if len(variants) != 1:
        raise RuntimeError('unknown exact FreeBSD component profile')
    variant = variants[0]
    if args.phase == 'object':
        dep = Path(variant['depfile'])
        text = dep.read_text().replace('\\\n', ' ')
        if '\\' in text or ':' not in text:
            raise RuntimeError('unsupported actual dependency file encoding')
        dependencies = text.split(':', 1)[1].split()
        if not dependencies:
            raise RuntimeError('compiler emitted no actual dependencies')
        for name in dependencies:
            pinned_record(Path(name), pinned)
        obj = Path(variant['object'])
        print(write_new(output / (variant['label'] + '-object.json'), {'object': record(obj),
              **elf(obj, False), 'depfile': record(dep), 'actual_dependencies': dependencies}))
        return
    saved = json.loads((output / (variant['label'] + '-object.json')).read_text())
    obj = Path(variant['object'])
    if record(obj) != saved['object']:
        raise RuntimeError('bound FreeBSD object changed before linking')
    link_log = Path(variant['link_log'])
    if link_log.parent != Path(plan['evidence']) or link_log.is_symlink() or link_log.stat().st_size > 32 << 20:
        raise RuntimeError('missing actual bounded regular linker log')
    inputs, members = [], []
    for line in link_log.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        match = re.fullmatch(r'(/[^\s()]+)(?:\(([^\r\n()]+)\))?', line)
        if match is None:
            raise RuntimeError('unknown actual ELF --trace input: ' + line)
        path = Path(match[1])
        if path != obj:
            pinned_record(path, pinned)
        inputs.append(str(path))
        if match[2] is not None:
            if path.suffix != '.a':
                raise RuntimeError('actual archive contribution is not an archive')
            members.append({'archive': str(path), 'member': match[2]})
    required_crt = {str(sdk / 'usr/lib' / name) for name in
                    ('crt1.o', 'crti.o', 'crtbegin.o', 'crtend.o', 'crtn.o')}
    if str(obj) not in inputs or not required_crt <= set(inputs):
        raise RuntimeError('actual linker trace omits expected object/FreeBSD CRT inputs')
    binary = Path(variant['binary'])
    info = elf(binary, True)
    if info['interpreters'] != ['/libexec/ld-elf.so.1'] or info['ELF_OSABI'] != 9:
        raise RuntimeError('fresh FreeBSD executable lacks its exact loader/OS ABI')
    runtime, queue, visited = [], list(info['DT_NEEDED']), set()
    runtime.append(pinned_record(sdk / 'libexec/ld-elf.so.1', pinned))
    while queue:
        name = queue.pop(0)
        if name in visited:
            continue
        visited.add(name)
        candidates = [sdk / prefix / name for prefix in ('lib', 'usr/lib') if (sdk / prefix / name).is_file()]
        if len({str(path.resolve(strict=True)) for path in candidates}) != 1:
            raise RuntimeError('missing or ambiguous actual target runtime library: ' + name)
        path = candidates[0]
        entry = pinned_record(path, pinned)
        library = elf(path, True)
        if library['interpreters'] or library['DT_SONAME'] != [name]:
            raise RuntimeError('actual target runtime SONAME/loader mismatch')
        runtime.append({**entry, **library})
        queue.extend(library['DT_NEEDED'])
        if len(visited) > 32:
            raise RuntimeError('actual target runtime closure exceeds fixed library budget')
    print(write_new(output / (variant['label'] + '-binary.json'), {'binary': record(binary), **info,
          'link_log': record(link_log), 'link_map': record(Path(variant['link_map'])),
          'actual_trace_inputs': [pinned_record(Path(path), pinned) for path in sorted(set(inputs)) if Path(path) != obj],
          'actual_archive_members': members, 'actual_target_runtime_closure': runtime,
          'actual_guest_qualified': False, 'ROS_paired_LLVM23_product_qualified': False}))


if __name__ == '__main__':
    main()
