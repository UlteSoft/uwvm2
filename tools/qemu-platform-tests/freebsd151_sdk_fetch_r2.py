#!/usr/bin/env python3
"""Fetch the exact official FreeBSD 15.1 SDK subset, only inside the shared CG.

This fixed data helper executes no compiler, target program, shell or guest.
It must itself run as an owned command of the original PIDFD supervisor.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys
import tarfile
import urllib.request

BASE = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'
CG_NAMESPACE = (4, 4026533623)
HOST_CG_NAMESPACE = (4, 4026531835)
MANIFEST_SHA = '70ba0347054099662f22434797f1f33be289fc868ba58d6069a490b3e395d684'
URL = 'https://download.freebsd.org/releases/amd64/15.1-RELEASE/base.txz'
ARCHIVE_SHA = '3768988b151c20f965679062b065c63a977d6bbb9f47fd83695ec2c40790c18f'
ARCHIVE_BYTES = 164624792
RESERVE = 8 << 30
SDK_LIMIT = 1 << 30
RECORD_LIMIT = 16 << 20
PREFIXES = ('usr/include/', 'usr/lib/', 'lib/', 'libexec/')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def admit(output):
    if sys.platform != 'linux' or os.getuid() != 1000 or os.geteuid() != 1000:
        raise RuntimeError('only the original unprivileged Linux CG command may prepare this SDK')
    if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != BOOT:
        raise RuntimeError('the pinned host boot changed')
    # The cold guardian moves a stopped host process into the original
    # cgroup without setns/docker-exec. Accept only its exact host namespace
    # and actual full CG path, or the original init's private namespace with
    # namespace-root membership. Both select the same kernel budget.
    ns = Path('/proc/self/ns/cgroup').stat()
    namespace = (ns.st_dev, ns.st_ino)
    member = Path('/proc/self/cgroup').read_text().strip()
    if namespace == HOST_CG_NAMESPACE and member == '0::' + CG:
        cg = Path('/sys/fs/cgroup') / CG[1:]
    elif namespace == CG_NAMESPACE and member == '0::/':
        cg = Path('/sys/fs/cgroup')
    else:
        raise RuntimeError('SDK command is outside the exact original CG membership/namespace')
    expected = {'memory.max': '68719476736', 'memory.swap.max': '0',
                'cpuset.cpus.effective': '0,2,4,6,16-31'}
    for name, value in expected.items():
        if (cg / name).read_text().strip() != value:
            raise RuntimeError('original CG budget changed')
    if not os.sched_getaffinity(0) <= set(range(16, 32)):
        raise RuntimeError('SDK preparation is restricted to E16-31')
    if output.parent.resolve(strict=True) != BASE / 'qemu-platform-tests':
        raise RuntimeError('unexpected fixed private SDK parent')
    if output.exists() or output.is_symlink():
        raise RuntimeError('fresh private SDK directory required')
    free = os.statvfs(output.parent)
    free_bytes = free.f_bavail * free.f_frsize
    budget = ARCHIVE_BYTES + SDK_LIMIT + RECORD_LIMIT
    if free_bytes < RESERVE + budget:
        raise RuntimeError('SDK preparation requires 8 GiB free reserve plus its complete byte budget')
    return {'boot': BOOT, 'cgroup': CG, 'cgroup_namespace': list(namespace), 'uid': os.getuid(), 'e_cpu_pool': sorted(os.sched_getaffinity(0)),
            'free_before_bytes': free_bytes, 'reserve_bytes': RESERVE,
            'archive_budget_bytes': ARCHIVE_BYTES, 'sdk_budget_bytes': SDK_LIMIT,
            'record_budget_bytes': RECORD_LIMIT, 'kernel_memory_max_bytes': 64 << 30,
            'kernel_swap_max_bytes': 0}


def free_reserve(path):
    st = os.statvfs(path)
    if st.f_bavail * st.f_frsize < RESERVE:
        raise RuntimeError('the 8 GiB free disk reserve was reached')


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise RuntimeError('the pinned official SDK URL must not redirect')


def canonical(name):
    # Normalizing only leading './' preserves '..' as an explicit error.
    while name.startswith('./'):
        name = name[2:]
    parts = PurePosixPath(name).parts
    if (not parts or name.startswith('/') or any(part in ('..', '') for part in parts)
            or '\\' in name or '\x00' in name):
        raise RuntimeError('unsafe archive path: ' + repr(name))
    return '/'.join(parts)


def selected(name):
    return any(name.startswith(prefix) or name == prefix[:-1] for prefix in PREFIXES)


def real_parents(root, name):
    target = root / name
    for parent in reversed(target.parents):
        if parent == root.parent:
            continue
        if parent != root and root not in parent.parents:
            continue
        if parent.is_symlink():
            raise RuntimeError('archive parent must never follow a symbolic link')
        parent.mkdir(mode=0o755, exist_ok=True)
        if not parent.is_dir():
            raise RuntimeError('archive parent is not an owned directory')
    return target


def resolve_link(name, link, symbolic):
    if '\\' in link or '\x00' in link or not link:
        raise RuntimeError('unsafe SDK link')
    # Upstream root-relative absolute links are rewritten to remain inside
    # this private sysroot. The original string is retained in the manifest.
    parts = [] if link.startswith('/') or not symbolic else list(PurePosixPath(name).parent.parts)
    for part in PurePosixPath(link).parts:
        if part in ('/', '.'):
            continue
        if part == '..':
            if not parts:
                raise RuntimeError('SDK link escapes its private sysroot')
            parts.pop()
        else:
            parts.append(part)
    target = '/'.join(parts)
    if not selected(target):
        raise RuntimeError('SDK link points outside the selected provider subset')
    return target


def extract(archive, root):
    root.mkdir(mode=0o700)
    rows, links, seen = [], [], set()
    regular_bytes = 0
    all_members = 0
    with tarfile.open(archive, 'r|xz') as tf:
        for member in tf:
            all_members += 1
            if all_members > 40000:
                raise RuntimeError('official SDK archive member limit exceeded')
            if member.name in ('.', './'):
                continue
            name = canonical(member.name)
            if not selected(name):
                continue
            if name in seen:
                raise RuntimeError('duplicate selected SDK archive path')
            seen.add(name)
            target = real_parents(root, name)
            if member.isdir():
                target.mkdir(mode=0o755, exist_ok=True)
                continue
            if member.isreg():
                if not 0 <= member.size <= 256 << 20 or regular_bytes + member.size > SDK_LIMIT:
                    raise RuntimeError('selected SDK data exceeds its fixed extraction budget')
                sha = hashlib.sha256()
                copied = 0
                stream = tf.extractfile(member)
                if stream is None:
                    raise RuntimeError('selected regular SDK member has no data')
                with stream, target.open('xb') as out:
                    while chunk := stream.read(min(1 << 20, member.size - copied + 1)):
                        copied += len(chunk)
                        if copied > member.size:
                            raise RuntimeError('SDK member exceeds its declared size')
                        sha.update(chunk)
                        out.write(chunk)
                if copied != member.size:
                    raise RuntimeError('truncated official SDK member')
                target.chmod(0o644)
                regular_bytes += copied
                rows.append({'path': name, 'kind': 'regular', 'bytes': copied, 'sha256': sha.hexdigest()})
                if len(rows) % 128 == 0:
                    free_reserve(root)
            elif member.issym() or member.islnk():
                links.append((name, resolve_link(name, member.linkname, member.issym()),
                              member.linkname, member.issym()))
            else:
                raise RuntimeError('unexpected special file in the selected SDK subset')
    # Create links only after regular files: extracting through an upstream
    # symbolic parent cannot redirect a later file onto the host filesystem.
    pending = links
    while pending:
        next_pending = []
        progress = False
        for name, linked_name, original, symbolic in pending:
            target = real_parents(root, name)
            linked = root / linked_name
            if not linked.exists():
                next_pending.append((name, linked_name, original, symbolic))
                continue
            resolved = linked.resolve(strict=True)
            if root not in resolved.parents or not (resolved.is_file() or (symbolic and resolved.is_dir())):
                raise RuntimeError('SDK link must resolve inside the selected private providers')
            if symbolic:
                target.symlink_to(os.path.relpath(linked, target.parent))
            else:
                os.link(resolved, target, follow_symlinks=False)
            rows.append({'path': name, 'kind': 'symlink' if symbolic else 'hardlink',
                         'upstream_link': original, 'private_target': linked_name,
                         'resolved_private_path': str(resolved.relative_to(root)),
                         'bytes': resolved.stat().st_size if resolved.is_file() else 0,
                         'sha256': digest(resolved) if resolved.is_file() else None})
            progress = True
        if not progress and next_pending:
            raise RuntimeError('selected SDK contains unresolved or cyclic provider links')
        pending = next_pending
    for name in ('usr/include/sys/param.h', 'usr/include/sys/stat.h',
                 'usr/include/c++/v1/__config', 'usr/lib/crt1.o', 'usr/lib/libc++.so',
                 'lib/libc.so.7', 'libexec/ld-elf.so.1'):
        path = root / name
        if not path.is_file():
            raise RuntimeError('official SDK is missing a required actual provider: ' + name)
    free_reserve(root)
    return {'archive_members': all_members, 'selected_regular_bytes': regular_bytes,
            'selected_files': sorted(rows, key=lambda row: row['path'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.absolute()
    admission = admit(output)
    if digest(args.manifest) != MANIFEST_SHA:
        raise RuntimeError('the already pinned official release MANIFEST changed')
    fields = [line.split() for line in args.manifest.read_text().splitlines()]
    matched = [row for row in fields if row and row[0] == 'base.txz']
    if len(matched) != 1 or matched[0][1] != ARCHIVE_SHA:
        raise RuntimeError('the official MANIFEST does not bind this exact base.txz')
    output.mkdir(mode=0o700)
    archive = output / 'base.txz'
    sha = hashlib.sha256()
    received = 0
    opener = urllib.request.build_opener(NoRedirect)
    with opener.open(URL, timeout=30) as response, archive.open('xb') as out:
        if response.geturl() != URL or response.status != 200:
            raise RuntimeError('unexpected official SDK response')
        length = response.headers.get('Content-Length')
        if length is not None and int(length) != ARCHIVE_BYTES:
            raise RuntimeError('official SDK length differs from the fixed download budget')
        while chunk := response.read(1 << 20):
            received += len(chunk)
            if received > ARCHIVE_BYTES:
                raise RuntimeError('official SDK exceeds its fixed download budget')
            sha.update(chunk)
            out.write(chunk)
            free_reserve(output)
    if received != ARCHIVE_BYTES or sha.hexdigest() != ARCHIVE_SHA:
        raise RuntimeError('official SDK byte count or SHA256 did not match')
    result = {'schema': 'uwvm-freebsd151-sdk-provider-actual-v1', 'admission': admission,
              'official_url': URL, 'official_manifest_sha256': MANIFEST_SHA,
              'archive_bytes': received, 'archive_sha256': sha.hexdigest(),
              'native_compilation_qualified': False, 'actual_guest_qualified': False,
              'ROS_paired_LLVM23_product_qualified': False,
              **extract(archive, output / 'sysroot')}
    data = (json.dumps(result, indent=2, sort_keys=True) + '\n').encode()
    if len(data) > RECORD_LIMIT:
        raise RuntimeError('SDK provenance record exceeds its budget')
    with (output / 'provider-actual.json').open('xb') as out:
        out.write(data)
    print(json.dumps({'archive_sha256': ARCHIVE_SHA, 'selected_regular_bytes': result['selected_regular_bytes'],
                      'provider_manifest_sha256': hashlib.sha256(data).hexdigest(),
                      'native_compilation_qualified': False, 'actual_guest_qualified': False}))


if __name__ == '__main__':
    main()
