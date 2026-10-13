#!/usr/bin/env python3
"""Check Core 3 funcidx element nullability and defined-global element expressions."""

import argparse
import hashlib
import json
import resource
import subprocess
from pathlib import Path


CASES = (
    ("wasm3_funcidx_nonnull_active", True),
    ("wasm3_funcidx_nonnull_passive", True),
    ("wasm3_element_defined_global", True),
    ("wasm3_nullable_elem_nonnull_table_invalid", False),
)
FEATURES = ("-WFE-function-references", "-WFE-extended-const", "-WFE-table-initializer", "-WFE-bulk-memory")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(argv, timeout=120):
    return subprocess.run([str(part) for part in argv], capture_output=True, timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=True)

    configurations = (
        (("int-full", ("-Rint",)), ("jit-full", ("-Raot", "-Rllvm-cache-path", "disable")))
        if args.ros else (
            ("int-full", ("-Rcc", "int", "-Rcm", "full")),
            ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
            ("int-lazy-verification", ("-Rcc", "int", "-Rcm", "lazy+verification")),
            ("jit-full-instruction", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", "instruction")),
            ("jit-full-unwind", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", "unwind")),
            ("jit-lazy-instruction", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", "instruction")),
            ("jit-lazy-unwind", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", "unwind")),
            ("tiered-lazy", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rllvm-cache-path", "disable")),
        )
    )

    rows = []
    for stem, expected_valid in CASES:
        wat = args.fixtures / f"{stem}.wat"
        wasm = args.out / f"{stem}.wasm"
        parsed = run((args.wasm_tools, "parse", wat, "-o", wasm), 30)
        if parsed.returncode:
            raise RuntimeError(f"wasm-tools parse {wat}: {parsed.stderr.decode(errors='replace')}")
        oracle = run((args.wasm_tools, "validate", "--features", "all", wasm), 30)
        (args.out / f"{stem}-oracle.log").write_bytes(oracle.stdout + oracle.stderr)
        oracle_valid = oracle.returncode == 0
        rows.append({"case": stem, "engine": "wasm-tools", "valid": oracle_valid, "passed": oracle_valid == expected_valid, "wasm_sha256": sha256(wasm)})
        if oracle_valid != expected_valid:
            break
        if expected_valid and args.wasmtime:
            reference = run((args.wasmtime, "run", wasm))
            (args.out / f"{stem}-wasmtime.log").write_bytes(reference.stdout + reference.stderr)
            rows.append({"case": stem, "engine": "wasmtime", "exit": reference.returncode, "passed": reference.returncode == 0})
            if reference.returncode:
                break
        for configuration, flags in configurations:
            command = (args.uwvm, *flags, *FEATURES, "--run", wasm)
            actual = run(command)
            (args.out / f"{stem}-{configuration}.log").write_bytes(actual.stdout + actual.stderr)
            passed = (actual.returncode == 0) == expected_valid
            rows.append({"case": stem, "engine": configuration, "exit": actual.returncode, "passed": passed})
            if not passed:
                break
        if not all(row["passed"] for row in rows):
            break

    result = {
        "source_id": args.source_id,
        "uwvm_sha256": sha256(args.uwvm),
        "wasm_tools_sha256": sha256(args.wasm_tools),
        "wasmtime_sha256": sha256(args.wasmtime) if args.wasmtime else None,
        "rows": rows,
    }
    # Only the three valid fixtures are executed in Wasmtime; the fourth is rejected by the validator.
    result["passed"] = len(rows) == len(CASES) * (len(configurations) + 1) + (3 if args.wasmtime else 0) and all(row["passed"] for row in rows)
    (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": result["passed"], "checks": len(rows), "source_id": args.source_id}), flush=True)
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
