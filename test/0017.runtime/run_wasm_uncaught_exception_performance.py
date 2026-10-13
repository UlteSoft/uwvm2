#!/usr/bin/env python3
"""Nine-pair source-bound Core 3 uncaught-exception diagnostic timing.

One throw ends each process. Report whole-process latency only; these samples
cannot isolate per-throw cost from startup/JIT compilation or terminal output.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import time


BENCH = Path(__file__).resolve().parents[2] / "benchmark/0004.wasm3-core"
sys.path.insert(0, str(BENCH))
from run import (cgroup_preflight, cpu_telemetry, engine_order_for_pair, run_one,
                 sha256, verify_product_build)


ANSI = re.compile(rb"\x1b\[[0-9;]*m")
CASES = {"struct": ("exnref_uncaught_struct_execution.wat", "-WFE-gc"),
         "exn": ("exnref_uncaught_exn_execution.wat", "-WFD-gc")}


def check_diagnostic(log, kind, engine):
    plain = ANSI.sub(b"", log).decode(errors="replace")
    if engine == "wasmtime":
        return "wasm backtrace" in plain and "error" in plain.lower()
    return (b"\x1b[" in log and "Uncaught WebAssembly exception" in plain and
            f"payload[0] wasm_reference kind={kind} opaque_token=0x" in plain and
            "entry_func_idx=0" in plain and "func_idx=0" in plain and
            "Wasm call stack captured at throw" in plain and
            "Validation error" not in plain and "truncated" not in plain.lower())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--pairs", type=int, default=9)
    parser.add_argument("--cpu", type=int, default=0)
    args = parser.parse_args()
    if args.out.exists() or args.pairs < 9 or args.cpu not in (0, 2, 4, 6):
        parser.error("fresh output, at least nine pairs and a configured P core required")
    before = cgroup_preflight(args.cpu)
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    products = {name: verify_product_build(name, binary, args.out, "before")
                for name, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected_ids = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {name: item["source_id"] for name, item in products.items()} != expected_ids:
        raise RuntimeError("uncaught EH benchmark product source IDs differ from intended builds")
    if sha256(Path(products["ordinary"]["source"]) /
              "test/0017.runtime/run_wasm_uncaught_exception_performance.py") != sha256(Path(__file__)):
        raise RuntimeError("ordinary frozen runner differs from staged runner")
    if sha256(Path(products["ros"]["source"]) /
              "test/0017.runtime/run_wasm_uncaught_exception_performance.py") != sha256(Path(__file__)):
        raise RuntimeError("ROS frozen runner differs from staged runner")
    inputs = {"products": products, "source_ids": expected_ids,
              "runner_sha256": sha256(Path(__file__)),
              "wasm_tools_sha256": sha256(args.wasm_tools),
              "wasmtime_sha256": sha256(args.wasmtime),
              "wasm_tools": str(args.wasm_tools), "wasmtime": str(args.wasmtime),
              "pairs": args.pairs, "cpu": args.cpu, "cgroup_before": before,
              "scope": "one uncaught throw per process; no per-throw cost claim"}
    modules = {}
    for kind, (filename, _) in CASES.items():
        source = Path(products["ordinary"]["source"]) / "test/0017.runtime/fixtures" / filename
        peer = Path(products["ros"]["source"]) / "test/0017.runtime/fixtures" / filename
        if sha256(source) != sha256(peer):
            raise RuntimeError(f"{kind}: product fixture WAT differs")
        binary = args.out / (kind + ".wasm")
        subprocess.run([str(args.wasm_tools), "parse", str(source), "-o", str(binary)],
                       check=True, timeout=30)
        subprocess.run([str(args.wasm_tools), "validate", str(binary)], check=True, timeout=30)
        modules[kind] = binary
    inputs["wat_sha256"] = {kind: sha256(Path(products["ordinary"]["source"]) /
                                  "test/0017.runtime/fixtures" / value[0])
                            for kind, value in CASES.items()}
    inputs["wasm_sha256"] = {kind: sha256(path) for kind, path in modules.items()}
    (args.out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    profiles = (("ordinary-int", "ordinary", "int", None),
                ("ordinary-jit-unwind", "ordinary", "jit", "unwind"),
                ("ordinary-jit-instruction", "ordinary", "jit", "instruction"),
                ("ros-int", "ros", "int", None),
                ("ros-jit-unwind", "ros", "jit", "unwind"),
                ("ros-jit-instruction", "ros", "jit", "instruction"),
                ("wasmtime", "wasmtime", "native", None))

    def command(profile, kind):
        label, product, compiler, policy = profile
        module = modules[kind]
        if product == "wasmtime":
            return ["taskset", "-c", str(args.cpu), str(args.wasmtime), "run",
                    "-C", "cache=n", "-W", "exceptions=y", "-W", "gc=y", str(module)]
        binary = args.ordinary if product == "ordinary" else args.ros
        selected = (["-Rcc", "int" if compiler == "int" else "jit", "-Rcm", "full"]
                    if product == "ordinary" else ["-Rint" if compiler == "int" else "-Raot"])
        trace = (["-Rllvm-call-stack", policy, "-Rllvm-cache-path", "disable"]
                 if policy else [])
        return ["taskset", "-c", str(args.cpu), str(binary), *selected, *trace,
                "-WFE-exceptions", "-WFD-function-references", CASES[kind][1],
                "--log-color", "enable", "--run", str(module)]

    exclusions = []

    def run_checked(profile, kind, label, allow_unsupported=False):
        invocation = command(profile, kind)
        result = run_one(invocation, label, logs, os.environ.copy(),
                         allow_failure=True, timeout_seconds=90)
        log = logs / (label + ".log")
        output = log.read_bytes()
        if (allow_unsupported and profile[1] == "wasmtime" and
                result["exit"] != 0 and b"unsupported feature" in output.lower()):
            exclusions.append({"engine": profile[0], "case": kind,
                               "reason": "comparator rejected Core 3 syntax",
                               "exit": result["exit"], "command": invocation,
                               "log": str(log), "log_sha256": sha256(log)})
            return None
        if result["exit"] == 0 or not check_diagnostic(output, kind, profile[1]):
            raise RuntimeError(f"{label}: uncaught diagnostic did not meet semantic/color checks")
        result.update(engine=profile[0], case=kind, log_sha256=sha256(log))
        return result

    supported = {}
    for profile in profiles:
        enabled = set()
        for kind in CASES:
            if run_checked(profile, kind, f"warmup-{profile[0]}-{kind}",
                           allow_unsupported=True) is not None:
                enabled.add(kind)
        supported[profile[0]] = enabled
    (args.out / "qualification.json").write_text(json.dumps(
        {"supported": {name: sorted(kinds) for name, kinds in supported.items()},
         "excluded": exclusions}, indent=2) + "\n")
    rows = []
    for pair in range(args.pairs):
        order = engine_order_for_pair(list(range(len(profiles))), pair)
        kind_order = tuple(CASES) if pair % 2 == 0 else tuple(reversed(CASES))
        telemetry_before = cpu_telemetry(args.cpu)
        for index in order:
            profile = profiles[index]
            for kind in kind_order:
                if kind not in supported[profile[0]]:
                    continue
                row = run_checked(profile, kind, f"pair{pair}-{profile[0]}-{kind}")
                row.update(pair=pair, engine_order=order, case_order=kind_order)
                rows.append(row)
                with (args.out / "raw.jsonl").open("a") as stream:
                    stream.write(json.dumps(row) + "\n")
        with (args.out / "pair-telemetry.jsonl").open("a") as stream:
            stream.write(json.dumps({"pair": pair, "engine_order": order,
                                     "before": telemetry_before,
                                     "after": cpu_telemetry(args.cpu)}) + "\n")
    summary = []
    for profile in profiles:
        for kind in CASES:
            if kind not in supported[profile[0]]:
                continue
            values = [row["elapsed_ns"] for row in rows
                      if row["engine"] == profile[0] and row["case"] == kind]
            summary.append({"engine": profile[0], "case": kind, "samples": len(values),
                            "median_process_ms": statistics.median(values) / 1e6,
                            "p95_process_ms_estimate": statistics.quantiles(
                                values, n=20, method="inclusive")[18] / 1e6,
                            "p99_qualified": False,
                            "sub_100ms_sample": min(values) < 100000000,
                            "latency_scope": "full process including startup/JIT/diagnostic output"})
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    after = cgroup_preflight(args.cpu)
    products_after = {name: verify_product_build(name, binary, args.out, "after")
                      for name, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    first = dict(line.split() for line in before["memory.events"].splitlines())
    last = dict(line.split() for line in after["memory.events"].splitlines())
    first_cpu = dict(line.split() for line in before["cpu.stat"].splitlines())
    last_cpu = dict(line.split() for line in after["cpu.stat"].splitlines())
    if (products_after != products or
            any(first[name] != last[name] for name in ("oom", "oom_kill")) or
            any(first_cpu.get(name) != last_cpu.get(name)
                for name in ("nr_throttled", "throttled_usec")) or
            any(sha256(path) != inputs["wasm_sha256"][kind] for kind, path in modules.items()) or
            sha256(Path(__file__)) != inputs["runner_sha256"] or
            sha256(args.wasm_tools) != inputs["wasm_tools_sha256"] or
            sha256(args.wasmtime) != inputs["wasmtime_sha256"]):
        raise RuntimeError("uncaught EH timing invalidated by inputs or cgroup changes")
    inputs["cgroup_after"] = after
    inputs["comparator_exclusions"] = exclusions
    inputs["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    print("PASS paired uncaught Core 3 diagnostics; whole-process timing only")


if __name__ == "__main__":
    main()
