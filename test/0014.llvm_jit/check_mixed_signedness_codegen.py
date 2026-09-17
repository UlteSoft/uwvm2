#!/usr/bin/env python3
"""Execute production conversion IR across x86 sub-ISAs, without host-FP oracles.

Build llvm_jit_mixed_signedness_conversion.cc first. Its `emit TRIPLE FEATURES`
mode emits optimized production IR. This runner checks exact bits in separately
compiled C wrappers; Python precomputes the finite integer conversion oracle.
The selected high integers are not double-rounding halfway cases. This is not
an exhaustive integer-to-float rounding test. QEMU/SDE are execution evidence,
not a claim about complete CLI/OS integration on those architectures.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--full-probe', type=Path, required=True)
    p.add_argument('--ros-probe', type=Path, required=True)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--sde', type=Path, required=True)
    p.add_argument('--qemu-dir', type=Path, required=True)
    p.add_argument('--cross-root', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(exist_ok=False)
    sdk, out = a.sdk.resolve(), a.out.resolve()
    rows = []
    def run(command, stem, expected=0):
        result = subprocess.run(list(map(str, command)), capture_output=True, timeout=120)
        (out/(stem+'.log')).write_bytes(result.stdout+result.stderr)
        if result.returncode != expected:
            raise RuntimeError((stem, result.returncode, result.stderr[-2000:].decode(errors='replace')))
        return result.stdout
    declarations, statements = [], []
    for fp in [32, 64]:
        fmt = '<f' if fp == 32 else '<d'
        bits = lambda x: int.from_bytes(struct.pack(fmt, x), 'little')
        for iw in [32, 64]:
            for ts in [False, True]:
                for cs in [False, True]:
                    name = f'roundtrip_{fp}_{iw}_{"s" if ts else "u"}_{"s" if cs else "u"}'
                    declarations.append(f'extern uint{fp}_t {name}(uint{fp}_t);')
                    for candidate in [-1.5, -.5, -0., 0., .5, 1.5, 2147483520., 2147483648.,
                                      4294967040., 9223372036854775808., 18446744073709549568.]:
                        x = struct.unpack(fmt, struct.pack(fmt, candidate))[0]
                        if (not ts and x <= -1) or x >= 2**(iw-int(ts)):
                            continue
                        integer = math.trunc(x) % 2**iw
                        if cs and integer >= 2**(iw-1):
                            integer -= 2**iw
                        statements.append(f'if ({name}(UINT{fp}_C({bits(x)})) != UINT{fp}_C({bits(float(integer))})) '
                                          f'{{ puts("FAIL {name} input={x}"); return 1; }}')
    driver = out/'runner.c'
    driver.write_text('#include <stdint.h>\n#include <stdio.h>\n'+'\n'.join(declarations)+
                      '\nint main(void) {\n'+'\n'.join(statements)+f'\nputs("PASS {len(statements)} exact-bit checks"); return 0; }}\n')
    # An integer-only public ABI lets x86-64 no-SSE code execute without relying
    # on the platform's SSE float argument/return convention.
    variants = [
        ('sse2', 'x86_64-linux-gnu', 'x86-64', '+sse2,-avx,-avx512f', []),
        ('avx2', 'x86_64-linux-gnu', 'haswell', '+sse2,+avx2,-avx512f', []),
        ('avx512', 'x86_64-linux-gnu', 'skylake-avx512', '+sse2,+avx512f,+avx512dq,+avx512vl', [a.sde, '-skx', '--']),
        ('avx512-no-vl', 'x86_64-linux-gnu', 'skylake-avx512', '+sse2,+avx512f,+avx512dq,-avx512vl', [a.sde, '-skx', '--']),
        ('x64-x87', 'x86_64-linux-gnu', 'x86-64', '-sse,-sse2,-avx,-avx512f', []),
        ('i686-sse2', 'i686-linux-gnu', 'pentium4', '+sse2,-avx', [a.qemu_dir/'qemu-i386', '-cpu', 'max']),
        ('i686-x87', 'i686-linux-gnu', 'pentium3', '-sse,-sse2,-avx', [a.qemu_dir/'qemu-i386', '-cpu', 'pentium3']),
    ]
    for product, probe in [('full', a.full_probe.resolve()), ('ros', a.ros_probe.resolve())]:
        for variant, triple, cpu, features, emulator in variants:
            ir = out/f'{product}-{variant}.ll'
            ir.write_bytes(run([probe, 'emit', triple, features], f'{product}-{variant}-emit'))
            for opt in [0, 3]:
                stem = f'{product}-{variant}-O{opt}'
                obj, binary = out/(stem+'.o'), out/stem
                run([sdk/'bin/llc', f'-O{opt}', '-mtriple='+triple, '-mcpu='+cpu, '-mattr='+features,
                     '-filetype=obj', ir, '-o', obj], stem+'-llc')
                asm = run([sdk/'bin/llvm-objdump', '-dr', obj], stem+'-disassembly')
                if triple.startswith('i686'):
                    compiler = [a.cross_root/'usr/bin/i686-linux-gnu-gcc-15', '--sysroot='+str(a.cross_root)]
                else:
                    compiler = [sdk/'bin/clang']
                run([*compiler, '-O0', '-static', '-no-pie', driver, obj, '-o', binary], stem+'-link')
                result = run([*emulator, binary], stem+'-run')
                rows.append(dict(product=product, variant=variant, optimization=opt, checks=len(statements),
                                 passed=True, output=result.decode(), object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                                 ir_sha256=hashlib.sha256(ir.read_bytes()).hexdigest(), disassembly_bytes=len(asm)))
                print(stem, result.decode().strip(), flush=True)
                (out/'results.json').write_text(json.dumps(dict(rows=rows), indent=2)+'\n')
    print(json.dumps(dict(runs=len(rows), exact_bit_checks=sum(r['checks'] for r in rows), failed=0)))


if __name__ == '__main__':
    main()
