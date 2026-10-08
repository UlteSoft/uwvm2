#!/usr/bin/env python3
"""Shared-size publication and managed waits on big-endian and 32-bit QEMU targets."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('repository', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--case', choices=('size','wait'), default='size')
a = p.parse_args()
subprocess.run(['bash', str(a.repository / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=True)
deps = Path('/work/deps')
llvm = Path('/work/artifacts/uwvm2-ros-jit/llvm/bin')
rows = []
for name, triple, cpu, features, emulator, flags in [
    ('ppc64-be', 'powerpc64-linux-gnu', 'pwr8', '+vsx', 'ppc64', ['-mcpu=power8']),
    ('arm32', 'arm-linux-gnueabihf', 'cortex-a15', '+neon,+vfp4', 'arm', ['-mcpu=cortex-a15', '-mfpu=neon-vfpv4']),
]:
    out = a.output / name
    out.mkdir(exist_ok=True)
    sysroot = deps / 'usr' / triple
    compiler = ['/toolchain/bin/clang++', f'--target={triple}', f'--sysroot={deps}',
        f'--gcc-install-dir={deps}/usr/lib/gcc-cross/{triple}/15', '-idirafter', str(sysroot / 'include'),
        '-stdlib=libstdc++', '-std=c++26', '-O2', '-DNDEBUG', *flags]
    target = ['-Xclang', '-target-cpu', '-Xclang', cpu]
    for feature in features.split(','):
        target += ['-Xclang', '-target-feature', '-Xclang', feature]
    codegen = [str(llvm / 'llc'), '-O2', '-verify-machineinstrs', '-relocation-model=pic', f'-mcpu={cpu}', f'-mattr={features}']
    linker = ['--ld-path=' + str(deps / 'usr/bin' / (triple + '-ld'))] if name == 'ppc64-be' else ['-fuse-ld=lld']
    source = a.repository / ('test/0017.runtime/shared_memory_size.cc' if a.case == 'size' else 'test/0017.runtime/wasm_wait.cc')
    phases = [
        ('clang', [*compiler, *target, '-I', str(a.repository / 'src'), '-I', str(a.repository / 'third-parties/fast_io/include'), '-I', str(a.repository / 'third-parties/bizwen/include'), '-I', str(a.repository / 'third-parties/boost_unordered/include'), '-S', '-emit-llvm', str(source), '-o', str(out / 'snapshot.ll')]),
        ('object', [*codegen, '-filetype=obj', str(out / 'snapshot.ll'), '-o', str(out / 'snapshot.o')]),
        ('assembly', [*codegen, '-filetype=asm', str(out / 'snapshot.ll'), '-o', str(out / 'snapshot.s')]),
        ('link', [*compiler, *linker, str(out / 'snapshot.o'), '-L' + str(sysroot / 'lib'), '-latomic', '-pthread', '-o', str(out / 'snapshot')]),
        ('run', [str(deps / 'usr/bin' / ('qemu-' + emulator)), '-U', 'LD_LIBRARY_PATH', '-L', str(sysroot), str(out / 'snapshot')]),
    ]
    row = {'profile': name, 'phases': []}
    for phase, cmd in phases:
        result = subprocess.run(cmd, capture_output=True, timeout=120)
        log = result.stdout + result.stderr
        (out / (phase + '.log')).write_bytes(log)
        row['phases'].append({'phase': phase, 'command': cmd, 'exit': result.returncode})
        if result.returncode:
            print(log.decode(errors='replace')[-2500:])
            break
    row['passed'] = phase == 'run' and result.returncode == 0
    rows.append(row)
    (a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(name, 'PASS' if row['passed'] else 'FAIL', flush=True)
(a.output / 'inputs.json').write_text(json.dumps({f: hashlib.sha256((a.repository / f).read_bytes()).hexdigest() for f in (
    'src/uwvm2/object/memory/linear/mmap.h', 'src/uwvm2/object/memory/linear/allocator.h',
    'src/uwvm2/runtime/wasm_threads/wait.h', 'src/uwvm2/utils/thread/keyed_wait_set.h',
    'test/0017.runtime/shared_memory_size.cc', 'test/0017.runtime/wasm_wait.cc')}, indent=2) + '\n')
raise SystemExit(not all(row['passed'] for row in rows))
