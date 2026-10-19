#!/usr/bin/env python3
"""One untimed correctness run per engine/artifact in the existing Linux cgroup.

This does not run a performance benchmark, collect PMU events, or rewrite a
producer module. A rejected GC module is recorded separately from usable cases.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def admission():
    assert os.uname().sysname == 'Linux'
    membership = Path('/proc/self/cgroup').read_text().strip()
    assert membership.startswith('0::/system.slice/docker-')
    cgroup = Path('/sys/fs/cgroup') / membership[3:].lstrip('/')
    assert (cgroup / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cgroup / 'memory.swap.max').read_text().strip() == '0'
    return {'membership': membership, 'memory_max': 64 << 30, 'swap_max': 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--engine', action='append', choices=['wasmtime', 'wavm', 'wasmedge'])
    parser.add_argument('--case', action='append')
    parser.add_argument('--cpu', type=int, default=23)
    parser.add_argument('--wasmedge-mode', choices=['interpreter', 'jit'], default='jit')
    args = parser.parse_args()
    guard = admission()
    assert args.cpu in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    selected = args.case or ['assemblyscript', 'assemblyscript-release', 'javascript-small',
                             'moonbit-wasm', 'moonbit-wasm-gc', 'java-full', 'csharp-default']
    cases = [case for case in json.loads(args.manifest.read_text()) if case['name'] in selected]
    assert {case['name'] for case in cases} == set(selected), 'manifest case missing'
    sdk = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
    specs = {
        'wasmtime': {'binary': Path('/home/macromodel/.wasmtime/bin/wasmtime'),
                     'arguments': ['run', '-C', 'cache=n,parallel-compilation=n', '-W',
                                   'gc=y,function-references=y,tail-call=y,extended-const=y,exceptions=y,memory64=y,multi-memory=y,relaxed-simd=y'],
                     'environment': {}, 'mode': 'Cranelift JIT, cache disabled'},
        'wavm': {'binary': Path('/home/macromodel/Documents/src/WAVM/build-clang/bin/wavm'),
                 'arguments': ['run', '--nocache', '--abi=wasi', '--enable', 'all-proposed'],
                 'environment': {'LD_LIBRARY_PATH': f'{sdk}/lib/x86_64-unknown-linux-gnu:{sdk}/lib'},
                 'mode': 'LLVM JIT, cache disabled, all-proposed features'},
        'wasmedge': {'binary': args.arena / 'engines/wasmedge-0.18.0/bin/wasmedge',
                     'arguments': ['--wasm-3', '--run-mode', args.wasmedge_mode, '--log-level', 'error'],
                     'environment': {}, 'mode': args.wasmedge_mode},
    }
    result = {'scope': 'Untimed correctness smoke only; not a performance comparison',
              'admission': guard, 'cpu': args.cpu, 'manifest_sha256': digest(args.manifest), 'runs': []}
    for engine in args.engine or list(specs):
        spec = specs[engine]
        binary = spec['binary']
        binary_hash = digest(binary)
        for case in cases:
            path = Path(case['wasm'])
            before = digest(path)
            argv = [str(binary), *spec['arguments'], str(path), *case.get('args', [])]
            folder = args.out / engine / case['name']
            folder.mkdir(parents=True)
            row = {'engine': engine, 'case': case['name'], 'argv': argv,
                   'environment_overrides': spec['environment'], 'mode': spec['mode'],
                   'binary_sha256': binary_hash, 'wasm_sha256': before}
            with (folder / 'stdout.txt').open('wb') as stdout, (folder / 'stderr.txt').open('wb') as stderr:
                process = subprocess.Popen(argv, env=os.environ | spec['environment'], stdout=stdout,
                                           stderr=stderr, start_new_session=True)
                try:
                    row['exit'] = process.wait(timeout=90)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    row.update(exit='timeout', classification='timeout')
            row['stdout'] = (folder / 'stdout.txt').read_text(errors='replace')
            row['stderr'] = (folder / 'stderr.txt').read_text(errors='replace')
            row['passed'] = (row['exit'] == case.get('expected_exit', 0)
                             and row['stdout'] == case['expected_stdout'])
            if 'expected_stderr' in case:
                row['passed'] &= row['stderr'] == case['expected_stderr']
            row.setdefault('classification', 'correct_output' if row['passed'] else 'failed')
            row['inputs_unchanged'] = digest(binary) == binary_hash and digest(path) == before
            assert row['inputs_unchanged']
            result['runs'].append(row)
            (args.out / 'report.json').write_text(json.dumps(result, indent=2))
            print(json.dumps({key: row[key] for key in ['engine', 'case', 'exit', 'passed', 'classification']}), flush=True)
    result['admission_after'] = admission()
    result['all_correct'] = all(row['passed'] for row in result['runs'])
    (args.out / 'report.json').write_text(json.dumps(result, indent=2))
    raise SystemExit(0 if result['all_correct'] else 1)


if __name__ == '__main__':
    main()
