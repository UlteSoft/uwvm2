#!/usr/bin/env python3
"""Build/run the small runtime guard regressions, independently of LLVM.

Run under the caller's aggregate resource/CPU limit. This driver is serial and
uses a fresh output directory; it never removes an existing build. These native
helper tests complement, but do not replace, whole-VM exhaustion/cache tests or
cross-ISA tests. A compile-time-only policy test is labelled as such in the log.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess


TESTS = [
    ('checked_frame_size.cc', [], 'runtime and compile-time boundaries'),
    ('native_stack_guard.cc', [], 'stack faults, cache lifecycle and host restoration'),
    # The wrapped sigaction fixture deliberately interrupts handler publication;
    # compiling it as an ordinary unit would silently omit the tested scenario.
    ('native_stack_signal_install.cpp', ['-Wl,--wrap=sigaction'], 'signal publication and chaining'),
    ('native_unwind_execution_gate.cc', [], 'recursive and concurrent execution gates'),
    ('call_indirect_table_view_reset.cc', [], 'borrowed table view reset'),
    ('runtime_generation_reset.cc', [], 'live-thread stale generation rejection'),
    ('runtime_state_signature.cc', [], 'runtime configuration reuse/rejection'),
    ('cache_source_provenance_policy.cc', [], 'compile-time policy; empty main'),
]


def invoke(command, log, timeout):
    with log.open('wb') as output:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # GCC forks cc1plus, and stack/signal fixtures fork child scenarios.
            # Killing only the driver would leave them consuming the shared
            # memory budget after this matrix has already reported a timeout.
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            output.write(b'\nTEST TIMEOUT\n')
            return 'timeout'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--gcc', required=True)
    parser.add_argument('--clang', required=True)
    parser.add_argument('--clang-stdlib', choices=['libc++', 'libstdc++'], default='libstdc++')
    args = parser.parse_args()
    if os.uname().sysname != 'Linux':
        parser.error('this matrix includes Linux sigaction --wrap fixtures; use platform-specific tests elsewhere')
    root = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    profiles = []
    for name, compiler, flags in [('gcc', args.gcc, []),
                                  ('clang', args.clang, ['-stdlib='+args.clang_stdlib])]:
        for opt in ['O0', 'O3']:
            profiles.append((name+'-'+opt, compiler, [*flags, '-'+opt]))
    # ASan owns its own signal/alternate-stack machinery and is not a substitute
    # for testing the production stack-fault handler. UBSan checks ordinary C++
    # operations without replacing the tested stack-guard mechanism.
    profiles.append(('clang-O3-ubsan', args.clang,
        ['-stdlib='+args.clang_stdlib, '-O3', '-fsanitize=undefined', '-fno-sanitize-recover=all']))
    identity = {compiler: subprocess.check_output([compiler, '--version'], text=True)
                for compiler in (args.gcc, args.clang)}
    sources = [root/'test/0017.runtime'/name for name, _, _ in TESTS]
    sources.extend(sorted((root/'src/uwvm2/runtime/lib').glob('*.h')))
    hashes = {str(source.relative_to(root)): hashlib.sha256(source.read_bytes()).hexdigest()
              for source in sources}
    records = []
    for profile, compiler, flags in profiles:
        for source, extra, coverage in TESTS:
            name = profile+'-'+Path(source).stem
            binary = out/name
            command = [compiler, '-std=c++23', '-Wall', '-Wextra', '-Werror', '-pthread',
                '-fstack-clash-protection', '-I'+str(root/'src'), *flags,
                str(root/'test/0017.runtime'/source), *extra, '-o', str(binary)]
            compiled = invoke(command, out/(name+'.build.log'), 180)
            executed = invoke([str(binary)], out/(name+'.run.log'), 90) if compiled == 0 else 'not-run'
            passed = compiled == 0 and executed == 0
            records.append(dict(name=name, coverage=coverage, build_command=command,
                compile_returncode=compiled, run_returncode=executed, passed=passed))
            print(name, 'PASS' if passed else 'FAIL', flush=True)
            # Publish each result so an interrupted matrix cannot look complete.
            (out/'results.json').write_text(json.dumps(dict(compilers=identity,
                sources=hashes, results=records, expected=len(profiles)*len(TESTS)), indent=2)+'\n')
    raise SystemExit(not all(record['passed'] for record in records))


if __name__ == '__main__':
    main()
