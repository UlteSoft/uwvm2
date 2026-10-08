#!/usr/bin/env python3
"""Inspect actual optimized x86-64 JIT code for dynamic Core 3 i31 conversions."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess

from check_wasm3_native_frame_codegen import decode_object


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, log):
    result = subprocess.run([str(x) for x in command], capture_output=True, timeout=120)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{command}: exit={result.returncode}: {log.read_text(errors="replace")[-700:]}')


def function_body(assembly, ordinal):
    pattern = re.compile(r'^([0-9a-f]+) <[^>]+_func_' + str(ordinal) + r'>:\n', re.MULTILINE)
    match = pattern.search(assembly)
    if not match:
        raise AssertionError(f'missing optimized Wasm function {ordinal}')
    following = re.search(r'^\n[0-9a-f]+ <', assembly[match.end():], re.MULTILINE)
    return assembly[match.end():match.end() + following.start()] if following else assembly[match.end():]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--llvm-objdump', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    wat = root / 'test/0017.runtime/fixtures/gc_i31_codegen.wat'
    wasm = args.out / 'gc_i31_codegen.wasm'
    run([args.wasm_tools, 'parse', wat, '-o', wasm], args.out / 'parse.log')
    run([args.wasm_tools, 'validate', '--features', 'all', wasm], args.out / 'validate.log')
    run([args.wasmtime, '-C', 'cache=n', '-W', 'gc=y', wasm], args.out / 'wasmtime.log')
    rows = []
    for policy in ('instruction', 'unwind'):
        directory = args.out / policy
        directory.mkdir()
        cache = directory / 'cache'
        cache.mkdir()
        command = [args.uwvm, '-Raot'] if args.ros else [args.uwvm, '-Rcc', 'jit', '-Rcm', 'full']
        command += ['-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'path', cache,
                    '-Rclog', 'file', directory / 'compile.log', '-WFE-gc', '--run', wasm]
        run(command, directory / 'run.log')
        objects = list(cache.rglob('*.uwvm-ljc'))
        if len(objects) != 1:
            raise AssertionError(f'{policy}: expected one actual signed JIT cache object, got {len(objects)}')
        native = directory / 'native.o'
        native.write_bytes(decode_object(objects[0].read_bytes(), args.ros))
        assembly = subprocess.check_output([str(args.llvm_objdump), '-dr', str(native)], text=True)
        (directory / 'native.s').write_text(assembly)
        signed = function_body(assembly, 0)
        unsigned = function_body(assembly, 1)
        if not re.search(r'\b(?:leal|addl)\b', signed) or not re.search(r'\bsarl\b', signed):
            raise AssertionError(f'{policy}: signed i31 hot path is missing direct sign extension')
        if not re.search(r'\bandl\b', unsigned):
            raise AssertionError(f'{policy}: unsigned i31 hot path is missing direct 31-bit mask')
        for name, body in (('signed', signed), ('unsigned', unsigned)):
            calls = re.search(r'\bcallq?\b', body)
            if policy == 'unwind' and (calls or re.search(r'\b(?:jmpq?|je|jne)\b', body)):
                raise AssertionError(f'{policy}: {name} i31 hot path contains a call or branch')
            if policy == 'instruction' and not calls:
                raise AssertionError(f'{policy}: {name} lacks the requested instruction stack observation')
            if not re.search(r'\bretq\b', body):
                raise AssertionError(f'{policy}: {name} i31 hot path has no return')
        rows.append({'policy': policy, 'object_sha256': sha256(native),
                     'signed_instructions': len(re.findall(r'^\s*[0-9a-f]+:', signed, re.MULTILINE)),
                     'unsigned_instructions': len(re.findall(r'^\s*[0-9a-f]+:', unsigned, re.MULTILINE))})
    summary = {'passed': True, 'rows': rows, 'binary_sha256': sha256(args.uwvm),
               'fixture_sha256': sha256(wat), 'runner_sha256': sha256(Path(__file__))}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS Core 3 i31 native JIT codegen: instruction/unwind, signed/unsigned')


if __name__ == '__main__':
    main()
