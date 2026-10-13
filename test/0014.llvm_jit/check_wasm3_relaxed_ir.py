#!/usr/bin/env python3
"""Check actual translator IR for frame-record overhead in the relaxed SIMD fixture.

Use llvm_jit_wasm3_initializers_ir with instruction/unwind to capture both inputs.
This checks the 25 native leaf functions; it excludes raw ABI wrappers and _start.
Actual trap stack correctness is separately exercised by run_wasm3_initializers.py.
"""
import argparse
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('instruction', type=Path)
    parser.add_argument('unwind', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    rows = []
    for policy, path, expected in (('instruction', args.instruction, 2), ('unwind', args.unwind, 0)):
        text = path.read_text()
        functions = re.findall(r'^define [^\n]*@uwvm_m_[0-9a-f]+_func_(\d+)\([^\n]*\) [^\n]*\{\n(.*?)^}', text, re.M | re.S)
        leaves = [(int(index), body) for index, body in functions if int(index) < 25]
        if sorted(i for i, _ in leaves) != list(range(25)):
            raise RuntimeError(f'{path}: expected exactly 25 native leaves')
        for index, body in leaves:
            calls = [line for line in body.splitlines() if re.search(r'\b(call|invoke|callbr)\b', line) and '@llvm.' not in line]
            if len(calls) != expected or any('@uwvm_bridge_' not in line for line in calls):
                raise RuntimeError(f'{policy} leaf {index}: unexpected helper calls: {calls}')
        rows.append({'mode': policy, 'native_leaf_functions': len(leaves), 'non_intrinsic_calls_per_leaf': expected})
    args.output.write_text(json.dumps(rows, indent=2) + '\n')
    print('PASS: 25 leaves, instruction=2 frame calls, unwind=0 helper calls')


if __name__ == '__main__':
    main()
