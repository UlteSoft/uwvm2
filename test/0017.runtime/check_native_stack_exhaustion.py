#!/usr/bin/env python3
"""Replay every self-contained core stack-exhaustion invocation on paired CLIs.

Unlike the general stateful WAST runner, this test also runs the later exhaustion
invocations in call/fac/skip-stack-guard-page: their recursive paths do not depend
on preceding successful actions. Ordinary lazy/tiered modes are optional; ROS
always stays full-only. Native Linux/Darwin guard diagnostics must be explicit.
"""
import argparse
import concurrent.futures
import importlib.util
import json
from pathlib import Path
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--full', type=Path, required=True)
    parser.add_argument('--ros', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--all-modes', action='store_true')
    parser.add_argument('--products', nargs='+', choices=['full', 'ros'], default=['full', 'ros'])
    parser.add_argument('--include-libunwind', action='store_true')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=True)
    helper_path = Path(__file__).resolve().parents[1] / '0012.validator/check_wasm2_execution.py'
    spec = importlib.util.spec_from_file_location('paired_execution', helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)

    def compile_text(name, text):
        wasm = args.out / (name + '.wasm')
        wasm.with_suffix('.wat').write_text(text)
        subprocess.run(['wat2wasm', '--debug-names', str(wasm.with_suffix('.wat')), '-o', str(wasm)], check=True)
        return str(wasm.resolve())

    spectest = compile_text('spectest', helper.SPECTEST)
    cases = []
    for source in ('call', 'fac', 'skip-stack-guard-page'):
        script = args.corpus / source / 'script.json'
        target = None
        for command in json.loads(script.read_text())['commands']:
            if command['type'] == 'module':
                target = script.parent / command['filename']
            elif command['type'] == 'assert_exhaustion':
                exports, imports, _ = helper.interface(target)
                if any(module != 'spectest' for module in imports):
                    raise RuntimeError('unexpected external state in exhaustion fixture')
                name = source + '-' + str(command['line'])
                wrapper = compile_text(name, helper.wrapper(exports, [command]))
                cases.append(dict(name=name, target=str(target.resolve()), wrapper=wrapper))
    if len(cases) != 13:
        raise RuntimeError(f'expected all 13 wg-2.0 exhaustion actions, got {len(cases)}')

    # No linear memory means the memory provider never installs its handler.
    for name, text in (
        ('no-memory-recursion', '(module (func $f (export "_start") call $f))'),
        ('no-memory-mutual', '(module (func $a (export "_start") call $b) (func $b call $a))'),
    ):
        cases.append(dict(name=name, wrapper=compile_text(name, text), target=None))

    profiles = [
        ('full', 'int-full', ['-Rcm', 'full', '-Rcc', 'int']),
        ('ros', 'int', ['-Rint']),
    ]
    for product in ('full', 'ros'):
        for policy in ('debug', 'pb-o1', 'pb-o3'):
            # The CLI calls the native libunwind strategy "unwind"; passing
            # the provider's library name would only test argument rejection.
            for tracking in (('none', 'instruction', 'unwind') if args.include_libunwind else ('none', 'instruction')):
                profiles.append((product, 'llvm-' + policy + '-' + tracking,
                                 ['-Raot', '-Rllvm-full-policy', policy, '-Rllvm-call-stack', tracking]))
    if args.all_modes:
        profiles.append(('full', 'int-lazy', ['-Rcm', 'lazy', '-Rcc', 'int']))
        for mode, switch in (('llvm-lazy', '--runtime-jit'), ('tiered', '--runtime-tiered')):
            for tracking in ('none', 'instruction'):
                profiles.append(('full', mode + '-' + tracking, [switch, '-Rllvm-call-stack', tracking]))

    profiles = [profile for profile in profiles if profile[0] in args.products]

    def run(job):
        case, (product, name, options) = job
        command = [str(getattr(args, product).resolve()), *options, '-Rct', '0',
                   '-Rllvm-cache-path', 'disable', '--wasm-feature-wasm2']
        if case['target']:
            command += ['--wasm-preload-library', spectest, 'spectest',
                        '--wasm-preload-library', case['target'], 'spec_target']
        command += ['--run', case['wrapper']]
        try:
            result = subprocess.run(command, capture_output=True, timeout=60)
            status, output = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            status, output = 124, (error.stdout or b'') + (error.stderr or b'')
        passed = status == 127 and b'Runtime crash: call stack exhausted (native stack guard).' in output
        (args.out / (case['name'] + '-' + product + '-' + name + '.log')).write_bytes(output)
        record = dict(case=case['name'], product=product, profile=name, status=status, passed=passed)
        if not passed:
            print(json.dumps(record), flush=True)
        return record

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(run, [(case, profile) for case in cases for profile in profiles]))
    (args.out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    failed = sum(not record['passed'] for record in results)
    print(json.dumps(dict(cases=len(cases), profiles=len(profiles), runs=len(results), failed=failed)))
    return int(failed != 0)


if __name__ == '__main__':
    raise SystemExit(main())
