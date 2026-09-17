#!/usr/bin/env python3
"""Run every retained downstream regression with freshly built ROS LLVM tools.

Build llc, FileCheck and llvm-rtdyld in the bundled CMake directory first. This
checks backend/machine verification and relocated bytes, not guest execution.
No tool may fall back to PATH's LLVM installation.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--llvm-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = args.llvm_build.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    version = json.loads((build/'uwvm-llvm-version.json').read_text())['version']
    if version != '23.1.1-uwvm-ros.6':
        raise RuntimeError(f'Not the pinned ROS LLVM: {version}')
    records = []
    tests = root/'third-parties/llvm/llvm/test/CodeGen'
    for test in sorted(path for path in tests.rglob('*') if path.is_file()):
        temporary = args.out/(test.name+'.files')
        temporary.mkdir()
        for number, line in enumerate(test.read_text().splitlines(), 1):
            if 'RUN: ' not in line:
                continue
            command = line.split('RUN: ', 1)[1]
            command = command.replace('llc ', shlex.quote(str(build/'bin/llc')) + ' ')
            command = command.replace('FileCheck ', shlex.quote(str(build/'bin/FileCheck')) + ' ')
            command = command.replace('llvm-rtdyld ', shlex.quote(str(build/'bin/llvm-rtdyld')) + ' ')
            command = command.replace('%S', shlex.quote(str(test.parent)))
            command = command.replace('%T', shlex.quote(str(temporary.resolve())))
            command = command.replace('%s', shlex.quote(str(test)))
            result = subprocess.run(['bash', '-o', 'pipefail', '-c', command],
                                    capture_output=True, timeout=120)
            log = args.out/f'{test.name}-{number}.log'
            log.write_bytes(result.stdout + result.stderr)
            records.append(dict(test=test.name, line=number, command=command, code=result.returncode))
    (args.out/'results.json').write_text(json.dumps(dict(version=version, rows=records), indent=2)+'\n')
    failures = sum(row['code'] != 0 for row in records)
    assert len(records) == 49, 'Regression RUN-line inventory changed; review this runner'
    print(f'{len(records)} backend/RuntimeDyld regression steps; {failures} failures')
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
