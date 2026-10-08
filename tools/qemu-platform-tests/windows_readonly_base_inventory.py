#!/usr/bin/env python3
"""SOURCE ONLY proposal: one fixed original-cgroup read-only Windows hash cell.

Execute only through the reviewed original guardian. This hashes the provided
base in an owned read-only descriptor; it never invokes a compiler, qemu-img,
QEMU, shell, or guest. The record is input evidence, not platform acceptance.
"""
import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import time


B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
BASE_ROOT = Path('/home/macromodel/Documents/qemu/uwvm2-win11-storage')
OUT = B / 'qemu-platform-tests/windows-readonly-base-actual-20261003-r1'
SOURCE = B / 'qemu-platform-tests/windows-readonly-base-source-20261003-r1'
PROVIDERS = SOURCE / 'host-env-python-provider-static-r2.json'
PROVIDERS_SHA = 'e4c85845b1a7d7140240aef22f10b27e9727e092b9d26c7e3cb3eec1cafb8006'
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG_NAME = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'
CG = Path('/sys/fs/cgroup') / CG_NAME.lstrip('/')
FILES = (
    ('data.img', 64 << 30, None),
    ('windows.rom', 3653632, '2a653a97ea757753b40574074c008a1d74b8356513db114f891226b1f777df17'),
    ('windows.vars', 540672, '98086997557cbfb1966429b425d7f7da68ab0e412dfb07c17b79da8c4df18376'),
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def file_identity(row):
    return {'device': row.st_dev, 'inode': row.st_ino, 'uid': row.st_uid,
            'gid': row.st_gid, 'mode': stat.S_IMODE(row.st_mode),
            'links': row.st_nlink, 'bytes': row.st_size,
            'mtime_ns': row.st_mtime_ns, 'ctime_ns': row.st_ctime_ns}


def admission():
    require(sys.platform == 'linux' and __debug__, 'unoptimized original Linux host required')
    require(os.getuid() == os.geteuid() == 1000, 'original unprivileged UID1000 required')
    require(Path('/proc/sys/kernel/random/boot_id').read_text().strip() == BOOT,
            'original boot authority changed')
    require(Path('/proc/self/cgroup').read_text().strip() == '0::' + CG_NAME,
            'original guardian cgroup admission required')
    require(os.sched_getaffinity(0) and os.sched_getaffinity(0) <= set(range(16, 32)),
            'E-core-only original guardian affinity required')
    for name, expected in (('memory.max', str(64 << 30)), ('memory.swap.max', '0'),
                           ('cpuset.cpus.effective', '0,2,4,6,16-31')):
        require((CG / name).read_text().strip() == expected, 'original cgroup resource changed')
    require(not OUT.exists() and not OUT.is_symlink(), 'fresh private hash record directory required')
    free = os.statvfs(B / 'qemu-platform-tests')
    require(free.f_bavail * free.f_frsize >= (8 << 30) + (1 << 20),
            'original 8GiB reserve and finite metadata budget required')


def hash_original(name, expected_bytes, expected_sha256):
    path = BASE_ROOT / name
    require(path.resolve(strict=True) == path, 'resolved fixed original input required')
    original = path.lstat()
    require(stat.S_ISREG(original.st_mode) and original.st_uid == 1000 and original.st_nlink == 1,
            'original UID1000 single-link regular file required')
    require(original.st_size == expected_bytes, 'fixed original image/firmware extent changed')
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        actual = os.fstat(fd)
        require(file_identity(actual) == file_identity(original), 'original descriptor identity changed')
        # A sequential 64GiB input must not intentionally retain a 64GiB
        # page-cache working set inside the original 64GiB cgroup. These
        # Linux-host libc hints do not change original file bytes; the kernel
        # memory limit and actual original guardian remain the authority.
        os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_SEQUENTIAL)
        digest = hashlib.sha256()
        count = 0
        first104 = b''
        while count < expected_bytes:
            piece = os.read(fd, min(1 << 20, expected_bytes - count))
            require(piece, 'original read-only input ended early')
            if count == 0:
                first104 = piece[:104]
            digest.update(piece)
            count += len(piece)
            os.posix_fadvise(fd, count - len(piece), len(piece), os.POSIX_FADV_DONTNEED)
        require(os.read(fd, 1) == b'', 'original read-only input grew during hash')
        final = os.fstat(fd)
        require(file_identity(final) == file_identity(actual) and
                file_identity(path.lstat()) == file_identity(actual),
                'original image/firmware changed during full hash')
        value = digest.hexdigest()
        require(expected_sha256 is None or value == expected_sha256,
                'original fixed firmware source hash changed')
        return {'path': str(path), 'identity': file_identity(actual),
                'bytes_read': count, 'sha256': value, 'first104_hex': first104.hex(),
                'descriptor_opened_readonly': True, 'original_file_modified': False,
                'page_cache_profile': 'SEQUENTIAL then DONTNEED after each bounded 1MiB read'}
    finally:
        os.close(fd)


