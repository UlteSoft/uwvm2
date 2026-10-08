#!/usr/bin/env python3
"""Execute Core 3 branch label subtype cases through eager and lazy backends."""

import argparse
import hashlib
import json
import resource
import subprocess
from pathlib import Path


CASES = ("br_if", "br_on_null", "br_on_non_null")
CONFIGURATIONS = (
    ("int-full", ("-Rcc", "int", "-Rcm", "full")),
    ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
    ("int-lazy-verification", ("-Rcc", "int", "-Rcm", "lazy+verification")),
    ("jit-full-instruction", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "instruction")),
    ("jit-full-unwind", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "unwind")),
    ("jit-lazy-instruction", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "instruction")),
    ("jit-lazy-unwind", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "unwind")),
    ("tiered-lazy-instruction", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rllvm-call-stack", "instruction")),
    ("tiered-lazy-unwind", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rllvm-call-stack", "unwind")),
)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def start_body(opcode):
    if opcode == "br_if":
        return "call 1 drop"
    if opcode == "br_on_null":
        return "ref.null func call 1 drop"
    return "ref.null func call 1 drop drop"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=True)
    rows = []

    for opcode in CASES:
        for status in ("valid", "invalid"):
            case = f"branch_label_{opcode}_{status}"
            wat = (args.fixtures / f"{case}.wat").read_text().rstrip()
            if not wat.endswith(")"):
                raise RuntimeError(f"expected module closing parenthesis: {case}")
            # Function index 1 is the typed-branch function; _start forces lazy compilation.
            wrapped_wat = args.out / f"{case}-start.wat"
            wrapped_wat.write_text(wat[:-1] + f'  (func (export "_start") {start_body(opcode)})\n)\n')
            wasm = args.out / f"{case}-start.wasm"
            parsed = subprocess.run(
                (str(args.wasm_tools), "parse", str(wrapped_wat), "-o", str(wasm)),
                capture_output=True,
                timeout=30,
            )
            (args.out / f"{case}-parse.log").write_bytes(parsed.stdout + parsed.stderr)
            if parsed.returncode:
                raise RuntimeError(f"wasm-tools parse failed: {case}")
            oracle = subprocess.run(
                (str(args.wasm_tools), "validate", "--features", "all", str(wasm)),
                capture_output=True,
                timeout=30,
            )
            (args.out / f"{case}-oracle.log").write_bytes(oracle.stdout + oracle.stderr)
            expected_valid = status == "valid"
            rows.append({
                "case": case,
                "engine": "wasm-tools",
                "valid": oracle.returncode == 0,
                "passed": (oracle.returncode == 0) == expected_valid,
                "wasm_sha256": sha256(wasm),
            })
            if not rows[-1]["passed"]:
                break
            for config, flags in CONFIGURATIONS:
                command = (
                    str(args.uwvm), *flags, "-Rllvm-cache-path", "disable",
                    "-WFE-function-references", "--run", str(wasm),
                )
                actual = subprocess.run(command, capture_output=True, timeout=120)
                (args.out / f"{case}-{config}.log").write_bytes(actual.stdout + actual.stderr)
                text = (actual.stdout + actual.stderr).decode(errors="replace")
                # A successful valid case must complete _start, while an invalid case
                # must fail in validation or compilation rather than trapping in _start.
                passed = ((actual.returncode == 0) == expected_valid)
                if not expected_valid:
                    passed = passed and "Validation error" in text
                rows.append({
                    "case": case,
                    "engine": config,
                    "exit": actual.returncode,
                    "passed": passed,
                    "command": list(command),
                })
                if not passed:
                    break
            if not all(row["passed"] for row in rows):
                break
        if not all(row["passed"] for row in rows):
            break

    result = {
        "source_id": args.source_id,
        "uwvm_sha256": sha256(args.uwvm),
        "wasm_tools_sha256": sha256(args.wasm_tools),
        "checks": len(rows),
        "passed": len(rows) == len(CASES) * 2 * (len(CONFIGURATIONS) + 1) and all(row["passed"] for row in rows),
        "rows": rows,
    }
    (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("source_id", "checks", "passed")}), flush=True)
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
