#!/usr/bin/env python3
"""Cross-build ordinary uwvm2 LLVM-full Windows products from frozen sources.

The external LLVM build may be the separately patched ROS LLVM, but its origin
is recorded explicitly.  Its certificate qualifies the libraries; this script
independently qualifies the ordinary executable and host broker.  A successful
cross-link is not a Windows VM result.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shlex
import shutil
import struct
import subprocess
import sys

from windows_compiler_rt_builtins import qualify as qualify_builtins, verify_unchanged as verify_builtins_unchanged


TARGET = 'x86_64-w64-windows-gnu'
CPUS = sorted([0, 2, 4, 6, *range(16, 32)])


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_cgroup(scope: str) -> dict[str, object]:
    require(bool(re.fullmatch(r'docker-[0-9a-f]{64}\.scope', scope)),
            'expected an exact Docker cgroup scope')
    base = Path('/sys/fs/cgroup/system.slice') / scope
    require((base / 'memory.max').read_text().strip() == '68719476736',
            'Windows build must have a 64 GiB memory limit')
    require((base / 'memory.swap.max').read_text().strip() == '0',
            'Windows build must have swap disabled')
    require(sorted(os.sched_getaffinity(0)) == CPUS,
            'Windows build must use the qualified 20-CPU set')
    require(str(os.getpid()) in (base / 'cgroup.procs').read_text().splitlines(),
            'Windows build process is outside the qualified Docker cgroup')
    return {'scope': scope, 'memory_max': '68719476736', 'swap_max': '0',
            'cpus': CPUS, 'builder_pid': os.getpid()}


def fingerprint(source: Path, output: Path, name: str) -> str:
    result = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / name)], text=True).strip()
    require(result.startswith('sha256:') and len(result) == 71,
            'invalid source fingerprint')
    return result


def recipe_hashes(source: Path) -> dict[str, str]:
    files = [source / 'xmake.lua', *sorted((source / 'xmake').rglob('*.lua')),
             source / 'tools/ci/windows_llvm_x64_toolchain.cmake',
             source / 'tools/ci/windows_external_llvm_config.py',
             source / 'tools/ci/build_windows_full_llvm_external.py',
             source / 'tools/ci/windows_compiler_rt_builtins.py',
             source / 'tools/debug/secure_server_windows.cpp',
             source / 'test/0017.runtime/windows_debug_product_launcher.cc',
             source / 'test/0017.runtime/windows_control_broker_child.c']
    for file in files:
        require(file.is_file(), f'missing build input: {file}')
    return {file.relative_to(source).as_posix(): digest(file) for file in files}


def tool_hashes(paths: dict[str, Path]) -> dict[str, dict[str, str]]:
    support_dir = os.environ.get('XMAKE_PROGRAM_DIR')
    if support_dir:
        paths['xmake-clang-module-support'] = (Path(support_dir).resolve(strict=True)
                                               / 'rules/c++/modules/clang/support.lua')
    return {name: {'path': str(path.absolute()),
                   'realpath': str(path.resolve(strict=True)),
                   'sha256': digest(path.resolve(strict=True))}
            for name, path in paths.items()}


def cmake_cache(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            result[key.split(':', 1)[0]] = value
    return result


def verify_archive(path: Path, readobj: Path, ar: Path) -> int:
    members = subprocess.check_output([str(ar), 't', str(path)], text=True).splitlines()
    require(bool(members), f'empty LLVM archive: {path}')
    process = subprocess.Popen([str(readobj), '--file-headers', str(path)],
                               stdout=subprocess.PIPE, text=True)
    count = 0
    member = '<unknown>'
    assert process.stdout is not None
    for line in process.stdout:
        if line.startswith('File: '):
            member = line.strip()
        if line.lstrip().startswith('Machine:'):
            require('IMAGE_FILE_MACHINE_AMD64' in line,
                    f'non-AMD64 COFF member in {path}: {member}: {line.strip()}')
            count += 1
    require(process.wait() == 0 and count == len(members),
            f'archive member/header mismatch: {path}: {count} vs {len(members)}')
    return count


def verify_external_llvm(root: Path, llvm_source: Path, readobj: Path,
                         ar: Path) -> dict[str, object]:
    certificate_path = root / 'qualified-llvm.json'
    certificate = json.loads(certificate_path.read_text())
    require(certificate.get('schema') == 1 and certificate.get('host_target') == TARGET,
            'external LLVM certificate is not Windows x64')
    require(certificate.get('archive_count', 0) >= 40,
            'external LLVM certificate has an incomplete archive closure')
    manifest = llvm_source / 'sources.sha256'
    require(digest(manifest) == certificate.get('source_manifest_sha256'),
            'external LLVM source does not match the archive certificate')
    subprocess.run(['sha256sum', '--check', '--quiet', 'sources.sha256'],
                   cwd=llvm_source, check=True, stdout=subprocess.DEVNULL)
    cache_path = root / 'CMakeCache.txt'
    require(digest(cache_path) == certificate.get('cmake_cache_sha256'),
            'external LLVM CMake cache changed after qualification')
    cache = cmake_cache(cache_path)
    expected = {'CMAKE_BUILD_TYPE': 'Release', 'CMAKE_SYSTEM_NAME': 'Windows',
                'CMAKE_C_COMPILER_TARGET': TARGET,
                'CMAKE_CXX_COMPILER_TARGET': TARGET,
                'CMAKE_ASM_COMPILER_TARGET': TARGET,
                'LLVM_HOST_TRIPLE': TARGET, 'BUILD_SHARED_LIBS': 'OFF',
                'LLVM_ENABLE_EH': 'OFF', 'LLVM_ENABLE_RTTI': 'OFF'}
    for key, value in expected.items():
        require(cache.get(key) == value, f'external LLVM {key}={cache.get(key)!r}')
    require(Path(cache['CMAKE_HOME_DIRECTORY']).resolve(strict=True) ==
            (llvm_source / 'llvm').resolve(strict=True),
            'external LLVM was built from a different source tree')
    require('X86' in cache.get('LLVM_TARGETS_TO_BUILD', '').split(';'),
            'external LLVM lacks X86 target')
    response = root / 'consumer-link.rsp'
    require(digest(response) == certificate.get('consumer_link_sha256'),
            'external LLVM link response changed after qualification')
    names = {str((root / name).resolve(strict=True)) for name in certificate['archives']}
    response_names = {str(Path(argument).resolve(strict=True))
                      for argument in shlex.split(response.read_text())
                      if argument.endswith('.a')}
    require(names == response_names, 'external LLVM link closure differs from certificate')
    records: dict[str, dict[str, object]] = {}
    for name, expected_record in sorted(certificate['archives'].items()):
        archive = (root / name).resolve(strict=True)
        require(archive.parent == (root / 'lib').resolve(strict=True),
                f'external LLVM archive escaped its library directory: {name}')
        members = verify_archive(archive, readobj, ar)
        sha = digest(archive)
        require(sha == expected_record['sha256'] and
                archive.stat().st_size == expected_record['size'] and
                members == expected_record['members'],
                f'external LLVM archive changed after qualification: {name}')
        records[name] = {'sha256': sha, 'members': members, 'size': archive.stat().st_size}
    require(len(records) == certificate['archive_count'] and
            'lib/libLLVMX86Disassembler.a' in records,
            'external LLVM disassembler or archive count is missing')
    require('lib/libLLVMDebugInfoDWARF.a' in records,
            'external LLVM lacks the source debugger DWARF component; regenerate and qualify its consumer closure')
    return {'origin': 'external-patched-llvm', 'version': certificate['llvm_version'],
            'target': TARGET, 'source_manifest_sha256': digest(manifest),
            'archive_certificate_sha256': digest(certificate_path),
            'consumer_link_sha256': digest(response),
            'archive_count': len(records), 'archives': records}


def run(command: list[str], source: Path, log: Path, environment: dict[str, str]) -> None:
    with log.open('xb') as stream:
        result = subprocess.run(command, cwd=source, env=environment,
                                stdout=stream, stderr=subprocess.STDOUT, check=False)
    require(result.returncode == 0,
            f'command failed with {result.returncode}; inspect {log}: {command!r}')


def pe_x64(path: Path) -> bool:
    with path.open('rb') as stream:
        require(stream.read(2) == b'MZ', f'not PE: {path}')
        stream.seek(0x3c)
        offset = struct.unpack('<I', stream.read(4))[0]
        require(0x40 <= offset < 1 << 20, f'invalid PE offset: {path}')
        stream.seek(offset)
        require(stream.read(4) == b'PE\0\0', f'missing PE signature: {path}')
        require(struct.unpack('<H', stream.read(2))[0] == 0x8664,
                f'PE is not AMD64: {path}')
        stream.seek(offset + 24)
        require(struct.unpack('<H', stream.read(2))[0] == 0x20b,
                f'PE is not PE32+: {path}')
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'prebuilt-llvm', 'llvm-source', 'sdk', 'output',
                 'docker-scope', 'xmake', 'clang-bin', 'lld', 'llvm-readobj', 'llvm-ar'):
        parser.add_argument('--' + name, required=True,
                            type=str if name == 'docker-scope' else Path)
    parser.add_argument('--openssl-root', type=Path, default=Path('default'))
    parser.add_argument('--windows-builtins', type=Path,
                        help='Explicit target compiler-rt archive; preserves the existing C++ EH runtime')
    parser.add_argument('--builtins-nm', type=Path,
                        help='Actual llvm-nm used to qualify an explicit --windows-builtins archive')
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--resume', action='store_true',
                        help='Reuse an interrupted, identically qualified Xmake tree')
    parser.add_argument('--disk-build', type=Path,
                        help='Persistent disk directory for the large Xmake BMI/object tree')
    args = parser.parse_args()
    cgroup = verify_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True)
    # Finder's AppleDouble sidecars are excluded from the source fingerprint,
    # but Xmake's recursive *.cppm glob would still scan/compile them as C++.
    # Require the isolated build copy to be cleaned before configuration;
    # never mutate the frozen source candidate from inside this builder.
    finder_sidecars = [path for path in source.rglob('._*') if path.is_file()]
    require(not finder_sidecars,
            f'clean AppleDouble metadata from the private Windows build copy: {finder_sidecars[0] if finder_sidecars else ""}')
    # A copied private source may carry Xmake's previous checkout/configuration
    # cache, including absolute source paths. It is generated state, never a
    # build input; a fresh qualification must let Xmake recreate it for this
    # exact source and output tree. A resume keeps its matching incremental BMI.
    xmake_state = source / '.xmake'
    if not args.resume and (xmake_state.exists() or xmake_state.is_symlink()):
        require(xmake_state.is_dir() and not xmake_state.is_symlink(),
                'unsafe Xmake project state in private source root')
        shutil.rmtree(xmake_state)
    llvm = args.prebuilt_llvm.resolve(strict=True)
    llvm_source = args.llvm_source.resolve(strict=True)
    sdk = args.sdk.resolve(strict=True)
    xmake = args.xmake.resolve(strict=True)
    clang_bin = args.clang_bin.resolve(strict=True)
    # Preserve the C/C++ driver names even when both are symlinks to one
    # Clang binary. Its argv[0] selects C++ runtime linking for host fixtures.
    clang = (clang_bin / 'clang').absolute()
    clangxx = (clang_bin / 'clang++').absolute()
    require(clang.is_file() and clangxx.is_file(), 'missing qualified Clang C/C++ drivers')
    # LLVM's multicall lld selects the COFF-capable GNU driver from argv[0].
    # Preserve an ld.lld symlink's spelling for --ld-path, as the ROS builder
    # does; resolving it to the generic `lld` executable loses that driver.
    lld = args.lld.absolute()
    require(lld.is_file(), f'missing COFF-capable ld.lld: {lld}')
    readobj = args.llvm_readobj.resolve(strict=True)
    ar = args.llvm_ar.resolve(strict=True)
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or output.is_relative_to(Path('/dev/shm')),
            'Windows qualification output must stay on tmpfs')
    require(args.configure_only or args.disk_build is not None,
            'full Windows cross-build requires --disk-build outside tmpfs')
    require(not (args.resume and args.configure_only),
            'configure-only has no resumable build state')
    if args.resume:
        require(output.is_dir() and not (output / 'build.json').exists(),
                'resume requires an incomplete qualification output')
        attempt = 2
        while (output / f'resume-{attempt}').exists():
            attempt += 1
        log_dir = output / f'resume-{attempt}'
        log_dir.mkdir(mode=0o700)
    else:
        output.mkdir(mode=0o700, parents=True, exist_ok=False)
        log_dir = output
    build_tree = None
    if args.disk_build is not None:
        build_tree = args.disk_build.resolve()
        require(not build_tree.is_relative_to(Path('/tmp')) and
                not build_tree.is_relative_to(Path('/dev/shm')),
                'large Xmake BMI/object tree must be on persistent disk')
        if args.resume:
            require(build_tree.is_dir() and (output / 'xmake-build').is_symlink() and
                    (output / 'xmake-build').resolve(strict=True) == build_tree,
                    'resume Xmake tree differs from the prior build')
        else:
            build_tree.mkdir(mode=0o700, parents=True, exist_ok=False)
            (output / 'xmake-build').symlink_to(build_tree, target_is_directory=True)
    external = verify_external_llvm(llvm, llvm_source, readobj, ar)
    if args.resume:
        require(json.loads((output / 'external-llvm.json').read_text()) == external,
                'resume LLVM archive certificate or closure changed')
    else:
        with (output / 'external-llvm.json').open('x') as stream:
            json.dump(external, stream, indent=2, sort_keys=True)
            stream.write('\n')
    recipes = recipe_hashes(source)
    facade = source / 'tools/ci/windows_external_llvm_config.py'
    tools = tool_hashes({'xmake': xmake, 'clang': clang, 'clangxx': clangxx,
                         'clang-scan-deps': clang_bin / 'clang-scan-deps',
                         'lld': lld, 'llvm-readobj': readobj, 'llvm-ar': ar,
                         'target-llvm-config': facade})
    source_id = fingerprint(source, log_dir, 'source-before.json')
    attempt_state = {'source_id': source_id, 'recipes': recipes, 'tools': tools,
                     'llvm': external, 'disk_build_tree': str(build_tree),
                     'docker_scope': args.docker_scope}
    if args.resume:
        require(json.loads((output / 'build-attempt.json').read_text()) == attempt_state,
                'resume source, tools, recipe, LLVM or cgroup changed')
    else:
        with (output / 'build-attempt.json').open('x') as stream:
            json.dump(attempt_state, stream, indent=2, sort_keys=True)
            stream.write('\n')
    openssl = ('default' if args.openssl_root == Path('default')
               else str(args.openssl_root.resolve(strict=True)))
    environment = dict(os.environ)
    environment['HOME'] = pwd.getpwuid(os.getuid()).pw_dir
    # Xmake uses the C++ driver with -fuse-ld=lld for the actual product.
    # The independently qualified GNU ld.lld can live outside clang_bin;
    # preserve that invocation spelling and make its directory searchable.
    environment['PATH'] = os.pathsep.join((str(clang_bin), str(lld.parent),
                                         environment.get('PATH', '')))
    environment['TMPDIR'] = str(output)
    environment['LLVM_CONFIG'] = str(facade)
    environment['UWVM_WINDOWS_LLVM_BUILD'] = str(llvm)
    environment['UWVM_WINDOWS_LLVM_SOURCE'] = str(llvm_source)
    environment.pop('UWVM_WINDOWS_QUALIFIED_BUILTINS', None)
    require((args.windows_builtins is None) == (args.builtins_nm is None),
            '--windows-builtins and --builtins-nm must be supplied together')
    builtins = None
    if args.windows_builtins is not None:
        builtins = qualify_builtins(args.windows_builtins, ar, readobj, args.builtins_nm,
                                   log_dir / 'builtins-qualification', environment)
        environment['UWVM_WINDOWS_QUALIFIED_BUILTINS'] = builtins['archive']
    require((sdk / 'include/c++/v1').is_dir() and
            (sdk / 'x86_64-w64-mingw32/include').is_dir(),
            'missing Windows SDK headers')
    configure = [str(xmake), 'f', '-p', 'mingw', '-a', 'x86_64', '-m', 'release',
                 '-o', str(output / 'xmake-build'), '--toolchain=clang',
                 '--cc=' + str(clang), '--cxx=' + str(clangxx), '--ld=' + str(clangxx),
                 '--use-llvm-compiler=y', '--enable-lto=n', '--static=non-system',
                 '--target=' + TARGET, '--sysroot=' + str(sdk), '--sdk=' + str(sdk),
                 '--stdlib=libc++', '--cxflags=-nostdinc++ -I' +
                 str(sdk / 'include/c++/v1') + ' -I' +
                 str(sdk / 'x86_64-w64-mingw32/include'),
                 '--execution-int=none', '--execution-jit=llvm',
                 '--use-cxx-module=y', '--openssl-root=' + openssl,
                 '--build-source-id=' + source_id]
    run(configure, source, log_dir / 'configure.log', environment)
    require(fingerprint(source, log_dir, 'source-after-configure.json') == source_id,
            'ordinary source changed during Windows configuration')
    require(recipe_hashes(source) == recipes and tool_hashes({
        'xmake': xmake, 'clang': clang, 'clangxx': clangxx,
        'clang-scan-deps': clang_bin / 'clang-scan-deps', 'lld': lld,
        'llvm-readobj': readobj, 'llvm-ar': ar, 'target-llvm-config': facade}) == tools,
        'Windows build recipes or tools changed during configuration')
    if args.configure_only:
        print(json.dumps({'status': 'configure-only', 'source_id': source_id,
                          'build_cgroup': cgroup,
                          'llvm_version': external['version'],
                          'archive_count': external['archive_count'],
                          'llvm_certificate_sha256': external['archive_certificate_sha256'],
                          'tools': tools}, sort_keys=True))
        return
    run([str(xmake), 'b', '-j', '1', 'uwvm'], source,
        log_dir / 'uwvm-build.log', environment)
    run([str(xmake), 'b', '-j', '1', 'uwvm-debug-server'], source,
        log_dir / 'broker-build.log', environment)
    install = output / 'install'
    run([str(xmake), 'i', '-o', str(install), 'uwvm'], source,
        log_dir / 'uwvm-install.log', environment)
    run([str(xmake), 'i', '-o', str(install), 'uwvm-debug-server'], source,
        log_dir / 'broker-install.log', environment)
    launcher = install / 'bin/windows_debug_product_launcher.exe'
    run([str(clangxx), '--target=' + TARGET, '--sysroot=' + str(sdk),
         '-std=c++26', '-stdlib=libc++', '-nostdinc++',
         '-I' + str(sdk / 'include/c++/v1'),
         '-I' + str(sdk / 'x86_64-w64-mingw32/include'),
         '--ld-path=' + str(lld), '-municode', '-O1', '-static',
         str(source / 'test/0017.runtime/windows_debug_product_launcher.cc'),
         '-L' + str(sdk / 'x86_64-w64-mingw32/lib'), '-o', str(launcher)],
        source, log_dir / 'launcher-build.log', environment)
    broker_child = install / 'bin/windows_control_broker_child.exe'
    run([str(clang), '--target=' + TARGET, '--sysroot=' + str(sdk), '-std=c17',
         '-I' + str(sdk / 'x86_64-w64-mingw32/include'),
         '--ld-path=' + str(lld), '-municode', '-O1', '-static',
         str(source / 'test/0017.runtime/windows_control_broker_child.c'),
         '-L' + str(sdk / 'x86_64-w64-mingw32/lib'), '-o', str(broker_child)],
        source, log_dir / 'broker-child-build.log', environment)
    products = {}
    for name in ('uwvm.exe', 'uwvm-debug-server.exe',
                 'windows_debug_product_launcher.exe',
                 'windows_control_broker_child.exe'):
        path = install / 'bin' / name
        require(path.is_file() and pe_x64(path), f'missing AMD64 PE: {path}')
        products[name] = {'path': str(path), 'sha256': digest(path),
                          'size': path.stat().st_size}
    require(fingerprint(source, log_dir, 'source-after-build.json') == source_id,
            'ordinary source changed during Windows cross-build')
    require(recipe_hashes(source) == recipes and tool_hashes({
        'xmake': xmake, 'clang': clang, 'clangxx': clangxx,
        'clang-scan-deps': clang_bin / 'clang-scan-deps', 'lld': lld,
        'llvm-readobj': readobj, 'llvm-ar': ar, 'target-llvm-config': facade}) == tools,
        'Windows build recipes or tools changed during cross-build')
    require(verify_external_llvm(llvm, llvm_source, readobj, ar) == external,
            'external target LLVM changed during ordinary cross-build')
    if builtins is not None:
        verify_builtins_unchanged(builtins)
    result = {'status': 'cross-built-awaiting-real-windows-vm',
              'source_id': source_id, 'external_llvm': external,
              'build_cgroup': cgroup,
              'llvm_certificate_sha256': external['archive_certificate_sha256'],
              'disk_build_tree': str(build_tree),
              'build_attempt_log_dir': str(log_dir),
              'windows_sdk': str(sdk), 'openssl_root': openssl,
              'windows_builtins': builtins,
              'xmake_configure_argv': configure,
              'build_recipe_files_sha256': recipes, 'build_tools': tools,
              'products': products}
    with (output / 'build.json').open('x') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
