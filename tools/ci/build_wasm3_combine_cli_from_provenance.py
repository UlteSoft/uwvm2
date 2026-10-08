#!/usr/bin/env python3
"""Rebuild a qualified VM CLI with all interpreter combine levels compiled in.

The full/heavy/extra and soft/heavy local-delay implementations are compile-time
choices. Both runtime and CLI frontend need the same five definitions. Merely
accepting -Rint-no-delay-local does not prove delay code was compiled in.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
from requalify_wasm3_cli_from_provenance import bind_source_paths


COMBINE_DEFINES = (
    '-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS',
    '-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS',
    '-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS',
)
DELAY_DEFINES = (
    '-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT',
    '-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY',
)
INTERPRETER_DEFINES = COMBINE_DEFINES + DELAY_DEFINES


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def output_index(command):
    positions = [i for i, token in enumerate(command) if token == '-o']
    if len(positions) != 1 or positions[0] + 1 == len(command):
        raise RuntimeError('expected one complete compiler output option')
    return positions[0] + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--runtime-command', type=Path, required=True)
    parser.add_argument('--cli-command', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / 'runner.py').write_bytes(Path(__file__).read_bytes())

    def fingerprint(name):
        result = subprocess.run(['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
                                 str(source), str(out / name)], check=True, capture_output=True, text=True)
        return result.stdout.strip()

    before = fingerprint('source-before.json')
    source_flag = '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"'
    runtime_old = shlex.split(args.runtime_command.read_text())
    cli_old = shlex.split(args.cli_command.read_text())
    prior_source_root = bind_source_paths(runtime_old, cli_old, source)
    if prior_source_root is not None and Path(prior_source_root).resolve() != source:
        raise RuntimeError('combine input names an older absolute source snapshot')
    if source_flag not in runtime_old or source_flag not in cli_old:
        raise RuntimeError('compiler commands do not identify this source snapshot')
    if '-c' not in runtime_old or '-c' in cli_old:
        raise RuntimeError('expected qualified runtime-object and CLI-link commands')
    if any(flag in runtime_old or flag in cli_old for flag in INTERPRETER_DEFINES):
        raise RuntimeError('input already contains a combine/delay definition')
    old_object = runtime_old[output_index(runtime_old)]
    if cli_old.count(old_object) != 1:
        raise RuntimeError('CLI does not link exactly the qualified runtime object')
    runtime = [runtime_old[0], *INTERPRETER_DEFINES, *runtime_old[1:]]
    cli = [cli_old[0], *INTERPRETER_DEFINES, *cli_old[1:]]
    runtime[output_index(runtime)] = str(out / 'runtime.o')
    cli[cli.index(old_object)] = str(out / 'runtime.o')
    cli[output_index(cli)] = str(out / 'uwvm')
    metadata = dict(source=str(source), source_id=before, combine_definitions=COMBINE_DEFINES,
                    delay_definitions=DELAY_DEFINES,
                    host_optimization=[token for token in runtime if token.startswith('-O')],
                    runtime_command=runtime, cli_command=cli,
                    input_runtime_command_sha256=sha256(args.runtime_command),
                    input_cli_command_sha256=sha256(args.cli_command),
                    compiler_sha256=sha256(Path(runtime[0])),
                    runner_sha256=sha256(Path(__file__)),
                    cgroup={name: Path('/sys/fs/cgroup', name).read_text().strip()
                            for name in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')})
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    for label, command in (('runtime', runtime), ('cli', cli)):
        with (out / (label + '.log')).open('wb') as log:
            subprocess.run(command, cwd=source, check=True, stdout=log, stderr=subprocess.STDOUT)
        print('BUILT', label, flush=True)
    if before != fingerprint('source-after.json'):
        (out / 'uwvm').rename(out / 'uwvm-source-changed-invalid')
        raise RuntimeError('source changed during combine CLI build')
    metadata['runtime_object_sha256'] = sha256(out / 'runtime.o')
    metadata['binary_sha256'] = sha256(out / 'uwvm')
    (out / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('PASS all-level interpreter CLI', before, flush=True)


if __name__ == '__main__':
    main()
