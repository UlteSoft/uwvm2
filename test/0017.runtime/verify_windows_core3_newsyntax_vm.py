#!/usr/bin/env python3
"""Verify the real Win11 Core 3 syntax result against the staged PE/oracle."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


GATES = {'memory64': {'memory64': 'memory64'},
         'relaxed-simd': {'relaxed-simd': 'relaxed-simd',
                          'simd': 'illegal value type'},
         'exnref-table64': {'table64': 'table64',
                            'exceptions': 'exceptions',
                            'reference-types': 'reference-types',
                            'table-instructions': 'table-instructions'},
         'atomic-fence': {'threads': 'threads'}}


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--oracle', type=Path, required=True)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    build = json.loads((args.build / 'build.json').read_text())
    oracle_file = (args.oracle / 'summary.json').resolve(strict=True)
    oracle = json.loads(oracle_file.read_text())
    stage_dir = args.stage.resolve(strict=True)
    stage_file = stage_dir / 'stage.json'
    stage = json.loads(stage_file.read_text())
    qualification_file = stage_dir / 'qualification.json'
    qualification = json.loads(qualification_file.read_text())
    result_file = stage_dir / 'result.json'
    result = json.loads(result_file.read_text(encoding='utf-8-sig'))
    source_id = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(args.output.with_suffix('.source.json'))], text=True).strip()
    pe_sha = build['products']['uwvm.exe']['sha256']
    require(build['status'] == 'cross-built-awaiting-real-windows-vm' and
            source_id == build['source_id'] == oracle['source_id'] ==
            stage['source_id'] == qualification['source_id'] == result['source_id'] and
            pe_sha == stage['product_sha256'] == qualification['product_sha256'] and
            pe_sha.lower() == result['product_sha256'].lower() and
            digest(oracle_file) == stage['oracle_sha256'] == qualification['oracle_sha256'] and
            digest(qualification_file).lower() == result['qualification_sha256'].lower() and
            digest(stage_dir / 'bootstrap-command.txt') == stage['bootstrap_command_sha256'],
            'Win11 result differs from the exact source, PE, oracle, or bootstrap')
    for name, expected in stage['files'].items():
        require(digest(stage_dir / name) == expected,
                f'Win11 staged artifact changed: {name}')
    require(result.get('passed') is True and result.get('mode') == 'llvm-jit-full' and
            result.get('policies') == ['instruction', 'unwind'] and
            str(result.get('os', '')).lower().find('windows') >= 0 and
            str(result.get('architecture', '')).lower() in ('amd64', 'x86_64') and
            result.get('oracle_sha256') == digest(oracle_file),
            'Win11 new-syntax guest result is incomplete or failed')
    expected = {}
    for policy in ('instruction', 'unwind'):
        for stem, gates in GATES.items():
            expected[f'{stem}-{policy}'] = (True, '')
            for gate, diagnostic in gates.items():
                expected[f'{stem}-{policy}-{gate}-off'] = (False, diagnostic)
    rows = result.get('cases', [])
    require(len(rows) == len(expected) and {row.get('name') for row in rows} == set(expected),
            'Win11 new-syntax test matrix omits or repeats a mode/feature gate')
    for row in rows:
        success, diagnostic = expected[row['name']]
        require(row.get('passed') is True and row.get('expected_success') is success and
                row.get('diagnostic') == diagnostic and
                ((row.get('exit_code') == 0) is success) and
                isinstance(row.get('log_sha256'), str) and
                len(row['log_sha256']) == 64,
                f'Win11 new-syntax case failed: {row["name"]}')
    summary = {'passed': True, 'source_id': source_id, 'product_sha256': pe_sha,
               'stage_sha256': digest(stage_file), 'oracle_sha256': digest(oracle_file),
               'guest_result_sha256': digest(result_file), 'cases': len(rows),
               'policies': ['instruction', 'unwind'], 'mode': 'llvm-jit-full'}
    args.output.write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
