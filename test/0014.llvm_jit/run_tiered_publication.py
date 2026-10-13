#!/usr/bin/env python3
"""Focused publication and concurrent CFI tests; SSH Linux cgroup only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--llvm-config', type=Path, required=True)
p.add_argument('--link-rsp', type=Path)
p.add_argument('--llvm-tools', type=Path, default=Path('/toolchain/bin'))
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.out = a.out.resolve()
a.out.mkdir(parents=True, exist_ok=False)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
files = [Path(__file__), Path(__file__).with_name('tiered_entry_publication.cc'),
         Path(__file__).with_name('native_exception_concurrent_unwind.cc'),
         root / 'src/uwvm2/runtime/lib/uwvm_runtime_tiered_publication.h',
         root / 'src/uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h',
         root / 'src/uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h',
         root / 'src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h']
before = {str(x.relative_to(root)): sha(x) for x in files}
for source in files:
    target = a.out / 'sources' / source.relative_to(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(source.read_bytes())
(a.out / 'source-before.json').write_text(json.dumps(before, indent=2) + '\n')

def run(name, command, timeout=180):
    (a.out / (name + '.command')).write_text(shlex.join([str(x) for x in command]) + '\n')
    with (a.out / (name + '.log')).open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=timeout)

def config(*args):
    return shlex.split(subprocess.check_output([str(a.llvm_config), *args], text=True))

common = [os.environ.get('CXX', '/toolchain/bin/clang++'), '-std=c++26', '-stdlib=libc++',
          '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '-pthread', '-I' + str(root / 'src')]
unit = a.out / 'publication'
run('publication-build', common + ['-O3', str(files[1]), '-o', str(unit)])
run('publication-run', [unit])
flags = [x for x in config('--cxxflags') if not x.startswith('-std=') and x != '-fno-exceptions']
link = ['@' + str(a.link_rsp)] if a.link_rsp else config('--ldflags', '--libs', 'core', 'executionengine', 'mcjit', 'native', '--system-libs')
native = a.out / 'concurrent-unwind'
run('unwind-build', common + flags + ['-std=c++26', '-fexceptions', '-fno-rtti', '-O1', '-g0',
                                    str(files[2]), '-o', str(native), *link], timeout=300)
run('unwind-run', [native, a.out])
run('unwind-metadata', [a.llvm_tools / 'llvm-readobj', '--sections', '--relocations', '--symbols', '--unwind', a.out / 'unwind.o'])
run('unwind-assembly', [a.llvm_tools / 'llvm-objdump', '-dr', a.out / 'unwind.o'])
assert 'PASS tiered publication:' in (a.out / 'publication-run.log').read_text()
assert 'PASS concurrent CFI:' in (a.out / 'unwind-run.log').read_text()
ir = (a.out / 'stable.ll').read_bytes()
assert all(x.read_bytes() == ir for x in a.out.glob('registration-*.ll'))
metadata = (a.out / 'unwind-metadata.log').read_text()
assert '__gxx_personality_v0' in metadata and 'uwvm_guest_exception_typeinfo_v1' in metadata
assert 'probe_trace' not in metadata and 'call_stack_push' not in metadata and 'call_stack_pop' not in metadata
assert before == {str(x.relative_to(root)): sha(x) for x in files}
provider = Path('/toolchain/lib/x86_64-unknown-linux-gnu/libunwind.so.1.0')
run('dependencies', ['ldd', native])
run('unwind-provider-symbols', [a.llvm_tools / 'llvm-readobj', '--dyn-symbols', provider])
provider_symbols = (a.out / 'unwind-provider-symbols.log').read_text()
assert all(name in provider_symbols for name in ('pthread_rwlock_rdlock', 'pthread_rwlock_wrlock', 'pthread_rwlock_unlock'))
assert str(provider.parent) in (a.out / 'dependencies.log').read_text()
(a.out / 'summary.json').write_text(json.dumps(dict(
    passed=True, sources=before, binary_sha256=sha(native), unit_sha256=sha(unit),
    libunwind_path=str(provider), libunwind_sha256=sha(provider),
    cgroup=Path('/proc/self/cgroup').read_text(),
    scope='Cold target publication ordering and concurrent native CFI/EH; no full VM throughput claim.'), indent=2) + '\n')
print((a.out / 'publication-run.log').read_text(), end='')
print((a.out / 'unwind-run.log').read_text(), end='')
