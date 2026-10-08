#!/usr/bin/env python3
"""Run a pinned real CLI in fresh processes; require signed persistent-cache hits."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import subprocess
import time


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(pins):
    for path, expected in pins.items():
        assert digest(path) == expected, ('input changed', path)


def save(path, value):
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temp.replace(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    a = p.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    verify(config['pins'])
    inputs = json.loads(Path(config['build_inputs']).read_text())
    verify(inputs['pins'])
    plan = json.loads(Path(config['build_plan']).read_text())
    receipts = []
    product = Path(config['product'])
    for index in config['build_indices']:
        label, argv = plan[index]
        path = Path(config['build_evidence']) / f'{index:03d}-{label}' / 'receipts.json'
        rows = json.loads(path.read_text())
        assert len(rows) == 1 and rows[0]['passed'] and rows[0]['returncode'] == 0
        assert rows[0]['argv'] == argv
        receipts.append({'path': str(path), 'sha256': digest(path), 'label': label})
    dependencies = {}
    for file in product.parent.glob('*.d'):
        paths = shlex.split(file.read_text().replace('\\\n', ' ').split(':', 1)[1])
        for path in paths:
            dependencies[path] = digest(path)
            if path in inputs['pins']:
                assert dependencies[path] == inputs['pins'][path], path
    assert dependencies
    pinned_product = {str(product): digest(product)}
    out = Path(config['output'])
    out.mkdir(exist_ok=False)
    manifest = json.loads(Path(config['fixtures']).read_text())
    selected = set(config['cases'])
    fixtures = [f for f in manifest['fixtures'] if f['name'] in selected]
    assert {f['name'] for f in fixtures} == selected
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for profile in config['profiles']:
        for fixture in fixtures:
            for path_mode in ('default', 'explicit'):
                name = profile['name'] + '--' + fixture['name'] + '--' + path_mode
                work = out / name
                work.mkdir()
                env = dict(os.environ)
                env.pop('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                prefix = list(config.get('prefix', []))
                guest_root = None
                if prefix:
                    # QEMU can resolve root-based directory FDs under -L,
                    # including cache creation. Keep those writes outside the
                    # qualified loader/sysroot, and inspect their physical path.
                    loader_root = Path(prefix[prefix.index('-L') + 1])
                    guest_root = work / 'guest-root'
                    guest_root.mkdir()
                    for directory in ('lib', 'lib64', 'usr'):
                        target = loader_root / directory
                        if target.exists():
                            (guest_root / directory).symlink_to(target, target_is_directory=True)
                    prefix[prefix.index('-L') + 1] = str(guest_root)
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-E', 'XDG_CACHE_HOME=' + env['XDG_CACHE_HOME']]
                namespace = config.get('cache_product', 'uwvm2')
                assert namespace in ('uwvm2', 'uwvm2ros')
                cache = work / ('xdg/' + namespace + '/llvm-jit' if path_mode == 'default' else 'cache')
                cache_flags = [] if path_mode == 'default' else [
                    '-Rllvm-cache-path', 'path', str(cache)]
                before = None
                for phase in ('cold', 'warm', 'warm-again'):
                    compiler_log = work / (phase + '.compiler.log')
                    run_log = work / (phase + '.run.log')
                    argv = [*prefix, str(product), *profile['argv'], '-Rct', '0',
                            *cache_flags, '-Rclog', 'file', str(compiler_log),
                            *fixture['argv'], '--run', fixture['path']]
                    started = time.monotonic()
                    with run_log.open('xb') as stream:
                        completed = subprocess.run(argv, cwd=work, env=env, stdout=stream,
                                                   stderr=subprocess.STDOUT,
                                                   timeout=config.get('timeout', 120))
                    actual_compiler_log = compiler_log
                    if not actual_compiler_log.exists() and guest_root is not None:
                        actual_compiler_log = guest_root / str(compiler_log).lstrip('/')
                    text = ansi.sub('', actual_compiler_log.read_text() if actual_compiler_log.exists() else '')
                    hit_lines = [line for line in text.splitlines() if 'object-cache-hit module=' in line]
                    physical_roots = [cache]
                    if guest_root is not None:
                        physical_roots.append(guest_root / str(cache).lstrip('/'))
                    entries = {}
                    for physical in physical_roots:
                        for f in physical.rglob('*.uwvm-ljc'):
                            key = str(f.relative_to(physical))
                            assert key not in entries, ('duplicate physical cache', key)
                            entries[key] = digest(f)
                    row = dict(profile=profile['name'], fixture=fixture['name'],
                               path_mode=path_mode, phase=phase, argv=argv,
                               exit=completed.returncode, seconds=time.monotonic() - started,
                               compiler_log_sha256=digest(actual_compiler_log) if actual_compiler_log.exists() else None,
                               physical_compiler_log=str(actual_compiler_log),
                               run_log_sha256=digest(run_log), cache_entries=entries,
                               physical_cache_roots=[str(p) for p in physical_roots],
                               signed_cache_hits=len(hit_lines))
                    rows.append(row)
                    save(out / 'checkpoint.json', dict(state='running', rows=rows))
                    assert completed.returncode == 0, (name, phase, run_log.read_text()[-3000:])
                    assert entries, (name, phase, text[-4000:])
                    if phase == 'cold':
                        assert not hit_lines, (name, text)
                        assert re.search(r'object-cache-store-complete .* status=ok\b', text), (name, text[-4000:])
                        before = entries
                    else:
                        assert hit_lines and all('signature_verified=1' in line for line in hit_lines), (name, text[-4000:])
                        assert entries == before, (name, 'cache objects changed on hit')
                    row['passed'] = True
                    save(out / 'checkpoint.json', dict(state='running', rows=rows))
                print('PASS signed cold/warm/warm-again', name, flush=True)
    verify(config['pins'])
    verify(inputs['pins'])
    verify(dependencies)
    verify(pinned_product)
    verify(fixture_pins)
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(out / 'summary.json', dict(passed=True, rows=rows, completed=len(rows),
         product_sha256=pinned_product[str(product)], source_identities=inputs['source_identities'],
         build_receipts=receipts, actual_dependency_pins=dependencies,
         configuration_sha256=digest(a.config), code_sha256=digest(__file__),
         scope='Real CLI in fresh processes, signed cold/store and repeated cache hits, default and explicit paths; only the listed target/profiles/features'))
    print('PASS real signed persistent cache', len(rows), flush=True)


if __name__ == '__main__':
    main()