def host_provider_snapshot():
    require(PROVIDERS.resolve(strict=True) == PROVIDERS, 'fixed resolved provider ledger required')
    source_before = PROVIDERS.lstat()
    require(stat.S_ISREG(source_before.st_mode) and source_before.st_uid == 1000 and
            source_before.st_nlink == 1 and source_before.st_size == 14724,
            'fixed bounded owned provider ledger required')
    source_fd = os.open(PROVIDERS, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        require(file_identity(os.fstat(source_fd)) == file_identity(source_before),
                'provider ledger descriptor changed during admission')
        source = os.read(source_fd, 14725)
        require(file_identity(os.fstat(source_fd)) == file_identity(source_before) and
                file_identity(PROVIDERS.lstat()) == file_identity(source_before),
                'provider ledger changed during bounded read')
    finally:
        os.close(source_fd)
    require(len(source) == 14724 and hashlib.sha256(source).hexdigest() == PROVIDERS_SHA,
            'fixed original host-provider source ledger changed')
    ledger = json.loads(source)
    require(ledger['schema'] == 'uwvm-env-python-host-provider-static-r2',
            'fixed source-only host-provider ledger required')
    candidates = ledger['static_provider_candidate_files']
    require(isinstance(candidates, list) and len(candidates) == 21,
            'exact original 21 candidate host providers required')
    rows = []
    for pin in candidates:
        alias = Path(pin['path'])
        path = alias.resolve(strict=True)
        require(str(path) == pin['resolved'], 'source-time provider alias resolved differently')
        initial = path.lstat()
        require(stat.S_ISREG(initial.st_mode) and initial.st_uid == pin['uid'] and
                1 <= initial.st_size == pin['bytes'] <= 32 << 20,
                'fixed regular provider extent/UID required')
        fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK)
        try:
            require(file_identity(os.fstat(fd)) == file_identity(initial),
                    'actual provider descriptor changed during admission')
            digest = hashlib.sha256()
            count = 0
            while count < initial.st_size:
                chunk = os.read(fd, min(1 << 20, initial.st_size - count))
                require(chunk, 'actual provider ended early')
                count += len(chunk)
                digest.update(chunk)
            require(os.read(fd, 1) == b'' and digest.hexdigest() == pin['sha256'],
                    'source-time provider SHA or extent changed')
            require(file_identity(os.fstat(fd)) == file_identity(initial) and
                    file_identity(path.lstat()) == file_identity(initial) and
                    alias.resolve(strict=True) == path,
                    'actual provider identity changed during snapshot')
        finally:
            os.close(fd)
        # Historical device/inode values in the source ledger are not runtime
        # authority. These actual current-run values are compared before/after.
        rows.append({'alias': str(alias), 'resolved': str(path),
                     'identity': file_identity(initial), 'sha256': pin['sha256']})
    return rows


def main():
    require(len(sys.argv) == 1, 'no caller-supplied paths, commands, limits or image selection')
    admission()
    started = time.monotonic()
    provider_before = host_provider_snapshot()
    # All originals remain untouched. O_EXCL is used only for our new record.
    rows = [hash_original(*row) for row in FILES]
    for row in rows:
        require(file_identity(Path(row['path']).lstat()) == row['identity'],
                'original input changed between finite hash stages')
    require(host_provider_snapshot() == provider_before,
            'actual current-run host provider changed across image hashing')
    os.umask(0o077)
    OUT.mkdir(mode=0o700)
    value = {'schema': 'uwvm-windows-original-readonly-base-hash-actual-v1',
             'boot_id': BOOT, 'cgroup': CG_NAME, 'uid': 1000,
             'base_and_firmware_full_hashes': rows,
             'actual_host_provider_candidate_files_before_after': provider_before,
             'Python_helper_loaded_maps_qualified': False,
             'elapsed_hash_seconds': time.monotonic() - started,
             'actual_QEMU_or_guest_executed': False,
             'actual_Windows_boot_qualified': False,
             'whole_ROS_bundled_product_qualified': False,
             'original_guardian_retirement_is_separate': True}
    raw = (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()
    require(len(raw) <= 64 << 10, 'finite original metadata record exceeded')
    record = OUT / 'base-inputs-actual.json'
    with record.open('xb') as stream:
        stream.write(raw)
    print(json.dumps({'actual_readonly_base_input_record': str(record),
                      'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
                      'actual_QEMU_or_guest_executed': False}, sort_keys=True))


if __name__ == '__main__':
    main()
