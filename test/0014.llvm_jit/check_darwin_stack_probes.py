#!/usr/bin/env python3
"""Validate native macOS arm64 JIT probes on a real 16-KiB-page host.

Build the production attribute emitter, inspect native code, and sweep guard
entry alignments for 18 frame sizes. Other hosts explicitly skip (exit 77).
"""
import argparse
import json
import os
from pathlib import Path
import platform
import re
import resource
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FRAMES = (1, 16, 2048, 4096, 16336, 16352, 16368, 16384, 16400, 20000,
          32736, 32752, 32768, 32784, 65504, 65536, 65552, 131072)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'clang++'))
    parser.add_argument('--llvm-config', default=os.environ.get('LLVM_CONFIG', 'llvm-config'))
    parser.add_argument('--cxxflag', action='append', default=[], help='extra compiler/linker option; repeat as needed')
    parser.add_argument('--out', type=Path, help='new output directory; must not already exist')
    args = parser.parse_args()
    if sys.platform != 'darwin' or platform.machine() != 'arm64' or os.sysconf('SC_PAGESIZE') != 16384:
        print('SKIP: requires native macOS arm64 with 16-KiB pages')
        return 77
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='uwvm-darwin-probes-'))
    if args.out:
        out.mkdir(parents=True, exist_ok=False)
    # Avoid a project shell injecting an incompatible libclang-cpp into the
    # explicitly selected compiler. Changes affect only these child processes.
    environment = os.environ.copy()
    environment.pop('DYLD_LIBRARY_PATH', None)
    environment.pop('DYLD_FALLBACK_LIBRARY_PATH', None)
    commands = []

    def run(command, timeout=120):
        commands.append([str(arg) for arg in command])
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        result = subprocess.run(commands[-1], capture_output=True, text=True,
                                env=environment, timeout=timeout)
        (out / f'command-{len(commands):03}.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{shlex.join(commands[-1])}: exit {result.returncode}\n{result.stdout}{result.stderr}')
        return result.stdout

    def query(*arguments):
        return run([args.llvm_config, *arguments]).strip()

    bindir = Path(query('--bindir'))
    includedir = query('--includedir')
    libdir = query('--libdir')
    version = query('--version')
    links = shlex.split(query('--link-shared', '--ldflags', '--libs', 'core', '--system-libs'))
    generator = out / 'generate'
    includes = [f'-I{ROOT / path}' for path in ('src', 'third-parties/fast_io/include',
                'third-parties/bizwen/include', 'third-parties/boost_unordered/include')]
    run([args.cxx, *args.cxxflag, '-std=c++26', '-O1', '-fno-rtti', '-DUWVM=2', '-DUWVM_TEST=2',
         '-DUWVM_USE_LLVM_JIT', '-DUWVM_USE_UWVM_INT', '-DUWVM_USE_THREAD_LOCAL',
         '-Wno-undefined-inline', '-Wno-deprecated-declarations', *includes,
         '-isystem', includedir, '-include', 'uwvm2/uwvm/io/impl.h',
         ROOT / 'test/0014.llvm_jit/llvm_jit_stack_probes.cc',
         *links, f'-Wl,-rpath,{libdir}', '-o', generator], timeout=180)
    run([generator])
    policies = {'arm64-apple-macosx': '16384', 'aarch64-unknown-linux-gnu': '4096',
                'arm64-apple-ios': '4096', 'x86_64-apple-darwin': '4096',
                'riscv64-unknown-linux-gnu': '2048', 'aarch64-pc-windows-msvc': None}
    for triple, expected in policies.items():
        ir = run([generator, triple])
        if expected is None:
            assert '"probe-stack"' not in ir, triple
        else:
            assert f'"stack-probe-size"="{expected}"' in ir, triple
    runner = out / 'runner.o'
    run([args.cxx, *args.cxxflag, '-std=c++26', '-O3', f'-I{ROOT / "src"}', '-c',
         ROOT / 'test/0014.llvm_jit/fixtures/llvm_stack_probes_runner.cpp', '-o', runner])
    records = []
    for frame in FRAMES:
        source = out / f'{frame}.ll'
        source.write_text(run([generator, 'arm64-apple-macosx', str(frame)]))
        obj = out / f'{frame}.o'
        run([bindir / 'llc', '-O3', '-filetype=obj', source, '-o', obj])
        assembly = run([bindir / 'llvm-objdump', '-d', '--no-show-raw-insn', obj])
        (out / f'{frame}.s').write_text(assembly)
        assert '__chkstk' not in assembly and '__probestack' not in assembly
        small = assembly[assembly.index('<_small_frame>:'):]
        assert re.findall(r'^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)', small, re.M) == ['ret'], small
        if frame == 20000:
            # LLVM 20/22 write zero; the tested LLVM 23 development backend
            # reads into XZR. Both must fault on the real PROT_NONE guard in
            # the execution sweep below; an opcode count alone is not a pass.
            assert len(re.findall(r'\b(?:ldr|str)\s+xzr,\s*\[sp\]', assembly)) == 2, assembly
        binary = out / f'{frame}.test'
        run([args.cxx, *args.cxxflag, runner, obj, '-o', binary])
        output = run([binary, str(frame)], timeout=60)
        alignments = min(16384, (frame + 15) & ~15) // 16
        assert f'all {alignments} entry alignments caught' in output, frame
        records.append(dict(frame_bytes=frame, recursion=True, entry_alignments=alignments, passed=True))
    result = dict(llvm=version, page_size=16384, policies=len(policies), frames=records,
                  entry_alignments=sum(record['entry_alignments'] for record in records),
                  failed=0)
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(llvm=version, frames=len(records), entry_alignments=result['entry_alignments'], failed=0, out=str(out))))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
