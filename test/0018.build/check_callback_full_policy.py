#!/usr/bin/env python3
"""Check the real policy callback's state/output, without building the CLI.

POSIX ANSI bytes and a real fast_io lockable native sink are exercised. The
fixture stubs the CLI registry and usage-printer result; this is not a full CLI
or legacy Windows console integration test. Run inside the shared resource cap.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


def expected(color, scenario, ordinary):
    def c(code):
        return '\033['+code+'m' if color else ''
    prefix = c('0')+c('97')+'uwvm: '+c('31')+'[error] '+c('97')
    if scenario < 6:
        return b''
    if scenario == 7:
        return (prefix+'Usage: USAGE\n\n').encode()
    if scenario == 8:
        policy = 'runtime LLVM JIT' if ordinary else 'LLVM AOT'
        return (prefix+'Conflicting '+policy+' policy parameters: "'+c('36')+
            '--runtime-llvm-jit-full-policy'+c('97')+'" conflicts with the high-level policy preset '
            '(--runtime-llvm-jit-policy).\nuwvm: '+c('92')+'[info]  '+c('97')+
            'Use the high-level policy preset or scoped policies, not both.\n\n'+c('0')).encode()
    policy = 'runtime LLVM JIT full' if ordinary else 'full-module LLVM AOT'
    result = prefix+'Invalid '+policy+' policy: "'+c('36')+'broken-policy'+c('97')+'". Expected '
    for i, name in enumerate(['auto', 'debug', 'legacy-light', 'pb-o1', 'pb-o2', 'pb-o3']):
        result += c('36')+name+c('97')+('. Usage: USAGE\n\n' if i == 5 else ', or ' if i == 4 else ', ')
    return result.encode()


def run(command, timeout):
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          start_new_session=True) as process:
        try:
            stdout, stderr = process.communicate(timeout=timeout)
            return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
        except subprocess.TimeoutExpired:
            # An accidentally reintroduced giant print pack can leave cc1
            # behind when only the compiler driver is killed. Reap our group.
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = process.communicate()
            return subprocess.CompletedProcess(command, 'timeout', stdout, stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--gcc')
    parser.add_argument('--clang')
    parser.add_argument('--clang-stdlib', default='libstdc++', choices=['libstdc++', 'libc++'])
    parser.add_argument('--header', action='append', help='label=header; defaults to this product')
    args = parser.parse_args()
    if os.name != 'posix' or os.uname().sysname not in ('Linux', 'Darwin'):
        parser.error('this driver checks Linux/Darwin ANSI output and POSIX process groups')
    compilers = []
    if args.gcc:
        compilers.append(('gcc', args.gcc, []))
    if args.clang:
        compilers.append(('clang', args.clang, ['-stdlib='+args.clang_stdlib]))
    if not compilers:
        parser.error('provide at least one actual --gcc or --clang compiler')
    repo = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    headers = args.header or ['current='+str(repo/'src/uwvm2/uwvm/cmdline/callback/runtime_llvm_jit_full_policy.h')]
    records = []
    for specification in headers:
        label, header = specification.split('=', 1)
        assert re.fullmatch(r'[A-Za-z0-9_-]+', label) and Path(header).is_file()
        # Product wording is intentionally different; identify the ordinary
        # product by its tiered build gate, not by copying the observed message
        # into the oracle or normalizing away a real formatting difference.
        header_text = Path(header).read_text()
        ordinary = 'UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED' in header_text
        # Keep umbrella includes stubbed, but compile BOTH real declaration
        # forms: module linkage and the header's inline constexpr qualifier.
        # Changing only this gate validates the local RAII guard under constexpr
        # function rules; it is not a substitute for a complete include build.
        gate = '# if defined(UWVM_MODULE)'
        assert header_text.count(gate) == 1
        header_variant = out/(label+'-header-declaration.h')
        header_variant.write_text(header_text.replace(gate, '# if 0 // test header declaration form'))
        for declaration, tested_header in [('module', Path(header)), ('header', header_variant)]:
            for compiler_name, compiler, flags in compilers:
                for opt in ['O0', 'O3']:
                    name = label+'-'+declaration+'-'+compiler_name+'-'+opt
                    binary = out/name
                    command = [compiler, '-std=c++26', '-'+opt, '-pthread', '-Wall', '-Wextra', '-Werror',
                        *flags, '-I'+str(repo/'src'), '-I'+str(repo/'third-parties/fast_io/include'),
                        '-DUWVM_TEST_POLICY_HEADER="'+str(tested_header.resolve())+'"',
                        str(repo/'test/0018.build/callback_full_policy_fixture.cpp'), '-o', str(binary)]
                    compiled = run(command, 180)
                    (out/(name+'.build.log')).write_bytes(compiled.stdout+compiled.stderr)
                    record = dict(name=name, header_sha256=hashlib.sha256(header_text.encode()).hexdigest(), command=command, compile_returncode=compiled.returncode, cases=[])
                    if compiled.returncode == 0:
                        for color in [0, 1]:
                            for scenario in range(9):
                                executed = run([str(binary), str(color), str(scenario)], 10)
                                passed = executed.returncode == 0 and executed.stdout == b'' and executed.stderr == expected(color, scenario, ordinary)
                                record['cases'].append(dict(color=color, scenario=scenario, passed=passed, returncode=executed.returncode))
                                if not passed:
                                    (out/f'{name}-{color}-{scenario}.stderr').write_bytes(executed.stderr)
                            # Four threads x 64 diagnostics must remain whole: all
                            # chunks share one lock, including ANSI/no-color modes.
                            executed = run([str(binary), str(color), '6', 'threads'], 15)
                            passed = executed.returncode == 0 and not executed.stdout and executed.stderr == expected(color, 6, ordinary)*256
                            record['cases'].append(dict(color=color, scenario='concurrent', passed=passed, returncode=executed.returncode))
                            if not passed:
                                (out/f'{name}-{color}-concurrent.stderr').write_bytes(executed.stderr)
                    record['passed'] = compiled.returncode == 0 and len(record['cases']) == 20 and all(case['passed'] for case in record['cases'])
                    records.append(record)
                    (out/'results.json').write_text(json.dumps(records, indent=2)+'\n')
                    print(name, 'PASS' if record['passed'] else 'FAIL', flush=True)
    raise SystemExit(not all(record['passed'] for record in records))


if __name__ == '__main__':
    main()
