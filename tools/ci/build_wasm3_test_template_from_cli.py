#!/usr/bin/env python3
"""Derive the VM fixture link command from a source-matched CLI build.

This keeps the runtime object, compiler, LLVM linkage, feature definitions and
source identity identical to the qualified product. Only the CLI entry point
is replaced by the existing tail-call fixture entry point.
"""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import shlex
import subprocess


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def source_id(root: Path, manifest: Path) -> str:
    return subprocess.check_output(
        ['python3', str(root / 'tools/ci/wasm3_source_fingerprint.py'),
         str(root), str(manifest)], text=True).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--cli-build', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    build = args.cli_build.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    before = source_id(root, build / 'test-source-before.json')
    command = shlex.split((build / 'cli.command').read_text())
    source_flag = '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"'
    if command.count(source_flag) != 1 or '-c' in command:
        raise RuntimeError('CLI command is not from this source-matched product')
    main_indices = [i for i, token in enumerate(command)
                    if token.endswith('src/uwvm2/uwvm/main.default.cpp')]
    outputs = [i for i, token in enumerate(command) if token == '-o']
    runtime_object = str(build / 'runtime.o')
    if len(main_indices) != 1 or len(outputs) != 1 or command.count(runtime_object) != 1:
        raise RuntimeError('expected one CLI entry, output and qualified runtime object')
    main_index = main_indices[0]
    command[main_index] = str(root / 'test/0013.uwvm_int/wasm3/tail_call.cc')
    command[outputs[0] + 1] = str(build / 'test-template')
    command.insert(main_index, '-DUWVM2TEST_RUNNER_USE_LLVM_JIT')
    object_hash = digest(build / 'runtime.o')
    (build / 'test.command').write_text(shlex.join(command) + '\n')
    with (build / 'test-template.build.log').open('wb') as log:
        subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT,
                       check=True)
    after = source_id(root, build / 'test-source-after.json')
    if before != after or object_hash != digest(build / 'runtime.o'):
        (build / 'test-template').rename(build / 'test-template-invalid')
        raise RuntimeError('source or runtime object changed during fixture build')
    (build / 'test-template.summary.json').write_text(json.dumps({
        'source_id': before,
        'runtime_object_sha256': object_hash,
        'binary_sha256': digest(build / 'test-template'),
        'cli_command_sha256': digest(build / 'cli.command'),
        'test_command_sha256': digest(build / 'test.command'),
    }, indent=2) + '\n')
    print('PASS source-matched VM test template', before)


if __name__ == '__main__':
    main()
