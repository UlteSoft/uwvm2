#!/usr/bin/env python3
"""Run exact-width SIMD accesses against real guard pages, under all ROS full LLVM optimization policies.

Usage: python3 check_simd_memory_boundaries.py /absolute/path/to/uwvm [--out /tmp/directory]
All generated Wasm, logs, and result metadata stay in the temporary output directory.
Run again with mmap, single-thread-alloc, and multi-thread-alloc builds.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import resource
from pathlib import Path
import subprocess
import tempfile


def vector(data):
    return "v128.const i8x16 " + " ".join(map(str, data))


def module(body, initial=1):
    data = "".join(f"\\{x:02x}" for x in range(0x80, 0xa0))
    return f'(module (memory {initial}) (data (i32.const 65504) "{data}") (func (export "_start") {body}))'


def fixtures():
    yield "simd-direct-vector-abi", (Path(__file__).parent / "wat/simd_direct_vector_abi.wat").read_text(), False
    old = bytes(range(16))
    loads = [("v128.load", 16, "raw", 1, False)]
    for bits in (8, 16, 32):
        for sign in (True, False):
            loads.append((f"v128.load{bits}x{64 // bits}_{'s' if sign else 'u'}", 8, "extend", bits // 8, sign))
    for bits in (8, 16, 32, 64):
        loads.append((f"v128.load{bits}_splat", bits // 8, "splat", bits // 8, False))
        loads.append((f"v128.load{bits}_lane", bits // 8, "lane", bits // 8, False))
    for bits in (32, 64):
        loads.append((f"v128.load{bits}_zero", bits // 8, "zero", bits // 8, False))
    for op, width, shape, lane_width, signed in loads:
        address = 65536 - width
        memory = bytes(range(0xa0 - width, 0xa0))
        lane = 16 // lane_width - 1
        prefix = vector(old) if shape == "lane" else ""
        suffix = str(lane) if shape == "lane" else ""
        if shape == "extend":
            expected = b"".join(int.from_bytes(memory[i:i + lane_width], "little", signed=signed)
                                .to_bytes(lane_width * 2, "little", signed=signed) for i in range(0, 8, lane_width))
        elif shape == "splat":
            expected = memory * (16 // width)
        elif shape == "zero":
            expected = memory + bytes(16 - width)
        elif shape == "lane":
            expected = old[:16 - width] + memory
        else:
            expected = memory
        for align in (1, width):
            body = (f"i32.const {address} {prefix} {op} align={align} {suffix} "
                    f"{vector(expected)} i8x16.eq i8x16.all_true i32.eqz if unreachable end")
            yield f"{op}-last-valid-align{align}", module(body), False
        for dynamic, offset in ((address + 1, 0), (65536, 0), (0xffffffff, 0),
                                (0xffffffff, 1), (0xffffffff, 0xffffffff), (0x80000000, 0x80000000)):
            body = f"i32.const {dynamic} {prefix} {op} offset={offset} align=1 {suffix} drop"
            yield f"{op}-trap-{dynamic:x}-{offset:x}", module(body), True
    for width in (1, 2, 4, 8, 16):
        op = "v128.store" if width == 16 else f"v128.store{width * 8}_lane"
        suffix = "" if width == 16 else str(16 // width - 1)
        address = 65536 - width
        body = f"i32.const {address} {vector(old)} {op} align=1 {suffix} "
        for i in range(width):
            body += f"i32.const {address + i} i32.load8_u i32.const {16 - width + i} i32.ne if unreachable end "
        # Also check the preceding byte: a narrow store must not clobber neighbouring memory.
        body += f"i32.const {address - 1} i32.load8_u i32.const {0x9f - width} i32.ne if unreachable end"
        yield f"{op}-last-valid", module(body), False
        for dynamic, offset in ((address + 1, 0), (65536, 0), (0xffffffff, 1), (0xffffffff, 0xffffffff)):
            yield f"{op}-trap-{dynamic:x}-{offset:x}", module(
                f"i32.const {dynamic} {vector(old)} {op} offset={offset} align=1 {suffix}"), True
    # memory.grow must not leave a frozen allocator base or stale byte length in generated code.
    yield "grow-reload-base", module(
        f"i32.const 1 memory.grow i32.const 1 i32.ne if unreachable end "
        f"i32.const 131056 {vector(old)} v128.store i32.const 131056 v128.load "
        f"{vector(old)} i8x16.eq i8x16.all_true i32.eqz if unreachable end"), False


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uwvm", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--jobs", type=int, default=1, help="concurrent test processes (bound externally by CPU/memory limits)")
    args = parser.parse_args()
    out = args.out or Path(tempfile.mkdtemp(prefix="uwvm-simd-memory-", dir="/tmp"))
    out.mkdir(parents=True, exist_ok=True)
    modes = {
        "full-o3": ["-Rllvm-full-policy", "pb-o3"],
        "full-debug": ["-Rllvm-full-policy", "debug"],
        "full-o1": ["-Rllvm-full-policy", "pb-o1"],
        "full-o2": ["-Rllvm-full-policy", "pb-o2"],
    }
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    tests = []
    for name, wat, trap in fixtures():
        wasm = out / f"{name}.wasm"
        subprocess.run(["wat2wasm", "-", "-o", str(wasm)], input=wat.encode(), capture_output=True, check=True)
        for mode, options in modes.items():
            command = [str(args.uwvm.resolve()), "-Raot", "-Rct", "0", *options,
                       "-Rllvm-call-stack", "instruction", "-Rllvm-cache-path", "disable", "--wasm-feature-wasm2", "--", str(wasm)]
            tests.append((name, mode, trap, command))
    def run_test(test):
        name, mode, trap, command = test
        result = subprocess.run(command, capture_output=True, timeout=30)
        log = result.stdout + result.stderr
        (out / f"{name}-{mode}.log").write_bytes(log)
        okay = (result.returncode != 0 and b"memory access out of bounds" in log) if trap else result.returncode == 0
        if not okay:
            print(log.decode(errors="replace"))
            raise RuntimeError(f"FAIL {name} / {mode}; evidence: {out}")
        return {"name": name, "mode": mode, "trap": trap, "pass": okay, "returncode": result.returncode}
    with ThreadPoolExecutor(max_workers=args.jobs) as workers:
        results = list(workers.map(run_test, tests))
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"PASS {len(results)} real SIMD memory boundary/grow checks; evidence: {out}")


if __name__ == "__main__":
    main()
