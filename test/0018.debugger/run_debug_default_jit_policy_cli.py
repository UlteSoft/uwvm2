#!/usr/bin/env python3
"""Verify effective JIT policy through real Linux VM logs and debug pauses."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from run_wasm_operand_preview_cli import OperandConsole


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--repo', choices=('uwvm2', 'uwvm2-ros'), required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--runner-prefix-json', type=Path)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(a.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    runner = json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(v, str) and v for v in runner)
    wat = a.out / 'policy.wat'
    wat.write_text('(module (global $v (mut i64) (i64.const 37)) '
                   '(func (export "_start") (local $n i64) '
                   'global.get $v i64.const 5 i64.add local.set $n '
                   'local.get $n global.set $v nop))\n')
    wasm = wat.with_suffix('.wasm')
    subprocess.run([str(a.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(a.wasm_tools), 'validate', str(wasm)], check=True)
    cases = [
        ('ordinary-default', False, [], ('aot:max', 'passbuilder-tuned', 'aggressive')),
        ('session-default', True, [], ('session:debug', 'none', 'none')),
        ('session-full-auto', True, ['-Rllvm-full-policy', 'auto'], ('session:debug', 'none', 'none')),
        ('session-general-default', True, ['-Rllvm-policy', 'default'], ('session:debug', 'none', 'none')),
        ('session-explicit-max', True, ['-Rllvm-policy', 'max'], ('policy:max', 'passbuilder-tuned', 'aggressive')),
        ('session-explicit-full-o1', True, ['-Rllvm-full-policy', 'pb-o1'], ('full:pb-o1', 'passbuilder-tuned', 'less')),
        ('session-explicit-full-debug', True, ['-Rllvm-full-policy', 'debug'], ('full:debug', 'none', 'none')),
        ('ordinary-explicit-debug', False, ['-Rllvm-policy', 'debug'], ('policy:debug', 'none', 'none')),
    ]
    rows = []
    OperandConsole.prompt_timeout = 120
    for name, debug, options, expected in cases:
        mode = ['-Rdbg'] if debug else (['-Rcc', 'jit', '-Rcm', 'full'] if a.repo == 'uwvm2' else [])
        argv = [*runner, str(a.binary), *mode, '-Rct', '0', '-Rllvm-cache-path', 'disable',
                '-Rclog', 'err', *options, '--run', str(wasm)]
        row = {'case': name, 'actual_VM': True, 'argv': argv, 'expected': expected,
               'binary_sha256': hashlib.sha256(a.binary.read_bytes()).hexdigest()}
        c = None
        try:
            if debug:
                c = OperandConsole(argv, a.out / (name + '.log'))
                c.prompt()
                assert b'prepared; no Wasm instruction executed' in c.send('status'), c.transcript[-4000:]
                c.send('continue')
                deadline = time.monotonic() + 20
                while True:
                    reply = c.send('status')
                    if b'exited' in reply:
                        break
                    assert time.monotonic() < deadline, reply
                    time.sleep(.01)
                c.close()
                output = bytes(c.transcript)
                c = None
            else:
                completed = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
                output = completed.stdout
                (a.out / (name + '.log')).write_bytes(output)
                assert completed.returncode == 0, (completed.returncode, output[-4000:])
            policies = re.findall(rb'\[llvm-jit-full\] optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)', output)
            assert policies, output[-4000:]
            observed = [tuple(v.decode() for v in fields) for fields in policies]
            assert all(v == expected for v in observed), (name, observed, expected)
            row.update(passed=True, observed=observed)
        except Exception as error:
            row.update(passed=False, error=repr(error))
        finally:
            if c is not None:
                try:
                    c.close()
                except Exception as error:
                    row['cleanup_error'] = repr(error)
                    row['passed'] = False
            rows.append(row)
            (a.out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(name, 'PASS' if row['passed'] else row.get('error'), flush=True)
    assert all(v['passed'] for v in rows), [v for v in rows if not v['passed']]


if __name__ == '__main__':
    main()
