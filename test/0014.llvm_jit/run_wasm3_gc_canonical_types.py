#!/usr/bin/env python3
"""Check Core 3 GC canonical type casts and tests in each runtime mode.

The WAT cases reduce the pinned Core 3 ref_cast.wast canonical-type scenario:
two identical sibling types must be interchangeable, while an i64-field
sibling must fail both ref.test and ref.cast. A Wasmtime run is the oracle.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

from run_wasm3_multi_memory import configurations


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, log, timeout=60):
    try:
        result = subprocess.run(command, capture_output=True, timeout=timeout)
        output = result.stdout + result.stderr
        code = result.returncode
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or b'') + (exc.stderr or b'') + b'\nTIMEOUT\n'
        code = 'timeout'
    log.write_bytes(output)
    return code, output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixtures', type=Path, help='directory containing the two WAT files')
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--configuration', help='run only one UWVM configuration')
    parser.add_argument('--source-id', help='verified source fingerprint for a staged binary')
    parser.add_argument('--combine-matrix', action='store_true',
                        help='also test all four interpreter combine levels with delay-local on/off; requires an all-combine build')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=True)

    fixtures = {}
    files = []
    for case in ('positive', 'negative'):
        wat = args.fixtures / f'wasm3_gc_canonical_{case}.wat'
        wasm = args.output / f'wasm3_gc_canonical_{case}.wasm'
        command = [str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)]
        code, output = run(command, args.output / f'{case}-parse.log')
        if code:
            raise RuntimeError(f'failed to parse {wat}: {output.decode(errors="replace")}')
        command = [str(args.wasm_tools), 'validate', str(wasm)]
        code, output = run(command, args.output / f'{case}-validate.log')
        if code:
            raise RuntimeError(f'failed to validate {wasm}: {output.decode(errors="replace")}')
        fixtures[case] = wasm
        files.append(dict(case=case, wat=str(wat), wat_sha256=sha256(wat),
                          wasm=str(wasm), wasm_sha256=sha256(wasm)))

    build_json = args.uwvm.parent / 'build.json'
    source_id = json.loads(build_json.read_text()).get('source_id') if build_json.exists() else None
    if args.source_id and source_id and args.source_id != source_id:
        raise ValueError('staged binary source ID conflicts with build.json')
    source_id = args.source_id or source_id
    configs = configurations(args.uwvm, args.ros)
    if args.combine_matrix:
        # The eight tuning combinations apply only to the interpreter. Keep
        # ordinary full/lazy/verified and ROS full as separate runtime modes.
        int_configs = [(name, command) for name, command in configs if name.startswith('int-')]
        for name, command in int_configs:
            for level in ('disable', 'soft', 'heavy', 'extra'):
                for no_delay in (False, True):
                    suffix = f'combine-{level}-' + ('no-delay' if no_delay else 'delay')
                    tuning = ['-Rint-op-conbine-level', level]
                    if no_delay:
                        tuning.append('-Rint-no-delay-local')
                    configs.append((f'{name}-{suffix}', command[:-1] + tuning + command[-1:]))
    if args.configuration:
        configs = [config for config in configs if config[0] == args.configuration]
        if not configs:
            raise ValueError(f'unknown configuration: {args.configuration}')
    rows = []
    for name, command in [('wasmtime', [str(args.wasmtime), 'run', '-C', 'cache=n',
                                           '-W', 'gc=y'])] + configs:
        for case, wasm in fixtures.items():
            base = [part for part in command if part != '-WFE-multi-memory']
            if name == 'wasmtime':
                cmd = base + [str(wasm)]
            else:
                # configurations() ends in --run; add the precise GC feature gate.
                cmd = base[:-1] + ['-WFE-gc', '--run', str(wasm)]
            log = args.output / f'{name}-{case}.log'
            code, output = run(cmd, log)
            if case == 'positive':
                passed = code == 0
            elif name == 'wasmtime':
                passed = isinstance(code, int) and code != 0 and b'cast failure' in output
            else:
                passed = isinstance(code, int) and code != 0 and b'reference cast failed' in output
            rows.append(dict(configuration=name, case=case, exit=code, passed=passed,
                             command=cmd, log=str(log)))
            print(('PASS' if passed else 'FAIL'), name, case, f'exit={code}', flush=True)

    summary = dict(source_id=source_id, uwvm=str(args.uwvm), uwvm_sha256=sha256(args.uwvm),
                   wasmtime=str(args.wasmtime), wasmtime_sha256=sha256(args.wasmtime),
                   ros=args.ros, combine_matrix=args.combine_matrix, fixtures=files,
                   configurations=len(configs), runs=rows,
                   passed=sum(row['passed'] for row in rows), failed=sum(not row['passed'] for row in rows))
    (args.output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    if summary['failed']:
        raise RuntimeError(f"{summary['failed']} canonical GC cases failed; see {args.output / 'results.json'}")


if __name__ == '__main__':
    main()
