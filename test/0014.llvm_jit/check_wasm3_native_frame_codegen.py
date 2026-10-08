#!/usr/bin/env python3
"""Inspect actual full-JIT objects for a new indexed-memory leaf on x86-64/AArch64/RV64.

Objects come from this run's private cache, not an offline substitute emitter.
The bounded decoder is inspection-only: it does not authenticate or execute
cache payloads. Signed-cache execution is checked by run_wasm3_native_unwind.py.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys


def decode_object(blob, ros):
    if len(blob) < 64:
        raise ValueError('truncated cache header')
    magic, version, header, codec, signature, expected, size, isa, context, sig_size = struct.unpack('<8sIIIIQQQQQ', blob[:64])
    if magic != (b'UWVMROS\1' if ros else b'UWVMLJC\1') or version != 5 or header != 64 or signature != 1 or sig_size != 64:
        raise ValueError('unexpected cache format/signature layout')
    start = header + isa + context + sig_size
    if expected > 1048576 or start + size != len(blob):
        raise ValueError('invalid object size')
    payload = blob[start:]
    if codec == 0:
        result = payload
    elif codec == 2:
        result = bytearray()
        cursor = 0

        def byte():
            nonlocal cursor
            if cursor == len(payload):
                raise ValueError('truncated sequence')
            value = payload[cursor]
            cursor += 1
            return value

        def extended(length):
            while True:
                extra = byte()
                length += extra
                if length > expected:
                    raise ValueError('oversized sequence')
                if extra != 255:
                    return length

        while len(result) != expected:
            token = byte()
            literals = token >> 4
            if literals == 15:
                literals = extended(literals)
            if cursor + literals > len(payload) or len(result) + literals > expected:
                raise ValueError('invalid literal extent')
            result.extend(payload[cursor:cursor + literals])
            cursor += literals
            if len(result) == expected:
                break
            distance = byte() | (byte() << 8)
            count = 4 + (token & 15)
            if token & 15 == 15:
                count = extended(count)
            if not distance or distance > len(result) or len(result) + count > expected:
                raise ValueError('invalid backward match')
            for _ in range(count):
                result.append(result[-distance])
        if cursor != len(payload):
            raise ValueError('trailing payload')
    else:
        raise ValueError('unsupported inspection codec')
    if len(result) != expected or result[:4] != b'\x7fELF':
        raise ValueError('not the expected ELF object')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--llvm', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    fixtures = args.output / 'fixtures'
    subprocess.run([sys.executable, str(root / 'test/0013.uwvm_int/wasm3/multi_memory_cases.py'), str(fixtures)], check=True)
    rows = []
    for policy in ('instruction', 'unwind'):
        cache = args.output / (policy + '-cache')
        cache.mkdir()
        command = [str(args.uwvm.resolve())]
        command += ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        command += ['-Rct', '0', '-Rllvm-call-stack', policy, '-Rllvm-full-policy', 'pb-o3',
                    '-Rllvm-cache-path', 'path', cache.name, '-Rclog', 'file', policy + '.compile.log',
                    '-WFE-multi-memory', '--run', str(fixtures / 'load-memory1-exec.wasm')]
        run = subprocess.run(command, cwd=args.output, capture_output=True, timeout=120)
        (args.output / (policy + '.log')).write_bytes(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(f'{policy}: actual target execution failed')
        files = list(cache.rglob('*.uwvm-ljc'))
        if len(files) != 1:
            raise RuntimeError(f'{policy}: expected one freshly emitted object, got {len(files)}')
        obj = args.output / (policy + '.o')
        obj.write_bytes(decode_object(files[0].read_bytes(), args.ros))
        symbols = subprocess.check_output([str(args.llvm / 'llvm-nm'), '--defined-only', str(obj)], text=True)
        leaf = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_0)$', symbols, re.M)
        if len(leaf) != 1:
            raise RuntimeError('missing actual translated memory leaf')
        dis = subprocess.check_output([str(args.llvm / 'llvm-objdump'), '-dr', '--disassemble-symbols=' + leaf[0], str(obj)], text=True)
        (args.output / (policy + '.assembly.txt')).write_text(dis)
        if 'file format elf64-x86-64' in dis:
            call_pattern = r'\bcallq?\s'
        elif 'file format elf64-littleaarch64' in dis:
            call_pattern = r'\bbl[r]?\s'
        elif 'file format elf64-littleriscv' in dis:
            # Expose the link destination: jalr zero is a return/jump, whereas
            # jalr ra (including the compressed form) is an ABI call. Counting
            # the default jalr alias would incorrectly include other transfers.
            dis = subprocess.check_output([str(args.llvm / 'llvm-objdump'), '-dr', '-M', 'no-aliases',
                                           '--disassemble-symbols=' + leaf[0], str(obj)], text=True)
            (args.output / (policy + '.assembly.txt')).write_text(dis)
            call_pattern = r'\b(?:jal|jalr)\s+ra\s*,|\bc\.jalr\s'
        else:
            raise RuntimeError('unsupported object ISA in this explicit assembly assertion')
        calls = len(re.findall(call_pattern, dis))
        expected_calls = 2 if policy == 'instruction' else 0
        if calls != expected_calls:
            raise RuntimeError(f'{policy}: expected {expected_calls} frame-maintenance calls, got {calls}')
        sections = subprocess.check_output([str(args.llvm / 'llvm-objdump'), '-h', str(obj)], text=True)
        (args.output / (policy + '.sections.txt')).write_text(sections)
        if policy == 'unwind' and '.eh_frame' not in sections:
            raise RuntimeError('native unwind object has no registered CFI section')
        rows.append(dict(policy=policy, leaf=leaf[0], calls=calls, unwind_cfi='.eh_frame' in sections,
                         object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(), command=command))
    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('PASS actual new-syntax JIT objects: instruction has 2 frame calls; unwind has 0 and retains CFI')


if __name__ == '__main__':
    main()
