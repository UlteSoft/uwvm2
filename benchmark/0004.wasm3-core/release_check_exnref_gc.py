#!/usr/bin/env python3
"""Opt-in, source-bound RSS/collection gate for discarded exnrefs and cycles.

The current monotonic store has no authoritative collection counter and is
expected to FAIL. RSS-only plateau or a selected case/mode subset never
qualifies a collector.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import time

from generate_exnref_reclamation import CASES, expected_after, module
from release_check_gc import PRODUCT_ENGINES, release_gate_reasons
from run import LIB_PATHS, cgroup_preflight, command, run_one, sha256, verify_product_build


def gate_reasons(selected_cases, selected_engines, rss_failures, collection_counts):
    reasons = []
    missing_cases = set(CASES) - set(selected_cases)
    if missing_cases:
        reasons.append("exnref workloads not tested: " + ", ".join(sorted(missing_cases)))
    for case in selected_cases:
        case_reasons = release_gate_reasons(
            selected_engines, rss_failures.get(case, set()),
            collection_counts.get(case, {}))
        reasons.extend(f"{case}: {reason}" for reason in case_reasons)
    return reasons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--wasmtime", type=Path,
                        help="optional same-Wasm copying-collector oracle")
    parser.add_argument("--case", action="append", choices=CASES,
                        help="repeat for staged cases; only both cases can clear the gate")
    parser.add_argument("--engine", action="append", choices=PRODUCT_ENGINES,
                        help="repeat for staged modes; only all four can clear the gate")
    parser.add_argument("--counts", type=int, nargs=3,
                        default=(100000, 1000000, 10000000),
                        metavar=("LOW", "MIDDLE", "HIGH"))
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--rss-plateau-mib", type=float, default=64.0)
    parser.add_argument("--cpu", type=int, default=0)
    args = parser.parse_args()
    if (args.out.exists() or not 0 < args.counts[0] < args.counts[1] < args.counts[2] <= 0xffffffff
            or args.samples < 3 or args.rss_plateau_mib <= 0):
        parser.error("fresh output, three increasing counts, >=3 samples and positive RSS limit required")
    selected_cases = tuple(dict.fromkeys(args.case or CASES))
    selected_engines = tuple(dict.fromkeys(args.engine or PRODUCT_ENGINES))
    before = cgroup_preflight(args.cpu)
    args.out.mkdir(parents=True)
    logs, fixtures = args.out / "logs", args.out / "fixtures"
    logs.mkdir()
    fixtures.mkdir()
    for name in (Path(__file__).name, "generate_exnref_reclamation.py", "run.py",
                 "release_check_gc.py"):
        shutil.copyfile(Path(__file__).with_name(name), args.out / name)
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected_ids = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != expected_ids:
        raise RuntimeError("exnref GC gate does not match intended frozen source IDs")
    staged_generator = args.out / "generate_exnref_reclamation.py"
    for label, build in builds.items():
        source = Path(build["source"])
        for name in ("generate_exnref_reclamation.py", "release_check_exnref_gc.py"):
            peer = source / "benchmark/0004.wasm3-core" / name
            if sha256(peer) != sha256(args.out / name):
                raise RuntimeError(f"{label}: frozen snapshot differs from staged {name}")
    products = {"ordinary-jit": args.ordinary, "ordinary-int": args.ordinary,
                "ros-jit": args.ros, "ros-int": args.ros}
    engines = {name: products[name] for name in selected_engines}
    if args.wasmtime:
        engines["wasmtime-copying"] = args.wasmtime
    meta = {"scope": "dropped exnref and exnref/aggregate cycle reclamation; not throughput",
            "reference_specification": "WebAssembly 3.0 (2026-09-21)",
            "source_builds": builds, "expected_source_ids": expected_ids,
            "tools_sha256": {"wasm-tools": sha256(args.wasm_tools),
                             "generator": sha256(staged_generator),
                             "runner": sha256(args.out / Path(__file__).name),
                             "run": sha256(args.out / "run.py"),
                             "gc_gate": sha256(args.out / "release_check_gc.py")},
            "engine_binary_sha256": {name: sha256(binary) for name, binary in engines.items()},
            "selected_cases": selected_cases, "selected_product_modes": selected_engines,
            "counts": args.counts, "samples": args.samples,
            "rss_plateau_mib": args.rss_plateau_mib, "cgroup_before": before,
            "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    wasm = {}
    for case in selected_cases:
        for count in args.counts:
            source, features = module(case, count)
            wat = fixtures / f"{case}-{count}.wat"
            binary = wat.with_suffix(".wasm")
            wat.write_text(source)
            subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(binary)], check=True)
            subprocess.run([str(args.wasm_tools), "validate", str(binary)], check=True)
            wasm[(case, count)] = (binary, features)
    meta["fixtures"] = {case: {str(count): {
        "wat_sha256": sha256(wasm[(case, count)][0].with_suffix(".wat")),
        "wasm_sha256": sha256(wasm[(case, count)][0]),
        "iterations": count, "expected_lcg": expected_after(count),
        "features": wasm[(case, count)][1]}
        for count in args.counts} for case in selected_cases}
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    rows = []
    unavailable = set()
    for case in selected_cases:
        for repetition in range(args.samples):
            counts = args.counts if repetition % 2 == 0 else tuple(reversed(args.counts))
            order = tuple(engines) if repetition % 2 == 0 else tuple(reversed(engines))
            for count in counts:
                binary, features = wasm[(case, count)]
                for engine in order:
                    if (case, engine) in unavailable:
                        continue
                    invocation = command(engine, engines[engine], binary, features,
                                         "unwind", args.cpu)
                    row = run_one(invocation, f"{case}-{engine}-{count}-sample{repetition}",
                                  logs, env, allow_failure=engine == "wasmtime-copying",
                                  timeout_seconds=600)
                    row.update(case=case, engine=engine, count=count, sample=repetition,
                               rss_mib=row["maxrss_kib"] / 1024)
                    rows.append(row)
                    with (args.out / "raw.jsonl").open("a") as output:
                        output.write(json.dumps(row) + "\n")
                    if row["exit"]:
                        unavailable.add((case, engine))
                        print(case, engine, "unsupported or failed; excluded", flush=True)
    (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
    results = []
    rss_failures = {}
    for case in selected_cases:
        rss_failures[case] = set()
        for engine in selected_engines:
            peaks = {count: statistics.median(row["rss_mib"] for row in rows
                    if row["case"] == case and row["engine"] == engine and
                    row["count"] == count and row["exit"] == 0)
                     for count in args.counts}
            growth = peaks[args.counts[2]] - peaks[args.counts[1]]
            passed = growth <= args.rss_plateau_mib
            results.append({"case": case, "engine": engine,
                            "median_peak_rss_mib": peaks, "high_minus_middle_mib": growth,
                            "rss_plateau_subcheck_passed": passed})
            if not passed:
                rss_failures[case].add(engine)
    collection_counts = {}  # No source-bound runtime counter currently exists.
    reasons = gate_reasons(selected_cases, selected_engines, rss_failures,
                           collection_counts)
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    state_after = cgroup_preflight(args.cpu)
    first_events = dict(line.split() for line in before["memory.events"].splitlines())
    last_events = dict(line.split() for line in state_after["memory.events"].splitlines())
    if (after != builds or
            any(sha256(args.out / name) != meta["tools_sha256"][key]
                for name, key in ((Path(__file__).name, "runner"),
                                  ("generate_exnref_reclamation.py", "generator"),
                                  ("run.py", "run"), ("release_check_gc.py", "gc_gate"))) or
            sha256(args.wasm_tools) != meta["tools_sha256"]["wasm-tools"] or
            any(sha256(binary) != meta["engine_binary_sha256"][name]
                for name, binary in engines.items()) or
            any(first_events[name] != last_events[name] for name in ("oom", "oom_kill"))):
        raise RuntimeError("exnref release gate invalidated by source, tool or OOM drift")
    meta.update(source_builds_after=after, cgroup_after=state_after,
                collection_counts=collection_counts, release_gate_reasons=reasons,
                release_gate_passed=not reasons,
                end_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    (args.out / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    for reason in reasons:
        print("FATAL exnref GC reclamation blocker:", reason)
    if any(rss_failures.values()):
        return 2
    return 3 if reasons else 0


if __name__ == "__main__":
    sys.exit(main())
