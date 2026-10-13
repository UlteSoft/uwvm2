#!/usr/bin/env python3
"""Rebuild an immutable, previously qualified Wasm 3 CLI at host -O3.

The input commands come from an actual successful O1 build of the same source
snapshot. This preserves all product, LLVM, dependency, and source-id flags
while changing only host optimization and the two output paths. Generated Wasm
code is selected separately by the runtime's pb-o3 policy.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def output_path(command):
    indices = [index for index, token in enumerate(command) if token == '-o']
    if len(indices) != 1 or indices[0] + 1 >= len(command):
        raise RuntimeError('expected exactly one -o output')
    return indices[0] + 1


def optimize(command):
    old = sum(token == '-O1' for token in command)
    if old < 1 or any(token in ('-O0', '-O2', '-O3', '-Os', '-Oz') for token in command):
        raise RuntimeError('expected the qualified O1 command without other host optimization flags')
    return ['-O3' if token == '-O1' else token for token in command]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--runtime-command', type=Path, required=True)
    parser.add_argument('--cli-command', type=Path, required=True)
    parser.add_argument('--test-command', type=Path,
                        help='Save an equally optimized VM fixture link template for later performance tests')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    fingerprint = source / 'tools/ci/wasm3_source_fingerprint.py'

    def source_id(label):
        result = subprocess.run(['python3', str(fingerprint), str(source), str(out / (label + '.json'))],
                                check=True, text=True, capture_output=True)
        return result.stdout.strip()

    before = source_id('source-before')
    runtime = optimize(shlex.split(args.runtime_command.read_text()))
    cli = optimize(shlex.split(args.cli_command.read_text()))
    source_flag = '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"'
    if source_flag not in runtime or source_flag not in cli:
        raise RuntimeError('original compiler commands do not identify this immutable source')
    runtime_index = output_path(runtime)
    old_object = runtime[runtime_index]
    runtime[runtime_index] = str(out / 'runtime.o')
    cli_index = output_path(cli)
    if cli.count(old_object) != 1:
        raise RuntimeError('CLI command does not link the qualified runtime object exactly once')
    cli[cli.index(old_object)] = runtime[runtime_index]
    cli[cli_index] = str(out / 'uwvm')
    if '-c' not in runtime or '-c' in cli:
        raise RuntimeError('expected separate runtime object and CLI link commands')
    if args.test_command:
        test = optimize(shlex.split(args.test_command.read_text()))
        if source_flag not in test or test.count(old_object) != 1:
            raise RuntimeError('test command is not from the same qualified runtime')
        test[test.index(old_object)] = runtime[runtime_index]
        test[output_path(test)] = str(out / 'test-template')
        (out / 'test.command').write_text(shlex.join(test) + '\n')
    (out / 'runtime.command').write_text(shlex.join(runtime) + '\n')
    metadata = dict(source=str(source), source_id=before, host_optimization='O3',
                    runtime_command=runtime, cli_command=cli,
                    input_runtime_command_sha256=sha256(args.runtime_command),
                    input_cli_command_sha256=sha256(args.cli_command),
                    compiler_sha256=sha256(Path(runtime[0])))
    if args.test_command:
        metadata['input_test_command_sha256'] = sha256(args.test_command)
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    for label, command in (('runtime', runtime), ('cli', cli)):
        with (out / (label + '.log')).open('wb') as log:
            subprocess.run(command, cwd=source, check=True, stdout=log, stderr=subprocess.STDOUT)
        print('BUILT', label, flush=True)
    after = source_id('source-after')
    if before != after:
        (out / 'uwvm').rename(out / 'uwvm-source-changed-invalid')
        raise RuntimeError('source changed during build')
    metadata['binary_sha256'] = sha256(out / 'uwvm')
    metadata['runtime_object_sha256'] = sha256(out / 'runtime.o')
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('PASS immutable O3 host CLI', before, flush=True)


if __name__ == '__main__':
    main()
