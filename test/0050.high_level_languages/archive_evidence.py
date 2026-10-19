#!/usr/bin/env python3
"""Archive explicit evidence paths in the existing Linux 64-GiB cgroup.

No source cleanup is performed. Success requires streaming member verification,
a second input inventory, unchanged boot/cgroup, and a durable final receipt.
"""
import argparse
from decimal import Decimal
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import signal
import stat
import subprocess
import tarfile
import tempfile
import time

from build_products import admission


EXPECTED_CGROUP = '0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
INVENTORY_MEMBER = 'ARCHIVE-INVENTORY.json'
MINIMUM_FREE = 512 << 20
CHUNK = 4 << 20


def progress(phase, **values):
    print(json.dumps({'phase': phase, **values}, ensure_ascii=True), flush=True)


def durable_json(path, value):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write((json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False) + '\n').encode())
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        sync_directory(path.parent)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def admitted():
    result = admission()
    if result['membership'] != EXPECTED_CGROUP:
        raise RuntimeError('Only the original existing 64-GiB container cgroup is admitted')
    return result


def disk_free(parent):
    free = shutil.disk_usage(parent).free
    if free < MINIMUM_FREE:
        raise RuntimeError(f'low_disk: {free} bytes free; at least {MINIMUM_FREE} required')
    return free


def checked_path(arena, relative):
    path = arena
    # Never traverse a symlink directory, including explicit include ancestors.
    for index, part in enumerate(PurePosixPath(relative).parts):
        path /= part
        metadata = path.lstat()
        if index + 1 < len(PurePosixPath(relative).parts) and not stat.S_ISDIR(metadata.st_mode):
            raise ValueError('include ancestor is not a real directory: ' + str(path))
    return path


def source_metadata(path, relative):
    value = path.lstat()
    kind = ('file' if stat.S_ISREG(value.st_mode) else 'directory' if stat.S_ISDIR(value.st_mode)
            else 'symlink' if stat.S_ISLNK(value.st_mode) else None)
    if kind is None:
        raise ValueError('special files are not evidence archive inputs: ' + relative)
    row = {'path': relative, 'type': kind, 'size': value.st_size if kind == 'file' else 0,
           'mode': stat.S_IMODE(value.st_mode), 'uid': value.st_uid, 'gid': value.st_gid,
           'mtime_ns': value.st_mtime_ns,
           'source_identity': {'device': value.st_dev, 'inode': value.st_ino,
                               'ctime_ns': value.st_ctime_ns, 'source_size': value.st_size}}
    if kind == 'symlink':
        row['link_target'] = os.readlink(path)
    return row


def hash_stream(stream, tick=None):
    checksum, size = hashlib.sha256(), 0
    while block := stream.read(CHUNK):
        checksum.update(block)
        size += len(block)
        if tick is not None:
            tick(size)
    return checksum.hexdigest(), size


def inventory(arena, includes, phase):
    rows, last = {}, time.monotonic()
    progress(phase, state='started')

    def tick(name, size):
        nonlocal last
        if time.monotonic() - last >= 5:
            progress(phase, files=sum(row['type'] == 'file' for row in rows.values()), current=name, bytes=size)
            last = time.monotonic()

    def visit(relative):
        if relative in rows:
            return
        path = checked_path(arena, relative)
        row = source_metadata(path, relative)
        if row['type'] == 'file':
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
            with os.fdopen(fd, 'rb') as stream:
                opened = os.fstat(stream.fileno())
                if not stat.S_ISREG(opened.st_mode) or (opened.st_dev, opened.st_ino) != (row['source_identity']['device'], row['source_identity']['inode']):
                    raise RuntimeError('input replaced before hashing: ' + relative)
                row['sha256'], size = hash_stream(stream, lambda size: tick(relative, size))
            if size != row['size'] or source_metadata(path, relative) != {key: value for key, value in row.items() if key != 'sha256'}:
                raise RuntimeError('input changed while hashing: ' + relative)
        rows[relative] = row
        if row['type'] == 'directory':
            with os.scandir(path) as directory:
                names = sorted(entry.name for entry in directory)
            for name in names:
                visit(relative + '/' + name)
        tick(relative, row['size'])

    for relative in includes:
        visit(relative)
    progress(phase, state='complete', entries=len(rows), files=sum(row['type'] == 'file' for row in rows.values()),
             bytes=sum(row['size'] for row in rows.values()))
    return rows


def stop_owned_tar(process):
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        # An unreaped direct child still owns its PID/process-group identity.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait(timeout=5)


