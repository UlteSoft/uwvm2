#!/usr/bin/env python3
"""Materialize only the pinned official clean FreeBSD QCOW2 in the shared CG.

Source-only until the finite 3 GiB per-file guardian stage is reviewed. No
qemu-img, guest, compiler, shell, mount or original-disk mutation occurs here.
"""
import argparse
import hashlib
import json
import lzma
import os
from pathlib import Path
import stat
import struct
import sys

BASE = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
ARCHIVE = Path('/home/macromodel/Documents/qemu/uwvm2-freebsd151-amd64/FreeBSD-15.1-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz')
ARCHIVE_BYTES = 664729340
ARCHIVE_SHA = 'e4ca4db889f8559c9b9dfcacc70405c038476f4b6d41649b152d3809a2ed9e1f'
QCOW_BYTES = 2671443968
FILE_CAP = 3 << 30
RESERVE = 8 << 30
FOLLOWUP_BUDGET = 164624792 + (1 << 30) + (512 << 20) + (16 << 20) + (64 << 20)
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG_NAMESPACE = (4, 4026533623)
HOST_CG_NAMESPACE = (4, 4026531835)
CG = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def identity(path):
    st = path.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_uid != 1000 or st.st_nlink != 1:
        raise RuntimeError('a single-link owned regular provider is required')
    return {'device': st.st_dev, 'inode': st.st_ino, 'bytes': st.st_size,
            'mtime_ns': st.st_mtime_ns, 'ctime_ns': st.st_ctime_ns,
            'mode': stat.S_IMODE(st.st_mode), 'uid': st.st_uid, 'links': st.st_nlink}


def free_bytes(path):
    st = os.statvfs(path)
    return st.f_bavail * st.f_frsize


