#!/usr/bin/env python3
"""Check ROS's actual public-header guard against synthetic version headers.

This tests accidental mixing, not source authenticity or archive linkage.
The production xmake manifest/static-library contract remains mandatory.
"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[2]
    guard = root/'src/uwvm2/runtime/compiler/llvm_jit/pinned_version.h'
    source = args.out/'guard.cpp'
    source.write_text('#include "' + str(guard.resolve()) + '"\n')
    cases = [('23.1.1-uwvm-ros.13', (23, 1, 1), True),
             ('23.1.1', (23, 1, 1), False),
             ('23.1.1-uwvm-ros.7', (23, 1, 1), False),
             ('23.1.1-uwvm-ros.9', (23, 1, 1), False),
             ('23.1.1-uwvm-ros.12', (23, 1, 1), False),
             ('23.1.1-uwvm-ros.130', (23, 1, 1), False),
             ('23.1.1-uwvm-ros.13git', (23, 1, 1), False),
             ('23.0.0git', (23, 0, 0), False),
             ('22.1.8', (22, 1, 8), False),
             ('23.1.1-uwvm-ros.13', (24, 0, 0), False)]
    rows = []
    controls = [(version, numbers, 1, expected) for version, numbers, expected in cases]
    controls += [('23.1.1-uwvm-ros.13', (23, 1, 1), None, False),
                 ('23.1.1-uwvm-ros.13', (23, 1, 1), 0, False)]
    for index, (version, numbers, capability, expected) in enumerate(controls):
        directory = args.out/str(index)
        headers = directory/'llvm/Config'
        headers.mkdir(parents=True)
        (headers/'llvm-config.h').write_text(
            ''.join(f'#define LLVM_VERSION_{name} {number}\n'
                    for name, number in zip(('MAJOR', 'MINOR', 'PATCH'), numbers)) +
            f'#define LLVM_VERSION_STRING "{version}"\n' +
            ('' if capability is None else
             f'#define LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED {capability}\n'))
        command = [args.cxx, '-std=c++20', '-fsyntax-only', '-I'+str(directory.resolve()), str(source.resolve())]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (directory/'compile.log').write_bytes(result.stdout+result.stderr)
        passed = (result.returncode == 0) == expected
        if not expected:
            passed &= b'uwvm2-ros requires' in result.stderr
        rows.append(dict(version=version, numbers=numbers, capability=capability, expected_success=expected,
                         command=command, returncode=result.returncode, passed=passed))
    (args.out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(f'{sum(row["passed"] for row in rows)}/{len(rows)} pinned-header controls passed')
    raise SystemExit(any(not row['passed'] for row in rows))


if __name__ == '__main__':
    main()
