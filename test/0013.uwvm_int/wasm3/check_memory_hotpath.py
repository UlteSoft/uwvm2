#!/usr/bin/env python3
"""Inspect executed full-mmap ring handlers, including the SIMD memory policies.

Scalar and SIMD loads must have no bounds branch, helper call, atomic lock or
memory fence. Stores retain their safe cross-page preflight. Every handler must
end in indirect tail dispatch. Record baseline instruction equality where the
same specialization exists; this is independent of wall-clock benchmark noise.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def symbols(binary, llvm):
    result = {}
    output = subprocess.check_output([str(llvm / 'llvm-nm'), '-C', '-S', '--defined-only', str(binary)], text=True)
    for line in output.splitlines():
        fields = line.split(maxsplit=3)
        if len(fields) != 4:
            continue
        address, size, kind, name = fields
        if kind.lower() not in ('t', 'w') or '_t{true,' not in name or not re.search(r'bounds_check_mmap_full(?:_standard_page)?\(', name):
            continue
        match = re.search(r'::(?:memop::)?((?:i32|i64|f32|f64)_(?:load|store)\w*|uwvmint_simd_(?:v128|full_mem)_(?:load|store))<', name)
        if match:
            result[name] = (int(address, 16), int(size, 16), match[1])
    return result


def assembly(binary, llvm, address, size):
    text = subprocess.check_output([str(llvm / 'llvm-objdump'), '-d', '--no-show-raw-insn',
        f'--start-address={address}', f'--stop-address={address + size}', str(binary)], text=True)
    if 'file format elf64-x86-64' not in text:
        raise RuntimeError('this assertion currently requires native x86-64 ELF')
    instructions = []
    for line in text.splitlines():
        match = re.match(r'\s*[0-9a-f]+:\s+(.*)', line)
        if not match:
            continue
        instruction = match[1].split('#', 1)[0].strip()
        instruction = re.sub(r'0x([0-9a-f]+) <[^>]*>',
            lambda m: 'relative:' + str(int(m[1], 16) - address), instruction)
        instructions.append(re.sub(r'\s+', ' ', instruction))
    return text, instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--llvm', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.output.mkdir(parents=True, exist_ok=False)
    before = symbols(args.baseline, args.llvm)
    current = symbols(args.binary, args.llvm)
    rows = []
    for name, (address, size, handler) in current.items():
        text, instructions = assembly(args.binary, args.llvm, address, size)
        rendered = '\n'.join(instructions)
        if not re.search(r'\bjmpq? \*', rendered):
            raise RuntimeError('missing indirect tail dispatch: ' + name)
        if re.search(r'\b(callq?|lock|pause|mfence|sfence|lfence|cmpxchg\w*|xadd\w*)\b', rendered):
            raise RuntimeError('helper/lock/fence in full mmap handler: ' + name)
        branches = [i for i in instructions if re.match(r'j(?!mp)[a-z]+\b', i)]
        if 'load' in handler and branches:
            raise RuntimeError('redundant bounds branch in full mmap load: ' + name)
        same = None
        same_mnemonics = None
        baseline_instruction_count = None
        baseline_name = name.replace('bounds_check_mmap_full_standard_page(', 'bounds_check_mmap_full(')
        if baseline_name in before:
            old_address, old_size, _ = before[baseline_name]
            old_text, old = assembly(args.baseline, args.llvm, old_address, old_size)
            same = old == instructions
            same_mnemonics = [i.split()[0] for i in old] == [i.split()[0] for i in instructions]
            baseline_instruction_count = len(old)
            (args.output / f'{address:x}.baseline.s').write_text(old_text)
        standard_page = 'bounds_check_mmap_full_standard_page(' in name
        if standard_page and 'store' in handler:
            if len(branches) > 1 or re.search(r'\b(?:shl|sal|shr|sar)\w* %cl', rendered):
                raise RuntimeError('runtime page-policy work in a specialized store: ' + name)
            if baseline_instruction_count is not None and len(instructions) > baseline_instruction_count:
                raise RuntimeError('specialized scalar store increased instructions: ' + name)
        (args.output / f'{address:x}.s').write_text(text)
        rows.append(dict(handler=handler, function=name, standard_page_policy=standard_page, bytes=size, instructions=len(instructions),
                         conditional_branches=len(branches), baseline_instructions_identical=same,
                         baseline_mnemonics_identical=same_mnemonics,
                         baseline_instruction_count=baseline_instruction_count))
    required = {'i32_load', 'i32_store', 'uwvmint_simd_v128_load', 'uwvmint_simd_v128_store',
                'uwvmint_simd_full_mem_load', 'uwvmint_simd_full_mem_store'}
    if not required.issubset({row['handler'] for row in rows}):
        raise RuntimeError('missing scalar/SIMD memory specialization coverage')
    if not any(row['standard_page_policy'] and 'store' in row['handler'] for row in rows):
        raise RuntimeError('missing standard-page store specialization')
    result = dict(binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  baseline_sha256=hashlib.sha256(args.baseline.read_bytes()).hexdigest(), handlers=rows)
    (args.output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    changed = sum(row['baseline_instructions_identical'] is False for row in rows)
    print(f'PASS {len(rows)} full mmap ring handlers: no calls/locks/fences; loads branch-free; all tail dispatch')
    print(f'{changed} matching baseline specializations differ; exact assembly retained for review')


if __name__ == '__main__':
    main()