def admission(output):
    if sys.platform != 'linux' or os.getuid() != 1000 or os.geteuid() != 1000:
        raise RuntimeError('only the original unprivileged Linux guardian may materialize this image')
    if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != BOOT:
        raise RuntimeError('original host boot changed')
    ns = Path('/proc/self/ns/cgroup').stat()
    namespace = (ns.st_dev, ns.st_ino)
    member = Path('/proc/self/cgroup').read_text().strip()
    if namespace == HOST_CG_NAMESPACE and member == '0::' + CG:
        cg = Path('/sys/fs/cgroup') / CG[1:]
    elif namespace == CG_NAMESPACE and member == '0::/':
        cg = Path('/sys/fs/cgroup')
    else:
        raise RuntimeError('outside the exact original CG membership/namespace')
    for name, expected in {'memory.max': '68719476736', 'memory.swap.max': '0',
                           'cpuset.cpus.effective': '0,2,4,6,16-31'}.items():
        if (cg / name).read_text().strip() != expected:
            raise RuntimeError('original CG budget changed')
    if not os.sched_getaffinity(0) <= set(range(16, 32)):
        raise RuntimeError('image preparation is restricted to E16-31')
    if output.parent.resolve(strict=True) != BASE / 'qemu-platform-tests':
        raise RuntimeError('unexpected private clean-image parent')
    if output.exists() or output.is_symlink():
        raise RuntimeError('clean-image output must be fresh')
    if free_bytes(output.parent) < RESERVE + QCOW_BYTES + FOLLOWUP_BUDGET:
        raise RuntimeError('8 GiB reserve plus clean-image/SDK/overlay/ISO/evidence budgets required')
    import resource
    soft, hard = resource.getrlimit(resource.RLIMIT_FSIZE)
    if soft != FILE_CAP or hard != FILE_CAP:
        raise RuntimeError('the reviewed fixed 3 GiB per-file stage is required')
    return {'boot': BOOT, 'cgroup_namespace': list(namespace), 'uid': 1000,
            'kernel_memory_max_bytes': 64 << 30, 'kernel_swap_max_bytes': 0,
            'free_before_bytes': free_bytes(output.parent), 'disk_reserve_bytes': RESERVE,
            'clean_image_budget_bytes': QCOW_BYTES, 'followup_budget_bytes': FOLLOWUP_BUDGET,
            'per_file_limit_bytes': FILE_CAP, 'e_cpu_pool': sorted(os.sched_getaffinity(0))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.absolute()
    admitted = admission(output)
    original = identity(ARCHIVE)
    if original['bytes'] != ARCHIVE_BYTES or sha(ARCHIVE) != ARCHIVE_SHA:
        raise RuntimeError('original archive does not match the official signed-checksum payload')
    if identity(ARCHIVE) != original:
        raise RuntimeError('official archive changed while hashing')
    output.mkdir(mode=0o700)
    image = output / 'FreeBSD-15.1-clean.qcow2'
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_XZ, memlimit=256 << 20)
    digest = hashlib.sha256()
    total = 0
    with ARCHIVE.open('rb') as source, image.open('xb') as target:
        while not decoder.eof:
            compressed = source.read(1 << 20) if decoder.needs_input else b''
            if decoder.needs_input and not compressed:
                raise RuntimeError('truncated official XZ stream')
            decoded = decoder.decompress(compressed, max_length=1 << 20)
            total += len(decoded)
            if total > QCOW_BYTES:
                raise RuntimeError('official QCOW2 exceeds its fixed decompressed byte count')
            digest.update(decoded)
            target.write(decoded)
            if free_bytes(output) < RESERVE:
                raise RuntimeError('the fixed 8 GiB disk reserve was reached')
        if decoder.unused_data or source.read(1):
            raise RuntimeError('unexpected data after the pinned official XZ stream')
    if total != QCOW_BYTES or image.stat().st_size != QCOW_BYTES:
        raise RuntimeError('official QCOW2 decompressed size differs from its CRC-checked XZ index')
    if identity(ARCHIVE) != original or sha(ARCHIVE) != ARCHIVE_SHA:
        raise RuntimeError('original readonly compressed provider changed during materialization')
    with image.open('rb') as stream:
        header = stream.read(104)
    if len(header) != 104 or header[:4] != b'QFI\xfb':
        raise RuntimeError('materialized provider is not an actual QCOW2')
    version, backing_offset, backing_size, cluster_bits, virtual_bytes = struct.unpack_from('>IQIIQ', header, 4)
    if version not in (2, 3) or backing_offset or backing_size or not 9 <= cluster_bits <= 21:
        raise RuntimeError('official clean image must have no implicit backing file')
    if not 64 << 20 <= virtual_bytes <= 8 << 30:
        raise RuntimeError('official clean-image virtual size is outside the fixed cold VM budget')
    image.chmod(0o444)
    result = {'schema': 'uwvm-freebsd151-clean-image-actual-v1', 'admission': admitted,
              'original_archive': {'path': str(ARCHIVE), **original, 'sha256': ARCHIVE_SHA},
              'clean_image': {'path': str(image), **identity(image), 'sha256': digest.hexdigest(),
                              'qcow2_version': version, 'qcow2_virtual_bytes': virtual_bytes,
                              'backing_file_offset': backing_offset, 'backing_file_bytes': backing_size},
              'original_7GB_customized_guest_disk_used': False,
              'PGP_signature_cryptographically_verified': False,
              'actual_guest_executed': False, 'actual_platform_qualified': False,
              'ROS_LLVM23_product_qualified': False}
    data = (json.dumps(result, indent=2, sort_keys=True) + '\n').encode()
    with (output / 'clean-image-actual.json').open('xb') as out:
        out.write(data)
    print(json.dumps({'clean_image_bytes': total, 'clean_image_sha256': digest.hexdigest(),
                      'provider_manifest_sha256': hashlib.sha256(data).hexdigest(),
                      'actual_guest_executed': False}))


if __name__ == '__main__':
    main()
