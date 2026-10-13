#!/usr/bin/env python3
"""Guarded, checkpointed real-CLI execution; planned rows never become passes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
from check_oracle import check
from numeric_cases import cases as numeric_cases
from tail_cases import cases as tail_cases
from matrix import runtime_profiles


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path = Path(path)
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temp.replace(path)


def verify(pins):
    for path, expected in pins.items():
        assert digest(path) == expected, ('input changed', path)


def execute(argv, log, cwd, timeout=120):
    started = time.monotonic()
    with Path(log).open('xb') as stream:
        completed = subprocess.run(argv, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                   timeout=timeout, check=False)
    return dict(argv=argv, exit=completed.returncode, seconds=time.monotonic() - started,
                log=str(log), log_sha256=digest(log))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--prepare-only', action='store_true')
    a = parser.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    check()
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    rows = []
    if a.prepare_only:
        fixtures = []
        generated = [(name, wat, count, None) for name, wat, count in numeric_cases() + tail_cases()]
        if config.get('core_generator'):
            from core_cases import cases as core_cases
            generated += core_cases(config['core_generator'])
        for name, wat, count, explicit_flags in generated:
            wat_path = out / (name + '.wat')
            wasm_path = out / (name + '.wasm')
            wat_path.write_text(wat + '\n')
            row = execute([config['wasm_tools'], 'parse', str(wat_path), '-o', str(wasm_path)],
                          out / (name + '.parse.log'), out)
            rows.append(row)
            assert row['exit'] == 0, row
            row = execute([config['wasm_tools'], 'validate', '--features', 'all', str(wasm_path)],
                          out / (name + '.validate.log'), out)
            rows.append(row)
            assert row['exit'] == 0, row
            flags = list(explicit_flags or [])
            if name.startswith('tail-'):
                flags += ['-WFE-tail-call']
                if 'typed-ref' in name:
                    flags += ['-WFE-function-references']
            fixtures.append(dict(name=name, checks=count, path=str(wasm_path),
                                 sha256=digest(wasm_path), wat_sha256=digest(wat_path), argv=flags))
            save(out / 'checkpoint.json', dict(state='running', fixtures=fixtures, tool_runs=rows))
        verify(config['pins'])
        verify(code_pins)
        save(out / 'fixtures.json', dict(fixtures=fixtures, code_pins=code_pins,
             pins=config['pins'], validated_with_all_features=True,
             scope='targeted scalar/SIMD FP and real Wasm tail calls; not all Core 3'))
        save(out / 'summary.json', dict(passed=True, fixtures=len(fixtures), tool_runs=rows,
                                       exact_config_sha256=digest(a.config), code_pins=code_pins))
        print('PASS independent oracle and fixture validation', len(fixtures), flush=True)
        return
    manifest_path = Path(config['fixtures'])
    manifest = json.loads(manifest_path.read_text())
    verify(manifest['code_pins'])
    fixtures = manifest['fixtures']
    if config.get('cases'):
        selected = set(config['cases'])
        fixtures = [f for f in fixtures if f['name'] in selected]
        assert {f['name'] for f in fixtures} == selected
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    profiles = runtime_profiles(config['ros'], config.get('all_interpreter_peepholes', False))
    if config.get('profiles'):
        selected = set(config['profiles'])
        profiles = [p for p in profiles if p['name'] in selected]
        assert {p['name'] for p in profiles} == selected
    planned = len(profiles) * len(fixtures)
    save(out / 'checkpoint.json', dict(state='running', planned=planned, completed=rows))
    for profile in profiles:
        for fixture in fixtures:
            name = profile['name'] + '--' + fixture['name']
            # No stale signed cache may substitute a previous policy's object.
            case_dir = out / name
            case_dir.mkdir()
            argv = [*config.get('prefix', []), config['product'], *profile['argv'],
                    '-Rct', '0', *(['-Rllvm-cache-path', 'disable']
                                  if profile['backend'] != 'uwvm-int' else []),
                    *fixture['argv'], '--run', fixture['path']]
            row = execute(argv, case_dir / 'run.log', case_dir, config.get('timeout', 120))
            row.update(profile=profile, fixture=fixture, product_sha256=config['pins'][config['product']],
                       executing_tier_verified=False if profile['backend'] == 'tiered' else None)
            rows.append(row)
            save(out / 'checkpoint.json', dict(state='running' if row['exit'] == 0 else 'failed',
                                               planned=planned, completed=rows))
            assert row['exit'] == 0, (name, row)
        print('PASS semantic profile', profile['name'], len(fixtures), flush=True)
    verify(config['pins'])
    verify(code_pins)
    verify(fixture_pins)
    verify(manifest['code_pins'])
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(out / 'summary.json', dict(passed=True, completed=len(rows), planned=planned, rows=rows,
        code_pins=code_pins, config_sha256=digest(a.config), fixtures_manifest_sha256=digest(manifest_path),
        scope=config['scope'], tiered_execution_qualified=False,
        assembly_efficiency_qualified=False, compile_time_variant=config.get('compile_time_variant')))
    print('PASS targeted real CLI semantics', len(rows), 'tier identity and assembly require separate evidence', flush=True)


if __name__ == '__main__':
    main()
