#!/usr/bin/env python3
"""Prepare actual Windows VM debugger fixtures inside the qualified Linux cgroup.

The caller must pass the Docker scope name of the same cgroup used for the
Windows cross-build and QEMU run. This script checks the host cgroup view even
when invoked by ``docker exec ... chroot /host`` (whose /sys is the host mount).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def wasm_uleb(data: bytes, at: int, end: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        if at >= end:
            raise ValueError('truncated Wasm LEB')
        octet = data[at]
        at += 1
        value |= (octet & 127) << shift
        if not octet & 128:
            return value, at
    raise ValueError('overlong Wasm LEB')


def export_function_index(path: Path, name: str) -> int:
    data = path.read_bytes()
    if data[:8] != b'\0asm\x01\0\0\0':
        raise ValueError(f'{path}: invalid Wasm header')
    at = 8
    while at < len(data):
        section = data[at]
        at += 1
        size, at = wasm_uleb(data, at, len(data))
        end = at + size
        if end > len(data):
            raise ValueError(f'{path}: truncated section')
        if section == 7:
            count, at = wasm_uleb(data, at, end)
            for _ in range(count):
                length, at = wasm_uleb(data, at, end)
                if length > end - at:
                    raise ValueError(f'{path}: invalid export name')
                label = data[at:at + length]
                at += length
                if at >= end:
                    raise ValueError(f'{path}: truncated export')
                kind = data[at]
                at += 1
                index, at = wasm_uleb(data, at, end)
                if label == name.encode() and kind == 0:
                    return index
            break
        at = end
    raise ValueError(f'{path}: missing exported function {name}')


def check_cgroup(scope: str) -> dict[str, object]:
    if not re.fullmatch(r'docker-[0-9a-f]{64}\.scope', scope):
        raise ValueError('expected exact Docker scope name')
    cg = Path('/sys/fs/cgroup/system.slice') / scope
    memory = (cg / 'memory.max').read_text().strip()
    swap = (cg / 'memory.swap.max').read_text().strip()
    cpus = sorted(os.sched_getaffinity(0))
    expected_cpus = sorted([0, 2, 4, 6, *range(16, 32)])
    if memory != '68719476736' or swap != '0' or cpus != expected_cpus:
        raise RuntimeError(f'wrong cgroup: memory={memory}, swap={swap}, cpus={cpus}')
    return {'scope': scope, 'memory_max': memory, 'swap_max': swap, 'cpus': cpus}


def run(*args: object) -> None:
    subprocess.run([str(arg) for arg in args], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--wasm-ld', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    cgroup = check_cgroup(args.docker_scope)
    args.out.mkdir(parents=True, exist_ok=False)
    source = args.source_root / 'test/0017.runtime/fixtures/debug_source_c.c'
    obj = args.out / 'source-c-dwarf4.o'
    source_wasm = args.out / 'source-c-dwarf4.wasm'
    run(args.clang, '--target=wasm32-unknown-unknown', '-gdwarf-4', '-g', '-O0', '-nostdlib', '-c', source, '-o', obj)
    run(args.wasm_ld, '--no-entry', '--export-all', obj, '-o', source_wasm)
    run(args.wasm_tools, 'validate', source_wasm)
    source_meta = {
        'entry': export_function_index(source_wasm, '_start'),
        'callee': export_function_index(source_wasm, 'debug_source_c'),
    }
    (args.out / 'source-c-dwarf4.meta.json').write_text(json.dumps(source_meta, sort_keys=True) + '\n')

    wat = args.out / 'replace.wat'
    wat.write_text('''(module
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (memory 1)
  (data (i32.const 128) "replaced-value=3\\0a")
  (func $value (result i32) i32.const 51)
  (func (export "_start")
    i32.const 143 call $value i32.store8
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 17 i32.store
    i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop))
''')
    replacement_wasm = args.out / 'replace.wasm'
    run(args.wasm_tools, 'parse', wat, '-o', replacement_wasm)
    run(args.wasm_tools, 'validate', replacement_wasm)
    if export_function_index(replacement_wasm, '_start') != 2:
        raise RuntimeError('replacement fixture public function index changed')
    bodies = {
        'good4': b'\x00\x41\x34\x0b',
        'good5': b'\x00\x41\x35\x0b',
        'malformed': b'\x00\xff\x0b',
        'wrong_result': b'\x00\x42\x01\x0b',
    }
    for name, body in bodies.items():
        (args.out / f'{name}.bin').write_bytes(body)
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(args.out.iterdir()) if p.is_file()}
    summary = {
        'cgroup': cgroup,
        'source_fixture': source_meta,
        'files_sha256': files,
        'clang_version': subprocess.check_output([str(args.clang), '--version'], text=True).splitlines()[0],
        'wasm_ld_version': subprocess.check_output([str(args.wasm_ld), '--version'], text=True).strip(),
        'wasm_tools_version': subprocess.check_output([str(args.wasm_tools), '--version'], text=True).strip(),
    }
    (args.out / 'fixture-summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
