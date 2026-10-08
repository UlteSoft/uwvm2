#!/usr/bin/env python3
"""Require real checked T2 entry on cold and authenticated fresh-process hits."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

from run_matrix import digest, execute, save, verify
from tiered_numeric import fixture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    assert not config.get('ros'), 'ROS has no tiered backend'
    output = args.output.resolve()
    output.mkdir(exist_ok=False)
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    wat, wasm = output / 'tiered-numeric.wat', output / 'tiered-numeric.wasm'
    high_memory = config.get('fixture_kind') == 'memory64-high'
    if high_memory:
        from tiered_high_memory import fixture as selected_fixture
        feature_flags = ['-WFE-memory64', '-WFE-simd', '-WFE-threads']
    elif config.get('fixture_kind') == 'simd-sign':
        from tiered_simd_sign import fixture as selected_fixture
        feature_flags = ['-WFE-simd']
    else:
        selected_fixture = fixture
        feature_flags = []
    wat.write_text(selected_fixture() + '\n')
    for stage, argv in [
        ('parse', [config['wasm_tools'], 'parse', str(wat), '-o', str(wasm)]),
        ('validate', [config['wasm_tools'], 'validate', '--features', 'all', str(wasm)])]:
        row = execute(argv, output / (stage + '.log'), output)
        assert row['exit'] == 0, row
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    modes = config.get('modes', ['lazy', 'lazy+verification'])
    levels = config.get('levels', [1, 2, 3])
    path_modes = config.get('path_modes', ['default', 'explicit'])
    for mode in modes:
        for level in levels:
            for path_mode in path_modes:
                assert path_mode in ('default', 'explicit')
                label = f'{mode}-O{level}-{path_mode}'
                work = output / label
                work.mkdir()
                prefix = list(config.get('prefix', []))
                guest_root = None
                env = dict(os.environ)
                env.pop('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                if prefix:
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
                cache = work / 'cache' if path_mode == 'explicit' else work / 'xdg/uwvm2/llvm-jit'
                flags = ['-Rllvm-cache-path', 'path', str(cache)] if path_mode == 'explicit' else []
                roots = [cache]
                if guest_root is not None:
                    roots.append(guest_root / str(cache).lstrip('/'))
                before = None
                for phase in ('cold', 'warm', 'warm-again'):
                    compiler_log = work / (phase + '.compiler.log')
                    argv = [*prefix, config['product'], '-Rcc', 'tiered', '-Rcm', mode,
                            '-Rct', '2', '-Rllvm-full-policy', f'pb-o{level}',
                            *flags, '-Rllvm-call-stack', 'instruction',
                            '-Rclog', 'file', str(compiler_log), *feature_flags,
                            '--run', str(wasm)]
                    # XDG must also be set for native execution; execute() uses
                    # its parent's environment, so supply env explicitly here.
                    started = time.monotonic()
                    run_log = work / (phase + '.run.log')
                    with run_log.open('xb') as stream:
                        completed = subprocess.run(argv, cwd=work, env=env, stdout=stream,
                                                   stderr=subprocess.STDOUT, timeout=config.get('timeout', 300))
                    if not compiler_log.exists() and guest_root is not None:
                        compiler_log = guest_root / str(compiler_log).lstrip('/')
                    log = ansi.sub('', compiler_log.read_text() if compiler_log.exists() else '')
                    entered = bool(re.search(r'tiered-full-enter .*?\bfn=1\b', log))
                    ready = bool(re.search(r'tiered-full-ready .*?\bfunctions=3\b', log))
                    counters = {key: max(map(int, re.findall(r'\b' + key + r'=(\d+)', log)), default=0)
                                for key in ['tiered_full_ready', 'tiered_full_failed', 'tiered_switches']}
                    hits = [line for line in log.splitlines() if 'object-cache-hit module=' in line]
                    entries = {}
                    for root in roots:
                        for file in root.rglob('*.uwvm-ljc'):
                            key = str(file.relative_to(root))
                            assert key not in entries, ('Duplicate physical cache', key)
                            entries[key] = digest(file)
                    row = dict(mode=mode, optimization=level, path_mode=path_mode, phase=phase,
                               argv=argv, exit=completed.returncode, seconds=time.monotonic() - started,
                               actual_t2_target_entry=entered, full_module_ready=ready, counters=counters,
                               checked_calls=4_000_000,
                               bit_checks_per_call=12 if config.get('fixture_kind') == 'simd-sign' else 6,
                               compiler_log_sha256=digest(compiler_log) if compiler_log.exists() else None,
                               physical_compiler_log=str(compiler_log), run_log_sha256=digest(run_log),
                               wasm_sha256=digest(wasm), cache_entries=entries,
                               physical_cache_roots=[str(p) for p in roots], signed_cache_hits=len(hits))
                    rows.append(row)
                    save(output / 'checkpoint.json', dict(state='running', rows=rows))
                    assert completed.returncode == 0, (label, phase, run_log.read_text()[-4000:])
                    assert entered and ready and counters['tiered_full_ready'] > 0 and counters['tiered_full_failed'] == 0, (label, phase, row, log[-8000:])
                    assert entries, (label, phase, log[-8000:])
                    if phase == 'cold':
                        assert not hits and re.search(r'object-cache-store-complete .* status=ok\b', log), (label, log[-8000:])
                        before = entries
                    else:
                        assert hits and all('signature_verified=1' in line for line in hits), (label, phase, log[-8000:])
                        assert entries == before, (label, phase, 'Cache object bytes changed on hit')
                    row['passed'] = True
                    save(output / 'checkpoint.json', dict(state='running', rows=rows))
                    print('PASS actual T2 signed cache', config.get('fixture_kind', 'numeric'),
                          label, phase, flush=True)
    assert len(rows) == len(modes) * len(levels) * len(path_modes) * 3
    verify(config['pins'])
    verify(code_pins)
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(output / 'summary.json', dict(passed=True, rows=rows, completed=len(rows), planned=len(rows),
         code_pins=code_pins, config_sha256=digest(args.config), product_sha256=digest(config['product']),
         fixture_kind=config.get('fixture_kind', 'numeric'),
         scope=('Actual high-memory64 T2 entry, integer/FP bits, SIMD, atomics and low aliases in every signed cold/warm/warm-again process; listed modes/O levels/paths only.'
                if high_memory else 'Actual numeric T2 target entry in every cold/warm/warm-again process with signed persistent cache; listed lazy modes/O levels/paths only.')))


if __name__ == '__main__':
    main()
