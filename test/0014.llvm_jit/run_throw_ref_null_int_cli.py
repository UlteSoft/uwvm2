#!/usr/bin/env python3
"""Compare the bounded, reachable Core 3 null throw_ref path with Wasmtime.

Only immediately adjacent ``ref.null exn/noexn; throw_ref`` is implemented by
the interpreter. A null throw_ref traps even under try_table; no guest exception
is created. Retained/non-null exception references remain a separate feature.
https://webassembly.github.io/spec/core/exec/instructions.html#control-instructions
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CASES = {
    'null-exn': '(module (func (export "_start") ref.null exn throw_ref))',
    'null-noexn': '(module (func (export "_start") ref.null noexn throw_ref))',
    'numeric-prefix': '(module (func (export "_start") i32.const 42 ref.null exn throw_ref))',
    'nested-null-trap': '''(module (tag $t)
      (func (export "_start") block $outer
        try_table (catch_all $outer) ref.null exn throw_ref end
        unreachable end))''',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--wasm-tools', required=True, type=Path)
    parser.add_argument('--wasmtime', required=True, type=Path)
    parser.add_argument('--uwvm', type=Path,
                        help='omit for a current-syntax Wasmtime reference preflight')
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--mode', choices=['full', 'lazy', 'lazy+verification'], action='append')
    args = parser.parse_args()
    modes = args.mode or (['full'] if args.ros else ['full', 'lazy', 'lazy+verification'])
    if args.ros and modes != ['full']:
        parser.error('ROS supports full only')
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    ansi = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
    rows = []

    def run(label, command, oracle):
        result = subprocess.run([str(x) for x in command], capture_output=True, timeout=60)
        raw = result.stdout + result.stderr
        (args.out / (label + '.log')).write_bytes(raw)
        output = ansi.sub('', raw.decode(errors='replace'))
        passed = (result.returncode != 0 and
                  ('wasm trap: null reference' in output if oracle else
                   'Runtime crash (' in output and 'null reference' in output.lower() and
                   'Uncaught WebAssembly exception' not in output))
        rows.append(dict(label=label, exit=result.returncode, passed=passed, oracle=oracle))
        (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if not passed:
            raise RuntimeError(f'{label}: expected null-reference trap, exit={result.returncode}\n{output}')

    for name, wat_text in CASES.items():
        wat = args.out / (name + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text(wat_text + '\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        run(name + '-wasmtime', [args.wasmtime, '-C', 'cache=n', '-W', 'exceptions=y', wasm], True)
        if args.uwvm:
            for mode in modes:
                flags = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', mode]
                run(name + '-' + mode,
                    [args.uwvm] + flags + ['-Rct', '0', '-WFE-function-references',
                                           '-WFE-exceptions', '--run', wasm], False)
                if name in ('null-exn', 'null-noexn'):
                    disabled = subprocess.run(
                        [str(x) for x in [args.uwvm] + flags + ['-Rct', '0',
                          '-WFE-function-references', '-WFD-exceptions', '--run', wasm]],
                        capture_output=True, timeout=60)
                    raw = disabled.stdout + disabled.stderr
                    (args.out / (name + '-' + mode + '-disabled.log')).write_bytes(raw)
                    output = ansi.sub('', raw.decode(errors='replace'))
                    passed = disabled.returncode != 0 and '--wasm-feature-enable-exceptions' in output
                    rows.append(dict(label=name + '-' + mode + '-disabled', exit=disabled.returncode,
                                     passed=passed, oracle=False))
                    (args.out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
                    if not passed:
                        raise RuntimeError(f'{name}-{mode}-disabled: expected precise feature gate:\n{output}')
        print('PASS', name, flush=True)

    (args.out / 'summary.json').write_text(json.dumps(dict(
        passed=True, cases=len(CASES), runs=len(rows), modes=modes,
        sha256={name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()
                for name in ('wasm_tools', 'wasmtime', 'uwvm') if getattr(args, name)},
        scope='adjacent null exn/noexn throw_ref only'), indent=2) + '\n')
    print('PASS Core 3 interpreter null throw_ref:', len(CASES), 'cases,', len(rows), 'executions')


if __name__ == '__main__':
    main()
