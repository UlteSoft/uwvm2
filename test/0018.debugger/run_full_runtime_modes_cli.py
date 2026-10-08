#!/usr/bin/env python3
"""Run full backends against a pure Wasm fixture inside the Linux test cgroup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--wasm-tools', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(a.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    wat = a.out / 'answer.wat'
    wat.write_text('(module (func (export "_start") i32.const 6 i32.const 7 i32.mul i32.const 42 i32.ne if unreachable end))\n')
    wasm = a.out / 'answer.wasm'
    subprocess.run([str(a.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(a.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True)
    rows = []

    def run(label, args):
        result = subprocess.run([str(a.binary), *args], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=30,
                                preexec_fn=lambda: time.sleep(.15))
        (a.out / (label + '.log')).write_bytes(result.stdout)
        text = re.sub(rb'\x1b\[[0-9;]*m', b'', result.stdout).decode(errors='replace')
        return result.returncode, text

    modes = [('-Rint',), ('--runtime-int',), ('-Raot',), ('--runtime-aot',)] if a.ros else [
        ('-Rcc', 'int', '-Rcm', 'full'), ('-Rcc', 'jit', '-Rcm', 'full')]
    for i, mode in enumerate(modes):
        code, text = run('full-' + str(i), [*mode, '-Rct', '0', '-Rllvm-cache-path', 'disable', '--run', str(wasm)])
        assert code == 0, (mode, code, text)
        rows.append({'mode': mode, 'actual_VM': True, 'passed': True})
    if a.ros:
        code, help_text = run('help', ['--help', 'runtime'])
        assert code == 0 and '--runtime-int' in help_text and '--runtime-aot' in help_text, help_text
        for option in ('-Rcc', '-Rcm', '--runtime-compilation-mode', '--runtime-compiler'):
            assert re.search(re.escape(option) + r'(?=[\s|\]])', help_text) is None, (option, help_text)
            code, text = run('reject-' + option.lstrip('-'), [option, 'basic', '--run', str(wasm)])
            assert code != 0 and 'invalid parameter:' in text and option in text, (option, code, text)
            rows.append({'deleted_option': option, 'parser_rejected': True, 'passed': True})
    (a.out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    (a.out / 'inputs.json').write_text(json.dumps({'binary_sha256': hashlib.sha256(a.binary.read_bytes()).hexdigest(),
        'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'cgroup': Path('/proc/self/cgroup').read_text(), 'WASIp1': False}, indent=2) + '\n')
    print(json.dumps({'passed': True, 'actual_runs': len(modes), 'rows': rows}), flush=True)


if __name__ == '__main__':
    main()
