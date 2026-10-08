#!/usr/bin/env python3
"""Require clean managed quit from actual paused full-JIT Wasm activations.

Large local frames reproduce the RISC-V shutdown exception regression without
using ASM commands or granting access to VM code. Run in the Linux test cgroup.
"""
import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from run_wasm_operand_preview_cli import OperandConsole


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'binary', 'wasm-tools', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--runner-prefix-json', type=Path)
    parser.add_argument('--local-count', type=int, action='append')
    parser.add_argument('--call-stack-policy', action='append', choices=('instruction', 'unwind'))
    parser.add_argument('--prompt-timeout', type=int, default=600)
    parser.add_argument('--stop-timeout', type=int, default=120)
    args = parser.parse_args()
    counts = args.local_count or (1, 10000)
    assert len(set(counts)) == len(counts) and all(1 <= n <= 10000 for n in counts)
    assert 1 <= args.prompt_timeout <= 600 and 1 <= args.stop_timeout <= 120
    runner = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(x, str) and x and '\0' not in x for x in runner)
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    OperandConsole.prompt_timeout = args.prompt_timeout
    args.out.mkdir(parents=True, exist_ok=False)
    results = []
    for count in counts:
        wat = args.out / f'locals-{count}.wat'
        wasm = wat.with_suffix('.wasm')
        wat.write_text('(module (func (export "_start") (local $value i32)' +
                       ' (local i32)' * (count - 1) +
                       ' i32.const 17 local.set $value (loop $again'
                       ' local.get $value i32.const 41 i32.xor local.set $value'
                       ' local.get $value i32.const 1 i32.rotl local.set $value br $again)))\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), 'validate', str(wasm)], check=True)
        for policy in args.call_stack_policy or ('instruction', 'unwind'):
            name = f'locals-{count}-{policy}'
            mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
            argv = [*runner, str(args.binary), '-Rdbg', *mode, '-Rct', '0',
                    '-Rllvm-cache-path', 'disable', '-Rllvm-call-stack', policy,
                    '--run', str(wasm)]
            row = dict(local_count=count, policy=policy, argv=argv, actual_VM=True,
                       fixture_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest())
            console = OperandConsole(argv, args.out / (name + '.log'))
            try:
                console.prompt()
                assert b'registered' in console.send('break 0 0 0')
                console.send('continue')
                deadline = time.monotonic() + args.stop_timeout
                while True:
                    reply = console.send('status')
                    if b'stopped: breakpoint' in reply:
                        break
                    assert time.monotonic() < deadline, reply
                    time.sleep(.01)
                thread, function, offset = console.location()
                assert (function, offset) == (0, 0)
                row['actual_thread'] = thread
                row['breakpoint_observed'] = True
            finally:
                try:
                    console.close()  # strict return-code and retirement oracle
                finally:
                    row['returncode'] = console.child.returncode
                    row['passed'] = row.get('breakpoint_observed', False) and row['returncode'] == 0
                    results.append(row)
                    (args.out / 'result.json').write_text(json.dumps(results, indent=2) + '\n')
            print('PASS', name, 'managed quit', flush=True)


if __name__ == '__main__':
    main()
