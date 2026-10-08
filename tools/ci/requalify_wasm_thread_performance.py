#!/usr/bin/env python3
"""Relink real VM thread benchmarks against an unchanged qualified runtime.

The previous successful commands supply product, LLVM and host ABI flags. Only
the source snapshot, its content ID, the verified runtime object and outputs
change. Build and execute this helper inside the required Linux cgroup.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


SOURCE_FLAG = re.compile(r'^-DUWVM2_BUILD_SOURCE_ID=u8"(sha256:[0-9a-f]{64})"$')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def output_index(command):
    indices = [i for i, value in enumerate(command) if value == '-o']
    if len(indices) != 1 or indices[0] + 1 >= len(command):
        raise RuntimeError('expected one complete output option')
    return indices[0] + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--runtime-build', type=Path, required=True)
    parser.add_argument('--timed-command', type=Path, required=True)
    parser.add_argument('--qualify-command', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    subprocess.run(['bash', str(source/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    runtime = args.runtime_build.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)

    def fingerprint(name):
        return subprocess.check_output(['python3', str(source/'tools/ci/wasm3_source_fingerprint.py'),
                                        str(source), str(out/name)], text=True).strip()

    before = fingerprint('source-before.json')
    metadata = json.loads((runtime/'build.json').read_text())
    if before != metadata['source_id'] or before != json.loads((runtime/'source-after.json').read_text())['source_id']:
        raise RuntimeError('runtime object was built from different production sources')
    object_path = runtime/'runtime.o'
    object_hash = digest(object_path)
    if object_hash != metadata['runtime_object_sha256']:
        raise RuntimeError('runtime object hash differs from its build manifest')
    commands = {}
    old_ids = set()
    for label, path in (('timed', args.timed_command), ('qualify', args.qualify_command)):
        command = shlex.split(path.read_text())
        positions = [i for i, value in enumerate(command) if SOURCE_FLAG.fullmatch(value)]
        if len(positions) != 1:
            raise RuntimeError(f'{label}: missing unique prior source ID')
        old_ids.add(SOURCE_FLAG.fullmatch(command[positions[0]]).group(1))
        command[positions[0]] = '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"'
        originals = [value for value in command if value.endswith('/test/0017.runtime/wasm_thread_performance.cc')]
        if len(originals) != 1:
            raise RuntimeError(f'{label}: expected exact guest thread fixture')
        old_root = originals[0].split('/test/0017.runtime/wasm_thread_performance.cc')[0]
        # A qualified frozen candidate may carry a descriptive suffix while
        # retaining the same ordinary/ROS product kind and exact source ID.
        product_name = r'core3-r[0-9]+-(ordinary|ros)(?:-[a-z0-9-]+)?'
        previous_product = re.fullmatch(product_name, Path(old_root).name)
        current_product = re.fullmatch(product_name, source.name)
        if previous_product is None or current_product is None or previous_product.group(1) != current_product.group(1):
            raise RuntimeError(f'{label}: provenance product differs from current source')
        command = [value.replace(old_root, str(source)) for value in command]
        old_objects = [value for value in command if value.endswith('/runtime.o')]
        if len(old_objects) != 1:
            raise RuntimeError(f'{label}: expected one prior runtime object')
        command[command.index(old_objects[0])] = str(object_path)
        command[output_index(command)] = str(out/label)
        if label == 'timed' and any(value.startswith('-DUWVM_THREAD_BENCH_QUALIFY_CREATION') for value in command):
            raise RuntimeError('timed benchmark must not instrument native thread creation')
        if label == 'qualify' and '-DUWVM_THREAD_BENCH_QUALIFY_CREATION' not in command:
            raise RuntimeError('qualifier must count real native thread creation')
        (out/(label+'.command')).write_text(shlex.join(command)+'\n')
        commands[label] = command
    if len(old_ids) != 1:
        raise RuntimeError('provenance benchmarks came from different sources')
    record = dict(source=str(source), source_id=before, prior_source_id=old_ids.pop(),
                  runtime_build=str(runtime), runtime_object_sha256=object_hash,
                  input_timed_sha256=digest(args.timed_command), input_qualify_sha256=digest(args.qualify_command),
                  runner_sha256=digest(Path(__file__)), compiler_sha256=digest(commands['timed'][0]),
                  cgroup={name: Path('/sys/fs/cgroup', name).read_text().strip()
                          for name in ('memory.max','memory.swap.max','cpuset.cpus.effective')},
                  commands=commands)
    (out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
    for label in ('qualify','timed'):
        with (out/(label+'.build.log')).open('wb') as log:
            subprocess.run(commands[label],cwd=source,check=True,stdout=log,stderr=subprocess.STDOUT)
        print('BUILT',label,flush=True)
    after = fingerprint('source-after.json')
    if before != after or object_hash != digest(object_path):
        for label in ('qualify','timed'):
            (out/label).rename(out/(label+'-source-changed-invalid'))
        raise RuntimeError('production source or runtime object changed during link')
    record['qualify_sha256'] = digest(out/'qualify')
    record['timed_sha256'] = digest(out/'timed')
    (out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS requalified VM thread binaries',before,flush=True)


if __name__ == '__main__':
    main()
