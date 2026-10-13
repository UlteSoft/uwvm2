#!/usr/bin/env python3
"""Build the CLI against a qualified runtime object with identical production inputs.

Run only inside the remote Linux test cgroup. The original fixture command supplies
all backend, memory, LLVM and ABI options; only fixture-local macros are removed.
"""
import argparse
import hashlib
import json
import os
import shlex
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-build', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
os.chdir(root)
subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
build = a.runtime_build.resolve()
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=False)

def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def fingerprint(path):
    return subprocess.check_output(['python3', 'tools/ci/wasm3_source_fingerprint.py', '.', str(path)], text=True).strip()

before = fingerprint(out / 'source-before.json')
assert before == json.loads((build / 'source-before.json').read_text())['source_id'], 'runtime inputs changed'
assert before == json.loads((build / 'source-after.json').read_text())['source_id'], 'unqualified runtime build'
command = shlex.split((build / 'test.command').read_text())
source = next(x for x in command if x.startswith('test/') and x.endswith('.cc'))
command[command.index(source)] = 'src/uwvm2/uwvm/main.default.cpp'
command[command.index('-o') + 1] = str(out / 'uwvm')
command = [x for x in command if not x.startswith('-DUWVM2TEST_')]
assert not any(x.startswith('-DUWVM_VERSION_') for x in command)
command += ['-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0']
object_hash = digest(build / 'runtime.o')
(out / 'build.command').write_text(shlex.join(command) + '\n')
with (out / 'build.log').open('w') as f:
    subprocess.run(command, stdout=f, stderr=f, check=True)
subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
assert before == fingerprint(out / 'source-after.json')
assert object_hash == digest(build / 'runtime.o')
(out / 'summary.json').write_text(json.dumps(dict(built=True, source_id=before,
    runtime_build=str(build), runtime_object_sha256=object_hash, binary_sha256=digest(out / 'uwvm')), indent=2) + '\n')
print('Built CLI with verified runtime object:', out / 'uwvm')
