#!/usr/bin/env python3
"""Prove signed cache reuse in three fresh processes per selected CLI case."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

from run_matrix import digest, save, verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    qualification_path = Path(config['product_qualification'])
    qualification_hash = digest(qualification_path)
    qualification = json.loads(qualification_path.read_text())
    assert qualification['passed']
    assert qualification['product_sha256'] == digest(config['product'])
    dependencies = qualification['actual_dependency_pins']
    verify(dependencies)
    manifest_path = Path(config['fixtures'])
    manifest_hash = digest(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    verify(manifest['code_pins'])
    fixtures = [f for f in manifest['fixtures'] if f['name'] in config['cases']]
    assert {f['name'] for f in fixtures} == set(config['cases'])
    assert all(f.get('expected_trap') is None for f in fixtures)
    assert all(p['backend'] == 'llvm-jit' for p in config['profiles'])
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    output = Path(config['output'])
    output.mkdir(exist_ok=False)
    rows = []
    planned = len(config['profiles']) * len(fixtures) * 6
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for profile in config['profiles']:
        for fixture in fixtures:
            for path_mode in ['default', 'explicit']:
                work = output / (profile['name'] + '--' + fixture['name'] + '--' + path_mode)
                work.mkdir()
                env = dict(os.environ)
                for name in ['LD_PRELOAD', 'LD_AUDIT', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT']:
                    env.pop(name, None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                prefix = list(config.get('prefix', []))
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
                               '-U', 'LD_PRELOAD', '-U', 'LD_AUDIT',
                               '-E', 'XDG_CACHE_HOME=' + env['XDG_CACHE_HOME']]
                namespace = 'uwvm2ros' if config['ros'] else 'uwvm2'
                cache = work / ('xdg/' + namespace + '/llvm-jit' if path_mode == 'default' else 'cache')
                flags = [] if path_mode == 'default' else ['-Rllvm-cache-path', 'path', str(cache)]
                original = None
                for phase in ['cold', 'warm', 'warm-again']:
                    log, run = work / (phase + '.compiler.log'), work / (phase + '.run.log')
                    argv = [*prefix, config['product'], *profile['argv'], '-Rct', '0', *flags,
                            '-Rclog', 'file', str(log), *fixture['argv'], '--run', fixture['path']]
                    started = time.monotonic()
                    with run.open('xb') as stream:
                        result = subprocess.run(argv, cwd=work, env=env, stdout=stream,
                                                stderr=subprocess.STDOUT, timeout=config.get('timeout', 300))
                    physical_log = log
                    if not physical_log.exists() and guest_root is not None:
                        physical_log = guest_root / str(log).lstrip('/')
                    assert result.returncode == 0, (phase, run.read_text(errors='replace')[-4000:])
                    text = ansi.sub('', physical_log.read_text())
                    hits = [line for line in text.splitlines() if 'object-cache-hit module=' in line]
                    roots = [cache] + ([guest_root / str(cache).lstrip('/')] if guest_root is not None else [])
                    entries = {}
                    for root in roots:
                        for path in root.rglob('*.uwvm-ljc'):
                            key = str(path.relative_to(root))
                            assert key not in entries
                            entries[key] = digest(path)
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
                    save(output / 'checkpoint.json', dict(state='running', planned=planned, completed=len(rows)))
                print('PASS signed cache', config['arch'], profile['name'], fixture['name'], path_mode, flush=True)
    for pins in [config['pins'], dependencies, fixture_pins, manifest['code_pins'], code_pins]:
        verify(pins)
    assert digest(qualification_path) == qualification_hash
    assert digest(manifest_path) == manifest_hash
    assert len(rows) == planned
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(output / 'summary.json', dict(passed=True, completed=len(rows), planned=planned, rows=rows,
        product_sha256=digest(config['product']), actual_dependency_pins=dependencies,
        source_identities=qualification['source_identities'], configuration_sha256=digest(args.config),
        code_sha256=digest(__file__), code_pins=code_pins, fixtures_manifest_sha256=manifest_hash,
        build_receipts=[dict(path=str(qualification_path), sha256=qualification_hash)],
        scope=config['scope'], tiered_execution_qualified=False, hardware_efficiency_qualified=False))


if __name__ == '__main__':
    main()
