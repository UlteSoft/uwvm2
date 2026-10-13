#!/usr/bin/env python3
"""Attest a built ros.10 Windows-x64 LLVM link closure without rebuilding LLVM.

Run under the same bounded Linux cgroup as the later Windows product build.
The output is consumed only when UWVM_ROS_QUALIFIED_PREBUILT_LLVM is explicit.
It never substitutes a system LLVM or a different architecture's archives.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


TARGET = 'x86_64-w64-windows-gnu'
VERSION = '23.1.1-uwvm-ros.13'
CPUS = [0, 2, 4, 6, *range(16, 32)]


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def verify_cgroup(scope: str) -> None:
    require(bool(re.fullmatch(r'docker-[0-9a-f]{64}\.scope', scope)), 'expected exact Docker scope')
    # docker exec + chroot /host exposes the host's /sys while the process
    # remains in the Docker cgroup. Inspect that exact scope, not host root.
    cgroup = Path('/sys/fs/cgroup/system.slice') / scope
    require((cgroup / 'memory.max').read_text().strip() == '68719476736', 'expected 64 GiB cgroup')
    require((cgroup / 'memory.swap.max').read_text().strip() == '0', 'swap must be disabled')
    require(sorted(os.sched_getaffinity(0)) == CPUS, 'expected the qualified 20-CPU set')


def cache_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def contract(build: Path) -> dict:
    reply = build / '.cmake/api/v1/reply'
    indexes = sorted(reply.glob('index-*.json'))
    require(bool(indexes), 'missing CMake file-API index')
    index = json.loads(indexes[-1].read_text())
    model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
    configurations = model['configurations']
    require(len(configurations) == 1 and configurations[0]['name'] == 'Release',
            'LLVM file-API contract is not a single Release configuration')
    for target in configurations[0]['targets']:
        if target['name'] == 'uwvm_ros_llvm_contract':
            value = json.loads((reply / target['jsonFile']).read_text())
            require(value['link']['language'] == 'CXX', 'LLVM link contract is not C++')
            return value
    raise RuntimeError('missing ROS LLVM consumer target')


def verify_sources(root: Path) -> str:
    manifest = root / 'sources.sha256'
    require(manifest.is_file(), f'missing LLVM source manifest: {manifest}')
    subprocess.run(['sha256sum', '--check', '--quiet', 'sources.sha256'],
                   cwd=root, check=True, stdout=subprocess.DEVNULL)
    return digest(manifest)


def verify_archive(path: Path, readobj: Path, ar: Path) -> int:
    listed = subprocess.check_output([str(ar), 't', str(path)], text=True).splitlines()
    require(bool(listed), f'empty LLVM archive: {path}')
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
                    f'non-Windows-x64 LLVM archive member: {member}: {line.strip()}')
            count += 1
    require(process.wait() == 0 and count == len(listed),
            f'archive member/header count mismatch: {path}, {count} vs {len(listed)}')
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--llvm-readobj', type=Path, required=True)
    parser.add_argument('--llvm-ar', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    verify_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True) / 'third-parties/llvm'
    build = args.build.resolve(strict=True)
    current_manifest = verify_sources(source)
    cache_path = build / 'CMakeCache.txt'
    require(cache_path.is_file(), 'missing qualified LLVM CMake cache')
    cache = cache_values(cache_path)
    expected = {
        'CMAKE_BUILD_TYPE': 'Release', 'CMAKE_SYSTEM_NAME': 'Windows',
        'CMAKE_C_COMPILER_TARGET': TARGET, 'CMAKE_CXX_COMPILER_TARGET': TARGET,
        'CMAKE_ASM_COMPILER_TARGET': TARGET, 'LLVM_HOST_TRIPLE': TARGET,
        'LLVM_VERSION_SUFFIX': '-uwvm-ros.13', 'BUILD_SHARED_LIBS': 'OFF',
        'LLVM_ENABLE_EH': 'OFF', 'LLVM_ENABLE_RTTI': 'OFF',
    }
    for name, value in expected.items():
        require(cache.get(name) == value, f'wrong LLVM CMake {name}: {cache.get(name)!r}')
    require('X86' in cache.get('LLVM_TARGETS_TO_BUILD', '').split(';'), 'missing X86 LLVM target')
    recorded = Path(cache['CMAKE_HOME_DIRECTORY']).resolve(strict=True)
    require(recorded.name == 'llvm' and (recorded.parent / 'sources.sha256').is_file(),
            'LLVM CMake source is not a manifest-backed ROS checkout')
    require(verify_sources(recorded.parent) == current_manifest,
            'built LLVM source differs from frozen ROS source')
    header = build / 'include/llvm/Config/llvm-config.h'
    require(header.is_file() and f'#define LLVM_VERSION_STRING "{VERSION}"' in header.read_text().splitlines(),
            'Windows LLVM generated version header is not ros.13')
    version = json.loads((build / 'uwvm-llvm-version.json').read_text())
    require(version == {'version': VERSION, 'host_target': TARGET}, 'LLVM target/version mismatch')
    link = contract(build)['link']['commandFragments']
    fragments = [{'role': item.get('role', ''), 'fragment': item['fragment']} for item in link]
    archives: dict[str, dict[str, object]] = {}
    for item in fragments:
        name = item['fragment']
        if item['role'] != 'libraries' or not name.endswith('.a'):
            continue
        archive = (build / name).resolve(strict=True)
        require(archive.parent == (build / 'lib').resolve(strict=True),
                f'cross LLVM link escapes its library directory: {name}')
        if name in archives:
            continue
        members = verify_archive(archive, args.llvm_readobj, args.llvm_ar)
        archives[name] = {'sha256': digest(archive), 'size': archive.stat().st_size,
                          'members': members}
    require(len(archives) >= 40 and 'lib/libLLVMX86CodeGen.a' in archives,
            'incomplete target LLVM archive closure')
    response = build / 'consumer-link.rsp'
    require(response.is_file(), 'missing qualified target LLVM response file')
    result = {
        'schema': 1, 'llvm_version': VERSION, 'host_target': TARGET,
        'source_manifest_sha256': current_manifest,
        'cmake_cache_sha256': digest(cache_path),
        'generated_version_header_sha256': digest(header),
        'consumer_link_sha256': digest(response),
        'link_fragments': fragments, 'archives': archives,
        'archive_count': len(archives),
    }
    output = build / 'qualified-llvm.json'
    payload = (json.dumps(result, indent=2, sort_keys=True) + '\n').encode()
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0), 0o600)
    with os.fdopen(descriptor, 'wb') as file:
        file.write(payload)
    print(json.dumps({'manifest': str(output), 'sha256': hashlib.sha256(payload).hexdigest(),
                      'archive_count': len(archives), 'source_manifest_sha256': current_manifest},
                     sort_keys=True))


if __name__ == '__main__':
    main()
