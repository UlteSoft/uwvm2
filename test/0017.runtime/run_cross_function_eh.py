#!/usr/bin/env python3
"""Run the same escaping Core 3 exception through interpreter and JIT full modes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', required=True, type=Path)
    parser.add_argument('--wasm', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--jit-only', action='store_true',
                        help='For a Windows LLVM-only PE with interpreter disabled')
    args = parser.parse_args()
    uwvm = args.uwvm.resolve(strict=True)
    wasm = args.wasm.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    if args.ros:
        modes = [] if args.jit_only else [('int-full', ['-Rint'])]
        modes += [(f'jit-full-{policy}', ['-Raot', '-Rct', '0',
                   '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable'])
                  for policy in ('instruction', 'unwind')]
    else:
        modes = [] if args.jit_only else [
            ('int-full', ['-Rcc', 'int', '-Rcm', 'full']),
            ('int-lazy', ['-Rcc', 'int', '-Rcm', 'lazy']),
            ('int-lazy-verified', ['-Rcc', 'int', '-Rcm', 'lazy+verification'])]
        for policy in ('instruction', 'unwind'):
            modes.extend((
                (f'jit-full-{policy}', ['-Rcc', 'jit', '-Rcm', 'full',
                    '-Rct', '0', '-Rllvm-call-stack', policy,
                    '-Rllvm-cache-path', 'disable']),
                (f'jit-lazy-{policy}', ['-Rcc', 'jit', '-Rcm', 'lazy',
                    '-Rct', '0', '-Rllvm-call-stack', policy,
                    '-Rllvm-cache-path', 'disable']),
                (f'tiered-t0-enabled-{policy}', ['-Rcc', 'tiered', '-Rcm', 'lazy',
                    '-Rtiered-disable-t2', '-Rct', '0',
                    '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable']),
                (f'tiered-t1-forced-{policy}', ['-Rcc', 'tiered', '-Rcm', 'lazy',
                    '-Rtiered-disable-t0', '-Rtiered-disable-t2',
                    '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable'])))
    if args.jit_only:
        modes = [row for row in modes if row[0].startswith('jit-full-')]
    rows = []
    for name, mode in modes:
        for gate in ('enabled', 'exceptions-off', 'gc-off', 'function-references-off'):
            features = ['-WFE-gc', '-WFE-exceptions', '-WFE-function-references']
            if gate == 'exceptions-off':
                features = ['-WFE-gc', '-WFD-exceptions', '-WFE-function-references']
            elif gate == 'gc-off':
                features = ['-WFD-gc', '-WFE-exceptions', '-WFE-function-references']
            elif gate == 'function-references-off':
                features = ['-WFE-gc', '-WFE-exceptions', '-WFD-function-references']
            compile_log = output / f'{name}-compile.log'
            trace = (['-Rclog', 'file', str(compile_log)]
                     if gate == 'enabled' and name.startswith('tiered-') else [])
            command = [str(uwvm), '-m', 'run', *mode, *trace,
                       *features, '--run', str(wasm)]
            begin = time.monotonic()
            proc = subprocess.run(command, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=90)
            log = output / f'{name}-{gate}.log'
            log.write_bytes(proc.stdout)
            text = proc.stdout.decode('utf-8', errors='replace').lower()
            disabled = ('exceptions' if gate == 'exceptions-off' else
                        'function-references' if gate == 'function-references-off' else None)
            passed = (proc.returncode == 0 if disabled is None else
                      proc.returncode > 0 and disabled in text)
            backend_proof = None
            if trace:
                compile_text = compile_log.read_text() if compile_log.exists() else ''
                expected = ('uwvm-int-lazy' if name.startswith('tiered-t0-')
                            else 'llvm-jit-lazy')
                other = ('llvm-jit-lazy' if expected == 'uwvm-int-lazy'
                         else 'uwvm-int-lazy')
                summary = re.search(r'\[tiered-lazy\] summary [^\n]*\bfunctions=(\d+) '
                                    r'compiled=(\d+)\b', compile_text)
                compiled_functions = set(re.findall(r'\[' + expected +
                    r'\] compile-end [^\n]*\bfn=(\d+)\b', compile_text))
                if expected == 'uwvm-int-lazy':
                    backend_proof = (len(compiled_functions) >= 2 and
                        summary is not None and int(summary.group(2)) == 0)
                else:
                    backend_proof = (bool(compiled_functions) and
                        summary is not None and int(summary.group(1)) >= 2 and
                        int(summary.group(2)) == int(summary.group(1)))
                backend_proof = backend_proof and (
                    '[' + other + '] compile-end' not in compile_text)
                passed = passed and backend_proof
            rows.append({'name': name, 'gate': gate, 'command': command,
                         'exit_code': proc.returncode, 'passed': passed,
                         'seconds': round(time.monotonic() - begin, 3),
                         'log_sha256': digest(log),
                         'backend_proof': backend_proof,
                         'compile_log_sha256': (digest(compile_log)
                                                if compile_log.exists() else None)})
    result = {'schema': 1, 'wasm_sha256': digest(wasm), 'uwvm_sha256': digest(uwvm),
              'ros': args.ros, 'jit_only': args.jit_only,
              'passed': all(row['passed'] for row in rows), 'runs': rows}
    (output / 'result.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': result['passed'],
                      'runs': [(row['name'], row['gate'], row['exit_code'],
                                row['passed']) for row in rows]}, sort_keys=True))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
