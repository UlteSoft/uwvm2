#!/usr/bin/env python3
"""Source inventory and finite Linux architecture/ABI qualification matrix.

Reading these sources grants no execution qualification. In particular a
TargetInfo HasJIT flag, an ELF linker dispatch and a working guest VM are three
different pieces of evidence. Run compiler/VM probes only under the SSH guard.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re


def profile(name, family, arch, triple, bits, endian, abi, qemu, requirement='mcjit', **extra):
    return dict(id=name, family=family, llvm_arch=arch, triple=triple,
                pointer_bits=bits, byte_order=endian, abi=abi, qemu=qemu,
                requirement=requirement, execution_status='unqualified', **extra)


LINUX_PROFILES = [
    profile('x86_64', 'X86', 'x86_64', 'x86_64-linux-gnu', 64, 'little', 'sysv-lp64', 'qemu-x86_64'),
    profile('i686', 'X86', 'x86', 'i686-linux-gnu', 32, 'little', 'sysv-ilp32', 'qemu-i386', cpu_features='sse2'),
    profile('x86_64-x32', 'X86', 'x86_64', 'x86_64-linux-gnux32', 32, 'little', 'x32', 'qemu-x86_64', 'abi-probe'),
    profile('armv5-soft', 'ARM', 'arm', 'armv5-linux-gnueabi', 32, 'little', 'eabi-soft', 'qemu-arm'),
    profile('armv7-soft', 'ARM', 'arm', 'armv7-linux-gnueabi', 32, 'little', 'eabi-soft', 'qemu-arm'),
    profile('armv7-hard', 'ARM', 'arm', 'armv7-linux-gnueabihf', 32, 'little', 'eabi-hard', 'qemu-arm', cpu_features='vfp,neon'),
    profile('thumbv7-hard', 'ARM', 'thumb', 'thumbv7-linux-gnueabihf', 32, 'little', 'eabi-hard', 'qemu-arm', 'loader-gap'),
    profile('armebv7-soft', 'ARM', 'armeb', 'armebv7-linux-gnueabi', 32, 'big', 'eabi-soft', 'qemu-armeb', 'loader-gap'),
    profile('thumbebv7-soft', 'ARM', 'thumbeb', 'thumbebv7-linux-gnueabi', 32, 'big', 'eabi-soft', 'qemu-armeb', 'loader-gap'),
    profile('aarch64', 'AArch64', 'aarch64', 'aarch64-linux-gnu', 64, 'little', 'lp64', 'qemu-aarch64'),
    profile('aarch64_be', 'AArch64', 'aarch64_be', 'aarch64_be-linux-gnu', 64, 'big', 'lp64', 'qemu-aarch64_be', 'ros-patched-loader'),
    profile('aarch64-ilp32', 'AArch64', 'aarch64', 'aarch64-linux-gnu_ilp32', 32, 'little', 'gnu-ilp32', 'qemu-aarch64', 'loader-gap'),
    profile('ppc32', 'PowerPC', 'ppc', 'powerpc-linux-gnu', 32, 'big', 'sysv32', 'qemu-ppc', 'ros-patched-loader'),
    profile('ppc32le', 'PowerPC', 'ppcle', 'powerpcle-linux-gnu', 32, 'little', 'sysv32', None, 'loader-gap'),
    profile('ppc64', 'PowerPC', 'ppc64', 'powerpc64-linux-gnu', 64, 'big', 'elfv1', 'qemu-ppc64', 'ros-patched-loader'),
    profile('ppc64-elfv2', 'PowerPC', 'ppc64', 'powerpc64-linux-gnu', 64, 'big', 'elfv2', 'qemu-ppc64', 'ros-patched-loader'),
    profile('ppc64le', 'PowerPC', 'ppc64le', 'powerpc64le-linux-gnu', 64, 'little', 'elfv2', 'qemu-ppc64le', 'ros-patched-loader'),
    profile('loongarch64', 'LoongArch', 'loongarch64', 'loongarch64-linux-gnu', 64, 'little', 'lp64d', 'qemu-loongarch64', cpu_features='baseline; LSX/LASX separate'),
    profile('loongarch64-lp64f', 'LoongArch', 'loongarch64', 'loongarch64-linux-gnu', 64, 'little', 'lp64f', 'qemu-loongarch64', 'abi-probe'),
    profile('loongarch64-lp64s', 'LoongArch', 'loongarch64', 'loongarch64-linux-gnu', 64, 'little', 'lp64s', 'qemu-loongarch64', 'abi-probe'),
    profile('loongarch32', 'LoongArch', 'loongarch32', 'loongarch32-linux-gnu', 32, 'little', 'ilp32', None, 'hasjit-false'),
    profile('s390x', 'SystemZ', 'systemz', 's390x-linux-gnu', 64, 'big', 'sysv-lp64', 'qemu-s390x'),
    profile('sparc64', 'Sparc', 'sparcv9', 'sparcv9-linux-gnu', 64, 'big', 'v9-lp64', 'qemu-sparc64', 'ros-patched-loader'),
    profile('sparc32', 'Sparc', 'sparc', 'sparc-linux-gnu', 32, 'big', 'v8-ilp32', 'qemu-sparc', 'hasjit-false'),
    profile('sparc32le', 'Sparc', 'sparcel', 'sparcel-linux-gnu', 32, 'little', 'v8-ilp32', None, 'hasjit-false'),
    profile('hexagon', 'Hexagon', 'hexagon', 'hexagon-linux-musl', 32, 'little', 'hexagon', 'qemu-hexagon', 'jitlink-only'),
    profile('m68k', 'M68k', 'm68k', 'm68k-linux-gnu', 32, 'big', 'sysv32', 'qemu-m68k', 'loader-gap'),
]
for suffix, endian in (('', 'big'), ('el', 'little')):
    for revision in ('r2', 'r6'):
        LINUX_PROFILES += [
            profile(f'mips32{suffix}-{revision}', 'Mips', 'mips' + suffix,
                    f'mips{suffix}-linux-gnu', 32, endian, 'o32', 'qemu-mips' + suffix,
                    cpu_features=f'mips32{revision}; FP NaN mode must match libc'),
            profile('mips64' + suffix + ('' if revision == 'r2' else '-r6'), 'Mips', 'mips64' + suffix,
                    f'mips64{suffix}-linux-gnuabi64', 64, endian, 'n64', 'qemu-mips64' + suffix,
                    cpu_features=f'mips64{revision}; FP NaN mode must match libc'),
            profile(f'mipsn32{suffix}-{revision}', 'Mips', 'mips64' + suffix,
                    f'mips64{suffix}-linux-gnuabin32', 32, endian, 'n32', 'qemu-mipsn32' + suffix,
                    cpu_features=f'mips64{revision}; FP NaN mode must match libc'),
        ]
for bits in (32, 64):
    for fp_suffix in ('', 'f', 'd'):
        abi = ('ilp32' if bits == 32 else 'lp64') + fp_suffix
        LINUX_PROFILES.append(profile(f'riscv{bits}' + ('' if fp_suffix == 'd' else '-' + abi),
            'RISCV', f'riscv{bits}', f'riscv{bits}-linux-gnu', bits, 'little', abi,
            f'qemu-riscv{bits}', cpu_features='I,M,A,C; F/D/V separately qualified'))
    LINUX_PROFILES.append(profile(f'riscv{bits}be', 'RISCV', f'riscv{bits}be',
        f'riscv{bits}be-linux-gnu', bits, 'big', 'unqualified', None, 'hasjit-false'))

NON_HOST_ABIS = {
    'BPF': 'HasJIT advertises BPF code generation; uwvm2 rejects its bytecode host ABI.',
    'AMDGPU': 'GPU execution ABI, not a Linux process hosting this MCJIT VM.',
    'NVPTX': 'GPU execution ABI, not a Linux process hosting this MCJIT VM.',
    'DirectX': 'DXIL shader representation, not a native Linux host CPU.',
    'SPIRV': 'Intermediate representation, not a native Linux host CPU.',
    'WebAssembly': 'Guest bytecode target, not a native Linux host CPU.',
}


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def source_inventory(root):
    cmake = root / 'CMakeLists.txt'
    text = cmake.read_text()
    def group(name):
        return re.search(r'set\(' + name + r'\s+(.*?)\)', text, re.S)[1].split()
    registry = root / 'include/llvm/MC/TargetRegistry.h'
    assert re.search(r'bool HasJIT\s*=\s*false', registry.read_text())
    pins = {str(cmake): digest(cmake), str(registry): digest(registry)}
    rows, named = [], []
    for path in sorted((root / 'lib/Target').glob('*/TargetInfo/*TargetInfo.cpp')):
        content = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
        pins[str(path)] = digest(path)
        pattern = (r'RegisterTarget<\s*(?:llvm::)?Triple::(\w+)\s*(?:,\s*(true|false))?\s*>'
                   r'\s+\w+\s*\([^;]*?"([^"]+)"')
        for arch, jit, name in re.findall(pattern, content):
            rows.append(dict(family=path.parts[-3], arch=arch, registered_name=name, has_jit=jit == 'true',
                             source=str(path), source_sha256=pins[str(path)],
                             omitted_flag_defaults_false=not jit))
        # Alias registrations use callbacks instead of RegisterTarget's arch
        # template (arm64, arm64_32, bpf and amdgcn). Preserve them separately.
        for match in re.finditer(r'TargetRegistry::RegisterTarget\s*\(', content):
            start = match.end() - 1
            depth, quoted, escaped = 0, False, False
            for end in range(start, len(content)):
                char = content[end]
                if quoted:
                    if escaped:
                        escaped = False
                    elif char == '\\':
                        escaped = True
                    elif char == '"':
                        quoted = False
                elif char == '"':
                    quoted = True
                elif char == '(':
                    depth += 1
                elif char == ')':
                    depth -= 1
                    if depth == 0:
                        break
            call = content[start:end + 1]
            strings = re.findall(r'"(?:[^"\\]|\\.)*"', call)
            flag = re.search(r',\s*(true|false)\s*\)$', call)
            assert len(strings) >= 3, (path, call)
            named.append(dict(family=path.parts[-3], registered_name=json.loads(strings[0]),
                              has_jit=bool(flag and flag[1] == 'true'),
                              omitted_flag_defaults_false=not flag,
                              source=str(path), source_sha256=pins[str(path)]))
    assert rows and len({r['arch'] for r in rows}) == len(rows)
    loaders = {}
    for name, rel in (('runtime_dyld_elf', 'lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.cpp'),
                      ('jitlink_elf', 'lib/ExecutionEngine/JITLink/ELF.cpp')):
        path = root / rel
        if path.is_file():
            pins[str(path)] = digest(path)
            loaders[name] = dict(source=str(path), sha256=pins[str(path)],
                architecture_case_labels=sorted(set(re.findall(r'case\s+(?:llvm::)?Triple::(\w+)\s*:', path.read_text()))),
                scope='Source dispatch inventory only; does not qualify relocations, ABI, tail calls or execution')
    return dict(core_families=group('LLVM_ALL_TARGETS'),
                experimental_families=group('LLVM_ALL_EXPERIMENTAL_TARGETS'),
                target_registrations=rows, named_alias_registrations=named,
                loaders=loaders, source_pins=pins,
                scope='Read-only source inventory; HasJIT is not runtime qualification')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('llvm_root', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--upstream-root', type=Path)
    p.add_argument('--upstream-commit')
    a = p.parse_args()
    bundled = source_inventory(a.llvm_root.resolve())
    registrations = {r['arch']: r for r in bundled['target_registrations']}
    rows = [{**r, 'bundled_has_jit': registrations[r['llvm_arch']]['has_jit']}
            for r in LINUX_PROFILES]
    covered = {r['llvm_arch'] for r in rows}
    omitted = [r['arch'] for r in registrations.values() if r['has_jit'] and r['arch'] not in covered]
    assert set(omitted) <= {'aarch64_32', 'bpfel', 'bpfeb'}, omitted
    output = dict(schema='uwvm-linux-codegen-architecture-catalog-v1', bundled=bundled,
                  linux_profiles=rows, other_advertised_jit_abis=omitted,
                  non_host_abis=NON_HOST_ABIS, qualified_guest_runs=0,
                  qualification_required='Both repos, llvm-full O1/O2/O3, exact interpreter macro builds; main lazy and observed tier-2 entry; real VM output and emitted-object inspection')
    if a.upstream_root:
        assert a.upstream_commit
        output['upstream'] = source_inventory(a.upstream_root.resolve())
        output['upstream_commit'] = a.upstream_commit
    a.output.write_text(json.dumps(output, indent=2) + '\n')
    print('SOURCE INVENTORY', len(rows), 'Linux architecture/ABI rows; no new guest qualification')


if __name__ == '__main__':
    main()
