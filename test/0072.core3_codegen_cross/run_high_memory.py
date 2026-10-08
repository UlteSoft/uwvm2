#!/usr/bin/env python3
"""Real, source-bound memory64 CLI execution; requires its own memory budget."""
import argparse
import json
from pathlib import Path
import subprocess
from high_memory_cases import cases
from matrix import runtime_profiles
from run_matrix import digest, execute, save, verify
from run_trapping_conversions import accepts_trap, check_classifier


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('config', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--prepare-only', action='store_true')
    a = ap.parse_args()
    c = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    verify(c['pins'])
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    check_classifier()
    if a.prepare_only:
        fixtures, rows = [], []
        for generated in cases():
            wat, wasm = out / (generated['name'] + '.wat'), out / (generated['name'] + '.wasm')
            wat.write_text(generated['wat'] + '\n')
            for label, argv in [('parse', [c['wasm_tools'], 'parse', str(wat), '-o', str(wasm)]),
                                ('validate', [c['wasm_tools'], 'validate', '--features', 'all', str(wasm)])]:
                row = execute(argv, out / (generated['name'] + '.' + label + '.log'), out)
                rows.append(row)
                assert row['exit'] == 0, row
            fixtures.append({k: v for k, v in generated.items() if k != 'wat'} |
                            dict(path=str(wasm), sha256=digest(wasm), wat_sha256=digest(wat)))
        verify(c['pins'])
        verify(code_pins)
        save(out / 'fixtures.json', dict(fixtures=fixtures, code_pins=code_pins,
                                         validated_with_all_features=True))
        save(out / 'qualification.json', dict(passed=True, fixtures=len(fixtures), tool_runs=rows,
            code_pins=code_pins, config_sha256=digest(a.config), scope='Fixture generation/validation; no guest execution'))
        print('PASS guarded memory64 fixtures', len(fixtures), flush=True)
        return
    assert c['owned_rss_budget_bytes'] >= 7 << 30, 'Separately admit high-memory witness'
    qpath = Path(c['product_qualification'])
    qhash = digest(qpath)
    q = json.loads(qpath.read_text())
    assert q['passed'] and q['product_sha256'] == digest(c['product'])
    assert q.get('semantic_runs', q.get('completed', 0)) > 0
    dependencies = q.get('actual_dependency_pins', q.get('actual_postbuild_dependency_pins'))
    assert dependencies
    verify(dependencies)
    manifest_path = Path(c['fixtures'])
    manifest = json.loads(manifest_path.read_text())
    verify(manifest['code_pins'])
    fixtures = manifest['fixtures']
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    profiles = [p for p in runtime_profiles(c['ros']) if p['name'] in c['profiles']]
    assert {p['name'] for p in profiles} == set(c['profiles'])
    rows, planned = [], len(profiles) * len(fixtures)
    for profile in profiles:
        for fixture in fixtures:
            case = out / (profile['name'] + '--' + fixture['name'])
            case.mkdir()
            argv = [*c.get('prefix', []), c['product'], *profile['argv'], '-Rct', '0',
                    *(['-Rllvm-cache-path', 'disable'] if profile['backend'] != 'uwvm-int' else []),
                    *(c.get('llvm_argv', []) if profile['backend'] != 'uwvm-int' else []),
                    *fixture['argv'], '--run', fixture['path']]
            row = execute(argv, case / 'run.log', case, c.get('timeout', 180))
            text = Path(row['log']).read_text(errors='replace')
            expected = fixture['expected_trap']
            passed = accepts_trap(row['exit'], text, expected) if expected else row['exit'] == 0
            row.update(profile=profile, fixture=fixture, passed=passed)
            rows.append(row)
            save(out / 'checkpoint.json', dict(state='running' if passed else 'failed', planned=planned, completed=rows))
            assert passed, (profile['name'], fixture['name'], row['exit'], text[-2400:])
        print('PASS high-memory profile', c['arch'], c['repo'], profile['name'], len(fixtures), flush=True)
    for pins in [c['pins'], code_pins, fixture_pins, manifest['code_pins'], dependencies]:
        verify(pins)
    assert digest(qpath) == qhash
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    save(out / 'qualification.json', dict(passed=True, completed=len(rows), planned=planned, rows=rows,
         arch=c['arch'], repo=c['repo'], product_sha256=digest(c['product']),
         source_identities=q.get('source_identities'), source_identity=q.get('source_identity'),
         compile_time_variant=q.get('variant'), compiled_macros=q.get('macros'),
         actual_dependency_pins=dependencies, code_pins=code_pins,
         prior_product_qualification_sha256=qhash, config_sha256=digest(a.config),
         fixtures_manifest_sha256=digest(manifest_path), negative_runs=sum(r['fixture']['expected_trap'] is not None for r in rows),
         high_address_runs=sum(r['fixture']['minimum_guest_bytes'] > 1 << 32 for r in rows),
         assembly_efficiency_qualified=False, tiered_execution_qualified=False,
         scope='Targeted high memory64 semantics and exact OOB traps; not all Core3, all macros, or T2'))
    print('PASS guarded high memory64', c['arch'], c['repo'], len(rows), flush=True)


if __name__ == '__main__':
    main()