def verify_archive(path, expected):
    seen, last = set(), time.monotonic()
    progress('verify', state='started')

    def tick(name, size):
        nonlocal last
        if time.monotonic() - last >= 5:
            progress('verify', members=len(seen), current=name, bytes=size)
            last = time.monotonic()

    with tarfile.open(path, 'r|gz') as archive:
        for member in archive:
            name = member.name.rstrip('/') if member.isdir() else member.name
            if name not in expected or name in seen or PurePosixPath(name).is_absolute() or '..' in PurePosixPath(name).parts:
                raise RuntimeError('unexpected, unsafe or duplicate archive member: ' + repr(name))
            seen.add(name)
            wanted = expected[name]
            kind = 'file' if member.isfile() else 'directory' if member.isdir() else 'symlink' if member.issym() else 'unsupported'
            if kind != wanted['type'] or member.size != wanted['size']:
                raise RuntimeError('archive member type/size mismatch: ' + name)
            if (member.mode, member.uid, member.gid) != (wanted['mode'], wanted['uid'], wanted['gid']):
                raise RuntimeError('archive member permission/ownership mismatch: ' + name)
            stamp = Decimal(member.pax_headers.get('mtime', str(member.mtime)))
            if int(stamp * 1000000000) != wanted['mtime_ns']:
                raise RuntimeError('archive member modification time mismatch: ' + name)
            if kind == 'symlink' and member.linkname != wanted['link_target']:
                raise RuntimeError('archive symlink target mismatch: ' + name)
            if kind == 'file':
                with archive.extractfile(member) as stream:
                    actual, size = hash_stream(stream, lambda size: tick(name, size))
                if actual != wanted['sha256'] or size != wanted['size']:
                    raise RuntimeError('archive member content mismatch: ' + name)
            if time.monotonic() - last >= 5:
                progress('verify', members=len(seen), current=name)
                last = time.monotonic()
    if seen != set(expected):
        raise RuntimeError('missing archive members: ' + repr(sorted(set(expected) - seen)))
    progress('verify', state='complete', members=len(seen))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--include', action='append', required=True, help='Repeat an explicit safe relative file/directory/symlink path')
    parser.add_argument('--out', type=Path, required=True, help='New .tar.gz outside every selected directory')
    args = parser.parse_args()
    before = admitted()
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    arena = args.arena.resolve(strict=True)
    if not arena.is_dir() or not args.out.name.endswith('.tar.gz'):
        raise ValueError('a real arena directory and .tar.gz output are required')
    includes = []
    for value in args.include:
        path = PurePosixPath(value)
        if not value or value.startswith('/') or not path.parts or any(part in ('..', '') for part in value.split('/')) or '\0' in value:
            raise ValueError('include must be a nonempty safe relative path: ' + repr(value))
        relative = str(path)
        checked_path(arena, relative)
        if relative not in includes:
            includes.append(relative)
    output = args.out.parent.resolve(strict=True) / args.out.name
    receipt = output.with_name(output.name + '.receipt.json')
    inventory_directory = output.with_name(output.name + '.inventory')
    log = output.with_name(output.name + '.tar.log')
    for destination in [output, receipt, inventory_directory, log]:
        if destination.exists() or destination.is_symlink():
            raise FileExistsError('use a fresh output name; evidence is never overwritten: ' + str(destination))
        for relative in includes:
            selected = arena / relative
            if destination == selected or (stat.S_ISDIR(selected.lstat().st_mode) and destination.is_relative_to(selected)):
                raise ValueError('archive outputs must be outside selected inputs: ' + str(destination))
    disk_free(output.parent)
    inventory_directory.mkdir(mode=0o700)
    data = {'passed': False, 'boot': boot, 'admission_before': before, 'arena_argument': str(args.arena),
            'arena_resolved': str(arena), 'includes': includes, 'archive': str(output),
            'inventory': str(inventory_directory / INVENTORY_MEMBER), 'minimum_free_bytes': MINIMUM_FREE,
            'cleanup': 'only this invocation partial archive and temporary NUL filelists; inputs are never deleted'}
    durable_json(receipt, data)
    temporary_lists, partial, process = [], None, None
    try:
        version = subprocess.run(['/usr/bin/tar', '--version'], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True, timeout=10, check=True)
        if 'GNU tar' not in version.stdout.splitlines()[0]:
            raise RuntimeError('GNU tar is required')
        data['tar_version'] = version.stdout
        rows = inventory(arena, includes, 'hash')
        if INVENTORY_MEMBER in rows:
            raise ValueError('reserved archive inventory member collides with an input')
        disk_free(output.parent)
        document = {'schema': 'uwvm-hll-evidence-inventory-v1', 'boot': boot, 'admission': before,
                    'arena': str(arena), 'includes': includes, 'entries': [rows[name] for name in sorted(rows)]}
        inventory_path = inventory_directory / INVENTORY_MEMBER
        durable_json(inventory_path, document)
        metadata = source_metadata(inventory_path, INVENTORY_MEMBER)
        with inventory_path.open('rb') as stream:
            metadata['sha256'], _ = hash_stream(stream)
        expected = rows | {INVENTORY_MEMBER: metadata}
        data.update(entries=len(rows), files=sum(row['type'] == 'file' for row in rows.values()),
                    input_bytes=sum(row['size'] for row in rows.values()), inventory_sha256=metadata['sha256'])
        for names in [sorted(rows), [INVENTORY_MEMBER]]:
            with tempfile.NamedTemporaryFile(dir=output.parent, prefix=output.name + '.filelist-', delete=False) as stream:
                temporary_lists.append(Path(stream.name))
                for name in names:
                    stream.write(os.fsencode(name) + b'\0')
        fd, name = tempfile.mkstemp(dir=output.parent, prefix=output.name + '.', suffix='.partial')
        os.close(fd)
        partial = Path(name)
        command = ['/usr/bin/tar', '--create', '--gzip', '--format=pax', '--numeric-owner', '--no-recursion',
                   '--hard-dereference', '--pax-option=delete=atime,delete=ctime', '--file', str(partial),
                   '--null', '--verbatim-files-from', '--directory', str(arena), '--files-from', str(temporary_lists[0]),
                   '--directory', str(inventory_directory), '--files-from', str(temporary_lists[1])]
        data.update(tar_argv=command, phase='archive')
        durable_json(receipt, data)
        progress('archive', state='started', entries=len(expected))
        with log.open('xb') as stream:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
            data['tar_pid'] = process.pid
            last = time.monotonic()
            while process.poll() is None:
                free = disk_free(output.parent)
                if time.monotonic() - last >= 5:
                    progress('archive', bytes=partial.stat().st_size, disk_free_bytes=free)
                    last = time.monotonic()
                try:
                    process.wait(timeout=.25)
                except subprocess.TimeoutExpired:
                    pass
        data['tar_exit'] = process.returncode
        if process.returncode != 0:
            raise RuntimeError('GNU tar failed; see ' + str(log))
        process = None
        disk_free(output.parent)
        with partial.open('rb') as stream:
            os.fsync(stream.fileno())
        progress('archive', state='complete', bytes=partial.stat().st_size)
        verify_archive(partial, expected)
        last_hash_progress = time.monotonic()

        def archive_hash_tick(size, step='archive-sha256'):
            nonlocal last_hash_progress
            if time.monotonic() - last_hash_progress >= 5:
                progress('verify', step=step, bytes=size)
                last_hash_progress = time.monotonic()

        # tarfile may stop at tar end markers before consuming the gzip trailer.
        # Drain a bounded gzip reader to EOF as well, checking CRC and ISIZE.
        with gzip.open(partial, 'rb') as stream:
            data['tar_stream_sha256'], data['tar_stream_bytes'] = hash_stream(
                stream, lambda size: archive_hash_tick(size, 'gzip-integrity'))
        with partial.open('rb') as stream:
            data['archive_sha256'], data['archive_bytes'] = hash_stream(stream, archive_hash_tick)
        if inventory(arena, includes, 'rehash') != rows:
            raise RuntimeError('input inventory changed after archiving')
        after = admitted()
        if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != boot or before['membership'] != after['membership'] or before['memory_events'] != after['memory_events']:
            raise RuntimeError('boot/cgroup/memory events changed')
        data['admission_after'] = after
        disk_free(output.parent)
        # Atomic publication without replacing any independently created output.
        os.link(partial, output)
        partial.unlink()
        partial = None
        sync_directory(output.parent)
        data.update(passed=True, phase='complete', verified_members=len(expected), input_rehash_passed=True)
    except BaseException as error:
        data.update(passed=False, error=repr(error), phase='failed')
        raise
    finally:
        cleanup_errors = []
        try:
            stop_owned_tar(process)
            if process is not None:
                data['tar_exit_after_cleanup'] = process.returncode
        except Exception as error:
            cleanup_errors.append(repr(error))
        for path in ([partial] if partial is not None else []) + temporary_lists:
            try:
                path.unlink(missing_ok=True)
            except Exception as error:
                cleanup_errors.append(repr(error))
        if cleanup_errors:
            data.update(passed=False, phase='failed', cleanup_errors=cleanup_errors)
        durable_json(receipt, data)
        if cleanup_errors:
            raise RuntimeError('owned temporary cleanup failed; inspect receipt')
    progress('complete', passed=True, archive=str(output), receipt=str(receipt))


if __name__ == '__main__':
    main()
