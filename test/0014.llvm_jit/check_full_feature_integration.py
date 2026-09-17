#!/usr/bin/env python3
"""ROS full-only execution: Wasm2 features, imported vector ABI/table aliases, and DataCount.

Usage: python3 check_full_feature_integration.py /absolute/path/to/uwvm
All generated modules and logs stay in /tmp (or --out). Requires wat2wasm.
"""
import argparse
import json
from pathlib import Path
import resource
import subprocess
import tempfile


PROVIDER = """(module
  (type $pair (func (param v128) (result v128 i32)))
  (table (export "table") 1 8 funcref)
  (memory (export "memory") 1)
  (global (export "global") (mut v128) (v128.const i32x4 1 2 3 4))
  (func $one (type $pair) (param v128) (result v128 i32)
    local.get 0 v128.const i32x4 1 1 1 1 i32x4.add i32.const 1)
  (func $two (export "two") (type $pair) (param v128) (result v128 i32)
    local.get 0 v128.const i32x4 2 2 2 2 i32x4.add i32.const 2)
  (elem (i32.const 0) $one)
  (func (export "probe") (param v128 i32) (result v128 i32)
    local.get 0 local.get 1 call_indirect (type $pair))
  (func (export "reset") i32.const 0 ref.func $one table.set))"""

ALIAS = """(module
  (type $pair (func (param v128) (result v128 i32)))
  (import "provider" "table" (table 1 8 funcref))
  (export "table" (table 0))
  (func (export "probe") (param v128 i32) (result v128 i32)
    local.get 0 local.get 1 call_indirect (type $pair)))"""

CONSUMER = """(module
  (type $pair (func (param v128) (result v128 i32)))
  (import "provider" "two" (func $two (type $pair)))
  (import "provider" "probe" (func $probe (param v128 i32) (result v128 i32)))
  (import "provider" "reset" (func $reset))
  (import "alias" "probe" (func $alias_probe (param v128 i32) (result v128 i32)))
  (import "alias" "table" (table $a 1 8 funcref))
  (import "provider" "table" (table $b 1 8 funcref))
  (import "provider" "memory" (memory 1))
  (import "provider" "global" (global $g (mut v128)))
  (elem declare func $two)
  (func $check (param v128 i32 i32)
    local.get 1 local.get 2 i32.ne if unreachable end
    local.get 0 local.get 2 i32x4.splat i32x4.eq i8x16.all_true i32.eqz if unreachable end)
  (func (export "_start")
    v128.const i32x4 0 0 0 0 call $two i32.const 2 call $check
    v128.const i32x4 0 0 0 0 i32.const 0 call $probe i32.const 1 call $check
    ;; Mutation through one alias must update every module's borrowed native target view.
    i32.const 0 ref.func $two table.set $a
    v128.const i32x4 0 0 0 0 i32.const 0 call $probe i32.const 2 call $check
    v128.const i32x4 0 0 0 0 i32.const 0 call $alias_probe i32.const 2 call $check
    v128.const i32x4 0 0 0 0 i32.const 0 call_indirect $b (type $pair) i32.const 2 call $check
    call $reset
    v128.const i32x4 0 0 0 0 i32.const 0 call_indirect $a (type $pair) i32.const 1 call $check
    ref.func $two i32.const 3 table.grow $a i32.const 1 i32.ne if unreachable end
    v128.const i32x4 0 0 0 0 i32.const 3 call $probe i32.const 2 call $check
    v128.const i32x4 0 0 0 0 i32.const 3 call $alias_probe i32.const 2 call $check
    i32.const 65520 global.get $g v128.store
    i32.const 65520 v128.load v128.const i32x4 1 2 3 4 i32x4.eq i8x16.all_true i32.eqz if unreachable end
    v128.const i32x4 5 6 7 8 global.set $g
    global.get $g v128.const i32x4 5 6 7 8 i32x4.eq i8x16.all_true i32.eqz if unreachable end))"""


