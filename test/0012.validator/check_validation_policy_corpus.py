#!/usr/bin/env python3
"""Compare explicit validator APIs against pinned official binary assertions.

Use WebAssembly/spec wg-2.0 (fffc6e12fa454e475455a7b58d3b5dc343980c10)
or Core 1 wg_v1 (fd4fe9f5f271740d22f0fe82aff383086fef7b11).
The validity oracle comes from that version's official WAST assertions.
Do not use WABT 1.0.36's --disable-* options as an MVP oracle: they still
accept some SIMD loads/stores and newer segment encodings, and use Core 2
br_table bottom typing even with every proposal disabled.
Text-only malformed assertions are not binary-parser tests and are counted,
not silently marked successful. No module is instantiated by the probe.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), capture_output=True, text=True, timeout=120, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--suite', type=Path, required=True)
    p.add_argument('--full-probe', type=Path, required=True)
    p.add_argument('--ros-probe', type=Path)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--wast2json', default='wast2json')
    p.add_argument('--fallback-wast2json', action='append', default=[],
                   help='Conversion-only fallback for historical text syntax; never changes the assertion oracle')
    p.add_argument('--core-version', choices=('1','2'), required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    cases, text_cases, conversion_failures = [], [], []
    for source in sorted(a.suite.rglob('*.wast')):
        relative = source.relative_to(a.suite)
        folder = a.out / relative.with_suffix('')
        folder.mkdir(parents=True)
        for attempt, converter in enumerate([a.wast2json, *a.fallback_wast2json]):
            result = run([converter, source, '-o', folder/'script.json'])
            (folder/f'conversion-{attempt}.log').write_text(result.stdout+result.stderr)
            if not result.returncode:
                break
        if result.returncode:
            conversion_failures.append(str(relative))
            continue
        for item in json.loads((folder/'script.json').read_text())['commands']:
            if 'filename' not in item:
                continue
            if item.get('module_type') == 'text':
                text_cases.append(dict(source=str(relative), line=item['line']))
                continue
            if item['type'] not in ('module','assert_invalid','assert_malformed','assert_unlinkable','assert_uninstantiable','assert_trap'):
                raise ValueError(item)
            path = folder/item['filename']
            cases.append(dict(source=str(relative), line=item['line'], kind=item['type'],
                converter=converter,
                path=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                valid=item['type'] not in ('assert_invalid','assert_malformed')))
    if not cases or conversion_failures:
        raise RuntimeError(f'Incomplete conversion: {conversion_failures}')
    configurations = ([('wasm1','mvp'),('wasm1p1','mvp'),('runtime','mvp')]
                      if a.core_version == '1' else
                      [('wasm1p1','wasm1p1'),('runtime','wasm1p1'),
                       ('wasm1p1','wasm2'),('wasm2','wasm2'),('runtime','wasm2')])
    records, failures = [], []
    for product, probe in [('full',a.full_probe),('ros',a.ros_probe)]:
        if probe is None:
            continue
        for api, profile in configurations:
            result = run([probe.resolve(),api,profile], input=''.join(c['path']+'\n' for c in cases))
            label = f'{product}-{api}-{profile}'
            (a.out/(label+'.stdout')).write_text(result.stdout)
            (a.out/(label+'.stderr')).write_text(result.stderr)
            lines = result.stdout.splitlines()
            if result.returncode or len(lines) != len(cases):
                raise RuntimeError(f'{label}: probe failure {result.returncode}, {len(lines)}/{len(cases)}')
            failed = []
            for case, actual in zip(cases, lines):
                expected = case['valid']
                if (actual == 'ok 0 0') != expected or actual.startswith('io '):
                    failed.append(dict(**case, actual=actual, expected=expected, configuration=label))
            failures.extend(failed)
            record = dict(configuration=label, cases=len(cases), failures=len(failed),
                          probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest())
            records.append(record)
            print(json.dumps(record), flush=True)
    report = dict(configurations=records, cases=cases, text_only=text_cases,
                  failures=failures, conversion_failures=conversion_failures,
                  core_version=a.core_version,
                  converters={tool: run([tool,'--version']).stdout.strip()
                              for tool in [a.wast2json, *a.fallback_wast2json]})
    (a.out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(binary_cases=len(cases), text_only=len(text_cases), failures=len(failures))))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
