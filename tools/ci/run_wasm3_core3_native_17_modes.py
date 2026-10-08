#!/usr/bin/env python3
"""Source-bound native Linux execution of 17 self-checking Core 3 witnesses.

This qualifies runtime backends and feature gates. It is not an official WAST
assertion runner, and a passing witness matrix is not full Core 3 conformance.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import sys

from wasm3_compile_fatal_policy import (
    negative_classification, prove_compile_fatal_trap,
)
from wasm3_tiered_native_entry_policy import (
    prepare_native_entry_site, prove_forced_t1_raw_entry,
)


CASES = (
    "call-ref-step", "gc-struct-heap-update", "gc-array-heap-update",
    "gc-cast", "memory64-random-store", "table64-indirect",
    "multi-memory-random-store", "extended-const-init", "tail-call-step",
    "eh-caught-step", "memory64-atomic-rmw",
    "memory32-atomic-wait-mismatch", "memory32-atomic-notify-empty",
    "memory64-atomic-wait-mismatch", "memory64-atomic-notify-empty",
    "relaxed-swizzle", "strict-swizzle",
)
GENERATOR_SHA256 = "48f5edd953e81452ef7ddd307c32e89d56e0897800ac3cc05793b4d65a665de7"
BASE = ("all-proposals=n", "bulk-memory=y", "multi-value=y",
        "reference-types=y", "simd=y")
FEATURES = {
    "gc": ("function-references=y", "gc=y"),
    "function-references": ("function-references=y",),
    "memory64": ("memory64=y",),
    "table64": ("memory64=y",),
    "multi-memory": ("multi-memory=y",),
    "extended-const": ("extended-const=y",),
    "tail-call": ("tail-call=y",),
    "threads": ("threads=y", "shared-memory=y"),
    "exceptions": ("exceptions=y",),
    "relaxed-simd": ("relaxed-simd=y",),
    "simd": (),
}
ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
FUNCTION_ID = re.compile(r"\bfn=(\d+)\b")


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def cgroup_events():
    return {name: int(value) for name, value in
            (line.split() for line in Path("/sys/fs/cgroup/memory.events").read_text().splitlines())}


def cgroup_state():
    root = Path("/sys/fs/cgroup")
    return {name: (root / name).read_text().strip() for name in
            ("memory.max", "memory.swap.max", "memory.current", "memory.peak",
             "cpuset.cpus.effective", "cpu.stat")}


def fingerprint(root, manifest):
    return subprocess.check_output(
        [sys.executable, str(root / "tools/ci/wasm3_source_fingerprint.py"),
         str(root), str(manifest)], text=True).strip()


def compiled_on_demand(log, backend, request_kinds=("demand-request",)):
    """Return function IDs whose demand preceded successful compilation."""
    requests = {}
    compiled = set()
    for offset, line in enumerate(log.splitlines()):
        if not line.startswith(f"[{backend}] "):
            continue
        found = FUNCTION_ID.search(line)
        if found is None:
            continue
        function_id = int(found.group(1))
        if any(" " + kind + " " in line for kind in request_kinds):
            requests.setdefault(function_id, offset)
        elif (" compile-end " in line and " state=compiled" in line and
              function_id in requests and requests[function_id] < offset):
            compiled.add(function_id)
    return sorted(compiled)


def tiered_evidence(log, forced_t1=False, native_entry_proof=None):
    """Prove active T0 demand or a completed native invocation, without forcing policy.

    The product intentionally compiles small hot-loop modules directly into T1.
    Therefore auto-tiered does not promise T0 for every module. Native compilation
    alone is insufficient: tiered_switches records completed native invocations.
    T0's ordered lazy demand is issued by the active interpreter entry path.
    """
    interpreter = compiled_on_demand(log, "uwvm-int-lazy")
    native = compiled_on_demand(log, "llvm-jit-lazy",
                               ("demand-request", "tiered-demand-request"))
    summaries = [line for line in log.splitlines()
                 if line.startswith("[tiered-lazy] summary ")]
    summary = summaries[-1] if summaries else ""
    counters = {key: int(value) for key, value in re.findall(r"\b(\w+)=(\d+)\b", summary)}
    native_entered = (counters.get("tiered_switches", 0) > 0 or
                      counters.get("tiered_osr_ready", 0) > 0 or
                      " tiered-osr-enter " in log)
    if forced_t1:
        # T0/T2-disabled mode executes LLVM lazy directly and never performs a
        # T0-to-native transition. Compilation alone still does not pass: GDB
        # must step this actual ELF's raw call into its generated RX mapping.
        native_entered = bool(native_entry_proof and
                              native_entry_proof.get("entered_generated_raw_entry"))
    phases = []
    if interpreter:
        phases.append("T0")
    if native and native_entered:
        phases.append("T1")
    witnessed = bool(phases)
    if forced_t1:
        witnessed = (phases == ["T1"] and
                      "[uwvm-int-lazy] demand-request " not in log)
    return {"tiered_phases": phases, "tiered_phase_witnessed": witnessed,
            "tiered_compiled_function_ids": {"T0": interpreter, "T1": native},
            "tiered_native_invocation_witnessed": native_entered,
            "tiered_native_invocation_witness_basis": (
                "actual GDB raw-call single step and normal guest return" if forced_t1 else
                "actual native-switch/OSR runtime evidence"),
            "tiered_runtime_counters": counters,
            "tiered_policy": "forced T1, T0/T2 disabled" if forced_t1 else "automatic T0/T1 selection"}


def modes(ros):
    if ros:
        return {
            "int-full": ["-Rint"],
            "jit-full-instruction": ["-Raot", "-Rllvm-full-policy", "pb-o3",
                                     "-Rllvm-call-stack", "instruction", "-Rllvm-cache-path", "disable"],
            "jit-full-unwind": ["-Raot", "-Rllvm-full-policy", "pb-o3",
                                "-Rllvm-call-stack", "unwind", "-Rllvm-cache-path", "disable"],
        }
    result = {f"int-{kind}": ["-Rcc", "int", "-Rcm", kind]
              for kind in ("full", "lazy", "lazy+verification")}
    for kind in ("full", "lazy"):
        for trace in ("instruction", "unwind"):
            result[f"jit-{kind}-{trace}"] = [
                "-Rcc", "jit", "-Rcm", kind, "-Rllvm-call-stack", trace,
                "-Rllvm-cache-path", "disable",
            ]
    for trace in ("instruction", "unwind"):
        result[f"tiered-lazy-{trace}"] = [
            "-Rcc", "tiered", "-Rcm", "lazy",
            "-Rllvm-call-stack", trace, "-Rllvm-cache-path", "disable",
        ]
        result[f"tiered-t1-lazy-{trace}"] = [
            "-Rcc", "tiered", "-Rcm", "lazy", "-Rtiered-disable-t0",
            "-Rtiered-disable-t2", "-Rllvm-call-stack", trace,
            "-Rllvm-cache-path", "disable",
        ]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--build-json", type=Path, required=True)
    parser.add_argument("--fixture-root", type=Path, required=True)
    parser.add_argument("--fixture-summary", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--mode", action="append")
    parser.add_argument("--keep-going", action="store_true",
                        help="collect all product execution failures; input/oracle failures still stop")
    parser.add_argument("--gdb", type=Path,
                        help="prove compile-fatal SIGILL PCs and forced T1 generated-entry PCs")
    parser.add_argument("--llvm-nm", type=Path, default=Path("/toolchain/bin/llvm-nm"))
    parser.add_argument("--llvm-objdump", type=Path, default=Path("/toolchain/bin/llvm-objdump"))
    args = parser.parse_args()

    source = args.source.resolve(strict=True)
    subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source_id = fingerprint(source, output / "source-before.json")
    if source_id != args.expected_source_id:
        raise RuntimeError("source fingerprint differs from the explicitly requested candidate")
    build_path = args.build_json.resolve(strict=True)
    build = json.loads(build_path.read_text())
    binary = build_path.parent / "uwvm"
    binary_sha = sha(binary)
    if (build.get("source_id") != source_id or
            Path(build.get("source", "")).resolve() != source or
            build.get("binary_sha256") != binary_sha or
            build.get("binary_embedded_source_ids") != [source_id]):
        raise RuntimeError("source, build manifest or native binary differs")
    fixtures = args.fixture_root.resolve(strict=True)
    bundle_path = args.fixture_summary.resolve(strict=True)
    bundle = json.loads(bundle_path.read_text())
    fixture_rows = {row["case"]: row for row in bundle["fixtures"]}
    if (not bundle.get("passed") or bundle.get("count") != 8192 or
            bundle.get("generator_sha256") != GENERATOR_SHA256 or
            bundle.get("checked") != len(CASES) or
            set(fixture_rows) != set(CASES) or
            bundle.get("source_bound_product_test") is not False):
        raise RuntimeError("17-case immutable witness bundle differs")
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    version = subprocess.check_output([wasmtime, "--version"], text=True).strip()
    if not version.startswith("wasmtime 49.0.1 "):
        raise RuntimeError(f"Wasmtime 49.0.1 required: {version}")
    selected_cases = tuple(dict.fromkeys(args.case or CASES))
    available = modes(args.ros)
    selected_modes = tuple(dict.fromkeys(args.mode or available))
    if not selected_modes or any(mode not in available for mode in selected_modes):
        parser.error(f"mode must be one of: {', '.join(available)}")
    environment = os.environ.copy()
    before_cgroup = cgroup_state()
    before_events = cgroup_events()
    debugger = args.gdb.resolve(strict=True) if args.gdb is not None else None
    debugger_sha = sha(debugger) if debugger is not None else None
    input_hashes = {path: sha(path) for path in
                    (Path(__file__), Path(__file__).with_name("wasm3_compile_fatal_policy.py"),
                     Path(__file__).with_name("wasm3_tiered_native_entry_policy.py"),
                     build_path, bundle_path, wasm_tools, wasmtime)}
    native_entry_site = None
    if debugger is not None and any(mode.startswith("tiered-t1-") for mode in selected_modes):
        llvm_nm = args.llvm_nm.resolve(strict=True)
        llvm_objdump = args.llvm_objdump.resolve(strict=True)
        native_entry_site = prepare_native_entry_site(binary, output,
            llvm_nm=llvm_nm, llvm_objdump=llvm_objdump)
        input_hashes.update({llvm_nm: sha(llvm_nm), llvm_objdump: sha(llvm_objdump)})
    rows = []

    def execute(label, command, diagnostic=None, *, product_execution=False):
        command = [str(item) for item in command]
        timed_out = False
        try:
            result = subprocess.run(command, env=environment, capture_output=True, timeout=180)
            raw, status = result.stdout + result.stderr, result.returncode
        except subprocess.TimeoutExpired as failure:
            timed_out = True
            raw = (failure.stdout or b"") + (failure.stderr or b"")
            status = -124
        log = output / (label + ".log")
        log.write_bytes(raw)
        plain = ANSI.sub(b"", raw).decode(errors="replace")
        classification = None
        proof = None
        if (not timed_out and diagnostic is not None and debugger is not None and
                status == -4 and diagnostic in plain):
            proof = prove_compile_fatal_trap(
                command, diagnostic, debugger, output / (label + ".fatal-pc.log"),
                binary_sha256=binary_sha, environment=environment)
        if diagnostic is None:
            passed = not timed_out and status == 0
        else:
            classification, passed = negative_classification(status, diagnostic in plain, proof)
            passed = not timed_out and passed
        row = {"label": label, "passed": passed, "exit": status,
               "timed_out": timed_out, "expected_diagnostic": diagnostic,
               "command": command, "log_sha256": sha(log)}
        if diagnostic is not None:
            row["negative_classification"] = classification
            row["strict_regular_diagnostic_exit_passed"] = not timed_out and status > 0 and diagnostic in plain
        if proof is not None:
            row["fatal_trap_proof"] = proof
        rows.append(row)
        save(output / "progress.json", rows)
        if not passed and not (args.keep_going and product_execution):
            raise RuntimeError(f"{label}: exit {status}; see {log}")
        return row

    for case in selected_cases:
        ref = fixture_rows[case]
        wat, wasm = fixtures / (case + ".wat"), fixtures / (case + ".wasm")
        if sha(wat) != ref["wat_sha256"] or sha(wasm) != ref["wasm_sha256"]:
            raise RuntimeError(f"{case}: fixture hash differs")
        features = ref["features"]
        if any(feature not in FEATURES for feature in features):
            raise RuntimeError(f"{case}: unknown feature")
        parsed = output / (case + ".wasm")
        execute(case + "-parse", [wasm_tools, "parse", wat, "-o", parsed])
        if sha(parsed) != sha(wasm):
            raise RuntimeError(f"{case}: reparsed Wasm differs")
        execute(case + "-validate", [wasm_tools, "validate", "--features", "all", wasm])
        oracle = dict.fromkeys((*BASE, *(flag for feature in features for flag in FEATURES[feature])))
        execute(case + "-wasmtime49", [wasmtime, "-C", "cache=n", "-W",
                                        ",".join(oracle), wasm])
        enabled = ["-WFE-" + feature for feature in features]
        for mode in selected_modes:
            label = case + "-" + mode
            compile_log = output / (label + ".compile.log")
            logging = ["-Rclog", "file", compile_log] if mode.startswith("tiered-") else []
            row = execute(label, [binary, *available[mode], "-Rct", "0", *logging,
                                  *enabled, "--run", wasm], product_execution=True)
            if logging:
                if not row["passed"]:
                    continue
                if not compile_log.is_file():
                    row["passed"] = False
                    row["failure_reason"] = "tiered compiler log missing"
                    save(output / "progress.json", rows)
                    if args.keep_going:
                        continue
                    raise RuntimeError(f"{label}: tiered compiler log missing")
                compilation = compile_log.read_text()
                native_entry_proof = None
                if mode.startswith("tiered-t1-") and native_entry_site is not None:
                    native_entry_proof = prove_forced_t1_raw_entry(
                        row["command"], debugger, output / (label + ".native-entry.log"),
                        native_entry_site, binary_sha256=binary_sha, environment=environment)
                    row["tiered_native_entry_proof"] = native_entry_proof
                evidence = tiered_evidence(compilation, mode.startswith("tiered-t1-"), native_entry_proof)
                row.update(evidence)
                row["compiler_log_sha256"] = sha(compile_log)
                save(output / "progress.json", rows)
                if not evidence["tiered_phase_witnessed"]:
                    row["passed"] = False
                    row["failure_reason"] = "configured tier was not proven by active demand and execution counters"
                    save(output / "progress.json", rows)
                    if args.keep_going:
                        continue
                    raise RuntimeError(f"{label}: configured tier was not witnessed in {compile_log}")
        for feature in dict.fromkeys(features):
            disabled = ["-WFD-" + feature if item == feature else "-WFE-" + item
                        for item in features]
            diagnostic = ("requires simd" if feature == "simd" else
                          "--wasm-feature-enable-" + feature)
            for mode in selected_modes:
                execute(case + "-" + feature + "-off-" + mode,
                        [binary, *available[mode], "-Rct", "0", *disabled, "--run", wasm],
                        diagnostic, product_execution=True)

    after_id = fingerprint(source, output / "source-after.json")
    after_cgroup = cgroup_state()
    after_events = cgroup_events()
    feature_checks = sum(len(fixture_rows[case]["features"]) for case in selected_cases)
    expected = len(selected_cases) * (3 + len(selected_modes)) + feature_checks * len(selected_modes)
    if (after_id != source_id or sha(binary) != binary_sha or
            (debugger is not None and sha(debugger) != debugger_sha) or
            len(rows) != expected or any(after_events[key] != before_events[key]
                                         for key in ("oom", "oom_kill"))):
        raise RuntimeError("source, binary, cgroup or coverage changed")
    if any(sha(path) != expected_sha for path, expected_sha in input_hashes.items()):
        raise RuntimeError("runner, fatal policy, build, fixture or oracle changed during qualification")
    passed_checks = sum(row["passed"] for row in rows)
    tiered_coverage = {}
    for trace in ("instruction", "unwind"):
        auto_mode = "tiered-lazy-" + trace
        if auto_mode in selected_modes:
            tiered_coverage[trace] = {
                phase: [row["label"] for row in rows
                        if row["label"].endswith("-" + auto_mode) and row["expected_diagnostic"] is None
                        and row["passed"] and phase in row.get("tiered_phases", [])]
                for phase in ("T0", "T1")}
    full_tiered_coverage = (selected_cases != CASES or args.ros or
                           all(all(witnesses.values()) for witnesses in tiered_coverage.values()))
    summary = {"passed": passed_checks == len(rows), "source_id": source_id, "binary_sha256": binary_sha,
               "build_json_sha256": sha(build_path), "product": "ros" if args.ros else "ordinary",
               "cases": selected_cases, "modes": selected_modes, "checks": len(rows),
               "passed_checks": passed_checks,
               "complete_matrix": (selected_cases == CASES and
                                   selected_modes == tuple(available)),
               "full_matrix_expected_checks": (len(CASES) * (3 + len(available)) +
                                               sum(len(fixture_rows[case]["features"])
                                                   for case in CASES) * len(available)),
               "fixture_generator_sha256": GENERATOR_SHA256,
               "fixture_summary_sha256": sha(bundle_path), "wasm_tools_sha256": sha(wasm_tools),
               "wasmtime_sha256": sha(wasmtime), "wasmtime_version": version,
               "runner_sha256": sha(Path(__file__)), "rows": rows,
               "negative_policy": "individually GDB-PC-qualified compile fatal traps" if debugger else "regular positive diagnostic exits only",
               "debugger_sha256": debugger_sha,
               "negative_policy_sha256": sha(Path(__file__).with_name("wasm3_compile_fatal_policy.py")),
               "native_entry_policy_sha256": sha(Path(__file__).with_name("wasm3_tiered_native_entry_policy.py")),
               "native_entry_site": native_entry_site,
               "tiered_auto_coverage": tiered_coverage,
               "tiered_auto_both_phases_witnessed": full_tiered_coverage,
               "cgroup_before": before_cgroup, "cgroup_after": after_cgroup,
               "memory_events_before": before_events, "memory_events_after": after_events,
               "tiered_phase_scope": ("not applicable to ROS full-only modes" if args.ros else
                                      "actual auto T0/T1 and forced T1; this witness matrix does not qualify T2"),
               "scope": "selected self-checking Core 3 witnesses; not all official WAST assertions"}
    summary["passed"] = summary["passed"] and full_tiered_coverage
    save(output / "summary.json", summary)
    status = "PASS" if summary["passed"] else "FAIL"
    print(f"{status} {summary['product']} native Core 3 witnesses: "
          f"{passed_checks}/{expected} checks passed", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
