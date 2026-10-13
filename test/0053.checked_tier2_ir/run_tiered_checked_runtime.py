#!/usr/bin/env python3
"""Finite actual T2 production regression; Linux keeper's existing cgroup only.

No compiler/native/QEMU execution on the local Mac. The matching fresh product
and wasm-tools are supplied by the native keeper. Real foreign-import native
switches must produce the original T2 request and full-entry publication plus
the exact checked-owned-IR input marker. Zero exit alone is insufficient. This
runner grants no CPU/cgroup/platform/debugger/performance qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
FEATURES = ("-WFE-memory64", "-WFE-reference-types")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    if sys.platform != "linux": raise RuntimeError("remote original cgroup Linux only")
    if not 1 <= args.timeout <= 60: raise ValueError("finite timeout 1..60 required")
    binary, oracle = args.uwvm.resolve(strict=True), args.wasm_tools.resolve(strict=True)
    args.out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent
    facts = []
    def run(name, command):
        argv = [str(value) for value in command]
        try:
            result = subprocess.run(argv, capture_output=True, timeout=args.timeout, check=False)
            rc, stdout, stderr = result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired as error:
            rc, stdout, stderr = None, error.stdout or b"", error.stderr or b""
        (args.out / (name + ".stdout")).write_bytes(stdout)
        (args.out / (name + ".stderr")).write_bytes(stderr)
        fact = {"argv": argv, "rc": rc, "stdout_sha256": hashlib.sha256(stdout).hexdigest(),
                "stderr_sha256": hashlib.sha256(stderr).hexdigest()}
        facts.append(fact)
        return rc, stdout, ANSI.sub("", stderr.decode("utf-8", "replace")), fact
    for name in ("provider", "consumer"):
        wat = source / ("tiered_runtime_" + name + ".wat")
        wasm = args.out / (name + ".wasm")
        rc, _, _, fact = run(name + "-parse", (oracle, "parse", wat, "-o", wasm))
        if rc != 0: raise RuntimeError("real official text assembly failed: " + name)
        rc, _, _, fact = run(name + "-validate", (oracle, "validate", "--features", "all", wasm))
        if rc != 0: raise RuntimeError("real Core3 official validation failed: " + name)
    rows = []
    for name, policy, extra in (("instruction-no-t0", "instruction", ("-Rtiered-disable-t0",)),
                               ("unwind-no-t0", "unwind", ("-Rtiered-disable-t0",)),
                               ("unwind-with-t0", "unwind", ())):
        command = (binary, "-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "1", "-Rclog", "err",
                   "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", policy, *extra, *FEATURES,
                   "--wasm-preload-library", args.out / "provider.wasm", "P", "--run", args.out / "consumer.wasm")
        rc, stdout, text, fact = run(name, command)
        request = bool(re.search(r'\[llvm-jit-lazy\] tiered-full-request module="P"[^\n]*reason=switch', text))
        native_provider = bool(re.search(r'\[llvm-jit-lazy\] compile-end module="P"[^\n]*local_fn=0[^\n]*state=compiled', text))
        actual_full = bool(re.search(r'\[llvm-jit-lazy\] tiered-full-ready module="P"[^\n]*functions=1 semantic_input=checked-owned-ir-v1', text))
        unrelated = any(marker in text.lower() for marker in ("unknown option", "invalid parameter", "parsing error",
            "validation error", "runtime crash", "unsupported native", "could not retain", "permission denied"))
        passed = rc == 0 and stdout == b"" and request and native_provider and actual_full and not unrelated
        rows.append({"case": name, "result": fact, "real_tiered_request": request,
                     "actual_native_provider": native_provider, "actual_all_entry_checked_ir_publication": actual_full,
                     "real_guest_checksum": rc == 0 and not unrelated, "passed": passed})
    report = {"product_sha256": sha(binary), "wasm_tools_sha256": sha(oracle), "rows": rows,
              "passed": bool(rows) and all(row["passed"] for row in rows), "parse_and_run_facts": facts,
              "wasm_inputs": {name: sha(args.out / (name + ".wasm")) for name in ("provider", "consumer")},
              "byte_walk_count_instrumented": False, "performance_qualified": False,
              "native_unwind_stack_report_qualified": False, "debugger_restore_or_cross_os_qualified": False}
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1
if __name__ == "__main__": raise SystemExit(main())
