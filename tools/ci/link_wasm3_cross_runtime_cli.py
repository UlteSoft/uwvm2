#!/usr/bin/env python3
"""Link a target-host Core 3 CLI against a qualified cross-runtime object.

The compiler, ABI, target sysroot and exact LLVM archives come from the
already-executed fixture link command. Compilation stays in the SSH cgroup.
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
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
root = a.source_root.resolve()
build = a.runtime_build.resolve()
out = a.out.resolve()
os.chdir(root)
subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
out.mkdir(parents=True, exist_ok=False)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def fingerprint(name):
    return subprocess.check_output(['python3', 'tools/ci/wasm3_source_fingerprint.py', '.', str(out / name)], text=True).strip()

source_id = fingerprint('source-before.json')
assert source_id == json.loads((build / 'source-before.json').read_text())['source_id']
assert source_id == json.loads((build / 'source-after.json').read_text())['source_id']
assert json.loads((build / 'summary.json').read_text())['passed']
toolchain = json.loads((build / 'toolchain.json').read_text())
assert all(sha(Path(path)) == digest for path, digest in toolchain.items())

command = shlex.split((build / 'unit-build.command').read_text())
source = next(part for part in command if part.startswith('test/') and part.endswith('.cc'))
command[command.index(source)] = 'src/uwvm2/uwvm/main.default.cpp'
command[command.index('-o') + 1] = str(out / 'uwvm')
command = [part for part in command if not part.startswith('-DUWVM2TEST_')]
assert '--target=riscv64-linux-gnu' in command
assert str(build / 'runtime.o') in command
assert any(part.startswith('@/dev/shm/uwvm-llvm-riscv64-ros9/build/consumer-link.rsp') for part in command)
command += ['-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0']
runtime_hash = sha(build / 'runtime.o')
(out / 'link.command').write_text(shlex.join(command) + '\n')
with (out / 'link.log').open('w') as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=900)
assert source_id == fingerprint('source-after.json')
assert runtime_hash == sha(build / 'runtime.o')
assert all(sha(Path(path)) == digest for path, digest in toolchain.items())
subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
(out / 'summary.json').write_text(json.dumps(dict(passed=True, source_id=source_id,
    runtime_build=str(build), runtime_object_sha256=runtime_hash,
    binary_sha256=sha(out / 'uwvm'), target='riscv64-linux-gnu'), indent=2) + '\n')
print('Built actual RISC-V LLVM full CLI:', out / 'uwvm')
