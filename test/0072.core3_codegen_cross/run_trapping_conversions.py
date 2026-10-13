#!/usr/bin/env python3
"""Qualified CLI conversion traps: exact diagnostic and intentional fatal signal."""
import argparse
import json
from pathlib import Path
import re
import signal
import subprocess
from check_oracle import check
from matrix import runtime_profiles
from run_matrix import digest, execute, save, verify
from trapping_conversion_cases import cases

ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
FATAL = re.compile(r'^uwvm: \[fatal\] Runtime crash \(([^\r\n]+)\)$', re.MULTILINE)
FATAL_SIGNALS = {-signal.SIGILL, -signal.SIGTRAP, -signal.SIGABRT}


def accepts_trap(exit_code, text, expected):
    # A loader error, unreachable sentinel, timeout, SIGSEGV, OOM kill or a
    # generic error exit must not earn negative-test credit.
    return exit_code in FATAL_SIGNALS and FATAL.findall(ANSI.sub('', text)) == [expected]


def check_classifier():
    for kind in ('integer overflow', 'invalid conversion to integer'):
        text = f'uwvm: [fatal] Runtime crash ({kind})\n'
        assert accepts_trap(-signal.SIGILL, text, kind)
        assert accepts_trap(-signal.SIGTRAP, text, kind)
        assert accepts_trap(-signal.SIGABRT, '\x1b[31m' + text + '\x1b[0m', kind)
        for status in (0, 1, 132, -signal.SIGSEGV, -signal.SIGKILL, -signal.SIGTERM):
            assert not accepts_trap(status, text, kind)
        assert not accepts_trap(-signal.SIGILL, text + text, kind)
        assert not accepts_trap(-signal.SIGILL, text.replace(kind, 'catch unreachable'), kind)
        assert not accepts_trap(-signal.SIGILL, text, 'unrelated trap')
        assert not accepts_trap(-signal.SIGILL, 'loader: ' + kind, kind)


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
    check()
    check_classifier()
    if a.prepare_only:
        fixtures, rows = [], []
        for generated in cases():
            name = generated['name']
            wat, wasm = out / (name + '.wat'), out / (name + '.wasm')
            wat.write_text(generated['wat'] + '\n')
            for label, argv in (('parse', [c['wasm_tools'], 'parse', str(wat), '-o', str(wasm)]),
                                ('validate', [c['wasm_tools'], 'validate', '--features', 'all', str(wasm)])):
                row = execute(argv, out / (name + '.' + label + '.log'), out)
                rows.append(row)
                assert row['exit'] == 0, row
            fixtures.append({k: v for k, v in generated.items() if k != 'wat'} |
                            dict(path=str(wasm), sha256=digest(wasm), wat_sha256=digest(wat)))
        verify(c['pins'])
        verify(code_pins)
        save(out / 'fixtures.json', dict(fixtures=fixtures, code_pins=code_pins,
             validated_with_all_features=True, scope='Eight scalar truncation families; targeted failure and adjacent valid boundaries'))
        save(out / 'qualification.json', dict(passed=True, fixtures=len(fixtures),
             tool_runs=rows, code_pins=code_pins, config_sha256=digest(a.config),
             scope='Oracle, classifier and Wasm validation; no guest execution'))
        print('PASS conversion trap fixtures', len(fixtures), flush=True)
        return
    qpath = Path(c['product_qualification'])
    qhash = digest(qpath)
    q = json.loads(qpath.read_text())
    assert q['passed'] and q['product_sha256'] == digest(c['product'])
    assert q.get('semantic_runs', q.get('completed', 0)) > 0, 'Prior actual semantic execution required'
    dependency_pins = q.get('actual_dependency_pins', q.get('actual_postbuild_dependency_pins'))
    assert dependency_pins
    verify(dependency_pins)
    manifest_path = Path(c['fixtures'])
    manifest = json.loads(manifest_path.read_text())
    verify(manifest['code_pins'])
    fixtures = manifest['fixtures']
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    profiles = [p for p in runtime_profiles(c['ros']) if p['name'] in c['profiles']]
    assert {p['name'] for p in profiles} == set(c['profiles'])
    planned, rows = len(fixtures) * len(profiles), []
    for profile in profiles:
        for fixture in fixtures:
            case = out / (profile['name'] + '--' + fixture['name'])
            case.mkdir()
            argv = [*c.get('prefix', []), c['product'], *profile['argv'], '-Rct', '0',
                    *(['-Rllvm-cache-path', 'disable'] if profile['backend'] != 'uwvm-int' else []),
                    '--run', fixture['path']]
            row = execute(argv, case / 'run.log', case, c.get('timeout', 120))
            expected = fixture['expected_trap']
            text = Path(row['log']).read_text(errors='replace')
            passed = accepts_trap(row['exit'], text, expected) if expected else row['exit'] == 0
            row.update(profile=profile, fixture=fixture, passed=passed,
                       observed_fatal_kinds=FATAL.findall(ANSI.sub('', text)))
            rows.append(row)
            save(out / 'checkpoint.json', dict(state='running' if passed else 'failed',
                                               planned=planned, completed=rows))
            assert passed, (profile['name'], fixture['name'], row['exit'], text[-2200:])
        print('PASS conversion traps', c['arch'], c['repo'], profile['name'], len(fixtures), flush=True)
    verify(c['pins'])
    verify(code_pins)
    verify(fixture_pins)
    verify(manifest['code_pins'])
    verify(dependency_pins)
    assert digest(qpath) == qhash
    subprocess.run(['/usr/bin/bash', c['cgroup_guard']], check=True)
    save(out / 'qualification.json', dict(passed=True, completed=len(rows), planned=planned,
         rows=rows, repo=c['repo'], arch=c['arch'], code_pins=code_pins,
         actual_dependency_pins=dependency_pins,
         source_identities=q.get('source_identities'), source_identity=q.get('source_identity'),
         product_sha256=digest(c['product']), prior_semantic_qualification_sha256=qhash,
         config_sha256=digest(a.config), fixtures_manifest_sha256=digest(manifest_path),
         negative_runs=sum(row['fixture']['expected_trap'] is not None for row in rows),
         valid_control_checks=sum(row['fixture']['checks'] for row in rows if row['fixture']['expected_trap'] is None),
         scope='Actual scalar truncation trap type and fatal-signal checks plus adjacent valid boundaries; no assembly or complete Core 3 qualification'))
    print('PASS targeted conversion CLI runs', len(rows), flush=True)


if __name__ == '__main__':
    main()
