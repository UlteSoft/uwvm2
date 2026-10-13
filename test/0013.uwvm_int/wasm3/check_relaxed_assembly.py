#!/usr/bin/env python3
"""Inspect x86-64 register-ring assembly for the six new relaxed-SIMD handlers.

The input is the non-stripped relaxed_simd.cc test executable. Canonicalized
relaxed opcodes reuse existing strict handlers; this checks the newly emitted
binary/ternary handlers and preserves their complete disassembly for review.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--llvm', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    symbols = subprocess.check_output([str(args.llvm / 'llvm-nm'), '-C', '-S', '--defined-only', str(args.binary)], text=True)
    rows = []
    assembly = []
    for line in symbols.splitlines():
        fields = line.split(maxsplit=3)
        if len(fields) != 4:
            continue
        address, size, _, name = fields
        if 'uwvmint_simd_full_' not in name or '_t{true,' not in name:
            continue
        match = re.search(r'opcode::op_simd\)(\d+),', name)
        if not match or int(match[1]) not in (261, 262, 263, 264, 274, 275):
            continue
        opcode = int(match[1])
        start = int(address, 16)
        end = start + int(size, 16)
        text = subprocess.check_output([
            str(args.llvm / 'llvm-objdump'), '--disassemble', '--demangle',
            f'--start-address={start}', f'--stop-address={end}', str(args.binary),
        ], text=True)
        if 'file format elf64-x86-64' not in text:
            raise RuntimeError('This assembly check requires x86-64 ELF')
        calls = re.findall(r'\bcallq?\s', text)
        tail = re.search(r'\bjmpq?\s+\*', text) is not None
        if calls or not tail:
            raise RuntimeError(f'{opcode:#x}: helper call or missing tail dispatch\n{text}')
        assembly.append(text)
        rows.append({'opcode': hex(opcode), 'function': name, 'calls': len(calls), 'indirect_tail_jump': tail, 'bytes': end - start})
    # A single-configuration fixture emits six handlers; an all-combine product
    # emits the same six opcodes for each enabled register-ring configuration.
    # Every emitted instance was checked above, so require the complete opcode
    # set without discarding the extra product configurations.
    required = {hex(n) for n in (261, 262, 263, 264, 274, 275)}
    if {row['opcode'] for row in rows} != required or len(rows) < len(required):
        raise RuntimeError(f'Missing relaxed-SIMD ring handlers: {rows}')
    (args.output / 'assembly.txt').write_text('\n'.join(assembly))
    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(f'PASS {len(rows)} new ring handlers across six opcodes: no helper calls, indirect tail dispatch')


if __name__ == '__main__':
    main()
