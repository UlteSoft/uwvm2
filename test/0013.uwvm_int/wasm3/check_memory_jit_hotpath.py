#!/usr/bin/env python3
"""Inspect objects produced and executed by the real full-JIT benchmark.

The independent read region must survive optimization. Instruction frames cost
two calls per guest function; native unwind must emit none. This native x86-64
check does not substitute for target-specific QEMU execution or timing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'test/0014.llvm_jit'))
from check_wasm3_native_frame_codegen import decode_object


def operands(text):
    result, begin, depth = [], 0, 0
    for index, char in enumerate(text):
        depth += (char == '(') - (char == ')')
        if char == ',' and depth == 0:
            result.append(text[begin:index].strip())
            begin = index + 1
    result.append(text[begin:].strip())
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('build', type=Path, help='directory containing baseline/current')
    p.add_argument('fixtures', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--llvm', type=Path, required=True)
    p.add_argument('--ros', action='store_true')
    args = p.parse_args()
    subprocess.run(['bash', str(ROOT / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for policy in ('instruction', 'unwind'):
        for kernel in ('scalar-aligned', 'scalar-unaligned', 'simd-aligned',
                       'load-scalar-aligned', 'load-scalar-unaligned', 'load-simd-aligned'):
            for version, syntax in (('baseline', 'legacy'), ('current', 'legacy'), ('current', 'indexed')):
                out = args.output / f'{policy}-{kernel}-{version}-{syntax}'
                out.mkdir()
                cache = out / 'cache'
                cache.mkdir(mode=0o700)
                fixture = args.fixtures / (kernel + '-' + syntax + '.wasm')
                command = [str((args.build / version).resolve()), str(fixture.resolve()), '20003', '1']
                env = dict(os.environ, UWVM_TEST_JIT_CALL_STACK=policy,
                           UWVM_TEST_JIT_OBJECT_CACHE_DIR=str(cache.resolve()))
                run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
                (out / 'execution.log').write_text(run.stdout + run.stderr)
                if run.returncode:
                    raise RuntimeError(f'{out.name}: execution/checksum failed: {run.returncode}')
                objects = list(cache.rglob('*.uwvm-ljc'))
                if len(objects) != 1:
                    raise RuntimeError(f'{out.name}: expected one freshly emitted cache object, got {len(objects)}')
                obj = out / 'guest.o'
                obj.write_bytes(decode_object(objects[0].read_bytes(), args.ros))
                symbols = subprocess.check_output([str(args.llvm / 'llvm-nm'), '--defined-only', str(obj)], text=True)
                functions = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_0)$', symbols, re.M)
                if len(functions) != 1:
                    raise RuntimeError('missing actual guest loop function')
                dis = subprocess.check_output([str(args.llvm / 'llvm-objdump'), '-dr', '--no-show-raw-insn',
                    '--disassemble-symbols=' + functions[0], str(obj)], text=True)
                (out / 'assembly.txt').write_text(dis)
                if 'file format elf64-x86-64' not in dis:
                    raise RuntimeError('this explicit assembly assertion requires x86-64 ELF')
                instructions = []
                reads, writes, calls = [], [], []
                for line in dis.splitlines():
                    match = re.match(r'\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\s*(.*)', line)
                    if not match:
                        continue
                    op, text = match.groups()
                    text = text.split('#', 1)[0].strip()
                    instructions.append(op + ' ' + text)
                    if op in ('call', 'callq'):
                        calls.append(text)
                    if op in ('lock', 'mfence', 'sfence', 'lfence'):
                        raise RuntimeError('unexpected lock/CPU fence in guest memory loop')
                    fields = operands(text)
                    if op.startswith(('lea', 'nop')) or len(fields) < 2:
                        continue
                    memory = lambda x: '(' in x and not re.search(r'%(?:rsp|rbp|rip)\b', x)
                    if any(memory(x) for x in fields[:-1]):
                        reads.append(op + ' ' + text)
                    if memory(fields[-1]):
                        writes.append(op + ' ' + text)
                if len(calls) != (2 if policy == 'instruction' else 0):
                    raise RuntimeError(f'{out.name}: wrong instruction/unwind frame-call count: {len(calls)}')
                if not writes:
                    raise RuntimeError('optimized benchmark lost all memory stores')
                if kernel.startswith('load-') and len(reads) < (2 if 'unaligned' in kernel else 1):
                    raise RuntimeError('independent data reads did not survive optimization')
                sections = subprocess.check_output([str(args.llvm / 'llvm-objdump'), '-h', str(obj)], text=True)
                if policy == 'unwind' and '.eh_frame' not in sections:
                    raise RuntimeError('native unwind object lost its CFI')
                rows.append(dict(policy=policy, kernel=kernel, version=version, syntax=syntax,
                    command=command, calls=calls, memory_reads=reads, memory_writes=writes,
                    instructions=len(instructions), object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
                    fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
                    binary_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()))
                (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
                print('PASS actual JIT object', out.name, flush=True)


if __name__ == '__main__':
    main()
