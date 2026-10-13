#!/usr/bin/env python3
"""Authenticate real cross-process cache reuse of the guarded high memory64 case."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time
from run_matrix import digest, save, verify


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('config', type=Path)
    a = ap.parse_args()
    c = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    verify(c['pins'])
    qpath = Path(c['product_qualification'])
    qhash = digest(qpath)
    q = json.loads(qpath.read_text())
    assert q['passed'] and q['product_sha256'] == digest(c['product'])
    dependencies = q['actual_dependency_pins']
    verify(dependencies)
    manifest = json.loads(Path(c['fixtures']).read_text())
    verify(manifest['code_pins'])
    fixtures = [f for f in manifest['fixtures'] if f['name'] in c['cases']]
    assert {f['name'] for f in fixtures} == set(c['cases'])
    assert all(f['expected_trap'] is None for f in fixtures)
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    out = Path(c['output'])
    out.mkdir(exist_ok=False)
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for profile in c['profiles']:
        for fixture in fixtures:
            for path_mode in ['default', 'explicit']:
                work = out / (profile['name'] + '--' + fixture['name'] + '--' + path_mode)
                work.mkdir()
                env = dict(os.environ)
                env.pop('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                prefix = list(c.get('prefix', []))
                guest_root = None
                if prefix:
                    loader = Path(prefix[prefix.index('-L') + 1])
                    guest_root = work / 'guest-root'
                    guest_root.mkdir()
                    for directory in ['lib', 'lib64', 'usr']:
                        if (loader / directory).exists():
                            (guest_root / directory).symlink_to(loader / directory, target_is_directory=True)
                    prefix[prefix.index('-L') + 1] = str(guest_root)
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-E', 'XDG_CACHE_HOME=' + env['XDG_CACHE_HOME']]
                namespace = 'uwvm2ros' if c['ros'] else 'uwvm2'
                cache = work / ('xdg/' + namespace + '/llvm-jit' if path_mode == 'default' else 'cache')
                flags = [] if path_mode == 'default' else ['-Rllvm-cache-path', 'path', str(cache)]
                original = None
                for phase in ['cold', 'warm', 'warm-again']:
                    log, run = work / (phase + '.compiler.log'), work / (phase + '.run.log')
                    argv = [*prefix, c['product'], *profile['argv'], '-Rct', '0', *flags,
                            '-Rclog', 'file', str(log), *fixture['argv'], '--run', fixture['path']]
                    started = time.monotonic()
                    with run.open('xb') as stream:
                        result = subprocess.run(argv, cwd=work, env=env, stdout=stream,
                                                stderr=subprocess.STDOUT, timeout=c.get('timeout', 240))
                    physical_log = log
                    if not physical_log.exists() and guest_root is not None:
                        physical_log = guest_root / str(log).lstrip('/')
                    assert result.returncode == 0, (phase, run.read_text(errors='replace')[-4000:])
                    text = ansi.sub('', physical_log.read_text())
                    hits = [line for line in text.splitlines() if 'object-cache-hit module=' in line]
                    roots = [cache] + ([guest_root / str(cache).lstrip('/')] if guest_root is not None else [])
                    entries = {}
                    for root in roots:
                        for p in root.rglob('*.uwvm-ljc'):
                            key = str(p.relative_to(root))
                            assert key not in entries
                            entries[key] = digest(p)
                    assert entries, (phase, text[-4000:])
                    if phase == 'cold':
                        assert not hits and re.search(r'object-cache-store-complete .* status=ok\b', text)
                        original = entries
                    else:
                        assert hits and all('signature_verified=1' in line for line in hits)
                        assert entries == original, 'Cache payload changed on authenticated hit'
                    rows.append(dict(passed=True, profile=profile['name'], fixture=fixture['name'],
                        path_mode=path_mode, phase=phase, exit=result.returncode, argv=argv,
                        seconds=time.monotonic() - started, signed_cache_hits=len(hits),
                        physical_compiler_log=str(physical_log), compiler_log_sha256=digest(physical_log),
                        run_log_sha256=digest(run), physical_cache_roots=[str(p) for p in roots], cache_entries=entries))
                    save(out / 'checkpoint.json', dict(state='running', rows=rows))
                print('PASS authenticated high memory64 cache', c['arch'], profile['name'], path_mode, flush=True)
    for pins in [c['pins'], dependencies, fixture_pins, manifest['code_pins']]:
        verify(pins)
    assert digest(qpath) == qhash
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    save(out / 'summary.json', dict(passed=True, completed=len(rows), rows=rows,
        product_sha256=digest(c['product']), actual_dependency_pins=dependencies,
        source_identities=q['source_identities'], configuration_sha256=digest(a.config),
        code_sha256=digest(__file__), build_receipts=[dict(path=str(qpath), sha256=qhash)],
        scope='Guarded high memory64 fresh processes, signed cold/store and two authenticated hits, default and explicit paths'))


if __name__ == '__main__':
    main()