def without_datacount(wasm):
    result = bytearray(wasm[:8])
    cursor = 8
    removed = 0
    while cursor < len(wasm):
        begin = cursor
        section = wasm[cursor]
        cursor += 1
        size = shift = 0
        while True:
            byte = wasm[cursor]
            cursor += 1
            size |= (byte & 127) << shift
            shift += 7
            if byte < 128:
                break
        cursor += size
        if section == 12:
            removed += 1
        else:
            result.extend(wasm[begin:cursor])
    assert removed == 1
    return result


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uwvm", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--product", choices=("full", "ros"), default="ros")
    args = parser.parse_args()
    out = args.out or Path(tempfile.mkdtemp(prefix="uwvm-ros-full-features-", dir="/tmp"))
    out.mkdir(parents=True, exist_ok=True)
    wat_dir = Path(__file__).parent / "wat"

    def compile_wat(name, wat):
        wasm = out / f"{name}.wasm"
        subprocess.run(["wat2wasm", "-", "-o", str(wasm)], input=wat.encode(), capture_output=True, check=True)
        return wasm

    provider = compile_wat("provider", PROVIDER)
    alias = compile_wat("alias", ALIAS)
    consumer = compile_wat("consumer", CONSUMER)
    preloads = ["--wasm-preload-library", str(provider), "provider", "--wasm-preload-library", str(alias), "alias"]
    cases = [(consumer, preloads, None)]
    for name in ("simd_direct_vector_abi", "simd_table_mutation", "wasm2_multivalue_typed", "wasm2_multiple_tables", "wasm2_bulk_memory_native"):
        wasm = compile_wat(name, (wat_dir / f"{name}.wat").read_text())
        cases.append((wasm, [], None))
    missing = out / "missing-datacount.wasm"
    missing.write_bytes(without_datacount((out / "wasm2_bulk_memory_native.wasm").read_bytes()))
    cases.append((missing, [], b"DataCount"))
    empty = compile_wat("empty-datacount", '(module (func (export "_start")))')
    # Section 12, payload length 1, count 0: legal even without any data segments.
    empty_bytes = empty.read_bytes()
    code_section = b"\x0a\x04\x01\x02\x00\x0b"
    assert empty_bytes.endswith(code_section)
    empty.write_bytes(empty_bytes[:-len(code_section)] + b"\x0c\x01\x00" + code_section)
    cases.append((empty, [], None))
    for name in ("table_get", "table_init", "table_copy", "memory_init", "memory_copy"):
        wasm = compile_wat(f"trap-{name}", (wat_dir / "trap_matrix" / f"wasm2_{name}_oob.wat").read_text())
        cases.append((wasm, [], b"out of bounds"))
    results = []
    modes = {policy: ["-Raot", "-Rct", "0", "-Rllvm-full-policy", policy, "-Rllvm-cache-path", "disable"]
             for policy in ("pb-o3", "pb-o2", "pb-o1", "debug")}
    modes["uwvm-int"] = (["-Rcm", "full", "-Rcc", "int"] if args.product == "full" else ["-Rint"]) + ["-Rct", "0"]
    for mode, options in modes.items():
        for wasm, extra, diagnostic in cases:
            command = [str(args.uwvm.resolve()), *options, "--wasm-feature-wasm2", *extra, "--run", str(wasm)]
            result = subprocess.run(command, capture_output=True, timeout=60)
            log = result.stdout + result.stderr
            (out / f"{wasm.stem}-{mode}.log").write_bytes(log)
            passed = result.returncode == 0 if diagnostic is None else result.returncode != 0 and diagnostic.lower() in log.lower()
            if not passed:
                raise RuntimeError(f"{wasm.stem}/{mode}: {log.decode(errors='replace')}")
            results.append({"case": wasm.stem, "mode": mode, "pass": True})
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"PASS {len(results)} full-feature/import-alias/DataCount checks; evidence: {out}")


if __name__ == "__main__":
    main()
