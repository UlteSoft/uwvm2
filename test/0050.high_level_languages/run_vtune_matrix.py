#!/usr/bin/env python3
"""Run bounded hardware collections serially and retain every failed attempt."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from build_products import admission, digest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['arena', 'manifest', 'out']:
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--repository', action='append', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    p.add_argument('--case', action='append', required=True)
    p.add_argument('--kind', action='append', choices=['hotspots', 'uarch-summary', 'uarch-detailed'])
    p.add_argument('--cpu', type=int, default=21)
    p.add_argument('--companion', type=int, default=9)
    p.add_argument('--ordinary-gated', action='store_true', help='profile ordinary independent stdin-gated fixtures')
    p.add_argument('--capture-jit-metadata', action='store_true')
    p.add_argument('--profile-libraries', type=Path)
    p.add_argument('--products-root', type=Path,
                   help='ordinary-only immutable product root; defaults to arena/products')
    a = p.parse_args()
    assert a.products_root is None or a.ordinary_gated, '--products-root requires the ordinary runner'
    before = admission()
    manifest = json.loads(a.manifest.read_text())
    names = [row['name'] for row in manifest]
    assert len(names) == len(set(names))
    assert set(a.case) <= set(names), set(a.case) - set(names)
    a.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'admission_before': before,
              'manifest_sha256': digest(a.manifest), 'ordinary_gated': a.ordinary_gated,
              'products_root': str(a.products_root if a.products_root is not None else a.arena / 'products'), 'runs': []}
    for repo in a.repository or ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
        for case in a.case:
            for kind in a.kind or ['hotspots', 'uarch-detailed']:
                folder = a.out / repo / case / kind
                folder.parent.mkdir(parents=True, exist_ok=True)
                runner = 'run_vtune_ordinary.py' if a.ordinary_gated else 'run_vtune.py'
                command = [sys.executable, str(Path(__file__).with_name(runner)),
                           '--arena', str(a.arena), '--manifest', str(a.manifest),
                           '--out', str(folder), '--repository', repo, '--case', case,
                           '--kind', kind, '--cpu', str(a.cpu), '--companion', str(a.companion)]
                if a.capture_jit_metadata:
                    assert a.ordinary_gated
                    command.append('--capture-jit-metadata')
                if a.profile_libraries is not None:
                    assert a.ordinary_gated
                    command.extend(['--profile-libraries', str(a.profile_libraries)])
                if a.products_root is not None:
                    command.extend(['--products-root', str(a.products_root)])
                row = {'repository': repo, 'case': case, 'kind': kind,
                       'argv': command, 'passed': False}
                report['runs'].append(row)
                print(json.dumps({**row, 'state': 'started'}), flush=True)
                with folder.with_suffix('.console.log').open('wb') as log:
                    # Each collector has its own bounded PMU window and cleanup.
                    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                row['exit'] = result.returncode
                receipt = folder / 'receipt.json'
                if receipt.is_file():
                    row['receipt_sha256'] = digest(receipt)
                    row['receipt'] = json.loads(receipt.read_text())
                    row['passed'] = result.returncode == 0 and row['receipt']['passed']
                (a.out / 'report.json').write_text(json.dumps(report, indent=2))
                print(json.dumps({key: row[key] for key in ['repository', 'case', 'kind', 'passed', 'exit']}), flush=True)
                alerts = row.get('receipt', {}).get('kernel_alerts', [])
                for filename in ['kernel-alerts.json', 'sampling-safety.json']:
                    path = folder / filename
                    if path.is_file():
                        value = json.loads(path.read_text())
                        alerts = alerts or (value.get('kernel_alerts', []) if isinstance(value, dict) else value)
                if alerts:
                    report['halted_for_kernel_alert'] = True
                    (a.out / 'report.json').write_text(json.dumps(report, indent=2))
                    raise RuntimeError('Hardware collection stopped after a kernel alert; see retained receipt')
    report['admission_after'] = admission()
    report['passed'] = bool(report['runs']) and all(row['passed'] for row in report['runs'])
    (a.out / 'report.json').write_text(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
