#!/usr/bin/env python3
"""Check Core 3 cross-module aggregate references and exact import linking.

Run inside the 64 GiB Linux test cgroup. The Wasmtime result is an independent
oracle; the VM matrix exercises new GC syntax on each supported backend.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []

    def check(name, command, success, message=None):
        process = subprocess.run([str(part) for part in command], capture_output=True, timeout=120)
        output = process.stdout + process.stderr
        (args.out / (name + '.log')).write_bytes(output)
        passed = (process.returncode == 0) == success and (message is None or message in output)
        rows.append({'name': name, 'command': [str(part) for part in command],
                     'exit': process.returncode, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            print(f'FAIL {name}: {output.decode(errors="replace")[-800:]}', flush=True)
        return passed

    names = ('provider', 'consumer', 'mismatch', 'call_provider', 'call_consumer')
    binaries = {}
    for name in names:
        wat = root / 'test/0017.runtime/fixtures' / f'gc_import_reference_{name}.wat'
        wasm = args.out / f'{name}.wasm'
        binaries[name] = wasm
        if not check(name + '-parse', [args.wasm_tools, 'parse', wat, '-o', wasm], True):
            raise SystemExit(1)
        if not check(name + '-validate', [args.wasm_tools, 'validate', '--features', 'all', wasm], True):
            raise SystemExit(1)

    oracle = [args.wasmtime, 'run', '-C', 'cache=n', '-W', 'gc=y',
              '--preload', f'P={binaries["provider"]}']
    if not check('wasmtime-equivalent', [*oracle, binaries['consumer']], True):
        raise SystemExit(1)
    if not check('wasmtime-incompatible', [*oracle, binaries['mismatch']], False, b'incompatible import type'):
        raise SystemExit(1)
    if not check('wasmtime-indirect-and-ref',
                 [args.wasmtime, 'run', '-C', 'cache=n', '-W', 'gc=y',
                  '--preload', f'P={binaries["call_provider"]}', binaries['call_consumer']], True):
        raise SystemExit(1)

    modes = {'int-full': ['-Rint'], 'jit-full': ['-Raot']} if args.ros else {
        'int-full': ['-Rcc', 'int', '-Rcm', 'full'],
        'int-lazy': ['-Rcc', 'int', '-Rcm', 'lazy'],
        'int-lazy-verified': ['-Rcc', 'int', '-Rcm', 'lazy+verification'],
        'jit-full': ['-Rcc', 'jit', '-Rcm', 'full'],
        'jit-lazy': ['-Rcc', 'jit', '-Rcm', 'lazy'],
        'tiered-lazy': ['-Rcc', 'tiered', '-Rcm', 'lazy', '-Rct', '0'],
        'tiered-lazy-verified': ['-Rcc', 'tiered', '-Rcm', 'lazy+verification', '-Rct', '0'],
    }
    preload = ['--wasm-set-main-module-name', 'C', '--wasm-preload-library', binaries['provider'], 'P']
    features = ['-WFE-gc', '-WFE-function-references']
    for mode, flags in modes.items():
        base = [args.uwvm, *flags, *features, *preload]
        check(mode + '-equivalent', [*base, '--run', binaries['consumer']], True)
        check(mode + '-incompatible', [*base, '--run', binaries['mismatch']], False, b'type mismatch')
        call_preload = ['--wasm-set-main-module-name', 'C', '--wasm-preload-library',
                        binaries['call_provider'], 'P']
        check(mode + '-indirect-and-ref',
              [args.uwvm, *flags, *features, '-WFE-tail-call', *call_preload,
               '--run', binaries['call_consumer']], True)
    check('gc-disabled',
          [args.uwvm, *modes['int-full'], '-WFE-function-references', '-WFD-gc',
           *preload, '--run', binaries['consumer']], False)
    check('gc-with-function-references-disabled',
          [args.uwvm, *modes['int-full'], '-WFE-gc', '-WFD-function-references',
           *preload, '--run', binaries['consumer']], True)
    call_preload = ['--wasm-set-main-module-name', 'C', '--wasm-preload-library',
                    binaries['call_provider'], 'P']
    check('function-references-disabled',
          [args.uwvm, *modes['int-full'], '-WFE-gc', '-WFD-function-references',
           '-WFE-tail-call', *call_preload, '--run', binaries['call_consumer']],
          False, b'--wasm-feature-enable-function-references')

    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'scope': 'uwvm2-ros' if args.ros else 'uwvm2',
               'runner_sha256': sha256(Path(__file__)), 'uwvm_sha256': sha256(args.uwvm),
               'wasm_tools_sha256': sha256(args.wasm_tools), 'wasmtime_sha256': sha256(args.wasmtime),
               'fixtures': {name: sha256(root / 'test/0017.runtime/fixtures' /
                                        f'gc_import_reference_{name}.wat') for name in names}}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Core 3 cross-module GC references {summary["scope"]}: '
          f'{sum(row["passed"] for row in rows)}/{len(rows)}', flush=True)
    if not summary['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
