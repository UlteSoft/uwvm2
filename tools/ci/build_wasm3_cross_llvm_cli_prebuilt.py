#!/usr/bin/env python3
"""Build the ROS full-JIT RISC-V CLI with a source-matched prebuilt LLVM.

Run only inside the bounded Linux test cgroup.  The LLVM archive package is
verified against its own build manifest and the frozen ROS vendor source before
it is allowed to supply any header or library to the compiler.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import subprocess
import sys


ROS9_SOURCE_MANIFEST_SHA256 = 'cc680aa09a6fb23fe1a9ca2746a51a029d635386e17fb9ac7d264b6e9971a8e1'


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(command: list[str], *, log: Path | None = None, cwd: Path | None = None) -> None:
    if log is None:
        subprocess.run(command, check=True, cwd=cwd)
    else:
        with log.open('wb') as stream:
            subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True, cwd=cwd)


def source_id(root: Path, out: Path, filename: str) -> str:
    return subprocess.check_output(
        [sys.executable, str(root / 'tools/ci/wasm3_source_fingerprint.py'), str(root), str(out / filename)],
        text=True,
    ).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--llvm-build', type=Path, required=True)
    parser.add_argument('--llvm-package', type=Path, required=True)
    parser.add_argument('--deps', type=Path, default=Path('/work/deps'))
    parser.add_argument('--clang', type=Path, default=Path('/toolchain/bin/clang++'))
    parser.add_argument('--readobj', type=Path, default=Path('/toolchain/bin/llvm-readobj'))
    parser.add_argument('--capture-jit-object', action='store_true',
                        help='Build a test-only CLI that exports emitted MCJIT objects without enabling cache reuse')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    build = args.llvm_build.resolve(strict=True)
    package_file = args.llvm_package.resolve(strict=True)
    deps = args.deps.resolve(strict=True)
    # Preserve the clang++ driver spelling: resolving its symlink to clang-23
    # would select the C driver and omit the target C++ runtime at link time.
    clang = args.clang.absolute()
    if not clang.is_file():
        parser.error(f'missing C++ compiler: {clang}')
    resolved_clang = clang.resolve(strict=True)
    readobj = args.readobj.resolve(strict=True)
    out = args.out.resolve()
    if out == root or root in out.parents:
        parser.error('output directory must be outside the frozen source tree')
    if out.exists():
        parser.error(f'output directory already exists: {out}')
    run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], cwd=root)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out.mkdir(parents=True)

    manifest = root / 'third-parties/llvm/sources.sha256'
    cache_file = build / 'CMakeCache.txt'
    cache = {}
    for line in cache_file.read_text().splitlines():
        if not line or line.startswith(('#', '//')) or '=' not in line:
            continue
        key_and_type, value = line.split('=', 1)
        cache[key_and_type.split(':', 1)[0]] = value
    triple = 'riscv64-linux-gnu'
    sysroot = deps / 'usr' / triple
    gcc = deps / 'usr/lib/gcc-cross' / triple / '15'
    expected_cache = {
        'CMAKE_BUILD_TYPE': 'Release',
        'CMAKE_CXX_COMPILER_TARGET': triple, 'CMAKE_SYSROOT': str(deps),
        'LLVM_HOST_TRIPLE': triple, 'LLVM_TARGETS_TO_BUILD': 'RISCV',
        'LLVM_VERSION_SUFFIX': '-uwvm-ros.9',
    }
    if ({key: cache.get(key) for key in expected_cache} != expected_cache or
            Path(cache.get('CMAKE_CXX_COMPILER', '/missing-compiler')).resolve() != resolved_clang):
        raise RuntimeError('prebuilt LLVM CMake configuration differs from cross compile target')
    for expected in (f'--gcc-install-dir={gcc}', f'-idirafter {sysroot / "include"}', '-stdlib=libstdc++'):
        if expected not in cache.get('CMAKE_CXX_FLAGS', ''):
            raise RuntimeError(f'prebuilt LLVM C++ ABI flag missing: {expected}')
    if str(sysroot / 'lib') not in cache.get('CMAKE_EXE_LINKER_FLAGS', ''):
        raise RuntimeError('prebuilt LLVM sysroot linker directory differs')
    cmake_source = Path(cache['CMAKE_HOME_DIRECTORY']).resolve(strict=True)
    actual_manifest = cmake_source.parent / 'sources.sha256'
    if cmake_source.name != 'llvm' or not actual_manifest.is_file():
        raise RuntimeError('prebuilt LLVM CMake source manifest missing')
    package = json.loads(package_file.read_text())
    if package_file != (build.parent / 'package-manifest.json').resolve(strict=True):
        raise RuntimeError('LLVM package manifest is not attached to the selected build')
    if package['version'] != '23.1.1-uwvm-ros.9' or package['host_target'] != 'riscv64-linux-gnu':
        raise RuntimeError('wrong prebuilt LLVM target or version')
    if (sha256(manifest) != ROS9_SOURCE_MANIFEST_SHA256 or
            sha256(actual_manifest) != ROS9_SOURCE_MANIFEST_SHA256 or
            package['source_manifest_sha256'] != ROS9_SOURCE_MANIFEST_SHA256):
        raise RuntimeError('prebuilt LLVM and frozen ROS vendor source differ')
    run(['sha256sum', '-c', '--quiet', str(manifest)], cwd=manifest.parent, log=out / 'llvm-source-check.log')
    run(['sha256sum', '-c', '--quiet', str(actual_manifest)], cwd=actual_manifest.parent,
        log=out / 'llvm-cmake-source-check.log')
    ninja = Path(cache['CMAKE_MAKE_PROGRAM']).resolve(strict=True)
    dry_run = subprocess.check_output([str(ninja), '-C', str(build), '-n', 'LLVMRISCVCodeGen',
                                        'LLVMRuntimeDyld'], text=True, stderr=subprocess.STDOUT)
    (out / 'llvm-dry-run.log').write_text(dry_run)
    if 'no work to do' not in dry_run.lower():
        raise RuntimeError('prebuilt LLVM is stale against its recorded CMake source')
    header = build / 'include/llvm/Config/llvm-config.h'
    response = build / 'consumer-link.rsp'
    if sha256(header) != package['version_header_sha256'] or sha256(response) != package['consumer_link_sha256']:
        raise RuntimeError('prebuilt LLVM generated header or link contract differs from package')
    if '23.1.1-uwvm-ros.9' not in header.read_text():
        raise RuntimeError('prebuilt LLVM version header does not identify ros.9')
    archives = {}
    seen_names = set()
    for item in package['archives']:
        name = item['name']
        if name != Path(name).name or not name.endswith('.a') or name in seen_names:
            raise RuntimeError('duplicate or unsafe prebuilt LLVM archive name')
        seen_names.add(name)
        path = build / 'lib' / name
        if path.stat().st_size != item['size'] or sha256(path) != item['sha256']:
            raise RuntimeError(f'prebuilt LLVM archive differs from package: {name}')
        archives[path.resolve(strict=True)] = item['sha256']
    used_words = [word for word in shlex.split(response.read_text()) if word.endswith('.a')]
    if any(not Path(word).is_absolute() for word in used_words):
        raise RuntimeError('prebuilt LLVM response has a relative archive')
    used = [Path(word).resolve(strict=True) for word in used_words]
    if len(used) != len(set(used)) or set(used) != set(archives):
        raise RuntimeError('prebuilt LLVM response references an unverified or unused archive')

    if not (sysroot / 'include').is_dir() or not gcc.is_dir():
        raise RuntimeError('missing RISC-V sysroot or cross GCC runtime')
    generated = [path for path in (build / 'include').rglob('*') if path.is_file()]
    inputs = {str(path): sha256(path) for path in
              (package_file, manifest, actual_manifest, cache_file, header, response, clang, readobj, ninja,
               *generated)}
    inputs[str(Path(__file__).resolve(strict=True))] = sha256(Path(__file__).resolve(strict=True))
    inputs.update({str(path): digest for path, digest in archives.items()})
    (out / 'toolchain-before.json').write_text(json.dumps(inputs, indent=2) + '\n')
    identity = source_id(root, out, 'source-before.json')
    flags = [str(clang), f'--target={triple}', f'--sysroot={deps}', f'--gcc-install-dir={gcc}',
             '-idirafter', str(sysroot / 'include'), '-std=c++26', '-stdlib=libstdc++',
             '-fno-rtti', '-fasynchronous-unwind-tables', '-O1', '-g0', '-Wno-undefined-inline',
             '-DUWVM=2', '-DUWVM_USE_LLVM_JIT', '-DUWVM_USE_UWVM_INT',
             '-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1',
             '-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519',
             '-DUWVM_VERSION_X=2', '-DUWVM_VERSION_Y=0', '-DUWVM_VERSION_Z=4', '-DUWVM_VERSION_S=0',
             '-DUWVM2_BUILD_SOURCE_ID=u8"' + identity + '"']
    if args.capture_jit_object:
        flags.append('-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT')
    for include in (deps / 'usr/include' / triple, deps / 'usr/include', build / 'include',
                    root / 'third-parties/llvm/llvm/include', root / 'src',
                    root / 'third-parties/bizwen/include', root / 'third-parties/fast_io/include',
                    root / 'third-parties/boost_unordered/include'):
        flags.extend(('-I', str(include)))
    runtime = out / 'runtime.o'
    binary = out / 'uwvm'
    commands = {
        'runtime': [*flags, '-c', 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', '-o', str(runtime)],
        'cli': [*flags, '-fuse-ld=lld', 'src/uwvm2/uwvm/main.default.cpp',
                'src/uwvm2/uwvm/host_api.default.cpp', str(runtime), '@' + str(response),
                '-O1', '-L' + str(deps / 'usr/lib' / triple), '-L' + str(sysroot / 'lib'),
                '-lssl', '-lcrypto', '-latomic', '-ldl', '-pthread', '-o', str(binary)],
    }
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    for label, command in commands.items():
        run(command, cwd=root, log=out / (label + '.build.log'))
    elf = subprocess.check_output([str(readobj), '--file-headers', str(binary)], text=True)
    (out / 'target-elf.txt').write_text(elf)
    if 'EM_RISCV' not in elf:
        raise RuntimeError('cross build produced a non-RISC-V executable')
    if identity != source_id(root, out, 'source-after.json'):
        raise RuntimeError('frozen source changed during cross build')
    if {path: sha256(Path(path)) for path in inputs} != inputs:
        raise RuntimeError('LLVM package, toolchain, or source manifest changed during cross build')
    run(['sha256sum', '-c', '--quiet', str(actual_manifest)], cwd=actual_manifest.parent,
        log=out / 'llvm-cmake-source-after.log')
    run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], cwd=root)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'target': triple,
        'source_id': identity, 'llvm_version': package['version'],
        'llvm_source_manifest_sha256': package['source_manifest_sha256'],
        'builder_script_sha256': sha256(Path(__file__).resolve(strict=True)),
        'test_object_capture': args.capture_jit_object,
        'binary_sha256': sha256(binary), 'runtime_object_sha256': sha256(runtime)}, indent=2) + '\n')
    print(f'PASS source-matched RISC-V full-JIT CLI: {binary}')


if __name__ == '__main__':
    main()
