#!/usr/bin/env python3
"""Prepare actual Windows VM debugger fixtures inside the qualified Linux cgroup.

The caller must pass the Docker scope name used for the Windows cross-build.
QEMU runs in the separate, equally bounded Linux main cgroup. This script
checks the host cgroup view even
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
    if str(os.getpid()) not in (cg / 'cgroup.procs').read_text().splitlines():
        raise RuntimeError('fixture generator is outside the qualified Docker cgroup')
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
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--rustc', type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    cgroup = check_cgroup(args.docker_scope)
    args.out.mkdir(parents=True, exist_ok=False)
    source_meta = {}
    for language, source_file, callee in (
        ('c', 'debug_source_c.c', 'debug_source_c'),
        ('cpp', 'debug_source_cpp.cc', 'debug_source_cpp'),
    ):
        source = args.source_root / 'test/0017.runtime/fixtures' / source_file
        for dwarf in (4, 5):
            stem = f'source-{language}-dwarf{dwarf}'
            obj = args.out / (stem + '.o')
            source_wasm = args.out / (stem + '.wasm')
            run(args.clang, '--target=wasm32-unknown-unknown', f'-gdwarf-{dwarf}', '-g',
                '-O0', '-nostdlib', '-c', source, '-o', obj)
            run(args.wasm_ld, '--no-entry', '--export-all', obj, '-o', source_wasm)
            run(args.wasm_tools, 'validate', source_wasm)
            meta = {
                'entry': export_function_index(source_wasm, '_start'),
                'callee': export_function_index(source_wasm, callee),
            }
            (args.out / (stem + '.meta.json')).write_text(json.dumps(meta, sort_keys=True) + '\n')
            source_meta[stem] = meta
    rust = args.out / 'source-rust-dwarf5.wasm'
    run(args.rustc, '--target', 'wasm32-unknown-unknown', '-C', 'panic=abort',
        '-C', 'debuginfo=2', '-C', 'opt-level=0',
        args.source_root / 'test/0017.runtime/fixtures/debug_source_rust.rs', '-o', rust)
    run(args.wasm_tools, 'validate', rust)
    rust_meta = {
        'entry': export_function_index(rust, '_start'),
        'callee': export_function_index(rust, 'debug_source_rust'),
    }
    (args.out / 'source-rust-dwarf5.meta.json').write_text(json.dumps(rust_meta, sort_keys=True) + '\n')
    source_meta['source-rust-dwarf5'] = rust_meta

    native_step_wat = args.out / 'native-step-fixture.wat'
    native_step_wat.write_text('(module (func (export "_start") (loop $again br $again)))\n')
    native_step_wasm = args.out / 'native-step-fixture.wasm'
    run(args.wasm_tools, 'parse', native_step_wat, '-o', native_step_wasm)
    run(args.wasm_tools, 'validate', native_step_wasm)

    typed_ref_wat = args.out / 'core3-typed-ref.wat'
    typed_ref_wat.write_text('''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        ref.func $f local.set $slot
        i32.const 41 local.get $slot call_ref $t
        i32.const 42 i32.ne if unreachable end))\n''')
    typed_ref_wasm = args.out / 'core3-typed-ref.wasm'
    run(args.wasm_tools, 'parse', typed_ref_wat, '-o', typed_ref_wasm)
    run(args.wasm_tools, 'validate', '--features', 'all', typed_ref_wasm)

    throw_ref_wat = args.out / 'core3-throw-ref-bottom.wat'
    throw_ref_wat.write_text('''(module
      (func (export "_start")
        block br 0 ref.as_non_null throw_ref end))\n''')
    throw_ref_wasm = args.out / 'core3-throw-ref-bottom.wasm'
    run(args.wasm_tools, 'parse', throw_ref_wat, '-o', throw_ref_wasm)
    run(args.wasm_tools, 'validate', '--features', 'all', throw_ref_wasm)

    branch_null_wat = args.out / 'core3-br-on-null-bottom.wat'
    branch_null_wat.write_text('''(module
      (func (export "_start")
        block br 0 br_on_null 0 throw_ref end))\n''')
    branch_null_wasm = args.out / 'core3-br-on-null-bottom.wasm'
    run(args.wasm_tools, 'parse', branch_null_wat, '-o', branch_null_wasm)
    run(args.wasm_tools, 'validate', '--features', 'all', branch_null_wasm)
    for core3_wasm in (typed_ref_wasm, throw_ref_wasm, branch_null_wasm):
        run(args.wasmtime, 'run', '-W', 'function-references=y', '-W', 'exceptions=y',
            core3_wasm)

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
        'wasmtime_version': subprocess.check_output([str(args.wasmtime), '--version'], text=True).strip(),
        'wasmtime_sha256': hashlib.sha256(args.wasmtime.read_bytes()).hexdigest(),
        'wasmtime_reference_passed': [typed_ref_wasm.name, throw_ref_wasm.name,
                                      branch_null_wasm.name],
        'rustc_version': subprocess.check_output([str(args.rustc), '--version'], text=True).strip(),
    }
    (args.out / 'fixture-summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')
    print(json.dumps(summary, sort_keys=True))


if __name__ == '__main__':
    main()
