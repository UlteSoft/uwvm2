#!/usr/bin/env python3
"""Check ROS's CRT mapping and real CMake/Clang Windows COFF compilation.

Four CRT choices must match the preprocessor ABI macros and COFF defaultlib.
The actual ROS deferred hook checks target properties against a tiny substitute
library graph. No Windows SDK, CRT link, LLVM build or Windows execution occurs.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', '--clang-cl', dest='cxx', required=True)
    parser.add_argument('--frontend', choices=['cl', 'gnu'], default='cl')
    parser.add_argument('--llvm-lib', required=True)
    parser.add_argument('--llvm-ar')
    parser.add_argument('--llvm-ranlib')
    parser.add_argument('--lld-link', required=True)
    parser.add_argument('--llvm-readobj', required=True)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--xmake', default='xmake')
    args = parser.parse_args()
    if args.frontend == 'gnu' and not (args.llvm_ar and args.llvm_ranlib):
        parser.error('GNU-style Windows Clang needs --llvm-ar and --llvm-ranlib')
    repo = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = []

    def run(command, label):
        result = subprocess.run(command, cwd=repo, capture_output=True, timeout=90)
        (out/(label+'.log')).write_bytes(result.stdout+result.stderr)
        return result

    common = [args.cmake, '-S', str(repo/'test/0018.build/fixtures/msvc_runtime'), '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_SYSTEM_NAME=Windows',
        '-DCMAKE_CXX_COMPILER='+args.cxx, '-DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc',
        '-DCMAKE_AR='+(args.llvm_ar if args.frontend == 'gnu' else args.llvm_lib), '-DCMAKE_LINKER='+args.lld_link,
        '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY', '-DLLVM_HOST_TRIPLE=x86_64-pc-windows-msvc',
        '-DCMAKE_PROJECT_LLVM_INCLUDE='+str(repo/'xmake/llvm/contract.cmake')]
    if args.frontend == 'gnu':
        common.append('-DCMAKE_RANLIB='+args.llvm_ranlib)
    for crt, library in [('MT', 'libcmt'), ('MD', 'msvcrt'), ('MTd', 'libcmtd'), ('MDd', 'msvcrtd'),
                         ('legacy-MDd', 'msvcrt'), ('override-MDd', None)]:
        legacy = crt == 'legacy-MDd'
        mismatch = crt == 'override-MDd'
        selected = 'MDd' if legacy or mismatch else crt
        mapped = run([args.xmake, 'lua', 'test/0018.build/print_bundled_llvm_crt.lua', selected], crt+'-mapping')
        assert mapped.returncode == 0
        runtime = mapped.stdout.decode().strip()
        extra = ['-DLLVM_USE_CRT_RELEASE=MDd'] if legacy else [
            '-DCMAKE_MSVC_RUNTIME_LIBRARY='+runtime, '-DUWVM_ROS_MSVC_RUNTIME='+runtime,
            '-DUWVM_TEST_EXPECT_DEBUG='+str(int(selected.endswith('d'))),
            '-DUWVM_TEST_EXPECT_DLL='+str(int(selected.startswith('MD')))]
        if mismatch:
            extra.append('-DUWVM_TEST_OVERRIDE_RUNTIME=MultiThreadedDLL')
        build = out/crt
        command = [*common, '-B', str(build), *extra]
        configured = run(command, crt+'-configure')
        row = dict(name=crt, command=command, configure_returncode=configured.returncode)
        if mismatch:
            row['passed'] = configured.returncode != 0 and b'ROS LLVM MSVC runtime mismatch' in configured.stderr
        elif configured.returncode != 0:
            row['passed'] = False
        else:
            built = run([args.cmake, '--build', str(build), '--target', 'LLVMFixture', '--parallel', '1'], crt+'-build')
            row['build_returncode'] = built.returncode
            row['passed'] = False
            if built.returncode == 0:
                inspected = run([args.llvm_readobj, '--coff-directives', str(build/'LLVMFixture.lib')], crt+'-coff')
                text = inspected.stdout.decode().lower()
                row['directives'] = text
                row['passed'] = inspected.returncode == 0 and re.search(
                    r'defaultlib:[\"]?'+library+r'(?:\.lib)?(?:[\"\s]|$)', text) is not None
        rows.append(row)
        (out/'results.json').write_text(json.dumps(dict(note=__doc__, rows=rows), indent=2)+'\n')
        print(crt, 'PASS' if row['passed'] else 'FAIL', flush=True)
    raise SystemExit(not all(row['passed'] for row in rows))


if __name__ == '__main__':
    main()
