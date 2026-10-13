#!/usr/bin/env python3
"""Bounded host-control parser/state tests; run only in the SSH Linux cgroup."""
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
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.out = a.out.resolve()
a.out.mkdir(parents=True, exist_ok=False)
files = sorted((root / 'src/uwvm2/utils/control').glob('*')) + [Path(__file__), Path(__file__).with_name('session.cc'),
                                                          Path(__file__).with_name('linux_launch_channel.cc'), Path(__file__).with_name('buffer_helpers.h'), Path(__file__).with_name('posix_test_abi.h'), Path(__file__).with_name('posix_nothrow_files.cc')]
files += [root / 'third-parties/fast_io/include/fast_io_hosted/filesystem' / x for x in ('native.h', 'posix_nothrow.h')]
files += [root / 'third-parties/fast_io/share/fast_io/fast_io_inc/host/posix.inc']
sha = lambda x: hashlib.sha256(x.read_bytes()).hexdigest()
before = {str(x.relative_to(root)): sha(x) for x in files if x.is_file()}
for source in files:
    if not source.is_file():
        continue
    target = a.out / 'sources' / source.relative_to(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(source.read_bytes())
(a.out / 'source-before.json').write_text(json.dumps(before, indent=2) + '\n')

def run(name, command, env=None):
    (a.out / (name + '.command')).write_text(shlex.join([str(x) for x in command]) + '\n')
    with (a.out / (name + '.log')).open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180, env=env)

common = [os.environ.get('CXX', '/toolchain/bin/clang++'), '-std=c++26', '-stdlib=libc++',
          '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '-pthread', '-I' + str(root / 'src'),
          '-I' + str(root / 'third-parties/fast_io/include')]
for name, options in [('o3', ['-O3']), ('sanitize', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']),
                      ('no-exceptions', ['-O2', '-fno-exceptions'])]:
    exe = a.out / name
    run(name + '-build', common + options + [str(Path(__file__).with_name('session.cc')), '-o', str(exe)])
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    run(name + '-run', [exe], env)
    assert 'PASS control session' in (a.out / (name + '-run.log')).read_text()
    print((a.out / (name + '-run.log')).read_text(), end='', flush=True)
    transport = a.out / (name + '-transport')
    run(name + '-transport-build', common + options + [str(Path(__file__).with_name('linux_launch_channel.cc')), '-o', str(transport)])
    run(name + '-transport-run', [transport], env)
    assert 'PASS Linux launch channel' in (a.out / (name + '-transport-run.log')).read_text()
    print((a.out / (name + '-transport-run.log')).read_text(), end='', flush=True)
    filesystem = a.out / (name + '-posix-nothrow')
    run(name + '-posix-nothrow-build', common + options + [str(Path(__file__).with_name('posix_nothrow_files.cc')), '-o', str(filesystem)])
    run(name + '-posix-nothrow-run', [filesystem], env)
    assert 'PASS fast_io POSIX nothrow:' in (a.out / (name + '-posix-nothrow-run.log')).read_text()
    print((a.out / (name + '-posix-nothrow-run.log')).read_text(), end='', flush=True)

module_dir = root / 'src/uwvm2/utils/control'
module_flags = [x for x in common if x not in ('-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld')]
bindings = []
objects = []
fast_io_pcm, fast_io_obj = a.out / 'fast_io.pcm', a.out / 'fast_io.o'
run('module-fast-io', module_flags + ['--precompile', str(root / 'third-parties/fast_io/share/fast_io/fast_io.cppm'), '-o', str(fast_io_pcm)])
bindings.append('-fmodule-file=fast_io=' + str(fast_io_pcm))
run('module-object-fast-io', module_flags + ['-c', str(fast_io_pcm), '-o', str(fast_io_obj)])
objects.append(str(fast_io_obj))
for name in ('protocol', 'sealed_input', 'session', 'linux_launch_channel', 'impl'):
    pcm, obj = a.out / (name + '.pcm'), a.out / (name + '.o')
    run('module-' + name, module_flags + bindings + ['--precompile', str(module_dir / (name + '.cppm')), '-o', str(pcm)])
    module_name = 'uwvm2.utils.control' + ('' if name == 'impl' else ':' + name)
    bindings += ['-fmodule-file=' + module_name + '=' + str(pcm)]
    run('module-object-' + name, module_flags + bindings + ['-c', str(pcm), '-o', str(obj)])
    objects.append(str(obj))
consumer = a.out / 'consumer.cc'
consumer.write_text('import fast_io;\nimport uwvm2.utils.control;\nint main(){uwvm2::utils::control::launch_authority a; fast_io::posix_file f; auto s=fast_io::posix_status_nothrow(fast_io::posix_io_observer{-1}); auto r=fast_io::posix_read_nothrow(fast_io::posix_io_observer{-1},nullptr,0); auto w=fast_io::posix_write_nothrow(fast_io::posix_io_observer{-1},nullptr,0); auto y=fast_io::posix_fsync_nothrow(fast_io::posix_io_observer{-1}); auto c=fast_io::posix_close_nothrow(f); auto o=fast_io::posix_openat_nothrow(fast_io::posix_at_entry{-1},"relative",0); return a.status()!=uwvm2::utils::control::error::disabled || s.error==0 || r.error==0 || w.error==0 || y.error==0 || c.error==0 || o.error==0;}\n')
run('module-consumer-build', common + bindings + [str(consumer), *objects, '-o', str(a.out / 'consumer')])
run('module-consumer-run', [a.out / 'consumer'])
assert before == {str(root.joinpath(x).relative_to(root)): sha(root / x) for x in before}
(a.out / 'summary.json').write_text(json.dumps({
    'passed': True, 'sources': before, 'profiles': ['o3', 'asan-ubsan-leaks', 'no-exceptions'],
    'named_module_consumer': True, 'cgroup': Path('/proc/self/cgroup').read_text(),
    'scope': 'Host handles/framing/state plus Linux pre-fork launch socketpair kernel credentials/pidfd; no exec bootstrap, runtime debugger, or replacement execution.'}, indent=2) + '\n')
print('PASS control named module precompile, object link and consumer', flush=True)
