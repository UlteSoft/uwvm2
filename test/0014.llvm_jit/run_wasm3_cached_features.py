#!/usr/bin/env python3
"""Reject disabled Core 3 features even after a compatible JIT object has been cached."""
import argparse
import json
from pathlib import Path
import re
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uwvm', type=Path)
    parser.add_argument('initializers', type=Path)
    parser.add_argument('relaxed', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--multi-memory', type=Path, help='new Core 3 explicit-memory fixture')
    parser.add_argument('--threads-fence', type=Path, help='threads atomic.fence fixture')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    rows = []
    fixtures = [
        ('initializers', args.initializers, ['extended-const', 'table-initializer']),
        ('relaxed', args.relaxed, ['relaxed-simd']),
    ]
    if args.multi_memory:
        fixtures.append(('multi-memory', args.multi_memory, ['multi-memory']))
    if args.threads_fence:
        fixtures.append(('threads-fence', args.threads_fence, ['threads']))
    for name, wasm, features in fixtures:
        cache = args.output / (name + '-cache')
        cache.mkdir(exist_ok=False)
        base = [str(args.uwvm.resolve())]
        base += ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        base += ['-Rllvm-cache-path', 'path', str(cache.resolve())]
        for phase, disabled in [('cold', None), ('warm', None), *[('disabled-' + f, f) for f in features]]:
            case = name + '-' + phase
            compile_log = args.output / (case + '.compile.log')
            switches = ['-WFE-' + f for f in features if f != disabled]
            if disabled is not None:
                switches += ['-WFD-' + disabled]
            command = [*base, '-Rclog', 'file', str(compile_log.resolve()), *switches, '--run', str(wasm.resolve())]
            result = subprocess.run(command, capture_output=True, timeout=60)
            output = ansi.sub('', (result.stdout + result.stderr).decode(errors='replace'))
            (args.output / (case + '.log')).write_text(output)
            compilation = compile_log.read_text() if compile_log.exists() else ''
            if disabled is None:
                if result.returncode != 0:
                    raise RuntimeError(f'{case}: execution failed: {output}')
                if phase == 'warm' and 'object-cache-hit' not in compilation:
                    raise RuntimeError(f'{case}: no verified object cache hit: {compilation}')
            elif result.returncode == 0 or '--wasm-feature-enable-' + disabled not in output or 'object-cache-hit' in compilation:
                raise RuntimeError(f'{case}: disabled feature reached cached execution: {output}\n{compilation}')
            rows.append({'case': case, 'exit': result.returncode, 'cache_hit': 'object-cache-hit' in compilation, 'passed': True})
    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(f'PASS {len(rows)} cached feature-policy cases')


if __name__ == '__main__':
    main()
