#!/usr/bin/env python3
"""Real memory64 trap inputs after bounded reservation; Linux cgroup only."""
import copy
import sys
from pathlib import Path

source_root = Path(sys.argv[sys.argv.index('--source-root') + 1])
sys.path.insert(0, str(source_root / 'test/0018.debugger'))
import run_wasm_trap_stops_cli as traps

original_examples = traps.trap_examples

def memory64_boundaries():
    originals = original_examples()
    bounds = next(c for c in originals if c['name'] == 'trap-memory-bounds')
    alignment = next(c for c in originals if c['name'] == 'trap-atomic-alignment')
    cases = []
    for name, declaration, address in (
        ('trap-memory64-one-page-bounds', '(memory i64 1 1)', 65536),
        ('trap-memory64-zero-maximum', '(memory i64 0 0)', 0),
        ('trap-memory64-unsigned-address', '(memory i64 1 1)', -1)):
        case = copy.deepcopy(bounds)
        assert case['wat'].count('(memory 1)') == 1 and case['wat'].count('i32.const 65536') == 1
        case['name'] = name
        case['wat'] = case['wat'].replace('(memory 1)', declaration).replace('i32.const 65536', f'i64.const {address}')
        case['expected'][0]['values'][-1] = f'i64 = {address}'
        cases.append(case)
    case = copy.deepcopy(alignment)
    assert case['wat'].count('(memory 1 1 shared)') == 1 and case['wat'].count('i32.const 1') == 1
    case['name'] = 'trap-memory64-atomic-alignment'
    case['wat'] = case['wat'].replace('(memory 1 1 shared)', '(memory i64 1 1 shared)').replace('i32.const 1', 'i64.const 1')
    case['expected'][0]['values'][-1] = 'i64 = 1'
    cases.append(case)
    return cases

traps.trap_examples = memory64_boundaries
if __name__ == '__main__':
    traps.main()
