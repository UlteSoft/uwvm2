#!/usr/bin/env python3
"""Assert taken and fallthrough semantics of three Core 3 branch instructions."""

import argparse
import hashlib
import json
import resource
import subprocess
from pathlib import Path


CASES = ("br_if", "br_on_null", "br_on_non_null")
ORDINARY = (
    ("int-full", ("-Rcc", "int", "-Rcm", "full")),
    ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
    ("int-lazy-verified", ("-Rcc", "int", "-Rcm", "lazy+verification")),
    ("jit-full-instruction", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "instruction")),
    ("jit-full-unwind", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "unwind")),
    ("jit-lazy-instruction", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "instruction")),
    ("jit-lazy-unwind", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "unwind")),
    ("tiered-lazy-instruction", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0", "-Rllvm-call-stack", "instruction")),
    ("tiered-lazy-unwind", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0", "-Rllvm-call-stack", "unwind")),
)
ROS = (("int-full", ("-Rint",)), ("jit-full", ("-Raot",)))


def combine_modes():
    for mode in ("full", "lazy", "lazy+verification"):
        for level in ("disable", "soft", "heavy", "extra"):
            for no_delay in (False, True):
                label = f"int-{mode}-{level}-" + ("no-delay" if no_delay else "delay")
                flags = ("-Rcc", "int", "-Rcm", mode, "-Rint-op-conbine-level", level)
                if no_delay:
                    flags += ("-Rint-no-delay-local",)
                yield label, flags


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--combine-matrix", action="store_true")
    args = parser.parse_args()
    if args.ros and args.combine_matrix:
        parser.error("ROS has no selectable interpreter combine matrix")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=True)
    modes = ROS if args.ros else tuple(combine_modes()) if args.combine_matrix else ORDINARY
    rows = []
    for opcode in CASES:
        stem = f"branch_semantics_{opcode}"
        wat = args.fixtures / f"{stem}.wat"
        wasm = args.out / f"{stem}.wasm"
        commands = (
            ("parse", (args.wasm_tools, "parse", wat, "-o", wasm)),
            ("validate", (args.wasm_tools, "validate", "--features", "all", wasm)),
            ("wasmtime", (args.wasmtime, "run", "-C", "cache=n", "-W", "function-references=y", wasm)),
            *((label, (args.uwvm, *flags, "-Rllvm-cache-path", "disable",
                        "-WFE-function-references", "--run", wasm)) for label, flags in modes),
        )
        for label, command in commands:
            actual = subprocess.run(tuple(str(part) for part in command), capture_output=True, timeout=120)
            (args.out / f"{stem}-{label}.log").write_bytes(actual.stdout + actual.stderr)
            passed = actual.returncode == 0
            rows.append({"case": stem, "phase": label, "exit": actual.returncode,
                         "passed": passed, "command": [str(part) for part in command]})
            if not passed:
                print(f"FAIL {stem}/{label}: {(actual.stdout + actual.stderr).decode(errors='replace')[-500:]}", flush=True)
                break
        if not all(row["passed"] for row in rows):
            break
    summary = {
        "source_id": args.source_id, "binary_sha256": digest(args.uwvm),
        "wasm_tools_sha256": digest(args.wasm_tools), "wasmtime_sha256": digest(args.wasmtime),
        "runner_sha256": digest(Path(__file__)),
        "fixtures": {f"branch_semantics_{opcode}": {
            "wat_sha256": digest(args.fixtures / f"branch_semantics_{opcode}.wat"),
            "wasm_sha256": digest(args.out / f"branch_semantics_{opcode}.wasm")
        } for opcode in CASES if (args.out / f"branch_semantics_{opcode}.wasm").exists()},
        "checks": len(rows), "rows": rows,
        "passed": len(rows) == len(CASES) * (3 + len(modes)) and all(row["passed"] for row in rows),
    }
    (args.out / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Core 3 taken/fallthrough branch semantics: {sum(row['passed'] for row in rows)}/{len(rows)}", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
