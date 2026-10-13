#!/usr/bin/env python3
"""Finite genuine GC-off Core3 EH acceptance matrix; Linux cgroup keeper only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys

CASES = (("catch_noexn_upcast", True), ("catch_ref_noexn_rethrow", True),
         ("catch_noexn_exact_control", True), ("catch_nullable_payload_invalid", False),
         ("catch_cross_family_invalid", False), ("late_unused_catch_nullable_invalid", False))
FEATURES = ("-WFD-gc", "-WFD-function-references", "-WFE-reference-types", "-WFE-exceptions")
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux": raise RuntimeError("remote Linux cgroup only")
    root = Path(__file__).resolve().parents[2]
    subprocess.run(("bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")), check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    inputs = Path(__file__).resolve().parent / "fixtures"
    modes = [("int-full", ("-Rint",)), ("jit-full-instruction", ("-Raot", "-Rllvm-call-stack", "instruction")),
             ("jit-full-unwind", ("-Raot", "-Rllvm-call-stack", "unwind"))] if args.ros else [
        ("int-full", ("-Rcc", "int", "-Rcm", "full")),
        ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
        ("int-lazy-verified", ("-Rcc", "int", "-Rcm", "lazy+verification")),
        *[("jit-" + mode + "-" + trace, ("-Rcc", "jit", "-Rcm", mode, "-Rllvm-call-stack", trace))
          for mode in ("full", "lazy") for trace in ("instruction", "unwind")],
        *[("tiered-lazy-" + trace, ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0", "-Rllvm-call-stack", trace))
          for trace in ("instruction", "unwind")],
        *[("tiered-t1-" + trace, ("-Rcc", "tiered", "-Rcm", "lazy", "-Rtiered-disable-t0", "-Rtiered-disable-t2", "-Rllvm-call-stack", trace))
          for trace in ("instruction", "unwind")]]
    rows = []
    def run(name, argv):
        command = [str(value) for value in argv]
        try:
            result = subprocess.run(command, capture_output=True, timeout=30)
            rc, output = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            rc, output = None, (error.stdout or b"") + (error.stderr or b"")
        log = args.out / (name + ".log")
        log.write_bytes(output)
        return rc, ANSI.sub("", output.decode("utf-8", "replace")).lower(), {"argv": command, "rc": rc, "log_sha256": sha(log)}
    for name, valid in CASES:
        wat, wasm = inputs / (name + ".wat"), args.out / (name + ".wasm")
        rc, text, parsed = run(name + "-parse", (args.wasm_tools, "parse", wat, "-o", wasm))
        if rc != 0: raise RuntimeError("official text assembly failed: " + name)
        rc, text, oracle = run(name + "-validate", (args.wasm_tools, "validate", wasm))
        if rc is None or rc < 0 or (rc == 0) != valid: raise RuntimeError("official type classification failed: " + name)
        if valid:
            rc, text, execution = run(name + "-wasmtime", (args.wasmtime, "run", "-C", "cache=n", "-W", "gc=n", "-W", "exceptions=y", wasm))
            if rc != 0: raise RuntimeError("independent actual EH execution failed: " + name)
        else: execution = None
        for mode, flags in modes:
            argv = (args.uwvm, *flags, "-Rllvm-cache-path", "disable", *FEATURES, "--run", wasm)
            rc, text, result = run(name + "-" + mode, argv)
            rejected = rc is not None and rc > 0 and "validation error in webassembly code" in text
            okay = rc == 0 if valid else rejected
            if any(marker in text for marker in ("invalid parameter", "parsing error", "could not retain", "resource quota", "runtime crash")):
                okay = False
            rows.append({"case": name, "mode": mode, "valid": valid, "gc_enabled": False,
                         "function_references_enabled": False, "wasm_sha256": sha(wasm), "wat_sha256": sha(wat),
                         "parse": parsed, "oracle": oracle, "independent_execution": execution,
                         "product_execution": result, "passed": okay})
        if valid:
            # Feature-disabled errors must identify the exact required exception feature.
            # Parser-time declaration refusal is legitimate for this disabled-feature test.
            for mode, flags in modes:
                rc, text, result = run(name + "-" + mode + "-exceptions-off", (args.uwvm, *flags,
                    "-Rllvm-cache-path", "disable", "-WFD-gc", "-WFD-function-references", "-WFE-reference-types", "-WFD-exceptions", "--run", wasm))
                okay = rc is not None and rc > 0 and "--wasm-feature-enable-exceptions" in text
                rows.append({"case": name, "mode": mode, "exceptions_enabled": False,
                             "product_execution": result, "passed": okay})
    report = {"source_id": args.source_id, "product_sha256": sha(args.uwvm), "wasm_tools_sha256": sha(args.wasm_tools),
              "wasmtime_sha256": sha(args.wasmtime), "rows": rows,
              "passed": bool(rows) and all(row["passed"] for row in rows),
              "assembly_qualified": False, "performance_qualified": False}
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1
if __name__ == "__main__": raise SystemExit(main())
