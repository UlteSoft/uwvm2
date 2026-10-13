#!/usr/bin/env python3
"""Finite Linux-only mandatory lazy-admission regression, no pure prepass.

The invalid function is NEVER called by _start. Genuine validation rejection
at ordinary lazy startup is required; timeout, crash, CLI/parser failure and
compiler-resource decline are failures. wasm-tools independently classifies
all exact binary bytes. Positive _start programs check their own real results.
Run only via keeper's original 64 GiB cgroup guard; this is not a guard creator.
No performance, sole-decode, kernel/debug or cross-OS qualification is inferred.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

CASES = (
    ("unused_bad_i31", False), ("unused_bad_memory64", False),
    ("unused_bad_nondefaultable_local", False), ("unused_bad_call_ref", False),
    ("unused_bad_try_table", False), ("unused_bad_struct", False),
    ("unused_bad_before_active_segment", False),
    ("valid_typed_recursive_tail", True), ("valid_memory64_gc_exception", True),
    ("valid_nondefaultable_select", True), ("valid_unused_nested_dead", True),
    ("valid_active_memory64_table64", True),
)
FEATURES = ("-WFE-reference-types", "-WFE-function-references", "-WFE-gc",
            "-WFE-exceptions", "-WFE-tail-call", "-WFE-memory64")
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()
    if sys.platform != "linux": raise RuntimeError("remote Linux cgroup execution only")
    if not 1 <= args.timeout <= 60: raise ValueError("bounded per-command timeout required")
    args.out.mkdir(parents=True, exist_ok=False)
    inputs = Path(__file__).resolve().parent
    modes = (("int-full", ("-Rint",)), ("jit-full", ("-Raot",))) if args.ros else tuple(
        (engine + "-" + mode, ("-Rcc", engine, "-Rcm", mode, "-Rct", "0"))
        for engine in ("int", "jit", "tiered") for mode in ("full", "lazy", "lazy+verification")
        if engine != "tiered" or mode != "full")
    rows = []
    def run(name, argv):
        command = [str(value) for value in argv]
        try:
            result = subprocess.run(command, capture_output=True, timeout=args.timeout)
            rc, output = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            rc, output = None, (error.stdout or b"") + (error.stderr or b"")
        log = args.out / (name + ".log")
        log.write_bytes(output)
        return rc, ANSI.sub("", output.decode("utf-8", "replace")).lower(), {"argv": command, "rc": rc, "log_sha256": sha(log)}
    for name, valid in CASES:
        wat, wasm = inputs / (name + ".wat"), args.out / (name + ".wasm")
        rc, text, parse = run(name + "-parse", (args.wasm_tools, "parse", wat, "-o", wasm))
        if rc != 0: raise RuntimeError("text assembly failed: " + name)
        rc, text, oracle = run(name + "-oracle", (args.wasm_tools, "validate", "--features", "all", wasm))
        # A negative oracle must be an actual BinaryReaderError with its byte
        # offset. Unknown switches/features, I/O failures, panics, signals and
        # timeouts cannot certify an invalid Wasm function.
        oracle_unavailable = any(marker in text for marker in (
            "unknown option", "unrecognized option", "unexpected argument", "unknown feature",
            "not enabled", "support is disabled", "no such file", "permission denied", "panicked"))
        oracle_binary_error = bool(re.search(r"\(at offset 0x[0-9a-f]+\)", text))
        oracle_ok = rc is not None and rc >= 0 and not oracle_unavailable and (
            rc == 0 if valid else rc > 0 and oracle_binary_error)
        oracle["actual_binary_error_with_offset"] = oracle_binary_error
        oracle["passed"] = oracle_ok
        if not oracle_ok:
            raise RuntimeError("independent validation expectation failed: " + name)
        for mode, flags in modes:
            argv = (args.uwvm, *flags, "-Rllvm-cache-path", "disable", *FEATURES, "--run", wasm)
            rc, text, fact = run(name + "-" + mode, argv)
            rejected = rc is not None and rc > 0 and "validation error in webassembly code" in text
            okay = rc == 0 if valid else rejected
            # An unrelated failure must never qualify as typed admission refusal.
            if any(marker in text for marker in ("invalid parameter", "parsing error", "could not retain", "resource quota", "runtime crash", "unknown option", "unknown feature", "not enabled", "support is disabled", "no such file", "permission denied", "panicked")):
                okay = False
            rows.append({"case": name, "valid": valid, "mode": mode, "unused_invalid_body": not valid,
                         "wasm_sha256": sha(wasm), "wat_sha256": sha(wat), "oracle": oracle,
                         "parse": parse, "result": fact, "passed": okay})
    report = {"product_sha256": sha(args.uwvm), "wasm_tools_sha256": sha(args.wasm_tools), "rows": rows,
              "passed": bool(rows) and all(row["passed"] for row in rows),
              "byte_walk_count_qualified": False, "performance_qualified": False}
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1
if __name__ == "__main__": raise SystemExit(main())
