#!/usr/bin/env python3
"""Copy the exact protected SDK dependencies for a capability-free target.

No source SDK permissions or binary bytes are changed. The copies are owned by
the profiling uid, and their hashes must match the measured environment.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from build_products import admission, digest


def profile_environment(arena, folder=None):
    folder = Path(folder) if folder is not None else Path(arena) / 'profile-libraries'
    receipt = folder / 'receipt.json'
    data = json.loads(receipt.read_text())
    assert data['passed'] and data['libraries']
    for row in data['libraries']:
        assert digest(row['copy']) == row['sha256']
    for row in [*data['binaries'].values(), *data['wasms'].values()]:
        assert digest(row['public']) == row['sha256']
    return dict(os.environ, LD_LIBRARY_PATH=str(Path(data['public_root']) / 'lib')), {
        'receipt': str(receipt), 'receipt_sha256': digest(receipt), 'libraries': data['libraries'],
        'binaries': data['binaries'], 'wasms': data['wasms'], 'public_root': data['public_root']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--environment', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--public-root', type=Path, default=Path('/var/tmp/uwvm-hll-profile-20261017'))
    p.add_argument('--receipt-root', type=Path,
                   help='new private library receipt directory; preserves previous cohorts')
    p.add_argument('--products-root', type=Path,
                   help='explicit immutable UWVM product directory; defaults to arena/products')
    a = p.parse_args()
    products_root = a.products_root if a.products_root is not None else a.arena / 'products'
    before = admission()
    environment = json.loads(a.environment.read_text())
    root = a.receipt_root if a.receipt_root is not None else a.arena / 'profile-libraries'
    root.mkdir(exist_ok=False)
    lib = root / 'lib'
    lib.mkdir(mode=0o755)
    a.public_root.mkdir(mode=0o755, exist_ok=False)
    (a.public_root / 'lib').mkdir(mode=0o755)
    data = {'passed': False, 'admission': before, 'environment_sha256': digest(a.environment),
            'scope': 'exact SDK library bytes copied into task-owned readable paths; original SDK untouched',
            'libraries': [], 'smoke': [], 'binaries': {}, 'wasms': {},
            'public_root': str(a.public_root), 'products_root': str(products_root), 'manifest_sha256': digest(a.manifest),
            'public_alias_method': 'hardlinks to exact task-owned bytes; no source SDK or home permissions changed'}
    sdk = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
    sources = {}
    for name in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        for path, row in environment['executables'][name]['dependencies'].items():
            source = Path(path)
            if source.is_relative_to(sdk):
                assert source.name not in sources or sources[source.name]['sha256'] == row['sha256']
                sources[source.name] = {'path': path, **row}
    assert sources and any(name.startswith('libLLVM.') for name in sources)
    try:
        for name, row in sorted(sources.items()):
            source, destination = Path(row['path']), lib / name
            assert digest(source) == row['sha256']
            shutil.copyfile(source, destination)
            destination.chmod(0o644)
            assert digest(destination) == row['sha256']
            public = a.public_root / 'lib' / name
            os.link(destination, public)
            data['libraries'].append({'source': str(source), 'resolved_source': row['resolved_path'],
                                      'private_copy': str(destination), 'copy': str(public), 'sha256': row['sha256'],
                                      'size': destination.stat().st_size})
        def alias(source, relative):
            source = Path(source)
            public = a.public_root / relative
            public.parent.mkdir(parents=True, exist_ok=True, mode=0o755)
            os.link(source, public)
            checksum = digest(source)
            assert digest(public) == checksum
            assert (source.stat().st_dev, source.stat().st_ino) == (public.stat().st_dev, public.stat().st_ino)
            return {'original': str(source), 'public': str(public), 'sha256': checksum,
                    'device': public.stat().st_dev, 'inode': public.stat().st_ino}
        for name in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            source = products_root / name / 'uwvm'
            assert digest(source) == environment['executables'][name]['sha256'], 'environment belongs to a different product'
            assert source.resolve(strict=True) == Path(environment['executables'][name]['path']).resolve(strict=True), 'environment product path differs'
            data['binaries'][name] = alias(source, Path('products') / name / 'uwvm')
        manifest = json.loads(a.manifest.read_text())
        cases = manifest if isinstance(manifest, list) else manifest['cases']
        for case in cases:
            checksum = digest(case['wasm'])
            assert checksum == case['wasm_sha256']
            if checksum not in data['wasms']:
                data['wasms'][checksum] = alias(case['wasm'], Path('wasm') / checksum / Path(case['wasm']).name)
        drop = ['/usr/bin/setpriv', '--bounding-set=-all', '--inh-caps=-all', '--ambient-caps=-all', '--no-new-privs']
        env = dict(os.environ, LD_LIBRARY_PATH=str(a.public_root / 'lib'))
        for name in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            binary = Path(data['binaries'][name]['public'])
            for action in ['ldd', 'version']:
                argv = [*drop, *(['/usr/bin/ldd', str(binary)] if action == 'ldd' else [str(binary), '--version'])]
                q = subprocess.run(argv, capture_output=True, timeout=30, env=env)
                (root / (name + '-' + action + '.stdout.log')).write_bytes(q.stdout)
                (root / (name + '-' + action + '.stderr.log')).write_bytes(q.stderr)
                data['smoke'].append({'repository': name, 'action': action, 'argv': argv, 'exit': q.returncode})
                assert q.returncode == 0, q.stderr[-4000:]
                if action == 'ldd':
                    assert b'not found' not in q.stdout
                    for leaf in sources:
                        if leaf.encode() in q.stdout:
                            assert str(a.public_root / 'lib' / leaf).encode() in q.stdout
        data['admission_after'] = admission()
        assert data['admission_after']['membership'] == before['membership']
        data['passed'] = True
    finally:
        (root / 'receipt.json').write_text(json.dumps(data, indent=2) + '\n')
    print(json.dumps({'passed': True, 'libraries': len(data['libraries']),
                      'bytes': sum(row['size'] for row in data['libraries'])}))


if __name__ == '__main__':
    main()
