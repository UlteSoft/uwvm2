#!/usr/bin/env python3
"""Check O1 register-ring GC-table musttail and unchanged funcref handlers."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


def output(args):
    return subprocess.check_output([str(arg) for arg in args], text=True)


def symbols(nm, obj):
    return [line.split()[0] for line in
            output([nm, '--defined-only', '--no-demangle', '--format=posix', obj]).splitlines()
            if 'uwvmint_table64' in line]


def select(names, *, operation, external, wide, exn, gc, combine, legacy):
    prefix = ('uwvmint_table64ILNS3_17table64_operationE'
              f'{operation}ELb{external}ELb{wide}ELb0EXtl'
              f'NS3_35uwvm_interpreter_translate_option_tELb1ELb{combine}')
    suffix = f'EEELb{exn}' + ('' if legacy else f'ELb{gc}') + 'ETp'
    matches = [name for name in names if prefix in name and suffix in name]
    if len(matches) != 1:
        raise RuntimeError(f'expected one tail handler for {prefix}/{suffix}, found {len(matches)}')
    return matches[0]


def code(objdump, obj, symbol):
    lines = output([objdump, '-d', f'--disassemble-symbols={symbol}', obj]).splitlines()
    raw = []
    instructions = []
    for line in lines:
        fields = line.split('\t')
        if len(fields) < 2 or not re.match(r'^\s*[0-9a-f]+:', fields[0]):
            continue
        byte_text = fields[0].split(':', 1)[1]
        octets = re.findall(r'\b[0-9a-f]{2}\b', byte_text)
        if not octets:
            continue
        raw.extend(octets)
        instructions.append('\t'.join(fields[1:]).strip())
    if not instructions:
        raise RuntimeError(f'no machine instructions for {symbol}')
    return bytes.fromhex(''.join(raw)), instructions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--baseline-object', type=Path, required=True)
    parser.add_argument('--current-object', type=Path, required=True)
    parser.add_argument('--baseline-gc-aware', action='store_true',
                        help='The baseline handler already has the GCRef template argument')
    parser.add_argument('--llvm-bin', type=Path, default=Path('/toolchain/bin'))
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    old = symbols(args.llvm_bin / 'llvm-nm', args.baseline_object)
    new = symbols(args.llvm_bin / 'llvm-nm', args.current_object)
    rows = []
    for operation, name in ((0, 'get'), (1, 'set')):
        for combine in (0, 1):
            old_name = select(old, operation=operation, external=0, wide=1,
                              exn=0, gc=0, combine=combine, legacy=not args.baseline_gc_aware)
            new_name = select(new, operation=operation, external=0, wide=1,
                              exn=0, gc=0, combine=combine, legacy=False)
            old_code, old_instructions = code(args.llvm_bin / 'llvm-objdump',
                                              args.baseline_object, old_name)
            new_code, new_instructions = code(args.llvm_bin / 'llvm-objdump',
                                              args.current_object, new_name)
            rows.append({'handler': f'funcref.table64.{name}.combine{combine}',
                         'baseline_symbol': old_name, 'current_symbol': new_name,
                         'baseline_bytes_sha256': hashlib.sha256(old_code).hexdigest(),
                         'current_bytes_sha256': hashlib.sha256(new_code).hexdigest(),
                         'baseline_instruction_count': len(old_instructions),
                         'current_instruction_count': len(new_instructions),
                         'unchanged_machine_bytes': old_code == new_code,
                         'musttail_indirect_jump': any(re.search(r'\bjmp\w*\s+\*', item)
                                                      for item in new_instructions)})
            gc_name = select(new, operation=operation, external=1, wide=0,
                             exn=0, gc=1, combine=combine, legacy=False)
            gc_code, gc_instructions = code(args.llvm_bin / 'llvm-objdump',
                                            args.current_object, gc_name)
            rows.append({'handler': f'gcref.table32.{name}.combine{combine}', 'symbol': gc_name,
                         'machine_bytes_sha256': hashlib.sha256(gc_code).hexdigest(),
                         'instruction_count': len(gc_instructions),
                         'musttail_indirect_jump': any(re.search(r'\bjmp\w*\s+\*', item)
                                                      for item in gc_instructions),
                         'has_return_instead_of_tail': any(re.match(r'^ret\w*\b', item)
                                                          for item in gc_instructions)})
    for row in rows:
        row['passed'] = (row['musttail_indirect_jump'] and
                         row.get('unchanged_machine_bytes', True) and
                         not row.get('has_return_instead_of_tail', False))
    args.out.mkdir(parents=True, exist_ok=False)
    summary = {'passed': all(row['passed'] for row in rows), 'checks': len(rows),
               'baseline_object_sha256': hashlib.sha256(args.baseline_object.read_bytes()).hexdigest(),
               'current_object_sha256': hashlib.sha256(args.current_object.read_bytes()).hexdigest(),
               'rows': rows}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'GC table assembly/musttail: {sum(row["passed"] for row in rows)}/{len(rows)}')
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
