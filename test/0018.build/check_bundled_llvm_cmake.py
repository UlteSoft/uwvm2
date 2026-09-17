#!/usr/bin/env python3
"""Configure real LLVM: Release passes; wrong mode/missing native backend fail.

This checks CMake's actual deferred contract, not just synthetic JSON parsing.
It builds no libraries or executables and does not establish target execution.
Use a fresh output directory; existing build/source trees are never removed.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess


def configure(command):
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          start_new_session=os.name == 'posix') as process:
        try:
            log, _ = process.communicate(timeout=180)
            return process.returncode, log
        except subprocess.TimeoutExpired:
            # CMake probes spawn compiler/linker children. Reap the owned group
            # on POSIX instead of leaving probes behind in the resource budget.
            try:
                if os.name == 'posix':
                    os.killpg(process.pid, signal.SIGKILL)
                else:
                    process.kill()
            except ProcessLookupError:
                pass
            log, _ = process.communicate()
            return 'timeout', log+b'\nCONFIGURE TIMEOUT\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cc', required=True)
    parser.add_argument('--cxx', required=True)
    parser.add_argument('--cxxflags', default='')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--xmake', default='xmake')
    parser.add_argument('--cross-toolchain', type=Path,
        help='optional real cross toolchain, deliberately without LLVM_HOST_TRIPLE')
    parser.add_argument('--cross-triple')
    parser.add_argument('--cross-backend')
    args = parser.parse_args()
    if any((args.cross_toolchain, args.cross_triple, args.cross_backend)) and not all(
            (args.cross_toolchain, args.cross_triple, args.cross_backend)):
        parser.error('supply all three --cross-* options together')
    repo = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    common = [args.cmake, '-S', str(repo/'third-parties/llvm/llvm'), '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER='+args.cc,
        '-DCMAKE_CXX_COMPILER='+args.cxx, '-DCMAKE_CXX_FLAGS='+args.cxxflags,
        '-DCMAKE_PROJECT_LLVM_INCLUDE='+str(repo/'xmake/llvm/contract.cmake'),
        '-DLLVM_VERSION_SUFFIX=-uwvm-ros.6', '-DLLVM_APPEND_VC_REV=OFF',
        '-DLLVM_TARGETS_TO_BUILD=Native', '-DLLVM_ENABLE_PROJECTS=', '-DLLVM_ENABLE_RUNTIMES=',
        '-DLLVM_INCLUDE_TESTS=OFF', '-DLLVM_INCLUDE_BENCHMARKS=OFF', '-DLLVM_INCLUDE_EXAMPLES=OFF',
        '-DLLVM_INCLUDE_DOCS=OFF', '-DLLVM_BUILD_TOOLS=OFF', '-DLLVM_BUILD_UTILS=OFF',
        '-DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF', '-DLLVM_BUILD_LLVM_DYLIB=OFF',
        '-DLLVM_LINK_LLVM_DYLIB=OFF', '-DBUILD_SHARED_LIBS=OFF',
        '-DLLVM_ENABLE_ZLIB=OFF', '-DLLVM_ENABLE_ZSTD=OFF', '-DLLVM_ENABLE_LIBXML2=OFF',
        '-DLLVM_ENABLE_CURL=OFF', '-DLLVM_ENABLE_LIBEDIT=OFF', '-DLLVM_ENABLE_ASSERTIONS=OFF',
        '-DLLVM_ENABLE_EH=OFF', '-DLLVM_ENABLE_RTTI=OFF', '-DLLVM_ENABLE_LTO=OFF']
    forced = out/'force-debug.cmake'
    forced.write_text('set(CMAKE_BUILD_TYPE Debug CACHE STRING "regression override" FORCE)\n')
    directory_flags = out/'directory-fast-math.cmake'
    # A toolchain/project hook can add directory flags which are invisible in
    # CMAKE_CXX_FLAGS. Real configuration must expose them to the Lua guard.
    directory_flags.write_text('add_compile_options(-ffast-math)\n')
    regenerated_flags = out/'regenerated-options.cmake'
    regenerated_flags.write_text('# initially precise\n')
    cases = [('release', [], None, None),
        ('directory-fastmath', ['-DCMAKE_PROJECT_INCLUDE='+str(directory_flags)], None, None),
        ('regenerated-fastmath', ['-DCMAKE_PROJECT_INCLUDE='+str(regenerated_flags)], None, None),
        ('forced-debug', ['-DCMAKE_TOOLCHAIN_FILE='+str(forced)],
         'ROS bundled LLVM must use the Release build configuration', None),
        ('foreign-default-triple', ['-DLLVM_DEFAULT_TARGET_TRIPLE=wasm32-unknown-unknown'],
         'ROS native JIT requires LLVM_DEFAULT_TARGET_TRIPLE to match LLVM_HOST_TRIPLE', None),
        ('missing-native', ['-DLLVM_TARGETS_TO_BUILD=WebAssembly'],
         'ROS JIT requires its native LLVM backend', None),
        # xmake's explicit cross request must also be checked when a toolchain
        # omits CMAKE_SYSTEM_NAME and CMake reports CMAKE_CROSSCOMPILING=false.
        ('requested-cross-missing-triple', ['-DUWVM_ROS_REQUIRE_EXPLICIT_HOST_TRIPLE=ON',
             '-ULLVM_HOST_TRIPLE', '-ULLVM_DEFAULT_TARGET_TRIPLE'],
         'ROS cross LLVM requires an explicit LLVM_HOST_TRIPLE', None)]
    if args.cross_toolchain:
        cross_file = args.cross_toolchain.resolve()
        # Remove a pre-existing inferred triple just like the production recipe.
        # The positive wrapper supplies it through the toolchain, not a host
        # compiler query or a target llvm-config executable.
        reset = ['-ULLVM_HOST_TRIPLE', '-ULLVM_DEFAULT_TARGET_TRIPLE']
        pinned = out/'pinned-cross.cmake'
        pinned.write_text('include([['+str(cross_file)+']])\n'
            'set(LLVM_HOST_TRIPLE [['+args.cross_triple+']] CACHE STRING "explicit cross target" FORCE)\n')
        cases.extend([
            ('cross-missing-triple', [*reset, '-DCMAKE_TOOLCHAIN_FILE='+str(cross_file)],
             'ROS cross LLVM requires an explicit LLVM_HOST_TRIPLE', None),
            ('cross-pinned', [*reset, '-DCMAKE_TOOLCHAIN_FILE='+str(pinned)], None, args.cross_triple)])
    records = []
    for name, extra, error, expected_triple in cases:
        build = out/name
        query = build/'.cmake/api/v1/query/codemodel-v2'
        query.parent.mkdir(parents=True)
        query.touch()
        command = [*common, '-B', str(build), *extra]
        returncode, log = configure(command)
        (out/(name+'.log')).write_bytes(log)
        if name == 'regenerated-fastmath' and returncode == 0:
            # An included file changes without changing the recipe or main
            # toolchain file. Regenerate the build system, not LLVM libraries,
            # before consulting the file-API reply: the old reply is precise.
            regenerated_flags.write_text('add_compile_options(-ffast-math)\n')
            refresh = [args.cmake, '--build', str(build), '--target', 'build.ninja']
            returncode, refreshed = configure(refresh)
            (out/(name+'-refresh.log')).write_bytes(refreshed)
            assert not list((build/'lib').glob('libLLVM*.a'))
        if error:
            # CMake wraps long diagnostics at whitespace. Match the actual
            # reason, not a terminal-width-dependent line layout; an unrelated
            # configuration failure must still fail this negative control.
            normalized_log = b' '.join(log.split())
            passed = isinstance(returncode, int) and returncode != 0 and error.encode() in normalized_log
        else:
            passed = returncode == 0
            if passed:
                reply = build/'.cmake/api/v1/reply'
                index = json.loads(sorted(reply.glob('index-*.json'))[-1].read_text())
                model = json.loads((reply/index['reply']['codemodel-v2']['jsonFile']).read_text())
                targets = model['configurations'][0]['targets']
                contract = next(target for target in targets if target['name'] == 'uwvm_ros_llvm_contract')
                payload = json.loads((reply/contract['jsonFile']).read_text())
                passed = (len(model['configurations']) == 1 and model['configurations'][0]['name'] == 'Release'
                    and payload['link']['language'] == 'CXX' and bool(payload['link']['commandFragments'])
                    and not any(target['name'] == 'llvm-config' for target in targets)
                    and not (build/'bin/uwvm_ros_llvm_contract').exists()
                    and not (build/'bin/llvm-config').exists())
                if expected_triple:
                    metadata = json.loads((build/'uwvm-llvm-version.json').read_text())
                    passed = passed and metadata['host_target'] == expected_triple and any(
                        target['name'] == 'LLVM'+args.cross_backend+'CodeGen' for target in targets)
                if passed:
                    # Read the actual generated target, not a synthetic copy.
                    # No library build follows a forbidden resolved flag.
                    reader = [args.xmake, 'lua', 'test/0018.build/check_bundled_llvm_actual_contract.lua', str(build)]
                    if name in ('directory-fastmath', 'regenerated-fastmath'):
                        fragments = payload['compileGroups'][0]['compileCommandFragments']
                        assert any('-ffast-math' in fragment['fragment'] for fragment in fragments)
                        reader.append('Unsafe floating-point')
                    inspected = subprocess.run(reader, cwd=repo, capture_output=True, timeout=30)
                    (out/(name+'-reader.log')).write_bytes(inspected.stdout+inspected.stderr)
                    passed = inspected.returncode == 0
        records.append(dict(name=name, command=command, returncode=returncode, passed=passed))
        print(name, 'PASS' if passed else 'FAIL', flush=True)
    (out/'results.json').write_text(json.dumps(records, indent=2)+'\n')
    raise SystemExit(not all(record['passed'] for record in records))


if __name__ == '__main__':
    main()
