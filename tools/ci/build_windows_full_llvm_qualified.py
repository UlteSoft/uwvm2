#!/usr/bin/env python3
"""Build exact-source Windows x64 LLVM-full uwvm and host debug broker.

Run this on the Linux host *inside* the shared 64 GiB/swap-free Docker cgroup.
The Windows VM acceptance runner consumes the two PE files after this build;
successful cross-linking alone is never recorded as Windows product acceptance.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import shutil
import struct
import subprocess
import sys

from qualify_windows_ros_llvm_prebuilt import verify_cgroup
from windows_compiler_rt_builtins import qualify as qualify_builtins, verify_unchanged as verify_builtins_unchanged


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(command: list[str], cwd: Path, log: Path, environment: dict[str, str],
        *, resume: bool = False) -> None:
    with log.open('ab' if resume and log.exists() else 'xb') as stream:
        if resume:
            stream.write(b'\n=== resumed qualified build ===\n')
            stream.flush()
        result = subprocess.run(command, cwd=cwd, env=environment,
                                stdout=stream, stderr=subprocess.STDOUT, check=False)
    require(result.returncode == 0,
            f'command failed with {result.returncode}; inspect {log}: {command!r}')


def fingerprint(source: Path, output: Path, name: str) -> str:
    result = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / name)], text=True).strip()
    require(result.startswith('sha256:') and len(result) == 71, 'invalid source fingerprint')
    return result


def recipe_hashes(source: Path) -> dict[str, str]:
    paths = [source / 'xmake.lua', *sorted((source / 'xmake').rglob('*.lua')),
             source / 'tools/ci/windows_llvm_x64_toolchain.cmake',
             source / 'tools/ci/qualify_windows_ros_llvm_prebuilt.py',
             source / 'tools/ci/build_windows_full_llvm_qualified.py',
             source / 'tools/ci/windows_compiler_rt_builtins.py',
             source / 'tools/ci/patches/xmake-3.0.5-clang22-modulefile.patch',
             source / 'tools/debug/secure_server_windows.cpp',
             source / 'test/0017.runtime/windows_debug_product_launcher.cc',
             source / 'test/0017.runtime/windows_control_broker_child.c']
    result: dict[str, str] = {}
    for path in paths:
        require(path.is_file(), f'missing build input: {path}')
        result[path.relative_to(source).as_posix()] = digest(path)
    return result


def tool_hashes(xmake: Path, clang: Path, clangxx: Path, lld: Path,
                clang_bin: Path) -> dict[str, dict[str, str]]:
    """Record the actual build tools, including optional Xmake module fixes."""
    paths = {'xmake': xmake, 'clang': clang, 'clangxx': clangxx, 'lld': lld}
    scanner = clang_bin / 'clang-scan-deps'
    if scanner.is_file():
        paths['clang-scan-deps'] = scanner.resolve(strict=True)
    program_dir = os.environ.get('XMAKE_PROGRAM_DIR')
    if program_dir:
        support = (Path(program_dir).resolve(strict=True) /
                   'rules/c++/modules/clang/support.lua')
        require(support.is_file(), 'XMAKE_PROGRAM_DIR lacks Clang module support recipe')
        paths['xmake-clang-module-support'] = support
    return {name: {'path': str(path.absolute()),
                   'realpath': str(path.resolve(strict=True)),
                   'sha256': digest(path.resolve(strict=True))}
            for name, path in paths.items()}


def pe_x64(path: Path) -> bool:
    with path.open('rb') as stream:
        require(stream.read(2) == b'MZ', f'not a PE executable: {path}')
        stream.seek(0x3c)
        pe_offset = struct.unpack('<I', stream.read(4))[0]
        require(0x40 <= pe_offset < 1 << 20, f'invalid PE header offset: {path}')
        stream.seek(pe_offset)
        require(stream.read(4) == b'PE\0\0', f'missing PE signature: {path}')
        machine = struct.unpack('<H', stream.read(2))[0]
        require(machine == 0x8664, f'not Windows x64 PE: {path}')
        stream.seek(pe_offset + 24)
        optional_magic = struct.unpack('<H', stream.read(2))[0]
        require(optional_magic == 0x20b, f'not PE32+: {path}')
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--prebuilt-llvm', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--openssl-root', type=Path, required=True,
                        help='An existing Windows OpenSSL root, or explicit default for the xmake package')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    parser.add_argument('--xmake', type=Path, required=True)
    parser.add_argument('--clang-bin', type=Path, required=True,
                        help='bootstrap Clang directory; named modules require the initializer fix')
    parser.add_argument('--lld', type=Path, required=True,
                        help='COFF-capable ld.lld for the test-only host launcher')
    parser.add_argument('--windows-builtins', type=Path,
                        help='Explicit target compiler-rt archive; preserves the existing C++ EH runtime')
    for name in ('builtins-ar', 'builtins-readobj', 'builtins-nm'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--configure-only', action='store_true',
                        help='Preflight only; does not qualify any PE or Windows VM run')
    parser.add_argument('--resume', action='store_true',
                        help='Continue a previously configured, source-matched build')
    parser.add_argument('--allow-persistent-output', action='store_true',
                        help='Use a disk build tree when tmpfs BMI plus compiler RSS risks the 64 GiB memory cap')
    parser.add_argument('--verbose-commands', action='store_true',
                        help='Record each actual compiler/linker invocation for optimization and archive audit')
    args = parser.parse_args()
    verify_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True)
    sidecars = [path for path in source.rglob('._*') if path.is_file()]
    require(not sidecars,
            f'private Windows build source contains {len(sidecars)} AppleDouble metadata files; remove them before Xmake scans modules')
    llvm = args.prebuilt_llvm.resolve(strict=True)
    sdk = args.sdk.resolve(strict=True)
    openssl = ('default' if args.openssl_root == Path('default')
               else str(args.openssl_root.resolve(strict=True)))
    xmake = args.xmake.resolve(strict=True)
    clang_bin = args.clang_bin.resolve(strict=True)
    # Preserve the C/C++ driver names even when both are symlinks to one
    # Clang binary. Its argv[0] selects C++ runtime linking for host fixtures.
    clang = (clang_bin / 'clang').absolute()
    clangxx = (clang_bin / 'clang++').absolute()
    require(clang.is_file() and clangxx.is_file(), 'missing qualified Clang C/C++ drivers')
    # LLVM's multicall lld selects the COFF-capable GNU driver from argv[0].
    # Resolving an ld.lld symlink to its generic `lld` target breaks --ld-path.
    lld = args.lld.absolute()
    require(lld.is_file(), f'missing COFF-capable ld.lld: {lld}')
    build_tools = tool_hashes(xmake, clang, clangxx, lld, clang_bin)
    output = args.output.resolve()
    if not (output.is_relative_to(Path('/tmp')) or
            output.is_relative_to(Path('/dev/shm'))):
        require(args.allow_persistent_output,
                'persistent output requires explicit --allow-persistent-output')
        # A fresh cold build needs room for all BMI/object output. An exact
        # source-matched resume reuses that output and retains the separately
        # authorized 16 GiB floor for link/install artifacts.
        free_floor = (16 if args.resume else 40) * (1 << 30)
        require(shutil.disk_usage(output.parent).free >= free_floor,
                f'persistent output requires at least {free_floor >> 30} GiB available')
    if args.resume:
        require(output.is_dir() and (output / 'xmake-build').is_dir(),
                'resume requires an existing configured output and Xmake build tree')
    else:
        output.mkdir(mode=0o700, parents=True, exist_ok=False)
    require((source / 'third-parties/llvm/sources.sha256').is_file(),
            'this builder requires the ROS source-owned LLVM checkout')
    certificate = llvm / 'qualified-llvm.json'
    require(certificate.is_file(), 'missing complete COFF archive qualification certificate')
    certificate_sha = digest(certificate)
    toolchain = source / 'tools/ci/windows_llvm_x64_toolchain.cmake'
    require(toolchain.is_file(), 'missing Windows x64 C/C++/ASM toolchain file')
    toolchain_sha = digest(toolchain)
    source_id = fingerprint(source, output,
                            'source-resume.json' if args.resume else 'source-before.json')
    if args.resume:
        configured = json.loads((output / 'source-before.json').read_text())
        require(configured['source_id'] == source_id,
                'resume source does not match the configured source fingerprint')
    recipes = recipe_hashes(source)
    environment = dict(os.environ)
    environment['UWVM_ROS_QUALIFIED_PREBUILT_LLVM'] = str(llvm)
    environment['TMPDIR'] = str(output)
    # docker exec --user may preserve HOME=/root even though the process has
    # unprivileged uid 1000. Resolve the account that actually owns xmake's
    # per-user cache; never write privileged/root-owned configuration there.
    environment['HOME'] = pwd.getpwuid(os.getuid()).pw_dir
    # The xmake clang toolchain resolves its compiler from PATH even when
    # configuration has explicit cc/cxx paths. Keep its actual module probe,
    # compile and link driver on the same, independently qualified Clang.
    # Xmake uses the C++ driver with -fuse-ld=lld for the actual product.
    # The independently qualified GNU ld.lld can live outside clang_bin;
    # preserve that invocation spelling and make its directory searchable.
    environment['PATH'] = os.pathsep.join((str(clang_bin), str(lld.parent),
                                         environment.get('PATH', '')))
    # Never accept a caller's unqualified inherited link input. This private
    # opt-in adds one real static archive, not a -rtlib or unwind substitution.
    environment.pop('UWVM_WINDOWS_QUALIFIED_BUILTINS', None)
    builtins = None
    builtins_tools = (args.builtins_ar, args.builtins_readobj, args.builtins_nm)
    require((args.windows_builtins is None and all(tool is None for tool in builtins_tools)) or
            (args.windows_builtins is not None and all(tool is not None for tool in builtins_tools)),
            '--windows-builtins requires all three explicit inspection tools')
    if args.windows_builtins is not None:
        attempt = 1
        while (output / f'builtins-qualification-{attempt}').exists():
            attempt += 1
        builtins = qualify_builtins(args.windows_builtins, *builtins_tools,
                                   output / f'builtins-qualification-{attempt}', environment)
        environment['UWVM_WINDOWS_QUALIFIED_BUILTINS'] = builtins['archive']
    libcxx = sdk / 'include/c++/v1'
    mingw_headers = sdk / 'x86_64-w64-mingw32/include'
    require(libcxx.is_dir() and mingw_headers.is_dir(), 'missing Windows x64 SDK headers')
    configure = [str(xmake), 'f', '-p', 'mingw', '-a', 'x86_64', '-m', 'release',
                 '-o', str(output / 'xmake-build'),
                 '--toolchain=clang', '--cc=' + str(clang), '--cxx=' + str(clangxx),
                 '--ld=' + str(clangxx), '--use-llvm-compiler=y', '--enable-lto=n',
                 '--target=x86_64-w64-windows-gnu', '--sysroot=' + str(sdk),
                 '--sdk=' + str(sdk), '--stdlib=libc++',
                 '--cxflags=-nostdinc++ -I' + str(libcxx) + ' -I' + str(mingw_headers),
                 '--execution-int=none', '--execution-jit=llvm', '--use-cxx-module=y',
                 '--llvm-build-targets=X86', '--llvm-build-jobs=1',
                 '--llvm-cmake-toolchain=' + str(toolchain),
                 '--openssl-root=' + openssl, '--build-source-id=' + source_id]
    if args.resume:
        require(json.loads((output / 'source-after-configure.json').read_text())['source_id']
                == source_id, 'resume source differs from configured source')
    else:
        run(configure, source, output / 'configure.log', environment)
        require(fingerprint(source, output, 'source-after-configure.json') == source_id,
                'source changed during Windows cross-configuration')
    require(digest(certificate) == certificate_sha and digest(toolchain) == toolchain_sha,
            'qualified LLVM or target toolchain changed during configuration')
    require(recipe_hashes(source) == recipes, 'build recipe changed during Windows configuration')
    # Xmake records -o during 'f' but creates the object/BMI directory only
    # when 'b' begins. A configure-only preflight must leave a resumable,
    # explicitly disk-backed build tree for the separately scheduled compile.
    (output / 'xmake-build').mkdir(mode=0o700, exist_ok=True)
    if args.configure_only:
        print(json.dumps({'status': 'configure-only', 'source_id': source_id,
                          'llvm_certificate_sha256': certificate_sha,
                          'openssl_root': openssl,
                          'build_tools': build_tools,
                          'output': str(output)}, sort_keys=True))
        return
    verbose = ['-vD'] if args.verbose_commands else []
    run([str(xmake), 'b', '-j', '1', *verbose, 'uwvm'], source,
        output / 'uwvm-build.log', environment, resume=args.resume)
    run([str(xmake), 'b', '-j', '1', *verbose, 'uwvm-debug-server'], source,
        output / 'broker-build.log', environment, resume=args.resume)
    install = output / 'install'
    run([str(xmake), 'i', '-o', str(install), 'uwvm'], source,
        output / 'uwvm-install.log', environment, resume=args.resume)
    run([str(xmake), 'i', '-o', str(install), 'uwvm-debug-server'], source,
        output / 'broker-install.log', environment, resume=args.resume)
    launcher_source = source / 'test/0017.runtime/windows_debug_product_launcher.cc'
    launcher = install / 'bin/windows_debug_product_launcher.exe'
    require(launcher_source.is_file(), 'missing real Windows product debugger launcher fixture')
    run([str(clangxx), '--target=x86_64-w64-windows-gnu', '--sysroot=' + str(sdk),
         '-std=c++26', '-stdlib=libc++', '-nostdinc++',
         '-I' + str(libcxx), '-I' + str(mingw_headers), '--ld-path=' + str(lld),
         '-municode', '-O1', '-static', str(launcher_source),
         '-L' + str(sdk / 'x86_64-w64-mingw32/lib'), '-o', str(launcher)],
        source, output / 'launcher-build.log', environment, resume=args.resume)
    broker_child_source = source / 'test/0017.runtime/windows_control_broker_child.c'
    broker_child = install / 'bin/windows_control_broker_child.exe'
    require(broker_child_source.is_file(), 'missing Windows broker descendant fixture')
    run([str(clang), '--target=x86_64-w64-windows-gnu', '--sysroot=' + str(sdk),
         '-std=c17', '-I' + str(mingw_headers), '--ld-path=' + str(lld),
         '-municode', '-O1', '-static', str(broker_child_source),
         '-L' + str(sdk / 'x86_64-w64-mingw32/lib'), '-o', str(broker_child)],
        source, output / 'broker-child-build.log', environment, resume=args.resume)
    products = {}
    for name in ('uwvm.exe', 'uwvm-debug-server.exe',
                 'windows_debug_product_launcher.exe', 'windows_control_broker_child.exe'):
        product = install / 'bin' / name
        require(product.is_file() and pe_x64(product), f'missing Windows x64 product: {product}')
        products[name] = {'path': str(product), 'sha256': digest(product),
                          'size': product.stat().st_size}
    require(fingerprint(source, output, 'source-after-build.json') == source_id,
            'source changed during Windows cross-build')
    require(digest(certificate) == certificate_sha and digest(toolchain) == toolchain_sha,
            'qualified LLVM or target toolchain changed during build')
    require(recipe_hashes(source) == recipes, 'build recipe changed during Windows build')
    require(tool_hashes(xmake, clang, clangxx, lld, clang_bin) == build_tools,
            'Windows cross-build tool changed during build')
    if builtins is not None:
        verify_builtins_unchanged(builtins)
    result = {'status': 'cross-built-awaiting-real-windows-vm', 'source_id': source_id,
              'llvm_certificate_sha256': certificate_sha,
              'toolchain_sha256': toolchain_sha, 'build_recipe_files_sha256': recipes,
              'openssl_root': openssl,
              'build_tools': build_tools,
              'builder_driver_sha256': digest(Path(__file__).resolve()),
              'verbose_commands': args.verbose_commands,
              'disk_free_floor_bytes': ((16 if args.resume else 40) << 30) if args.allow_persistent_output else None,
              'linker_search_directories': [str(clang_bin), str(lld.parent)],
              'windows_builtins': builtins,
              'xmake_build_storage': str((output / 'xmake-build').resolve()),
              'build_logs_sha256': {name: digest(output / name) for name in (
                  'uwvm-build.log', 'broker-build.log', 'uwvm-install.log',
                  'broker-install.log', 'launcher-build.log', 'broker-child-build.log')},
              'products': products}
    with (output / 'build.json').open('x') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
