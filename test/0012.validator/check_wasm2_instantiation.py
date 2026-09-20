#!/usr/bin/env python3
"""Run binary-module acceptance and instantiation checks through both full backends.

WAST register/import linking graphs need a resident embedding runner and are
reported separately. This driver never treats a missing CLI entry as rejection.
"""
import argparse
import concurrent.futures
import json
from pathlib import Path
import resource
import subprocess
import tempfile
from check_wasm2_execution import interface, SPECTEST


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--full', type=Path, required=True)
    parser.add_argument('--ros', type=Path, required=True)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--feature-mode', choices=('wasm2', 'wasm1p1', 'default'), default='wasm2')
    args = parser.parse_args()
    out = args.out or Path(tempfile.mkdtemp(prefix='uwvm-wasm2-instantiation-', dir='/tmp'))
    out.mkdir(parents=True, exist_ok=True)
    for name, wat in [('spectest', SPECTEST), ('driver', '(module (func (export "_start")))')]:
        subprocess.run(['wat2wasm', '-', '-o', str(out / (name + '.wasm'))],
                       input=wat.encode(), check=True, capture_output=True)
    cases, excluded = [], []
    for c in json.loads((args.corpus / 'results.json').read_text())['cases']:
        case = dict(c)
        if c['expected']:
            _, imports, _ = interface(Path(c['path']))
            if any(m != 'spectest' for m in imports):
                excluded.append(dict(case=c, reason='foreign linking graph'))
                continue
        cases.append(case)

    def run(job):
        number, c, product, compiler = job
        options = ['-Rcm', 'full', '-Rcc', compiler] if product == 'full' else ['-Rint' if compiler == 'int' else '-Raot']
        feature_options = [] if args.feature_mode == 'default' else ['--wasm-feature-' + args.feature_mode]
        cmd = [str(getattr(args, product)), *options, '-Rct', '0', '-Rllvm-cache-path', 'disable',
               *feature_options, '--wasm-preload-library', str(out / 'spectest.wasm'), 'spectest',
               '--wasm-preload-library', c['path'], 'spec_target', '--run', str(out / 'driver.wasm')]
        try:
            proc = subprocess.run(cmd, capture_output=True, timeout=60)
            status, log = proc.returncode, proc.stdout + proc.stderr
        except subprocess.TimeoutExpired as e:
            status, log = 124, (e.stdout or b'') + (e.stderr or b'')
        kind = c['kind']
        rejected = status not in (0, 124) and b'Entry function' not in log
        if kind == 'module':
            passed = status == 0
        elif kind in ('assert_invalid', 'assert_malformed'):
            passed = rejected and (b'Parse' in log or b'parse' in log or b'Validat' in log or b'validat' in log)
        else:
            passed = rejected and bool(log)
        result = dict(number=number, source=c['source'], line=c['line'], kind=kind,
                      product=product, compiler=compiler, status=status, passed=passed)
        (out / (str(number) + '-' + product + '-' + compiler + '.log')).write_bytes(log)
        if not passed:
            print(json.dumps(result), flush=True)
        return result
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(run, [(i,c,p,b) for i,c in enumerate(cases)
                                     for p in ('full','ros') for b in ('int','jit')]))
    (out / 'cases.json').write_text(json.dumps(dict(cases=cases, excluded=excluded), indent=2))
    (out / 'results.json').write_text(json.dumps(results, indent=2))
    failed = sum(not r['passed'] for r in results)
    print(json.dumps(dict(feature_mode=args.feature_mode, cases=len(cases), runs=len(results), failed=failed, exclusions=len(excluded), out=str(out))))
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
