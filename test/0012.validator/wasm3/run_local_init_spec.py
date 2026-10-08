#!/usr/bin/env python3
"""Test only the definite-local-initialization pass against original Core 3 modules."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--wast', type=Path, required=True)
    p.add_argument('--wasm-tools', required=True)
    p.add_argument('--wasmtime', required=True)
    p.add_argument('--checker', required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    assert hashlib.sha256(a.wast.read_bytes()).hexdigest() == '4a496f820a6863d429c772f4ac85018944fa96e1f1cec6df9fc9231c9795890d'
    a.out.mkdir(parents=True, exist_ok=True)
    script = a.out / 'script.json'
    subprocess.run([a.wasm_tools, 'json-from-wast', str(a.wast), '--wasm-dir', str(a.out), '-o', str(script)], check=True)
    rows = []
    for number, cmd in enumerate(json.loads(script.read_text())['commands']):
        if cmd['type'] not in ('module', 'assert_invalid'):
            continue
        if cmd['type'] == 'assert_invalid':
            assert cmd['text'] == 'uninitialized local', cmd
        path = a.out / Path(cmd['filename']).name
        directory = a.out / f'case-{number}'
        directory.mkdir(exist_ok=True)
        ours = subprocess.run([a.checker, str(path)], capture_output=True, timeout=60)
        oracle = subprocess.run([a.wasmtime, 'compile', '-C', 'cache=n', '-W', 'gc=y,function-references=y',
                                 str(path), '-o', str(directory / 'oracle.cwasm')], capture_output=True, timeout=60)
        for name, proc in (('init-pass', ours), ('wasmtime', oracle)):
            (directory / f'{name}.stdout').write_bytes(proc.stdout)
            (directory / f'{name}.stderr').write_bytes(proc.stderr)
        expected = 0 if cmd['type'] == 'module' else 1
        row = {'command': number, 'line': cmd['line'], 'expected': expected, 'init_exit': ours.returncode, 'wasmtime_exit': oracle.returncode}
        rows.append(row)
        (a.out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert ours.returncode == oracle.returncode == expected, row
        if expected:
            assert b'uninitialized local' in ours.stdout and b'uninitialized' in oracle.stderr, row
    assert len(rows) == 6
    print('PASS Core 3 local_init.wast: 6 original module assertions (definite-initialization pass only)')


if __name__ == '__main__':
    main()
