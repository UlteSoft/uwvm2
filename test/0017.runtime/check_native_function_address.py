#!/usr/bin/env python3
"""Check production callable-to-PC normalization against each target's unwinder.

Clang emits the real test IR; the selected LLVM emits target objects. This tests
native ABI boundaries, not the separate live-JIT/new-Wasm-syntax acceptance suite.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'test/0014.llvm_jit'))
from run_wasm3_relaxed_cross import PROFILES

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--clang', type=Path, required=True)
p.add_argument('--llvm', type=Path, required=True)
p.add_argument('--deps', type=Path, required=True)
a = p.parse_args()
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=False)
source = Path(__file__).with_suffix('.cc').with_name('native_function_address.cc')
header = root / 'src/uwvm2/runtime/lib/uwvm_runtime_native_function_address.h'
rows = []
seen = set()
for name, triple, cpu, features, emulator, _, flags in PROFILES:
    # ARM32 uses EHABI, not this runtime's DWARF registration interface. SIMD
    # variants do not change a platform's callable-address representation.
    if triple in seen or triple.startswith('arm-'):
        continue
    seen.add(triple)
    directory = a.output / name
    directory.mkdir()
    sysroot = a.deps / 'usr' / triple
    compiler = [str(a.clang), '--target=' + triple, '--sysroot=' + str(a.deps),
                '--gcc-install-dir=' + str(a.deps / 'usr/lib/gcc-cross' / triple / '15'),
                '-idirafter', str(sysroot / 'include'), '-stdlib=libstdc++',
                '-O2', '-fno-optimize-sibling-calls', '-fasynchronous-unwind-tables', *flags]
    target = ['-Xclang', '-target-cpu', '-Xclang', cpu]
    linker = ['--ld-path=' + str(a.deps / 'usr/bin' / (triple + '-ld'))] if triple in {
        'powerpc64-linux-gnu', 'powerpc-linux-gnu', 'sparc64-linux-gnu'} else ['-fuse-ld=lld']
    binary, ir, obj = (directory / suffix for suffix in ('test', 'test.ll', 'test.o'))
    phases = [
        ('clang', [*compiler, *target, '-std=c++26', '-I', str(root / 'src'), '-S', '-emit-llvm', str(source), '-o', str(ir)]),
        ('object', [str(a.llvm / 'llc'), '-O2', '-verify-machineinstrs', '-relocation-model=pic',
                    '-mcpu=' + cpu, '-mattr=' + features, '-filetype=obj', str(ir), '-o', str(obj)]),
        ('link', [*compiler, *linker, str(obj), '-L' + str(sysroot / 'lib'), '-o', str(binary)]),
        ('run', [str(a.deps / 'usr/bin' / ('qemu-' + emulator)), '-U', 'LD_LIBRARY_PATH', '-L', str(sysroot), str(binary)]),
    ]
    row = dict(profile=name, triple=triple, phases=[], passed=False)
    rows.append(row)
    for phase, command in phases:
        result = subprocess.run(command, capture_output=True, timeout=120)
        (directory / (phase + '.log')).write_bytes(result.stdout + result.stderr)
        row['phases'].append(dict(phase=phase, command=command, exit=result.returncode))
        if result.returncode:
            break
    row['passed'] = phase == 'run' and result.returncode == 0
    (a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(name, 'PASS native ABI regions and repeated activations' if row['passed'] else 'FAIL ' + phase, flush=True)
(a.output / 'inputs.json').write_text(json.dumps({str(f): hashlib.sha256(f.read_bytes()).hexdigest()
                                               for f in (source, header, Path(__file__))}, indent=2) + '\n')
raise SystemExit(any(not row['passed'] for row in rows))
