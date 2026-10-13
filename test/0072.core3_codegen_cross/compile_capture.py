#!/usr/bin/env python3
"""Build an inspection-only capture product from a qualified source cut.

The existing compile-time hook exports the actual MCJIT object into a fresh
exclusive file. Persistent cache policy remains unchanged. This product grants
no ordinary-product or guest semantic qualification by compilation alone.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import resource
from run_matrix import digest, execute, save, verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    a = parser.parse_args()
    config_hash = digest(a.config)
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    resource.setrlimit(resource.RLIMIT_AS, (12 << 30, 12 << 30))
    verify(config['pins'])
    out = Path(config['output'])
    out.mkdir(parents=True, exist_ok=False)
    builds = []
    for label, argv in config['build_commands']:
        assert '-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT' in argv
        row = execute(argv, out / (label + '.log'), out, 900)
        builds.append(row)
        save(out / 'build-checkpoint.json', dict(rows=builds))
        assert row['exit'] == 0, row
    deps = {}
    for path in out.glob('*.d'):
        text = path.read_text().replace('\\\n', ' ')
        for name in shlex.split(text.split(':', 1)[1]):
            deps[name] = digest(name)
    assert deps
    verify(config['pins'])
    verify(deps)
    assert digest(a.config) == config_hash
    product = out / 'uwvm'
    assert product.read_bytes()[:4] == b'\x7fELF'
    save(out / 'qualification.json', dict(test_capture_macro_qualified=True,
        product=str(product), product_sha256=digest(product),
        source_identity=config['source_identity'], arch=config['arch'], repo=config['repo'],
        macro='UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', build_commands=builds,
        configuration_sha256=config_hash, actual_dependency_pins=deps,
        pins=config['pins'], guest_semantics_qualified=False,
        scope='Inspection-only test-macro build; actual object execution still required'))
    # The linked capture product, actual dependency closure and build records
    # are retained. Only this build's unshared new intermediate is retired.
    retired = []
    for path in out.glob('*.o'):
        info = path.lstat()
        assert path.is_file() and not path.is_symlink() and info.st_nlink == 1
        retired.append(dict(path=str(path), bytes=info.st_size, sha256=digest(path)))
        path.unlink()
    save(out / 'retired-intermediates.json', retired)
    print('PASS inspection-only capture build', config['arch'], config['repo'], flush=True)


if __name__ == '__main__':
    main()
