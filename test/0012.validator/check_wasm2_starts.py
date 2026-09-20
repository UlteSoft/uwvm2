#!/usr/bin/env python3
"""Full-mode module starts: order, exactly-once effects, imported starts, and traps."""
import argparse
import json
from pathlib import Path
import resource
import subprocess
import tempfile

PROVIDER = """(module
  (global $g (export "g") (mut i32) (i32.const 0))
  (memory (export "memory") 1)
  (func $start
    global.get $g if unreachable end
    i32.const 1 global.set $g
    i32.const 0 i32.const 11 i32.store)
  (func (export "increment") global.get $g i32.const 1 i32.add global.set $g)
  (start $start))"""
SECOND = """(module
  (import "provider" "g" (global $g (mut i32)))
  (import "provider" "memory" (memory 1))
  (func $start
    global.get $g i32.const 1 i32.ne if unreachable end
    i32.const 0 i32.load i32.const 11 i32.ne if unreachable end
    i32.const 2 global.set $g
    i32.const 0 i32.const 22 i32.store)
  (start $start)
  (func (export "_start") unreachable))"""
CHECK = """(module
  (import "provider" "g" (global $g (mut i32)))
  (import "provider" "memory" (memory 1))
  (func (export "_start")
    global.get $g i32.const EXPECT i32.ne if unreachable end
    i32.const 0 i32.load i32.const MEMORY i32.ne if unreachable end))"""

def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--full", type=Path, required=True)
    p.add_argument("--ros", type=Path, required=True)
    p.add_argument("--out", type=Path)
    p.add_argument("--product", choices=("full", "ros", "both"), default="both")
    p.add_argument("--all-modes", action="store_true")
    a = p.parse_args()
    out = a.out or Path(tempfile.mkdtemp(prefix="uwvm-module-starts-", dir="/tmp"))
    out.mkdir(parents=True, exist_ok=True)
    modules = dict(provider=PROVIDER, second=SECOND,
        check_one=CHECK.replace("EXPECT", "1").replace("MEMORY", "11"),
        check_two=CHECK.replace("EXPECT", "2").replace("MEMORY", "22"),
        check_import=CHECK.replace("EXPECT", "2").replace("MEMORY", "11"),
        no_start='(module (func (export "_start") unreachable) (func (export "main") unreachable))',
        imported='(module (import "provider" "increment" (func $inc)) (start $inc))',
        empty='(module (func (export "_start")))',
        trap='(module (func $start unreachable) (start $start))',
        trap_provider='(module (func (export "start") unreachable))',
        trap_import='(module (import "trap_provider" "start" (func $s)) (start $s))',
        before_data='''(module (memory (export "memory") 1)
          (func $s i32.const 0 i32.load8_u if unreachable end) (start $s))''',
        after_data='''(module (import "before_data" "memory" (memory 1))
          (data (i32.const 0) "X")
          (func $s i32.const 0 i32.load8_u i32.const 88 i32.ne if unreachable end) (start $s))''',
        before_elem='''(module (table (export "table") 1 funcref)
          (func $s i32.const 0 table.get ref.is_null i32.eqz if unreachable end) (start $s))''',
        after_elem='''(module (import "before_elem" "table" (table 1 funcref))
          (func $f) (elem (i32.const 0) $f)
          (func $s i32.const 0 table.get ref.is_null if unreachable end) (start $s))''',
        before_growth='''(module (memory (export "memory") 1 2)
          (func $s i32.const 1 memory.grow i32.const 1 i32.ne if unreachable end) (start $s))''',
        after_growth='''(module (import "before_growth" "memory" (memory 1 2))
          (data (i32.const 65536) "X")
          (func $s i32.const 65536 i32.load8_u i32.const 88 i32.ne if unreachable end) (start $s))''')
    for name, wat in modules.items():
        subprocess.run(["wat2wasm", "-", "-o", str(out / (name + ".wasm"))],
                       input=wat.encode(), capture_output=True, check=True)
    cases = [
        ("start-once", "check_one", ["provider"], False),
        ("preload-order", "check_two", ["provider", "second"], False),
        ("exports-not-starts", "check_one", ["provider", "no_start"], False),
        ("imported-library-start", "check_import", ["provider", "imported"], False),
        ("preload-start-trap", "empty", ["trap"], True),
        ("main-start-trap", "trap", [], True),
        ("imported-main-start-trap", "trap_import", ["trap_provider"], True),
        ("imported-preload-start-trap", "empty", ["trap_provider", "trap_import"], True),
    ]
    for suffix in ("data", "elem", "growth"):
        cases.append(("preload-segment-start-order-" + suffix, "empty", ["before_" + suffix, "after_" + suffix], False))
        cases.append(("main-segment-start-order-" + suffix, "after_" + suffix, ["before_" + suffix], False))
    results = []
    for product in ("full", "ros"):
        if a.product != "both" and product != a.product:
            continue
        profiles = [(backend, ["-Rcm", "full", "-Rcc", backend] if product == "full"
                     else ["-Rint" if backend == "int" else "-Raot"]) for backend in ("int", "jit")]
        if product == "full" and a.all_modes:
            profiles += [("int-lazy", ["-Rcm", "lazy", "-Rcc", "int"]),
                         ("jit-lazy", ["--runtime-jit"]), ("tiered", ["--runtime-tiered"])]
        for backend, options in profiles:
            for name, main_module, preloads, trap in cases:
                cmd = [str(getattr(a, product)), *options, "-Rct", "0", "-Rllvm-cache-path", "disable", "--wasm-feature-wasm2"]
                for preload in preloads:
                    cmd += ["--wasm-preload-library", str(out / (preload + ".wasm")), preload]
                cmd += ["--run", str(out / (main_module + ".wasm"))]
                proc = subprocess.run(cmd, capture_output=True, timeout=60)
                log = proc.stdout + proc.stderr
                passed = proc.returncode != 0 and b"unreachable" in log.lower() if trap else proc.returncode == 0
                (out / f"{name}-{product}-{backend}.log").write_bytes(log)
                result = dict(case=name, product=product, backend=backend, status=proc.returncode, passed=passed)
                results.append(result)
                if not passed:
                    print(json.dumps(result), flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=2))
    failed = sum(not r["passed"] for r in results)
    print(json.dumps(dict(runs=len(results), failed=failed, out=str(out))))
    return bool(failed)

if __name__ == "__main__":
    raise SystemExit(main())
