#!/usr/bin/env python3
"""Remote-cgroup Core 3 table64 widths, polymorphism and feature independence.

Official parse/validation and Wasmtime execution precede product execution.
These are semantic witnesses, not throughput or tier-promotion evidence.
https://webassembly.github.io/spec/core/valid/instructions.html#table-instructions
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import resource
import subprocess

FUSED_SHA = "43e6eaa651db9c302f68af12e61d229542604bf1416557617522310f21531cb1"
FEATURES = ("-WFE-table64", "-WFE-reference-types", "-WFE-function-references",
            "-WFD-memory64", "-WFD-gc", "-WFD-exceptions", "-WFD-threads")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def cases(execution_wat):
    result = {"mixed-width-execution": (execution_wat, True)}
    # Each probe is reached by _start, forcing lazy compilation. Its body is
    # validated and translated, while the unreachable branch is never run.
    prelude = "(type $sig (func)) (table $wide i64 1 funcref) (table $narrow 1 funcref) (elem $items func) "
    for name, tail, operands in (
        ("get", "table.get $wide drop", ""),
        ("set", "table.set $wide", "ref.null func "),
        ("grow", "table.grow $wide drop", ""),
        ("fill", "table.fill $wide", ""),
        ("init-destination", "table.init $wide $items", "i32.const 0 i32.const 0 "),
        ("copy-wide-length", "table.copy $wide $wide", ""),
        ("indirect-index", "call_indirect $wide (type $sig)", ""),
    ):
        for valid in (True, False):
            value = "select " if valid else "i32.const 0 "
            probe = "(func $probe (param i32) local.get 0 if unreachable " + value + operands + tail + " end) "
            result[name + ("-bot-valid" if valid else "-known-i32-invalid")] = (
                "(module " + prelude + probe + '(func (export "_start") i32.const 0 call $probe))', valid)
    # table.init never widens its element-segment offset or count.
    for name, body in (
        ("init-source-i64-invalid", "i64.const 0 i64.const 0 i32.const 0 table.init $wide $items"),
        ("init-length-i64-invalid", "i64.const 0 i32.const 0 i64.const 0 table.init $wide $items"),
        ("copy-wide-narrow-length-i64-invalid", "i64.const 0 i32.const 0 i64.const 0 table.copy $wide $narrow"),
        ("copy-narrow-wide-length-i64-invalid", "i32.const 0 i64.const 0 i64.const 0 table.copy $narrow $wide"),
        ("copy-wide-narrow-source-i64-invalid", "i64.const 0 i64.const 0 i32.const 0 table.copy $wide $narrow"),
        ("copy-narrow-wide-source-i32-invalid", "i32.const 0 i32.const 0 i32.const 0 table.copy $narrow $wide"),
    ):
        result[name] = ("(module " + prelude + "(func $probe " + body + ") " +
                        '(func (export "_start") call $probe))', False)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "uwvm", "wasm-tools", "wasmtime", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--only-case", action="append")
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 90 or not re.fullmatch(r"sha256:[0-9a-f]{64}", args.source_id):
        parser.error("require a bounded deadline and an exact source identity")
    root = args.source_root.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 << 20, 8 << 20))
    for name in ("uwvm", "wasm_tools", "wasmtime"):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    helper = root / "test/0017.runtime/run_wasm3_fused_validation.py"
    if digest(helper) != FUSED_SHA:
        raise RuntimeError("The frozen classification/configuration helper changed")
    spec = importlib.util.spec_from_file_location("frozen_core3_width_witnesses", helper)
    fused = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fused)
    execution = root / "test/0017.runtime/fixtures/wasm3_table64_mixed_width_execution.wat"
    selected = cases(execution.read_text())
    if args.only_case:
        if set(args.only_case) - selected.keys():
            parser.error("unknown table64 case")
        selected = {key: value for key, value in selected.items() if key in args.only_case}
    modes = list(fused.configurations(args.ros, True))
    if not args.ros:
        modes.extend(("tiered-" + mode, ("-Rcc", "tiered", "-Rcm", mode, "-Rct", "0",
                      "-Rllvm-cache-path", "disable")) for mode in ("lazy", "lazy+verification"))
    immutable = [Path(__file__).resolve(), helper, execution, args.uwvm, args.wasm_tools, args.wasmtime]
    before = {str(path): digest(path) for path in immutable}
    args.out.mkdir(parents=True, exist_ok=False)
    rows, artifacts = [], {}

    def check(label, argv, expected="success", oracle=False, diagnostic=None):
        argv = list(map(str, argv))
        stdout, stderr = args.out / (label + ".stdout.log"), args.out / (label + ".stderr.log")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            try:
                process = subprocess.run(argv, stdout=out, stderr=err, timeout=args.timeout)
                code = process.returncode
            except subprocess.TimeoutExpired:
                code = None
        text = (stdout.read_bytes() + stderr.read_bytes()).decode(errors="replace")
        if oracle:
            passed = code == 0 if expected == "success" else code is not None and code > 0 and "offset" in text.lower() and "error" in text.lower()
            actual = "official-success" if code == 0 else "official-error"
        else:
            actual = fused.classify(code, text)
            passed = actual == expected and (code == 0) == (expected == "success")
        if diagnostic:
            passed = passed and diagnostic in fused.ANSI.sub("", text)
        rows.append({"label": label, "argv": argv, "exit": code, "actual": actual,
                     "expected": expected, "passed": passed, "required_diagnostic": diagnostic,
                     "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr),
                     "stdout_bytes": stdout.stat().st_size, "stderr_bytes": stderr.stat().st_size})
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(json.dumps({"label": label, "passed": passed}), flush=True)
        return passed

    for name, (wat_text, valid) in selected.items():
        wat, wasm = args.out / (name + ".wat"), args.out / (name + ".wasm")
        wat.write_text(wat_text + "\n")
        if not check(name + "-parse", [args.wasm_tools, "parse", wat, "-o", wasm], oracle=True):
            continue
        if not check(name + "-oracle", [args.wasm_tools, "validate", "--features", "all", wasm],
                     "success" if valid else "validation-error", oracle=True):
            continue
        artifacts[name] = {"wat_sha256": digest(wat), "wasm_sha256": digest(wasm)}
        if valid and not check(name + "-wasmtime", [args.wasmtime, "run", "-C", "cache=n", "-W", "memory64=y", wasm], oracle=True):
            continue
        policies = [("enabled-memory64-off", FEATURES, None)]
        if valid:
            policies.append(("table64-off", tuple(flag for flag in FEATURES if flag != "-WFE-table64") + ("-WFD-table64",), "--wasm-feature-enable-table64"))
        for policy, features, diagnostic in policies:
            for mode, flags in modes:
                expected = "validation-error" if not valid else "success"
                # Disabled table declarations fail parsing before body validation.
                if diagnostic:
                    expected = "parse-error"
                check(name + "-" + policy + "-" + mode.replace("+", "-"),
                      [args.uwvm, *flags, *features, "--run", wasm], expected, diagnostic=diagnostic)
    after = {str(path): digest(path) for path in immutable}
    passed = len(artifacts) == len(selected) and bool(rows) and all(row["passed"] for row in rows) and before == after
    summary = {"passed": passed, "source_id": args.source_id, "source_id_is_build_receipt_input": True,
               "repository": "ros" if args.ros else "ordinary", "cases": len(selected), "checks": len(rows),
               "modes": [mode for mode, _ in modes], "artifacts": artifacts,
               "immutable_before": before, "immutable_after": after,
               "failures": [row for row in rows if not row["passed"]],
               "tiered_is_entry_semantics_not_promotion_receipt": True,
               "cgroup": {key: Path("/sys/fs/cgroup", key).read_text().strip() for key in
                          ("memory.max", "memory.swap.max", "cpuset.cpus.effective")}}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
