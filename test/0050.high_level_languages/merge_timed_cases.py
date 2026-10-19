#!/usr/bin/env python3
"""Validate and combine independently built, pinned clocked guest manifests."""
import argparse
import json
from pathlib import Path

from bench_engines import select_cases

BASE_LANGUAGES = frozenset({'java', 'csharp', 'assemblyscript', 'javascript', 'moonbit'})
PYTHON_LANGUAGES = BASE_LANGUAGES | {'python'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, action='append', required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--cold-timeout', type=int)
    a = p.parse_args()
    rows = []
    for source in a.source:
        rows.extend(json.loads(source.read_text()))
    rows = select_cases(rows)
    languages = frozenset(row['language'] for row in rows)
    assert languages in (BASE_LANGUAGES, PYTHON_LANGUAGES), (
        'expected the original five languages, optionally plus python; '
        f'missing={sorted(BASE_LANGUAGES - languages)}, unknown={sorted(languages - PYTHON_LANGUAGES)}')
    for row in rows:
        assert row['purpose'] in ['ordinary-long', 'debug-hardware-short', 'hardware-gated']
        assert row['kernel_clock'] in ['monotonic', 'realtime']
        assert Path(row['generated_source']).is_file() and Path(row['oracle']).is_file()
        if a.cold_timeout is not None:
            assert 60 <= a.cold_timeout <= 1800
            row['initial_prompt_timeout'] = a.cold_timeout
            row['cold_command_timeout'] = a.cold_timeout
    a.out.write_text(json.dumps(rows, indent=2) + '\n')


if __name__ == '__main__':
    main()
