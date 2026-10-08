#!/usr/bin/env python3
"""Compare qualified native mmap leaves from two retained JIT codegen runs.

The full-JIT emitter intentionally salts module symbol names with the source
identity. This check compares every instruction byte in the actual leaf, while
the producing check_exception_cross_codegen.py separately verifies feature
on/off executable sections, relocations, and absence of hot-path guards.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def leaf(path: Path) -> bytes:
    lines = path.read_text().splitlines()
    if not any('file format elf64-x86-64' in line for line in lines):
        raise ValueError(f'not qualified x86-64 disassembly: {path}')
    if sum(bool(re.search(r'<uwvm_m_[0-9a-f]+_func_0>:', line)) for line in lines) != 1:
        raise ValueError(f'missing unique memory leaf: {path}')
    encoded = []
    for line in lines:
        match = re.match(r'^\s*[0-9a-f]+:\s*((?:[0-9a-f]{2}(?:\s+|$))+)', line)
        if match:
            encoded.extend(int(byte, 16) for byte in match.group(1).split())
    if not encoded:
        raise ValueError(f'empty memory leaf: {path}')
    return bytes(encoded)


def qualified(directory: Path) -> dict:
    report = json.loads((directory / 'summary.json').read_text())
    if not report.get('passed') or report.get('generated_optimization') != 'pb-o3':
        raise ValueError(f'unqualified native codegen report: {directory}')
    for policy in ('instruction', 'unwind'):
        relevant = [entry for entry in report['checks'] if entry['policy'] == policy]
        if not any(entry['kind'] == 'memory-feature-on-off' and
                   entry['equal_all_executable_sections'] and
                   entry['equal_complete_relocations'] for entry in relevant):
            raise ValueError(f'missing complete on/off code identity: {directory}/{policy}')
        if not any(entry['kind'] == 'protected-caller-memory-leaf' and
                   entry['identical_leaf'] for entry in relevant):
            raise ValueError(f'missing protected-caller leaf check: {directory}/{policy}')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(os.environ.get('UWVM_SOURCE_ROOT', Path(__file__).resolve().parents[2]))
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    before_report = qualified(args.before)
    after_report = qualified(args.after)
    if before_report['product'] != after_report['product']:
        raise ValueError('compared JIT objects are from different products')
    rows = []
    for policy in ('instruction', 'unwind'):
        for fixture in ('memory-loop', 'protected-normal'):
            relative = Path(f'{fixture}-{policy}-on/func-0.s')
            older, newer = leaf(args.before / relative), leaf(args.after / relative)
            if older != newer:
                raise ValueError(f'native memory leaf changed: {relative}')
            rows.append({'fixture': fixture, 'policy': policy,
                         'machine_bytes': len(newer),
                         'sha256': hashlib.sha256(newer).hexdigest()})
    result = {'passed': True, 'product': before_report['product'],
              'before_binary_sha256': before_report['binary_sha256'],
              'after_binary_sha256': after_report['binary_sha256'],
              'checks': rows}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS {len(rows)} actual mmap leaf byte comparisons: {result["product"]}')


if __name__ == '__main__':
    main()
