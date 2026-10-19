#!/usr/bin/env python3
"""Small, hash-bound post-run evidence; no compiler or VM is started."""
import argparse
import json
import os
from pathlib import Path
import sys

from linux_control import digest, guard
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from inspect_wasm import Reader, inspect


def typed_imports(path):
    module = inspect(path)
    reader = Reader(Path(path).read_bytes())
    reader.pos = 8
    types = []
    names = {127: 'i32', 126: 'i64', 125: 'f32', 124: 'f64', 123: 'v128',
             112: 'funcref', 111: 'externref'}
    while reader.pos < len(reader.data):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        if tag == 1:
            for _ in range(reader.leb()):
                assert reader.byte() == 0x60, 'expected plain CPython numeric function types'
                params = [names[reader.byte()] for _ in range(reader.leb())]
                results = [names[reader.byte()] for _ in range(reader.leb())]
                types.append({'params': params, 'results': results})
        reader.pos = end
    return sorted([{'module': row['module'], 'name': row['name'], **types[row['type']]}
                   for row in module['imports'] if row['kind'] == 0],
                  key=lambda row: (row['module'], row['name']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, required=True)
    args = parser.parse_args()
    guard(args.arena)
    os.sched_setaffinity(0, {4})
    relative_reports = [
        'work-r4/build-receipt.json', 'references-r1/report.json',
        'work-debug-r1/build-receipt.json', 'debug-references-r1/report.json',
        'debug-minimal-r1/build-receipt.json', 'debug-full-Oz-r1/build-receipt.json',
        'debug-full-Oz-references-r1/report.json', 'references-r3/report.json',
        'builtin-size-r1/report.json', 'debug-full-Oz-r1/size-comparison.json']
    reports = {name: json.loads((args.arena / name).read_text()) for name in relative_reports}
    leaf_roots = ['work-r4/commands', 'work-debug-r1/commands', 'debug-minimal-r1/commands',
                  'debug-full-Oz-r1/commands', 'debug-references-r1',
                  'debug-full-Oz-references-r1', 'references-r3', 'builtin-size-r1']
    leafs = []
    for relative in leaf_roots:
        for path in sorted((args.arena / relative).rglob('receipt.json')):
            record = json.loads(path.read_text())
            assert record['retired'] and record['ownership']['closed'], path
            assert all(not m['pidfd_retained'] for m in record['ownership']['members']), path
            leafs.append({'path': str(path), 'sha256': digest(path), 'exit': record['exit']})
    original = reports['work-r4/build-receipt.json']
    for artifact in original['artifacts'].values():
        assert digest(artifact['path']) == artifact['sha256']
    oz = reports['debug-full-Oz-r1/build-receipt.json']
    o2_path = original['artifacts']['semantics-wasi']['path']
    oz_path = oz['artifact']['path']
    o2_abi, oz_abi = typed_imports(o2_path), typed_imports(oz_path)
    assert o2_abi == oz_abi and len(oz_abi) == 42
    module = inspect(oz_path)
    summary = {key: value for key, value in module.items() if key != 'functions'}
    summary['function_anchors'] = {index: name for index, name in module['functions'].items()
                                    if name in {'main', '_start', '_PyEval_EvalFrameDefault'}}
    summary['typed_imports'] = oz_abi
    output = {'passed': True, 'scope': 'producer/reference/static metadata; UWVM DBG remains parent-owned',
              'reports': {name: {'path': str(args.arena / name),
                                 'sha256': digest(args.arena / name),
                                 'passed': value.get('passed')}
                          for name, value in reports.items()},
              'retired_process_receipts': leafs,
              'full_O2_artifacts_unchanged': True,
              'full_Oz_import_ABI_identical_to_full_O2': True,
              'full_Oz_module': summary,
              'size_comparison': reports['debug-full-Oz-r1/size-comparison.json'],
              'full_Oz_reference_cases': len(reports['debug-full-Oz-references-r1/report.json']['cases']),
              'O0_reference_cases': len(reports['debug-references-r1/report.json']['cases']),
              'O0_UWVM_DBG_attempted': False,
              'minimal_r1_failure': 'source Setup.local did not override VPATH build-dir Setup.local; '
                                    'exact builtin-table assertion prevented publication',
              'minimal_r2_attempted': False,
              'calibration_200k_checksum': reports['references-r3/report.json']['independent_checksum']}
    (args.arena / 'producer-updates-r1.json').write_text(json.dumps(output, indent=2) + '\n')


if __name__ == '__main__':
    main()
