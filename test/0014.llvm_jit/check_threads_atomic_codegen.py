#!/usr/bin/env python3
"""Inspect actual x86-64 atomic store/RMW interpreter and JIT outputs from focused fixtures."""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--operation', choices=('store', 'rmw'), default='store')
    parser.add_argument('binary', type=pathlib.Path)
    parser.add_argument('ir', type=pathlib.Path)
    parser.add_argument('--llvm-bin', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    assembly = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', str(args.binary)], text=True)
    parts = re.split(r'(?m)^([0-9a-f]+) <([^>]+)>:\n', assembly)
    rows = []
    selected = []
    for index in range(1, len(parts), 3):
        symbol, body = parts[index + 1:index + 3]
        # GNU demanglers may not understand Clang's C++26 constrained symbol spelling.
        marker = 'optable7details12atomic_storeI' if args.operation == 'store' else 'optable7details10atomic_rmwI'
        if marker not in symbol:
            continue
        selected.append(symbol + ':\n' + body)
        instructions = []
        for line in body.splitlines():
            match = re.match(r'\s*[0-9a-f]+:\s*(\S+)\s*(.*)', line)
            if match:
                instructions.append(match.groups())
                if match[1].startswith('jmp') and match[2].startswith('*'):
                    break
        assert instructions and instructions[-1][0].startswith('jmp') and instructions[-1][1].startswith('*'), symbol
        # This is the straight-line successful prefix, not cold trap branches.
        assert not any(op.startswith('call') or op in ('mfence', 'sfence', 'lfence') for op, _ in instructions), symbol
        assert sum((op.startswith('xchg') or op == 'lock') and '(' in operands for op, operands in instructions) == 1, symbol
        rows.append({'symbol': symbol, 'success_instructions': instructions, 'passed': True})
    assert rows, 'No instantiated tail handlers found'
    (args.output / f'atomic-{args.operation}-handlers.s').write_text('\n'.join(selected))
    jit_rows = []
    for source in sorted(args.ir.glob(f'atomic-{args.operation}-*.ll')):
        if '.opt.' in source.name:
            continue
        opt = args.output / source.with_suffix('.opt.ll').name
        asm = args.output / source.with_suffix('.s').name
        subprocess.run([str(args.llvm_bin / 'opt'), '-passes=default<O3>', '-S', str(source), '-o', str(opt)], check=True)
        subprocess.run([str(args.llvm_bin / 'llc'), '-O3', '-mtriple=x86_64-unknown-linux-gnu', '-mcpu=raptorlake', str(opt), '-o', str(asm)], check=True)
        ir = opt.read_text()
        body = re.search(r'(?ms)^define [^\n]*@uwvm_\w+_func_0\([^\n]*\).*?^}', ir).group()
        if args.operation == 'store':
            stores = re.findall(r'store atomic i(\d+) .*?seq_cst, align (\d+)', body)
            assert len(stores) == 2 and all(int(bits) == 8 * int(align) for bits, align in stores), source
        else:
            updates = re.findall(r'(?m)^.*(?:atomicrmw|cmpxchg) .*$', body)
            assert len(updates) == 2 and all('seq_cst' in line for line in updates), source
            for line in updates:
                bits = int(re.search(r'\bi(8|16|32|64)\b', line)[1])
                alignment = int(re.search(r'align (\d+)', line)[1])
                assert bits == alignment * 8 and ' weak ' not in line, source
        assert 'load volatile' not in body and 'memory.length' not in body, source
        memory = 'memory1_begin' if '-indexed-' in source.name else 'memory0_begin'
        assert memory in body, source
        machine = asm.read_text()
        hot = re.search(r'(?ms)^uwvm_\w+_func_0:.*?^\.Lfunc_end\d+:', machine).group()
        assert len(re.findall(r'(?m)^\s+(?:xchg[bwlq]|lock)\s', hot)) == 2, source
        assert not re.search(r'(?m)^\s+[msl]fence\b', hot), source
        assert '__atomic_' not in hot and '.cfi_startproc' in hot, source
        if '-unwind.' in source.name:
            # All external calls must belong to a terminating trap block.
            blocks = re.split(r'(?m)^[a-zA-Z0-9_.]+:[^\n]*\n', body)
            assert not any(re.search(r'\bcall\b[^\n]*@', block) for block in blocks if 'unreachable' not in block), source
        jit_rows.append({'file': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'passed': True})
    assert len(jit_rows) == (28 if args.operation == 'store' else 196), len(jit_rows)
    (args.output / 'results.json').write_text(json.dumps({'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        'interpreter_handlers': rows, 'jit_modules': jit_rows}, indent=2) + '\n')
    print(f'PASS {len(rows)} atomic-{args.operation} tail handlers and {len(jit_rows)} actual JIT modules')


if __name__ == '__main__':
    main()
