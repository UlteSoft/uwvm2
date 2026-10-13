#!/usr/bin/env python3
"""Check Core 3 function-reference identity through cross-module imported functions.

Run in the Linux 64 GiB cgroup. Wasmtime is the independent reference. Both
fixtures deliberately give provider function index 1 a different consumer body.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--include-jit', action='store_true',
                        help='Also exercise LLVM full/lazy under instruction and native unwind stack policies')
    parser.add_argument('--only-mode', action='append')
    parser.add_argument('--guard', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(args.guard or root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    fixture_dir = Path(__file__).resolve().parent / 'fixtures'
    fixture_names = ('call_ref_provider', 'call_ref_consumer')
    modules = {}
    for name in fixture_names:
        source = fixture_dir / (name + '.wat')
        target = args.output / (name + '.wasm')
        subprocess.run([str(args.wasm_tools), 'parse', str(source), '-o', str(target)], check=True)
        modules[name] = target
    provider = modules['call_ref_provider']
    consumer = modules['call_ref_consumer']
    modes = {'full': ['-Rcc', 'int', '-Rcm', 'full'],
             'lazy': ['-Rcc', 'int', '-Rcm', 'lazy'],
             'lazy+verification': ['-Rcc', 'int', '-Rcm', 'lazy+verification'],
             'tiered': ['--runtime-tiered']}
    if args.ros:
        modes = {'full': ['-Rint']}
    if args.include_jit:
        for policy in ('instruction', 'unwind'):
            if args.ros:
                modes['jit-full-' + policy] = ['-Raot', '-Rllvm-full-policy', 'pb-o3',
                                               '-Rllvm-call-stack', policy]
            else:
                modes['jit-full-' + policy] = ['-Rcc', 'jit', '-Rcm', 'full',
                                               '-Rllvm-full-policy', 'pb-o3', '-Rllvm-call-stack', policy]
                modes['jit-lazy-' + policy] = ['-Rcc', 'jit', '-Rcm', 'lazy',
                                               '-Rllvm-call-stack', policy]
    if args.only_mode:
        if not set(args.only_mode) <= set(modes):
            parser.error('unknown mode')
        modes = {name: modes[name] for name in args.only_mode}
    commands = [('wasmtime', [str(args.wasmtime), '-C', 'cache=n',
                             '-W', 'function-references=y', '-W', 'tail-call=y',
                             '--preload', f'A={provider}', str(consumer)])]
    for name, switches in modes.items():
        commands.append((name, [str(args.uwvm), *switches, '-Rct', '0',
                                '-Rllvm-cache-path', 'disable',
                                '-WFE-function-references', '-WFE-tail-call',
                                '--wasm-set-main-module-name', 'B',
                                '--wasm-preload-library', str(provider), 'A',
                                '--run', str(consumer)]))
    rows = []
    for name, command in commands:
        result = subprocess.run(command, capture_output=True, timeout=90)
        log = (result.stdout + result.stderr).decode(errors='replace')
        (args.output / (name + '.log')).write_text(log)
        row = {'mode': name, 'command': command, 'exit': result.returncode,
               'passed': result.returncode == 0}
        rows.append(row)
        (args.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode != 0:
            raise RuntimeError(f'{name}: exit={result.returncode}\n{log}')
    summary = {'passed': True, 'checks': len(rows), 'product': 'uwvm2-ros' if args.ros else 'uwvm2',
               'include_jit': args.include_jit,
               'binary_sha256': sha256(args.uwvm),
               'fixture_sha256': {name: sha256(fixture_dir / (name + '.wat')) for name in fixture_names},
               'runner_sha256': sha256(Path(__file__))}
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'PASS Core 3 cross-module call_ref/return_call_ref: {len(rows)} checks', flush=True)


if __name__ == '__main__':
    main()
