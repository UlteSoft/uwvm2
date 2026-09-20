#!/usr/bin/env python3
"""Paired Wasm 2.0 regressions: const-expression scope and br_table bottom typing."""
import argparse
import concurrent.futures
import json
from pathlib import Path
import resource
import subprocess
import tempfile

CASES = {
    "export-only-ref-func-declaration": (True, '(module (func $f (export "f")) (func (export "_start") ref.func $f ref.is_null if unreachable end))'),
    "nested-br-table-unwind-void": (True, '(module (func $f (result i32) (block (i32.const 3) (block (i64.const 1) (br_table 1 (i32.const 1))) drop) i32.const 9) (func (export "_start") call $f i32.const 9 i32.ne if unreachable end))'),
    "nested-br-table-unwind-value": (True, '(module (func $f (result i32) (block (result i32) (i32.const 3) (block (i64.const 1) (br_table 1 (i32.const 9) (i32.const 1))))) (func (export "_start") call $f i32.const 9 i32.ne if unreachable end))'),
    "data-local-global": (False, '(module (memory 1) (global i32 (i32.const 0)) (data (global.get 0) "a") (func (export "_start")))'),
    "element-local-global": (False, '(module (table 1 funcref) (global i32 (i32.const 0)) (func $f) (elem (global.get 0) $f) (func (export "_start")))'),
    "element-local-funcref": (False, '(module (table 1 funcref) (global funcref (ref.null func)) (elem (i32.const 0) funcref (global.get 0)) (func (export "_start")))'),
    "element-local-externref": (False, '(module (table 1 externref) (global externref (ref.null extern)) (elem (i32.const 0) externref (global.get 0)) (func (export "_start")))'),
    "global-local-global": (False, '(module (global i32 (i32.const 0)) (global i32 (global.get 0)) (func (export "_start")))'),
    "bottom-label-meet": (True, '(module (func (block (result f64) (block (result f32) unreachable i32.const 1 br_table 0 1 1) drop f64.const 0) drop) (func (export "_start")))'),
    "bottom-explicit-unknown": (True, '(module (func (block (result f64) (block (result f32) unreachable select i32.const 1 br_table 0 1 1) drop f64.const 0) drop) (func (export "_start")))'),
    "bottom-known-value": (False, '(module (func (block (result f64) (block (result f32) unreachable f32.const 0 i32.const 1 br_table 0 1 1) drop f64.const 0) drop) (func (export "_start")))'),
    "bottom-unequal-arity": (False, '(module (func (block (block (result f32) unreachable i32.const 1 br_table 0 1 1) drop)) (func (export "_start")))'),
    "bottom-partial-tuple": (True, '(module (func (block (result f64 i32) (block (result f32 i32) unreachable i32.const 7 i32.const 1 br_table 0 1 1) drop drop f64.const 0 i32.const 0) drop drop) (func (export "_start")))'),
    "bottom-partial-tuple-mismatch": (False, '(module (func (block (result f64 i64) (block (result f32 i32) unreachable i32.const 7 i32.const 1 br_table 0 1 1) drop drop f64.const 0 i64.const 0) drop drop) (func (export "_start")))'),
    "bottom-selector-type": (False, '(module (func (block (result f64) (block (result f32) unreachable f64.const 1 br_table 0 1 1) drop f64.const 0) drop) (func (export "_start")))'),
    "bottom-live-invalid": (False, '(module (func (block (result f64) (block (result f32) f32.const 0 i32.const 1 br_table 0 1 1) drop f64.const 0) drop) (func (export "_start")))'),
}

for _name, _dead in {
    "br": "br 0",
    "br-table": "i32.const 0 br_table 0",
    "return": "return",
    "call": "call $callee",
    "call-indirect": "i32.const 0 call_indirect (type $t)",
}.items():
    CASES["dead-" + _name + "-preserves-outer-frame"] = (True, f'''(module
      (type $t (func (param i32) (result i32))) (table 1 funcref)
      (func $callee (type $t) local.get 0)
      (func $test (result i32)
        i32.const 1
        block (result i32)
          block (result i32) i32.const 8 br 1 {_dead} end
          drop i32.const 16
        end i32.add)
      (func (export "_start") call $test i32.const 9 i32.ne if unreachable end))''')



for _width, _lanes, _snan, _mask in ((32, 4, 0x7fa00001, 0x7fc00000),
                                    (64, 2, 0x7ff4000000000001, 0x7ff8000000000000)):
    for _op, _constant in (("div", 1), ("mul", 1), ("sub", 0)):
        _vector = f"f{_width}x{_lanes}"
        CASES[f"simd-{_vector}-{_op}-snan"] = (True, f'''(module
          (func $f (param v128) (result v128)
            local.get 0 v128.const {_vector} {" ".join([str(_constant)] * _lanes)} {_vector}.{_op})
          (func (export "_start")
            i{_width}.const {_snan} i{_width}x{_lanes}.splat call $f
            i{_width}x{_lanes}.extract_lane 0
            i{_width}.const {_mask} i{_width}.and i{_width}.const {_mask}
            i{_width}.ne if unreachable end))''')


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--full", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    out = args.out or Path(tempfile.mkdtemp(prefix="uwvm-wasm2-regression-", dir="/tmp"))
    out.mkdir(parents=True, exist_ok=True)
    for name, (valid, wat) in CASES.items():
        (out / (name + ".wat")).write_text(wat + "\n")
        subprocess.run(["wat2wasm", "--no-check", str(out / (name + ".wat")), "-o", str(out / (name + ".wasm"))], check=True, capture_output=True)
        oracle = subprocess.run(["wasm-validate", str(out / (name + ".wasm"))], capture_output=True)
        if (oracle.returncode == 0) != valid:
            raise RuntimeError(f"regression oracle disagrees: {name}: {oracle.stderr!r}")
    jobs = []
    for product in ("full", "ros"):
        for compiler in ("int", "llvm"):
            options = ["-Rcm", "full", "-Rcc", "int" if compiler == "int" else "jit"] if product == "full" else ["-Rint" if compiler == "int" else "-Raot"]
            for name, (valid, _) in CASES.items():
                jobs.append((product, compiler, options, name, valid))
    def run(job):
        product, compiler, options, name, valid = job
        command = [str(getattr(args, product).resolve()), *options, "-Rct", "0", "-Rllvm-cache-path", "disable",
                   "--wasm-feature-wasm2", "--run", str(out / (name + ".wasm"))]
        result = subprocess.run(command, capture_output=True, timeout=30)
        log = result.stdout + result.stderr
        (out / f"{product}-{compiler}-{name}.log").write_bytes(log)
        # A signal, missing entry, runtime trap or any unrelated operational error is not a valid rejection.
        diagnosed = b"parse" in log.lower() or b"validation" in log.lower() or b"validate" in log.lower()
        passed = result.returncode == 0 if valid else result.returncode != 0 and diagnosed and b"Cannot resolve entry" not in log
        row = dict(product=product, compiler=compiler, case=name, expected_valid=valid, status=result.returncode, passed=passed)
        print(json.dumps(row), flush=True)
        return row
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(run, jobs))
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 0 if all(r["passed"] for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
