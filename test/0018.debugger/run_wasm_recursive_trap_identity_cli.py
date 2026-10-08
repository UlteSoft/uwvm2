#!/usr/bin/env python3
"""Distinguish live recursive leaf/caller trap packets by their exact values."""
import copy
import sys
from pathlib import Path

# Import the explicitly selected immutable source's CLI and lifecycle helpers.
source_root = Path(sys.argv[sys.argv.index('--source-root') + 1])
sys.path.insert(0, str(source_root / 'test/0018.debugger'))
import run_wasm_trap_stops_cli as traps

original_examples = traps.trap_examples

def distinct_recursive_example():
    case = copy.deepcopy(next(c for c in original_examples() if c['name'] == 'trap-recursive-72'))
    before = 'global.get $p global.get $z local.get 0 if'
    after = 'global.get $p local.get 0 i64.extend_i32_s i64.add global.get $z local.get 0 if'
    assert case['wat'].count(before) == 1
    case['wat'] = case['wat'].replace(before, after)
    # Leaf parameter is zero; the live immediate caller has parameter one.
    # A snapshot from another recursion depth must fail this value oracle.
    case['expected'][0]['values'] = ['i64 = -991', 'f32 = bits=0x80000000']
    case['expected'][0]['caller'] = ['i64 = -990', 'f32 = bits=0x80000000']
    return [case]

traps.trap_examples = distinct_recursive_example
if __name__ == '__main__':
    traps.main()
