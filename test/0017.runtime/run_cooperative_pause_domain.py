#!/usr/bin/env python3
"""Qualify cooperative pause coordination in the required remote Linux cgroup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='/toolchain/bin/clang++')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    os.chdir(root)
    subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    header = root / 'src/uwvm2/utils/thread/cooperative_pause_domain.h'
    partition = header.with_suffix('.cppm')
    fixture = root / 'test/0017.runtime/cooperative_pause_domain.cc'
    (out / 'inputs.json').write_text(json.dumps({str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (header, partition, fixture, Path(__file__))}, indent=2) + '\n')
    flags = [args.cxx, '-std=c++26', '-stdlib=libc++', '-DUWVM=2', '-fno-rtti', '-pthread',
        '-I', 'src', '-I', 'third-parties/fast_io/include', '-I', 'third-parties/bizwen/include']
    link = ['-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind']
    rows = []
    def run(name, command, timeout=90):
        (out / (name + '.command')).write_text(shlex.join(command) + '\n')
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=log, timeout=timeout,
                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        rows.append({'name': name, 'command': command, 'exit': result.returncode})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError((out / (name + '.log')).read_text()[-12000:])
    for name, optimization in [('o3', ['-O3']), ('asan-ubsan', ['-O1', '-g1', '-fno-omit-frame-pointer', '-fsanitize=address,undefined'])]:
        run(name + '-build', flags + optimization + [str(fixture), '-o', str(out / name)] + link)
        run(name + '-run', [str(out / name)])
    # Compile a real exported partition, primary module and importing consumer.
    # It is a focused consumer of this partition, not a claim that the full
    # project's complete module graph was built with all optional backends.
    primary = out / 'thread.cppm'
    primary.write_text('export module uwvm2.utils.thread;\nexport import :cooperative_pause_domain;\n')
    consumer = out / 'consumer.cc'
    consumer.write_text('''#include <chrono>
#include <cstdlib>
import uwvm2.utils.thread;
int main() {
    using namespace uwvm2::utils::thread;
    cooperative_pause_domain domain{1};
    auto ticket = domain.request_pause();
    if(!ticket || domain.wait_until_paused(ticket, std::chrono::steady_clock::now()) != cooperative_pause_result::paused) { std::abort(); }
    if(!domain.resume(ticket) || domain.is_closed()) { std::abort(); }
    domain.close();
    if(!domain.is_closed() || domain.enter()) { std::abort(); }
}
''')
    pcm = out / 'pause.pcm'
    primary_pcm = out / 'thread.pcm'
    reference = '-fmodule-file=uwvm2.utils.thread:cooperative_pause_domain=' + str(pcm)
    run('partition-precompile', flags + ['-O1', '--precompile', str(partition), '-o', str(pcm)])
    run('primary-precompile', flags + ['-O1', '--precompile', str(primary), reference, '-o', str(primary_pcm)])
    run('partition-object', flags + ['-O1', '-c', str(pcm), '-o', str(out / 'pause.o')])
    run('primary-object', flags + ['-O1', '-c', str(primary_pcm), reference, '-o', str(out / 'thread.o')])
    run('module-consumer-build', flags + ['-O1', str(consumer), reference,
        '-fmodule-file=uwvm2.utils.thread=' + str(primary_pcm), str(out / 'pause.o'), str(out / 'thread.o'),
        '-o', str(out / 'consumer')] + link)
    run('module-consumer-run', [str(out / 'consumer')])
    subprocess.run(['bash', 'tools/ci/require_wasm3_test_cgroup.sh'], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'commands': len(rows),
        'profiles': ['O3', 'ASan/UBSan/leaks', 'real module consumer']}, indent=2) + '\n')
    print('PASS cooperative pause O3, ASan/UBSan/leaks and actual module consumer')


if __name__ == '__main__':
    main()
