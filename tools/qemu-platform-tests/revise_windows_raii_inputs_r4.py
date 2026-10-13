#!/usr/bin/env python3
"""Freeze the four root-approved fast_io shared-memory RAII leaves over R2.

All other 1514 immutable header/backend/fixture inputs are retained. Run no
native tool, compiler, module, product or guest. The preserved R3 noEH failure
remains an independent input/output record.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

BASE_ARCHIVE = 'bfb2bf71b8097d0f0d706b98734310b44ffd54594479cd12cb4944bc24905f03'
BASE_MANIFEST = 'dc02a40b6aef585ee6a472064bf58c91e59626e7f4e91ab3008c3a30a110814d'
DELTA_ARCHIVE = '22d29e1438459cf271bd2d8e8efcc87719bcabe1c5da8d3dd0d5b2d6110a5944'
DELTA_MANIFEST = '0efdc51a273b4960c94edff61eedb45913c04f354a5b00014caafa9dfc116868'
PREFIX = 'uwvm-fastio-shared-memory-noeh-raii-source-applied-20261003-r1/'
LEAVES = {
    'win32.h': ('d4dc83ce6fc8875b1f1f8e3f101a1552d541574631652be0ca127d6ba00fbb2c',
                '5f04d6c90eded1ad742c0c1753a5d4ec737d0c0f17a8c510c73c90d99f62c977'),
    'nt.h': ('489e3e57e8c6d6d619ec387fe397ac32843dc6352d07bd42b16ac1d7b4314ffc',
             'bc9ba831eedc888085b3a2fb15e0daffdacb8b494279795e3e70a288109018eb'),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--delta', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    archive = (args.base / 'source.tar.gz').read_bytes()
    manifest = (args.base / 'manifest.json').read_bytes()
    if sha(archive) != BASE_ARCHIVE or sha(manifest) != BASE_MANIFEST:
        raise RuntimeError('preserved Windows R2 immutable closure changed')
    original = json.loads(manifest)
    wanted = {row['path']: row for row in original['files']}
    if len(wanted) != 1514:
        raise RuntimeError('unexpected base source inventory')
    records = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as stream:
        for member in stream:
            if not member.isfile() or member.name not in wanted or member.name in records:
                raise RuntimeError('invalid base immutable source member')
            data = stream.extractfile(member).read()
            if len(data) != wanted[member.name]['bytes'] or sha(data) != wanted[member.name]['sha256']:
                raise RuntimeError('immutable base source leaf changed')
            records[member.name] = data
    if records.keys() != wanted.keys():
        raise RuntimeError('incomplete immutable source closure')
    delta = args.delta.read_bytes()
    if sha(delta) != DELTA_ARCHIVE:
        raise RuntimeError('approved applied RAII delta archive changed')
    with tarfile.open(fileobj=io.BytesIO(delta), mode='r:gz') as stream:
        dm = stream.extractfile(PREFIX + 'source-manifest.json').read()
        if sha(dm) != DELTA_MANIFEST:
            raise RuntimeError('applied owner manifest changed')
        owner = json.loads(dm)
        if owner['live_vendor_applied'] is not True or owner['native_tests_run'] is not False:
            raise RuntimeError('unexpected owner source qualification')
        for repo in ('uwvm2', 'uwvm2-ros'):
            for leaf, (before, after) in LEAVES.items():
                path = 'third-parties/fast_io/include/fast_io_hosted/process/shared_memory/' + leaf
                rows = [row for row in owner['files'] if row['product'] == repo and row['path'] == path]
                name = repo + '/' + path
                if len(rows) != 1 or rows[0]['before_sha256'] != before or rows[0]['after_sha256'] != after or sha(records[name]) != before:
                    raise RuntimeError('unexpected exact RAII source transformation')
                saved_before = stream.extractfile(PREFIX + 'before/' + name).read()
                repaired = stream.extractfile(PREFIX + 'after/' + name).read()
                if saved_before != records[name] or len(repaired) != rows[0]['after_size'] or sha(repaired) != after:
                    raise RuntimeError('applied RAII leaf bytes changed')
                records[name] = repaired
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    updated = {**original, 'source_revision': 'r4-exact-shared-memory-RAII-header-repair',
               'base_manifest_sha256': BASE_MANIFEST, 'base_archive_sha256': BASE_ARCHIVE,
               'RAII_delta_archive_sha256': DELTA_ARCHIVE, 'RAII_delta_manifest_sha256': DELTA_MANIFEST,
               'native_tests_executed': False, 'actual_GNU_unwind_runtime_replaced': False,
               'new_readonly_sync_provider_qualified': False,
               'files': [{'path': name, 'bytes': len(data), 'sha256': sha(data)}
                         for name, data in sorted(records.items())]}
    data = (json.dumps(updated, indent=2, sort_keys=True) + '\n').encode()
    (output / 'manifest.json').write_bytes(data)
    with tarfile.open(output / 'source.tar.gz', 'w:gz') as stream:
        for name, content in sorted(records.items()):
            member = tarfile.TarInfo(name)
            member.size, member.mode = len(content), 0o600
            stream.addfile(member, io.BytesIO(content))
    print(json.dumps({'files': len(records), 'manifest_sha256': sha(data),
                      'source_archive_sha256': sha((output / 'source.tar.gz').read_bytes()),
                      'source_archive_bytes': (output / 'source.tar.gz').stat().st_size}))


if __name__ == '__main__':
    main()
