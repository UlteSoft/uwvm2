#!/usr/bin/env python3
"""Regression checks for ELF interpreter auditing in QEMU qualification."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mips-libc', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('matrix', Path(__file__).with_name('algorithm_matrix.py'))
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    args.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    rows = []
    for name, value, count, valid in [
        ('one-terminator', b'/lib64/ld.so.1\0', 1, True),
        ('zero-alignment-padding', b'/lib64/ld.so.1\0\0', 1, True),
        ('missing-terminator', b'/lib64/ld.so.1', 1, False),
        ('nonzero-after-terminator', b'/lib64/ld.so.1\0x', 1, False),
        ('empty-path', b'\0\0', 1, False),
        ('duplicate-interpreter', b'/lib64/ld.so.1\0', 2, False),
    ]:
        image = bytearray(64 + 56 * count)
        image[:7] = b'\x7fELF\x02\x02\x01'
        struct.pack_into('>H', image, 18, 8)
        struct.pack_into('>Q', image, 32, 64)
        struct.pack_into('>HH', image, 54, 56, count)
        for index in range(count):
            struct.pack_into('>IIQQQQQQ', image, 64 + 56 * index, 3, 4, len(image), 0, 0, len(value), len(value), 8)
        image += value
        path = args.output / name
        path.write_bytes(image)
        try:
            result = matrix.elf_identity(path)
            passed = valid and result['interpreter'] == '/lib64/ld.so.1'
        except RuntimeError:
            passed = not valid
        rows.append({'name': name, 'passed': passed})
    actual = matrix.elf_identity(args.mips_libc)
    rows.append({'name': 'authenticated actual MIPS libc interpreter',
                 'passed': (actual['machine'], actual['bits'], actual['byte_order'], actual['interpreter']) ==
                           (8, 64, 'big', '/lib64/ld.so.1'),
                 'path': str(args.mips_libc), 'sha256': matrix.sha(args.mips_libc), 'identity': actual})
    report = {'status': 'PASS' if all(row['passed'] for row in rows) else 'FAIL',
              'records': rows, 'driver_sha256': matrix.sha(__file__),
              'matrix_driver_sha256': matrix.sha(Path(__file__).with_name('algorithm_matrix.py'))}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'], len(rows))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
