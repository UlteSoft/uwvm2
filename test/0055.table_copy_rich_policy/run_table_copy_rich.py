#!/usr/bin/env python3
"""Mixed legacy/rich table.copy differential; remote Linux cgroup keeper only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys

ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
SOURCE_ROOT = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def classification(rc, plain):
    if rc is None:
        return "timeout"
    if rc < 0:
        return "signal-or-crash"
    if rc == 0:
        return "success"
    if "validation error in webassembly code" in plain:
        return "code-validation-rejection"
    if any(s in plain for s in ("invalid parameter", "unknown option", "unrecognized option")):
        return "cli-or-environment-failure"
    if any(s in plain for s in ("llvm jit first compiler decline", "could not retain",
                               "missing emitted module state", "checked llvm ir", "native artifact")):
        return "native-lowering-or-artifact-decline"
    if "parsing error" in plain or "parse error" in plain:
        return "declaration-or-binary-parser-rejection"
    return "runtime-or-resource-failure"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--phase", choices=("tiny", "extended"), default="tiny")
    parser.add_argument("--stack-strategy", choices=("instruction", "unwind"), default="instruction")
    parser.add_argument("--ros", action="store_true")
    # This descriptive label is NOT proof that the source patch is installed.
    # The keeper must tie the actual binary hash to its canonical build receipt.
    parser.add_argument("--lazy-admission-source-id", default="not-live-at-plan-freeze")
    args = parser.parse_args()
    if sys.platform != "linux":
        raise RuntimeError("run only on remote Linux inside the original cgroup")
    source_root = args.source_root.resolve(strict=True)
    subprocess.run(("bash", str(source_root / "tools/ci/require_wasm3_test_cgroup.sh")), check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    catalog = json.loads((SOURCE_ROOT / "catalog.json").read_text())
    selected = [c for c in catalog["cases"] if args.phase == "extended" or c["tier"] == "tiny"]
    modes = [("int-full", ("-Rint",)), ("jit-full", ("-Raot",))] if args.ros else [
        ("int-full", ("-Rcc", "int", "-Rcm", "full")),
        ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
        ("jit-full", ("-Rcc", "jit", "-Rcm", "full")),
        ("jit-lazy", ("-Rcc", "jit", "-Rcm", "lazy"))]
    rows, products = [], []

    def run(name, argv):
        command = [str(v) for v in argv]
        try:
            result = subprocess.run(command, capture_output=True, timeout=30)
            rc, output = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            rc = None
            output = (error.stdout or b"") + (error.stderr or b"")
        log = args.out / (name + ".log")
        log.write_bytes(output)
        plain = ANSI.sub("", output.decode("utf-8", "replace")).lower()
        return rc, plain, {"argv": command, "rc": rc, "classification": classification(rc, plain),
                           "log": log.name, "log_sha256": sha(log)}

    def save():
        report = {"source_id": args.source_id, "source_manifest_sha256": sha(args.source_manifest),
                  "product_sha256": sha(args.uwvm), "wasm_tools_sha256": sha(args.wasm_tools),
                  "wasmtime_sha256": sha(args.wasmtime), "runner_sha256": sha(Path(__file__)),
                  "catalog_sha256": sha(SOURCE_ROOT / "catalog.json"), "phase": args.phase,
                  "stack_strategy": args.stack_strategy, "ros": args.ros,
                  "lazy_admission_source_id_label": args.lazy_admission_source_id,
                  "rows": rows, "product_rows": products,
                  "passed": bool(products) and all(r["passed"] for r in rows + products),
                  "assembly_qualified": False, "performance_qualified": False,
                  "all_opcodes_or_complete_spec_qualified": False,
                  "source_receipt_proof_is_external_keeper_responsibility": True}
        (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        return report

    for tool_name, tool in (("wasm-tools", args.wasm_tools), ("wasmtime", args.wasmtime), ("uwvm", args.uwvm)):
        rc, plain, result = run(tool_name + "-version", (tool, "--version"))
        rows.append({"stage": "tool-version", "tool": tool_name, "result": result, "passed": rc == 0})
        if rc != 0:
            save()
            raise RuntimeError("tool version invocation failed: " + tool_name)

    for case in selected:
        original = SOURCE_ROOT / "input" / case["input"]
        if sha(original) != case["sha256"] or original.stat().st_size != case["bytes"]:
            raise RuntimeError("fixture pin mismatch: " + case["name"])
        wasm = args.out / (case["name"] + ".wasm")
        if case["kind"] == "wat":
            rc, plain, result = run(case["name"] + "-official-parse", (args.wasm_tools, "parse", original, "-o", wasm))
            rows.append({"case": case["name"], "stage": "official-text-assembly", "result": result, "passed": rc == 0})
            if rc != 0:
                save()
                continue  # Text assembly failure can never qualify a product rejection.
        else:
            wasm.write_bytes(original.read_bytes())  # Immutable literal malformed binary, no guest decoder.
        rc, plain, oracle = run(case["name"] + "-official-validate", (args.wasm_tools, "validate", "--features", "all", wasm))
        unavailable = any(s in plain for s in ("unknown option", "unrecognized option", "not enabled", "support is disabled"))
        # wasmparser's real BinaryReaderError Display carries a binary offset.
        # Generic CLI/I/O failures and panics cannot qualify an invalid oracle.
        binary_error = bool(re.search(r"\(at offset 0x[0-9a-f]+\)", plain))
        oracle_ok = rc is not None and rc >= 0 and not unavailable and (
            rc == 0 if case["valid"] else rc > 0 and binary_error)
        rows.append({"case": case["name"], "stage": "official-type-or-binary-validation", "valid": case["valid"],
                     "result": oracle, "passed": oracle_ok})
        if not oracle_ok:
            save()
            continue  # Unsupported oracle/incorrect fixture never turns a fatal into an invalid PASS.
        if case["valid"]:
            rc, plain, execution = run(case["name"] + "-wasmtime", (args.wasmtime, "run", "-C", "cache=n", *case["wasmtime"], wasm))
            rows.append({"case": case["name"], "stage": "independent-finite-execution", "result": execution, "passed": rc == 0})
        else:
            execution = None
        rc, plain, pure = run(case["name"] + "-pure-validation", (args.uwvm, "-m", "validation", *case["features"], "--run", wasm))
        pure_ok = rc == 0 if case["valid"] else pure["classification"] == "code-validation-rejection"
        rows.append({"case": case["name"], "stage": "pure-wasm3-validation", "result": pure, "passed": pure_ok})
        for mode, flags in modes:
            stack = ("-Rllvm-call-stack", args.stack_strategy) if mode.startswith("jit") else ()
            command = (args.uwvm, *flags, *stack, "-Rllvm-cache-path", "disable", "--log-verbose", *case["features"], "--run", wasm)
            rc, plain, result = run(case["name"] + "-" + mode, command)
            okay = rc == 0 if case["valid"] else result["classification"] == "code-validation-rejection"
            products.append({"case": case["name"], "mode": mode, "valid": case["valid"], "result": result,
                             "input_sha256": case["sha256"], "wasm_sha256": sha(wasm), "passed": okay,
                             "official_validation": oracle, "independent_execution": execution,
                             "requires_mandatory_lazy_admission": case["requires_mandatory_lazy_admission"],
                             "known_pending_dependency": "PRIVATE lazy R4 not installed at plan freeze" if
                                "lazy" in mode and case["requires_mandatory_lazy_admission"] else None})
            # A proposal gate may reject in declaration parsing. This separate
            # cell requires the exact feature diagnostic, not code-invalid status.
            if case["valid"] and case["disable_feature"]:
                feature = case["disable_feature"]
                disabled = ["-WFD-" + feature if f == "-WFE-" + feature else f for f in case["features"]]
                rc, plain, result = run(case["name"] + "-" + mode + "-feature-off",
                    (args.uwvm, *flags, *stack, "-Rllvm-cache-path", "disable", "--log-verbose", *disabled, "--run", wasm))
                gate_ok = rc is not None and rc > 0 and ("--wasm-feature-enable-" + feature) in plain
                products.append({"case": case["name"], "mode": mode, "stage": "exact-feature-off", "feature": feature,
                                 "result": result, "passed": gate_ok})
        save()
    report = save()
    print("Core3 finite differential:", sum(r["passed"] for r in products), "/", len(products), "product cells")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
