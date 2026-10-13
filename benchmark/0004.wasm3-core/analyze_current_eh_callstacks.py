#!/usr/bin/env python3
"""Attribute only visible callers in immutable, target-filtered VTune reports.

This is a report parser, not a native/VM/profiler runner. Missing raw stack
frames forbid treating observed categories as complete call-path attribution.
"""
import argparse
import csv
from decimal import Decimal
import hashlib
import json
from pathlib import Path

HEADER = ['Function/Function Stack', 'CPU Time', 'Module', 'Function (Full)',
          'Source File', 'Start Address']


def analyze(data):
    lines = data.decode('utf-8').splitlines()
    headers = [i for i, line in enumerate(lines) if line.split('\t') == HEADER]
    if len(headers) != 1:
        raise RuntimeError('exactly one expected VTune callstacks header required')
    start = headers[0]
    warnings = lines[:start]
    groups = []
    group = []
    for row in csv.reader(lines[start + 1:], delimiter='\t'):
        if not row or not any(row):
            if group:
                groups.append(group)
                group = []
            continue
        if len(row) != len(HEADER):
            raise RuntimeError('incomplete or expanded VTune report row')
        weight = Decimal(row[1])
        if not weight.is_finite() or weight < 0:
            raise RuntimeError('invalid sampled CPU time')
        group.append(row)
    if group:
        groups.append(group)
    totals = {}
    total = Decimal(0)
    skipped = Decimal(0)
    for group in groups:
        weighted = [row for row in group if Decimal(row[1]) != 0]
        if len(weighted) != 1:
            raise RuntimeError('one weighted leaf per callstack group required')
        leaf = weighted[0]
        weight = Decimal(leaf[1])
        functions = [row[0] for row in group]
        capture = any('capture_runtime_exception_trace' in f for f in functions)
        raise_exception = any('_Unwind_RaiseException' in f for f in functions)
        category = ('capture_and_raise_visible' if capture and raise_exception else
                    'trace_capture_visible' if capture else
                    'raise_exception_visible' if raise_exception else 'other_unattributed')
        missing = any(f == '[Skipped stack frame(s)]' for f in functions)
        record = totals.setdefault(category, dict(seconds=Decimal(0), groups=0,
                                                  skipped_seconds=Decimal(0), leaf_modules={}))
        record['seconds'] += weight
        record['groups'] += 1
        record['leaf_modules'][leaf[2]] = record['leaf_modules'].get(leaf[2], Decimal(0)) + weight
        if missing:
            skipped += weight
            record['skipped_seconds'] += weight
        total += weight
    if total <= 0:
        raise RuntimeError('empty sampled result')
    records = {}
    for name, record in totals.items():
        records[name] = dict(seconds=str(record['seconds']), groups=record['groups'],
                             percent_of_reported_sampled_seconds=str(record['seconds'] / total * 100),
                             skipped_seconds=str(record['skipped_seconds']),
                             leaf_modules={key: str(value) for key, value in record['leaf_modules'].items()})
    return dict(schema='uwvm-current-eh-visible-callstacks-analysis-v1',
                input_sha256=hashlib.sha256(data).hexdigest(), input_bytes=len(data),
                group_count=len(groups), total_sampled_seconds=str(total),
                skipped_sampled_seconds=str(skipped), visible_categories=records,
                original_report_warnings=warnings,
                complete_caller_attribution=False,
                wall_clock_or_instruction_counter_result=False,
                native_execution_by_parser=False,
                scope='original target-filtered immutable report; visible caller evidence only')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reports', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError('refusing to replace analysis')
    result = {path.name: analyze(path.read_bytes()) for path in args.reports}
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')


if __name__ == '__main__':
    main()
