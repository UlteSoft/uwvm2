#!/usr/bin/env python3
"""Qualify Core 3 exnref casts and function-call ABI across product backends.

Core 3 accepts ref.test, ref.cast, br_on_cast and br_on_cast_fail in the exn
heap hierarchy. The specified Wasmtime executable is the execution oracle;
older releases may not understand this grammar.
https://webassembly.github.io/spec/core/valid/instructions.html#valid-ref.cast
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess


CASES = ("ref-test-exn-null", "ref-cast-exn-null", "br-on-cast-exn-null",
         "exn-cast-retained", "ref-cast-exn-null-trap",
         "exnref_param_unused", "exnref_result_unused", "exnref_call_abi",
         "exnref_indirect_call_abi", "exnref_call_retained")
TRAPS = {"ref-cast-exn-null-trap"}
ABI_CASES = {"exnref_param_unused", "exnref_result_unused", "exnref_call_abi",
             "exnref_indirect_call_abi", "exnref_call_retained"}
ENABLED = ("-WFE-gc", "-WFE-exceptions", "-WFE-reference-types")
FEATURE_OFF = (
    ("gc", "-WFD-gc", "--wasm-feature-enable-gc"),
    ("exceptions", "-WFD-exceptions", "--wasm-feature-enable-exceptions"),
    ("reference-types", "-WFD-reference-types", "reference-types"),
)
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def policies(ros):
    if ros:
        yield "int-full", ("-Rint",)
        for trace in ("instruction", "unwind"):
            yield f"jit-full-{trace}", ("-Raot", "-Rllvm-call-stack", trace,
                                       "-Rllvm-cache-path", "disable")
        return
    for mode in ("full", "lazy", "lazy+verification"):
        yield f"int-{mode}", ("-Rcc", "int", "-Rcm", mode)
    for mode in ("full", "lazy"):
        for trace in ("instruction", "unwind"):
            yield f"jit-{mode}-{trace}", ("-Rcc", "jit", "-Rcm", mode,
                                          "-Rllvm-call-stack", trace,
                                          "-Rllvm-cache-path", "disable")
    for trace in ("instruction", "unwind"):
        yield f"tiered-lazy-{trace}", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0",
                                        "-Rllvm-call-stack", trace,
                                        "-Rllvm-cache-path", "disable")
        # A one-shot _start can finish before ordinary promotion occurs. Force
        # tier 1 so this case actually exercises the LLVM path as well as T0.
        yield f"tiered-t1-lazy-{trace}", ("-Rcc", "tiered", "-Rcm", "lazy",
                                           "-Rtiered-disable-t0", "-Rtiered-disable-t2",
                                           "-Rllvm-call-stack", trace,
                                           "-Rllvm-cache-path", "disable")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    wasmtime_version = subprocess.check_output(
        [str(args.wasmtime), '--version'], text=True).strip()
    args.out.mkdir(parents=True, exist_ok=False)
    fixtures = source / "test/0017.runtime/fixtures"
    environment = os.environ.copy()
    environment["HOME"] = str(args.out)
    rows = []

    def run(label, command, expected_success=True, diagnostic=None):
        command = [str(part) for part in command]
        try:
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=30, env=environment)
            output = result.stdout + result.stderr
            exit_code = result.returncode
        except subprocess.TimeoutExpired as error:
            output = f"TIMEOUT: {error}\n"
            exit_code = None
        log_name = label + ".log"
        (args.out / log_name).write_text(output)
        normalized = ANSI.sub("", output)
        passed = ((exit_code == 0) if expected_success else
                  (exit_code is not None and exit_code != 0 and
                   diagnostic is not None and diagnostic in normalized))
        rows.append({"label": label, "command": command, "exit": exit_code,
                     "expected_success": expected_success, "diagnostic": diagnostic,
                     "passed": passed, "log": log_name})
        return passed

    for case in CASES:
        wat = fixtures / (case + ".wat")
        wasm = args.out / (case + ".wasm")
        if not run(case + "-parse", [args.wasm_tools, "parse", wat, "-o", wasm]):
            break
        if not run(case + "-validate", [args.wasm_tools, "validate", "--features", "all", wasm]):
            break
        expected_success = case not in TRAPS
        if not run(case + "-wasmtime-oracle", [args.wasmtime, "run", "-C", "cache=n",
                                                "-W", "gc=n" if case in ABI_CASES else "gc=y",
                                                "-W", "exceptions=y", wasm],
                   expected_success=expected_success,
                   diagnostic=None if expected_success else "wasm trap: cast failure"):
            break
        for policy, options in policies(args.ros):
            run(case + "-" + policy, [args.uwvm, *options, *ENABLED, "--run", wasm],
                expected_success=expected_success,
                diagnostic=None if expected_success else
                    "reference cast failed: value does not match target heap type")
        # Check each dependency independently. Function references are unrelated
        # to this exception/GC intersection and must remain independently off.
        int_policy = ("-Rint",) if args.ros else ("-Rcc", "int", "-Rcm", "full")
        for feature, off_flag, diagnostic in FEATURE_OFF:
            enabled = [flag for flag in ENABLED if flag != "-WFE-" + feature]
            # Exnref function signatures require exceptions, but not GC.
            independent = feature == "gc" and case in ABI_CASES
            run(case + "-" + feature + "-off",
                [args.uwvm, *int_policy, *enabled, off_flag, "--run", wasm],
                expected_success=independent,
                diagnostic=None if independent else diagnostic)
        run(case + "-function-references-off",
            [args.uwvm, *int_policy, *ENABLED, "-WFD-function-references", "--run", wasm],
            expected_success=expected_success,
            diagnostic=None if expected_success else
                "reference cast failed: value does not match target heap type")
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")

    summary = {
        "passed": bool(rows) and all(row["passed"] for row in rows) and
                  len([row for row in rows if row["label"].endswith("-wasmtime-oracle")]) == len(CASES),
        "repository": "ros" if args.ros else "ordinary",
        "checks": len(rows),
        "fixture_wat_sha256": {case: sha256(fixtures / (case + ".wat")) for case in CASES},
        "fixture_wasm_sha256": {case: sha256(args.out / (case + ".wasm")) for case in CASES
                                if (args.out / (case + ".wasm")).exists()},
        "uwvm_sha256": sha256(args.uwvm),
        "wasm_tools_sha256": sha256(args.wasm_tools),
        "wasmtime_sha256": sha256(args.wasmtime),
        "wasmtime_version": wasmtime_version,
        "failures": [row for row in rows if not row["passed"]],
    }
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Core 3 exnref cast/call ABI: {len(rows)-len(summary['failures'])}/{len(rows)} "
          f"checks, {'PASS' if summary['passed'] else 'FAIL'}", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
