#!/usr/bin/env python3
"""Prepare fixed FreeBSD SDK oracle cross-compilation commands; execute none."""

import argparse
import hashlib
import json
from pathlib import Path


BASE = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
TOOLCHAIN = Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm')
SDK = BASE / 'qemu-platform-tests/freebsd-sdk-actual-20261003-r3/sysroot'
SOURCE_SHA = '58b066d3454eed5b972036bc14f081e9d8284408ab9f568e66507a445e42f8b3'
SDK_RECORD_SHA = '250aa737639c465c746a3332ffa59c8503d1cb399ba8b7c110b8d4ced31cf93a'
HOST_RECORD_SHA = 'd941b7c9a349dddfda5dbd6059006768ecabe3a0d5825c1d0eb0af0712fdcbe9'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--closure', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    closure = args.closure.resolve(strict=True)
    if hashlib.sha256((source.parent / 'manifest.json').read_bytes()).hexdigest() != SOURCE_SHA:
        raise RuntimeError('immutable FreeBSD component source changed')
    if hashlib.sha256((SDK.parent / 'provider-actual.json').read_bytes()).hexdigest() != SDK_RECORD_SHA:
        raise RuntimeError('actual selected FreeBSD SDK provider record changed')
    host_record = closure.parent / 'host-providers-source-static.json'
    if hashlib.sha256(host_record.read_bytes()).hexdigest() != HOST_RECORD_SHA:
        raise RuntimeError('static host cross-tool provider inventory changed')
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    evidence = args.evidence.resolve()
    env = ['/usr/bin/env', '-u', 'LD_PRELOAD', '-u', 'LD_AUDIT',
           '-u', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', 'PYTHONDONTWRITEBYTECODE=1',
           'LD_LIBRARY_PATH=' + ':'.join(map(str, (TOOLCHAIN / 'lib',
                                                   TOOLCHAIN / 'lib/x86_64-unknown-linux-gnu')))]
    target = ['--target=x86_64-unknown-freebsd15.1', '--sysroot=' + str(SDK), '-stdlib=libc++']
    common = [str(TOOLCHAIN / 'bin/clang++'), *target, '-nostdinc++', '-isystem',
              str(SDK / 'usr/include/c++/v1'), '-std=c++23', '-O3', '-g',
              '-fasynchronous-unwind-tables']
    plan_path = output / 'plan.json'
    commands = [['input-before', ['python3', str(closure), '--plan', str(plan_path), 'before']]]
    commands.append(['actual-tool-version', [*env, str(TOOLCHAIN / 'bin/clang++'), '--version']])
    variants = []
    for repo in ('uwvm2-ros', 'uwvm2'):
        root = source / repo
        for suffix, flags in (('eh', ['-fexceptions']), ('noeh', ['-fno-exceptions'])):
            label = repo + '-' + suffix
            directory = output / label
            directory.mkdir(mode=0o700)
            obj, binary = directory / 'sdk-oracle.o', directory / 'sdk-oracle'
            dep, link_map = directory / 'sdk-oracle.d', directory / 'link.map'
            commands.extend([
                [label + '-compile', [*env, *common, *flags,
                    '-I' + str(root / 'third-parties/fast_io/include'), '-MD', '-MF', str(dep),
                    '-c', str(root / 'test/0022.qemu_platforms/freebsd151_sdk_kernel_oracle.cc'), '-o', str(obj)]],
                [label + '-object-bind', ['python3', str(closure), '--plan', str(plan_path), 'object', '--label', label]],
                [label + '-link', [*env, str(TOOLCHAIN / 'bin/clang++'), *target,
                    '--ld-path=' + str(TOOLCHAIN / 'bin/ld.lld'), '-Wl,--trace',
                    '-Wl,-Map=' + str(link_map), str(obj), '-o', str(binary)]],
                [label + '-binary-bind', ['python3', str(closure), '--plan', str(plan_path), 'binary', '--label', label]],
                [label + '-elf-provider-inspection', [*env, str(TOOLCHAIN / 'bin/llvm-readobj'),
                    '--file-headers', '--program-headers', '--dynamic-table', '--needed-libs', '--notes', str(binary)]],
                [label + '-main-disassembly', [*env, str(TOOLCHAIN / 'bin/llvm-objdump'),
                    '--disassemble-symbols=main', str(obj)]],
            ])
            variants.append({'label': label, 'repository': repo, 'variant': suffix,
                             'object': str(obj), 'binary': str(binary), 'depfile': str(dep),
                             'link_map': str(link_map), 'link_log': str(evidence / (label + '-link.log'))})
    commands.append(['input-after', ['python3', str(closure), '--plan', str(plan_path), 'after']])
    command_data = (json.dumps(commands, indent=2) + '\n').encode()
    plan = {'schema': 'uwvm-freebsd151-sdk-oracle-cold-build-v1', 'kind': 'source-only',
            'target': 'x86_64-unknown-freebsd15.1', 'actual_guest_qualified': False,
            'ROS_paired_LLVM23_product_qualified': False, 'named_module_qualified': False,
            'source': str(source), 'sdk': str(SDK), 'toolchain': str(TOOLCHAIN),
            'output': str(output), 'closure': str(closure), 'evidence': str(evidence),
            'source_manifest_sha256': SOURCE_SHA, 'SDK_provider_record_sha256': SDK_RECORD_SHA,
            'host_provider_record': str(host_record), 'host_provider_record_sha256': HOST_RECORD_SHA,
            'variants': variants, 'commands_sha256': hashlib.sha256(command_data).hexdigest(),
            'runtime_scope': 'Actual FreeBSD 15.1 base SDK libc++ ABI1/libcxxrt/compiler_rt/unwind aliases only; no vendored ROS product qualification.',
            'driver_source': 'https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-23.1.1/clang/lib/Driver/ToolChains/FreeBSD.cpp',
            'guest_followup_required': 'Execute each exact fresh artifact as the isolated non-root user in the owned disposable FreeBSD 15.1 guest; compile/ELF inspection does not grant a platform PASS.'}
    (output / 'commands.json').write_bytes(command_data)
    blob = (json.dumps(plan, indent=2, sort_keys=True) + '\n').encode()
    plan_path.write_bytes(blob)
    print(json.dumps({'commands': len(commands), 'commands_sha256': plan['commands_sha256'],
                      'plan_sha256': hashlib.sha256(blob).hexdigest(), 'compiler_executed': False}))


if __name__ == '__main__':
    main()
