#!/usr/bin/env python3
"""Regression checks using retained actual VTune data, plus explicit mutations.

These are offline qualifier checks, never fabricated target performance runs.
Run on SSH Linux in the original resource owner, after hardware collection.
"""
import argparse
import copy
import hashlib
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from vtune_reports import EVENTS, qualify_microarchitecture_rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qualification', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    raw = args.qualification.read_bytes()
    original = json.loads(raw)
    header = list(original['vtune_formula_rows'][0])
    rows = original['vtune_formula_rows']
    selected, excluded, coverage = qualify_microarchitecture_rows(
        header, rows, original, original['target_pid'], True)
    assert selected and excluded and all(v['qualified'] * 100 >= v['all_target'] * 99
                                        for v in coverage.values())
    checks = [{'name': 'actual-JVM-kernel-with-undersampled-helpers', 'passed': True}]
    active = next(i for i, row in enumerate(rows)
                  if any(str(item['tid']) in row['Thread'] for item in selected))
    mutations = []
    changed = copy.deepcopy(rows); changed[0]['Thread'] = 'foreign (TID: 2147483647)'
    mutations.append(('foreign-thread', changed, header, original, 'unknown TID'))
    changed = copy.deepcopy(rows); changed[-1]['Core Type'] = 'P-Core'
    mutations.append(('wrong-PMU-even-on-idle-helper', changed, header, original, 'wrong PMU'))
    changed = copy.deepcopy(rows); changed.append(copy.deepcopy(changed[0]))
    mutations.append(('duplicate-thread', changed, header, original, 'duplicate formula TID'))
    changed = copy.deepcopy(rows); changed[active]['Clockticks:Self'] = '0'
    mutations.append(('active-clock-missing', changed, header, original, 'nonzero VTune'))
    changed = copy.deepcopy(rows); changed[active]['Efficient-core (E-core):Retiring:Self(%)'] = 'nan'
    mutations.append(('nonfinite-formula', changed, header, original, 'nonfinite VTune'))
    altered = copy.deepcopy(original)
    altered['target_samples'][EVENTS[0]] *= 2
    mutations.append(('unaccounted-target-samples', rows, header, altered, 'less than99%'))
    missing = [key for key in header if key != 'Efficient-core (E-core):Retiring:Self(%)']
    mutations.append(('missing-formula-column', rows, missing, original, 'formula missing'))
    for name, changed, columns, data, diagnostic in mutations:
        try:
            qualify_microarchitecture_rows(columns, changed, data, original['target_pid'], True)
        except RuntimeError as error:
            assert diagnostic in str(error), (name, str(error))
            checks.append({'name': name, 'passed': True, 'rejection': str(error)})
        else:
            raise AssertionError('unsafe qualifier mutation was accepted: ' + name)
    assert args.qualification.read_bytes() == raw
    result = {'passed': True, 'scope': 'offline actual-report regression plus seven explicit data mutations; no new collector/target',
              'actual_source_sha256': hashlib.sha256(raw).hexdigest(),
              'helper_sha256': hashlib.sha256(Path(__file__).with_name('vtune_reports.py').read_bytes()).hexdigest(),
              'checks': checks, 'qualified_threads': [item['tid'] for item in selected],
              'excluded_auxiliary_threads': [item['tid'] for item in excluded],
              'actual_sample_coverage': coverage}
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': len(checks)}))


if __name__ == '__main__':
    main()
