#!/usr/bin/env python3
"""Syntax-check real host EH gates with compiler-selected ARM/Thumb EH models.

This does not link or run ARM32 code against an incompatible EHABI sysroot and
makes no claim that an ARM32 DWARF native JIT has been execution-qualified.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument('--llvm-root', type=Path, required=True)
parser.add_argument('--llvm-source-root', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--deps', type=Path, default=Path('/work/deps'))
parser.add_argument('--guard', type=Path)
a = parser.parse_args()
root = a.source_root.resolve()
guard = a.guard or root/'tools/ci/require_wasm3_test_cgroup.sh'
subprocess.run(['bash', str(guard)], check=True)
a.out.mkdir(parents=True, exist_ok=False)
header = root/'src/uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h'
sha = lambda p: hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
before = sha(header)
fixture = a.out/'capability.cc'
fixture.write_text('#define UWVM_RUNTIME_LLVM_JIT\n'
    '#include <uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h>\n'
    'static_assert(uwvm2::runtime::lib::details::native_exception_host::available == bool(EXPECT_AVAILABLE));\n')
cases = []
for thumb in [False, True]:
    arch = 'thumb' if thumb else 'arm'
    for model, extra, expected, dwarf in [
        ('ehabi', [], False, False),
        ('dwarf', ['-fdwarf-exceptions'], True, True),
        ('dwarf-no-exceptions', ['-fdwarf-exceptions', '-fno-exceptions'], False, True),
        ('sjlj', ['-fsjlj-exceptions'], False, False)]:
        cases.append((arch+'-'+model, 'arm-linux-gnueabihf',
                      (['-mthumb'] if thumb else ['-marm'])+extra, expected, dwarf))
for name, triple in [('aarch64', 'aarch64-linux-gnu'), ('riscv64', 'riscv64-linux-gnu'), ('i386', 'i686-linux-gnu')]:
    cases.append((name+'-dwarf', triple, [], True, False))
results = []
for name, triple, extra, expected, dwarf in cases:
    sysroot = a.deps/'usr'/triple
    flags = [os.environ.get('CXX', '/toolchain/bin/clang++'), '--target='+triple,
        '--sysroot='+str(a.deps), '--gcc-install-dir='+str(a.deps/'usr/lib/gcc-cross'/triple/'15'),
        '-idirafter', str(sysroot/'include'), '-stdlib=libstdc++', '-std=c++26',
        '-fno-rtti', '-fexceptions', *extra]
    # The compiler, not a test-injected -D, must select ARM_DWARF_EH.
    macros = subprocess.check_output([*flags, '-dM', '-E', '-x', 'c++', '/dev/null'], text=True)
    (a.out/(name+'.macros')).write_text(macros)
    assert ('#define __ARM_DWARF_EH__ ' in macros) == dwarf, (name, macros)
    command = [*flags, '-I'+str(root/'src'), '-I'+str(a.llvm_root/'include'),
        '-I'+str(a.llvm_source_root/'include'), '-DEXPECT_AVAILABLE='+str(int(expected)),
        '-fsyntax-only', str(fixture)]
    (a.out/(name+'.command')).write_text(shlex.join(command)+'\n')
    with (a.out/(name+'.log')).open('w') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=120)
    results.append({'case': name, 'expected_available': expected,
                    'compiler_arm_dwarf_eh': dwarf, 'exit': result.returncode, 'command': command})
    (a.out/'commands.json').write_text(json.dumps(results, indent=2)+'\n')
    if result.returncode:
        raise RuntimeError((name, result.returncode, (a.out/(name+'.log')).read_text()[-6000:]))
    print('PASS', name, 'available='+str(expected), flush=True)
assert sha(header) == before
subprocess.run(['bash', str(guard)], check=True)
(a.out/'summary.json').write_text(json.dumps({'passed': True, 'cases': len(results),
    'header_sha256': before, 'runner_sha256': sha(Path(__file__)),
    'scope': 'actual header syntax under real compiler ARM/Thumb exception models; no ARM32 DWARF runtime or provider execution claim',
    'cgroup': {key: (Path('/sys/fs/cgroup')/key).read_text().strip()
               for key in ['memory.max', 'memory.swap.max', 'cpuset.cpus.effective']}}, indent=2)+'\n')
