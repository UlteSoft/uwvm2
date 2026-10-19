#!/usr/bin/env python3
"""Publish manifests only after real native + Wasmtime reference qualification."""
import argparse
import json
import re
from pathlib import Path
import sys

from linux_control import digest, guard

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from inspect_wasm import inspect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--references', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ordinary-rounds', type=int, default=20000)
    parser.add_argument('--profile-rounds', type=int, default=20000)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    guard(args.out)
    own = Path(__file__).resolve().parent
    build_path = args.build / 'build-receipt.json'
    reference_path = args.references / 'report.json'
    build = json.loads(build_path.read_text())
    references = json.loads(reference_path.read_text())
    assert build['passed'] and references['passed']
    assert digest(build_path) == references['build_receipt_sha256']
    modules = {}
    cpython_source = Path(build['source'])
    frozen_source = cpython_source / 'Python/frozen.c'
    upstream_headers = []
    for relative in re.findall(r'#include "(frozen_modules/[^\"]+)"', frozen_source.read_text()):
        path = Path(build['wasi_build']) / 'Python' / relative
        if not path.exists():
            path = cpython_source / 'Python' / relative
        upstream_headers.append({'path': str(path), 'sha256': digest(path), 'bytes': path.stat().st_size})
    for entry in ['semantics', 'timed', 'profile']:
        artifact = build['artifacts'][entry + '-wasi']
        assert digest(artifact['path']) == artifact['sha256']
        modules[entry] = {'artifact': artifact, 'module': inspect(artifact['path']),
                          'frozen_assets': json.loads(Path(artifact['frozen_assets']).read_text()),
                          'upstream_frozen_table_source': str(frozen_source),
                          'upstream_frozen_table_sha256': digest(frozen_source),
                          'upstream_frozen_headers': upstream_headers}
    (args.out / 'modules.json').write_text(json.dumps(modules, indent=2) + '\n')
    qualification = {'producer_receipt': str(build_path), 'producer_receipt_sha256': digest(build_path),
                     'oracle': str(reference_path), 'oracle_sha256': digest(reference_path),
                     'module_inspection': str(args.out / 'modules.json'),
                     'module_inspection_sha256': digest(args.out / 'modules.json'),
                     'source_language_debugger_claim': False,
                     'debug_scope': 'Wasm/native CPython entry/step/backtrace/breakpoint/restart/output; no Python locals bridge',
                     'external_runtime_assets': [], 'preopens': []}
    functional = []
    for reference in references['cases']:
        if reference['engine'] != 'wasmtime' or reference['entry'] != 'semantics':
            continue
        artifact = build['artifacts']['semantics-wasi']
        row = {'name': 'python-cpython-' + reference['name'], 'language': 'python',
               'backend': 'cpython-interpreter', 'target': 'wasm32-wasip1',
               'optimization': 'C-O2 Python-bytecode-optimize0', 'wasm': artifact['path'],
               'wasm_sha256': artifact['sha256'], 'args': reference['args'],
               'expected_stdout': reference['stable_stdout'],
               'expected_exit': reference.get('expected_exit', 0),
               'expected_stderr': reference.get('expected_stderr', ''),
               'skip_debug': reference['name'] != 'rounds-1000',
               'initial_prompt_timeout': 1200, 'cold_command_timeout': 1200,
               'generated_source': str(own / 'semantics.py'),
               'generated_source_sha256': digest(own / 'semantics.py'), **qualification}
        if reference.get('environment'):
            row['load_args'] = ['--wasip1-noinherit-system-environment',
                                *[token for pair in reference['environment'].items()
                                  for token in ['--wasip1-add-or-replace-environment', *pair]]]
        functional.append(row)
    (args.out / 'cases.json').write_text(json.dumps(functional, indent=2) + '\n')
    for entry, count, purpose, filename in [
        ('timed', args.ordinary_rounds, 'ordinary-long', 'perf-timed-cases.json'),
        ('profile', args.profile_rounds, 'hardware-gated', 'perf-profile-cases.json')]:
        reference = next(row for row in references['cases'] if row['engine'] == 'wasmtime'
                         and row['entry'] == entry and row['name'] == f'rounds-{count}')
        artifact = build['artifacts'][entry + '-wasi']
        row = {'name': f'python-cpython-{entry}-{count}', 'language': 'python',
               'backend': 'cpython-interpreter', 'target': 'wasm32-wasip1', 'mode': 'release',
               'optimization': 'C-O2 Python-bytecode-optimize0', 'purpose': purpose,
               'rounds': count, 'iterations': count, 'wasm': artifact['path'],
               'wasm_sha256': artifact['sha256'], 'args': [str(count)],
               'expected_checksum': references['independent_checksum_oracles'][str(count)],
               'checks': reference['stable_stdout'].splitlines(), 'kernel_time': 'ns',
               'kernel_clock': 'monotonic', 'clock_kind': 'monotonic-ns',
               'clock_line_prefix': 'Kernel ns: ',
               'checksum_line': reference['stable_stdout'].splitlines()[-1],
               'generated_source': str(own / (entry + '.py')),
               'generated_source_sha256': digest(own / (entry + '.py')), **qualification}
        if entry == 'profile':
            row.update(gate_stream='stderr', gate_ready='UWVM_PROFILE_READY',
                       gate_done='UWVM_PROFILE_DONE', gate_release='G', gate_finish='D',
                       gate_ready_timeout=1200, kernel_timeout=28, warmup_rounds=100)
        (args.out / filename).write_text(json.dumps([row], indent=2) + '\n')


if __name__ == '__main__':
    main()
