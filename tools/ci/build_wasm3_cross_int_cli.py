#!/usr/bin/env python3
"""Build an actual interpreter CLI with Clang IR and a complete LLVM backend.

This also covers target backends omitted from the installed Clang binary. All
musttail instructions and machine verification remain enabled in object emission.
"""
import argparse
import gzip
import json
from pathlib import Path
import resource
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--repository', type=Path, required=True)
p.add_argument('--clang', type=Path, required=True)
p.add_argument('--llvm', type=Path, required=True)
p.add_argument('--deps', type=Path, required=True)
p.add_argument('--profile', required=True)
a = p.parse_args()
sys.path.insert(0, str(a.repository / 'test/0014.llvm_jit'))
from run_wasm3_int_cross import PROFILES
profile = next((entry for entry in PROFILES if entry[0] == a.profile), None)
if profile is None:
    p.error('unknown profile')
_, triple, cpu, features, _, _, extra = profile
a.output.mkdir(parents=True, exist_ok=False)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
subprocess.run(['bash', str(a.repository / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
fingerprint = [sys.executable, str(a.repository / 'tools/ci/wasm3_source_fingerprint.py'), str(a.repository)]
before = subprocess.check_output(fingerprint + [str(a.output / 'source-manifest.json')], text=True).strip()
gcc = a.deps / 'usr/lib/gcc-cross' / triple / '15'
sysroot = a.deps / 'usr' / triple
compiler = [str(a.clang), f'--target={triple}', f'--sysroot={a.deps}', f'--gcc-install-dir={gcc}',
            '-idirafter', str(sysroot / 'include'), '-stdlib=libstdc++', '-O1', *extra]
target = ['-Xclang', '-target-cpu', '-Xclang', cpu]
for feature in features.split(','):
    target += ['-Xclang', '-target-feature', '-Xclang', feature]
includes = sum((['-I', str(a.repository / entry)] for entry in
                ('src', 'third-parties/bizwen/include', 'third-parties/fast_io/include', 'third-parties/boost_unordered/include')), [])
defines = ['-DUWVM=2', '-DUWVM_USE_UWVM_INT', '-DUWVM_DISABLE_JIT', '-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1',
           '-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
           '-DUWVM2_BUILD_SOURCE_ID=u8"' + before + '"']
codegen = [str(a.llvm / 'llc'), '-O2', '-verify-machineinstrs', '-relocation-model=pic', f'-mcpu={cpu}', f'-mattr={features}']
if triple.startswith('mips'):
    codegen.append('-mips-tail-calls')
rows = []

def run(phase, command):
    with (a.output / (phase + '.log')).open('wb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=1200)
    rows.append(dict(phase=phase, command=command, exit=result.returncode))
    (a.output / 'build-results.json').write_text(json.dumps(rows, indent=2) + '\n')
    if result.returncode:
        raise RuntimeError(f'{phase} failed: see {a.output / (phase + ".log")}')

objects = []
sources = [('runtime', 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),
           ('main', 'src/uwvm2/uwvm/main.default.cpp'), ('host', 'src/uwvm2/uwvm/host_api.default.cpp')]
counts = {}
for name, relative in sources:
    ir, obj, assembly = (a.output / (name + suffix) for suffix in ('.ll', '.o', '.s'))
    run(name + '-clang', [*compiler, *target, '-std=c++26', '-g0', '-ferror-limit=1', '-Wno-undefined-inline',
                         *includes, *defines, '-S', '-emit-llvm', str(a.repository / relative), '-o', str(ir)])
    counts[name] = ir.read_text().count('musttail call')
    run(name + '-object', [*codegen, '-filetype=obj', str(ir), '-o', str(obj)])
    run(name + '-assembly', [*codegen, '-filetype=asm', str(ir), '-o', str(assembly)])
    objects.append(str(obj))
    for file in (ir, assembly):
        with file.open('rb') as source, gzip.open(str(file) + '.gz', 'wb', compresslevel=3) as compressed:
            shutil.copyfileobj(source, compressed)
        file.unlink()
    print(name, 'built; musttail calls', counts[name], flush=True)
linker = ['-fuse-ld=lld']
if triple in {'powerpc64-linux-gnu', 'powerpc-linux-gnu', 'sparc64-linux-gnu'}:
    linker = ['--ld-path=' + str(a.deps / 'usr/bin' / (triple + '-ld'))]
run('link', [*compiler, *linker, *objects, '-L' + str(sysroot / 'lib'), '-latomic', '-pthread', '-ldl', '-lm', '-o', str(a.output / 'uwvm')])
after = subprocess.check_output(fingerprint + [str(a.output / 'source-manifest-after.json')], text=True).strip()
if before != after:
    (a.output / 'uwvm').rename(a.output / 'uwvm-source-changed-invalid')
    raise RuntimeError('Production sources changed during compilation')
(a.output / 'musttail-counts.json').write_text(json.dumps(counts, indent=2) + '\n')
print('Built actual target interpreter CLI', a.profile, before)
