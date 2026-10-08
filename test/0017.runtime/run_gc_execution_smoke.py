#!/usr/bin/env python3
"""Execute Core 3 GC i31/struct/array witnesses against Wasmtime and both VMs.

Run inside the Linux test cgroup.  --oracle-only qualifies the exact fixtures
before the VM GC execution path is ready; it is not a VM acceptance result.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


CASES = ('gc_i31_execution', 'gc_ref_null_abstract_execution', 'gc_ref_null_defined_execution',
         'gc_struct_execution', 'gc_array_execution', 'gc_ref_eq_execution', 'gc_cast_execution',
         'gc_abstract_cast_no_type_execution', 'gc_branch_cast_execution',
         'gc_branch_convert_execution', 'gc_block_abstract_execution',
         'gc_array_segments_execution', 'gc_array_fill_copy_execution',
         'gc_i31_ring_barrier_execution',
         'gc_extern_conversion_execution', 'gc_const_expr_execution')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--oracle-only', action='store_true')
    parser.add_argument('--cases', nargs='+', choices=CASES, default=CASES,
                        help='run a focused execution subset while aggregate lowering is being integrated')
    parser.add_argument('--combine-matrix', action='store_true',
                        help='also run all interpreter combination levels with delay-local on/off')
    parser.add_argument('--timeout-seconds', type=float, default=120.0,
                        help='bound each command; timeouts are recorded as failed checks and the matrix continues')
    args = parser.parse_args()
    if args.timeout_seconds <= 0:
        parser.error('--timeout-seconds must be positive')
    if not args.oracle_only and args.uwvm is None:
        parser.error('--uwvm is required unless --oracle-only is set')
    # --combine-matrix requires a binary compiled with all combine/delay
    # variants. The ROS full interpreter can use that qualified binary too.
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    modes = {'int-full': ['-Rint'], 'jit-full': ['-Raot']} if args.ros else {
        'int-full': ['-Rcc', 'int', '-Rcm', 'full'],
        'int-lazy': ['-Rcc', 'int', '-Rcm', 'lazy'],
        'int-lazy-verified': ['-Rcc', 'int', '-Rcm', 'lazy+verification'],
        'jit-full': ['-Rcc', 'jit', '-Rcm', 'full'],
        'jit-lazy': ['-Rcc', 'jit', '-Rcm', 'lazy'],
        'jit-lazy-verified': ['-Rcc', 'jit', '-Rcm', 'lazy+verification'],
        'tiered-lazy': ['-Rcc', 'tiered', '-Rcm', 'lazy', '-Rct', '0'],
        'tiered-lazy-verified': ['-Rcc', 'tiered', '-Rcm', 'lazy+verification', '-Rct', '0'],
    }

    def run(name, phase, command, expect_success=True, required_text=None):
        timed_out = False
        try:
            process = subprocess.run([str(part) for part in command], capture_output=True,
                                     timeout=args.timeout_seconds)
            output = process.stdout + process.stderr
            exit_code = process.returncode
        except subprocess.TimeoutExpired as failure:
            output = (failure.stdout or b'') + (failure.stderr or b'')
            exit_code = -124
            timed_out = True
        (args.out / (name + '.log')).write_bytes(output)
        passed = not timed_out and (exit_code == 0) == expect_success
        if required_text is not None:
            passed = passed and required_text in output
        if phase == 'feature-off':
            # The policy gate may run in either the legacy or Core 3 validator.
            # Both must reject before execution; their diagnostic wording differs.
            passed = passed and exit_code != 0
        rows.append({'name': name, 'phase': phase, 'command': [str(part) for part in command],
                     'exit': exit_code, 'timed_out': timed_out, 'passed': passed})
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            print(f'FAIL {name}: exit={exit_code}; {output.decode(errors="replace")[-500:]}', flush=True)
        return passed

    fixtures = {}
    for case in args.cases:
        wat = root / 'test/0017.runtime/fixtures' / (case + '.wat')
        wasm = args.out / (case + '.wasm')
        fixtures[case] = {'wat_sha256': digest(wat)}
        if not run(case + '-parse', 'oracle', [args.wasm_tools, 'parse', wat, '-o', wasm]):
            raise SystemExit(1)
        fixtures[case]['wasm_sha256'] = digest(wasm)
        if not run(case + '-validate', 'oracle', [args.wasm_tools, 'validate', '--features', 'all', wasm]):
            raise SystemExit(1)
        wasmtime_features = ['-W', 'gc=y']
        if case == 'gc_const_expr_execution':
            # ref.i31/struct.new/array.new_fixed are GC constant instructions;
            # the separate extended-const switch controls numeric operations.
            wasmtime_features += ['-W', 'extended-const=n']
        if not run(case + '-wasmtime', 'oracle',
                   [args.wasmtime, '-C', 'cache=n', *wasmtime_features, wasm]):
            raise SystemExit(1)
        if case == 'gc_array_segments_execution':
            for trap_name in ('trap_data', 'trap_elem'):
                if not run(case + '-' + trap_name + '-wasmtime', 'oracle',
                           [args.wasmtime, 'run', '-C', 'cache=n', '-W', 'gc=y',
                            '--invoke', trap_name, wasm], False, b'wasm trap:'):
                    raise SystemExit(1)
        if args.oracle_only:
            continue
        feature_flags = ['-WFE-gc']
        if case == 'gc_const_expr_execution':
            feature_flags.append('-WFD-extended-const')
        for mode, flags in modes.items():
            run(case + '-' + mode, 'product', [args.uwvm, *flags, *feature_flags, '--run', wasm])
            if case == 'gc_array_segments_execution':
                for trap_name, local_index in (('trap_data', 2), ('trap_elem', 3)):
                    run(case + '-' + trap_name + '-' + mode, 'product',
                        [args.uwvm, *flags, '-WFE-gc', '--wasm-set-start-func',
                         str(local_index), '--run', wasm], False, b'array access out of bounds')
        if args.combine_matrix:
            for level in ('disable', 'soft', 'heavy', 'extra'):
                for no_delay in (False, True):
                    tuning = ['-Rint-op-conbine-level', level] + (['-Rint-no-delay-local'] if no_delay else [])
                    label = f'{level}-' + ('no-delay' if no_delay else 'delay')
                    for mode in (('int-full',) if args.ros else ('int-full', 'int-lazy', 'int-lazy-verified')):
                        run(case + '-' + mode + '-' + label, 'combine-matrix',
                            [args.uwvm, *modes[mode], *tuning, *feature_flags, '--run', wasm])
        run(case + '-gc-off', 'feature-off',
            [args.uwvm, *modes['int-full'], '-WFD-gc', '--run', wasm], False)
        if case == 'gc_const_expr_execution':
            run(case + '-extended-const-off', 'product',
                [args.uwvm, *modes['int-full'], '-WFE-gc', '-WFD-extended-const', '--run', wasm],
                True)
            run(case + '-gc-off-extended-const-off', 'feature-off',
                [args.uwvm, *modes['int-full'], '-WFD-gc', '-WFD-extended-const', '--run', wasm],
                False, b'--wasm-feature-enable-gc')
            run(case + '-function-references-off', 'product',
                [args.uwvm, *modes['int-full'], *feature_flags,
                 '-WFD-function-references', '--run', wasm])
    summary = {'passed': all(row['passed'] for row in rows),
               'scope': 'oracle-only' if args.oracle_only else ('uwvm2-ros' if args.ros else 'uwvm2'),
               'checks': len(rows), 'fixtures': fixtures,
               'runner_sha256': digest(Path(__file__)),
               'wasm_tools_sha256': digest(args.wasm_tools), 'wasmtime_sha256': digest(args.wasmtime)}
    if args.uwvm is not None:
        summary['uwvm_sha256'] = digest(args.uwvm)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f"Core 3 GC execution {summary['scope']}: {sum(row['passed'] for row in rows)}/{len(rows)}", flush=True)
    if not summary['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
