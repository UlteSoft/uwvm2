#!/usr/bin/env python3
"""Opt-in release gate for bounded Core 3 GC memory under a fixed root set.

Default performance runners record results without gating. This checker is
deliberately stricter: after semantic self-checks it exits nonzero when the
10M-allocation peak RSS still grows materially beyond the 1M point despite
the module exporting only 1,024 replaceable roots.
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

from generate import module
from run import (LIB_PATHS, cgroup_preflight, command, run_one,
                 sha256, verify_product_build)

PRODUCT_ENGINES = ("ordinary-jit", "ordinary-int", "ros-jit", "ros-int")


def release_gate_reasons(selected_engines, rss_failures, collection_counts):
    """A bounded RSS subcheck is insufficient without all modes and real GC work.

    `collection_counts` must eventually come from a source-bound runtime
    counter, not an inferred RSS plateau or a synthetic guest checksum. The
    present runtime exposes no such counter, so callers pass an empty map and
    this release gate intentionally remains closed after an RSS-only pass.
    """
    reasons = []
    missing = set(PRODUCT_ENGINES) - set(selected_engines)
    if missing:
        reasons.append("product modes not tested: " + ", ".join(sorted(missing)))
    failed = set(rss_failures)
    if failed:
        reasons.append("bounded-RSS check failed: " + ", ".join(sorted(failed)))
    unverified = [engine for engine in PRODUCT_ENGINES
                  if engine in selected_engines and
                  (collection_counts.get(engine) is None or collection_counts[engine] <= 0)]
    if unverified:
        reasons.append("no measured positive collection count: " +
                       ", ".join(unverified))
    return reasons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True,
                        help="full intended frozen source ID for the ordinary product")
    parser.add_argument("--ros-source-id", required=True,
                        help="full intended frozen source ID for the ROS product")
    parser.add_argument("--wasmtime", type=Path,
                        help="optional copying-collector workload reference")
    parser.add_argument("--counts", type=int, nargs=3, default=(100000, 1000000, 10000000),
                        metavar=("LOW", "MIDDLE", "HIGH"))
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--rss-plateau-mib", type=float, default=64.0)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--engine", action="append", choices=("ordinary-jit", "ros-jit",
                        "ordinary-int", "ros-int"), help="restrict modes for a staged release gate")
    args = parser.parse_args()
    if (args.out.exists() or not 0 < args.counts[0] < args.counts[1] < args.counts[2] <= 0xffffffff
            or args.samples < 3 or args.rss_plateau_mib <= 0):
        parser.error("require fresh output, three increasing counts, >=3 samples and positive RSS limit")
    before = cgroup_preflight(args.cpu)
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    fixtures = args.out / "fixtures"
    logs.mkdir()
    fixtures.mkdir()
    shutil.copyfile(__file__, args.out / "release_check_gc.py")
    shutil.copyfile(Path(__file__).with_name("generate.py"), args.out / "generate.py")
    shutil.copyfile(Path(__file__).with_name("run.py"), args.out / "run.py")
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != expected:
        raise RuntimeError("O3 binaries do not match the intended frozen source IDs")
    all_binaries = {"ordinary-jit": args.ordinary, "ros-jit": args.ros,
                    "ordinary-int": args.ordinary, "ros-int": args.ros}
    binaries = {name: all_binaries[name] for name in
                (args.engine or tuple(all_binaries))}
    if args.wasmtime:
        binaries["wasmtime-copying"] = args.wasmtime
    metadata = {"source_builds": builds, "expected_source_ids": expected,
                "binary_sha256": {name: sha256(path)
                for name, path in binaries.items()}, "wasm_tools_sha256": sha256(args.wasm_tools),
                "runner_sha256": sha256(Path(__file__)), "counts": args.counts,
                "samples": args.samples, "rss_plateau_mib": args.rss_plateau_mib,
                "cgroup_before": before,
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    wasm = {}
    features = {}
    for count in args.counts:
        source, feature_list = module("gc-allocation-ring", count)
        wat = fixtures / f"gc-allocation-ring-{count}.wat"
        binary = wat.with_suffix(".wasm")
        wat.write_text(source)
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(binary)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(binary)], check=True)
        wasm[count] = binary
        features[count] = feature_list
    metadata["fixtures"] = {str(count): {"wasm_sha256": sha256(binary),
        "wat_sha256": sha256(binary.with_suffix(".wat"))} for count, binary in wasm.items()}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    rows = []
    for repetition in range(args.samples):
        count_order = args.counts if repetition % 2 == 0 else tuple(reversed(args.counts))
        engine_order = tuple(binaries) if repetition % 2 == 0 else tuple(reversed(binaries))
        for count in count_order:
            for engine in engine_order:
                invocation = command(engine, binaries[engine], wasm[count],
                                     features[count], "unwind", args.cpu)
                row = run_one(invocation, f"{engine}-{count}-sample{repetition}", logs, env)
                row.update(engine=engine, count=count, sample=repetition,
                           rss_mib=row["maxrss_kib"] / 1024)
                rows.append(row)
                (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
    results = []
    failures = []
    for engine in binaries:
        peaks = {count: statistics.median(row["rss_mib"] for row in rows
                    if row["engine"] == engine and row["count"] == count)
                 for count in args.counts}
        high_growth = peaks[args.counts[2]] - peaks[args.counts[1]]
        passed = high_growth <= args.rss_plateau_mib
        result = {"engine": engine, "median_peak_rss_mib": peaks,
                  "high_minus_middle_mib": high_growth,
                  "rss_plateau_limit_mib": args.rss_plateau_mib, "passed": passed}
        results.append(result)
        if not passed and engine != "wasmtime-copying":
            failures.append(result)
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != builds:
        raise RuntimeError("product source or binary changed during GC release check")
    metadata["cgroup_after"] = cgroup_preflight(args.cpu)
    before_events = dict(line.split() for line in before["memory.events"].splitlines())
    after_events = dict(line.split() for line in metadata["cgroup_after"]["memory.events"].splitlines())
    if any(int(after_events[name]) != int(before_events[name]) for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during GC release check")
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    # No product runtime currently exports an authoritative collection count.
    # Leave this empty rather than inferring collection from a flat RSS curve.
    collection_counts = {}
    reasons = release_gate_reasons(
        set(binaries) & set(PRODUCT_ENGINES),
        {failure["engine"] for failure in failures}, collection_counts)
    metadata["rss_plateau_subcheck_passed"] = not failures
    metadata["collection_counts"] = collection_counts
    metadata["release_gate_reasons"] = reasons
    metadata["release_gate_passed"] = not reasons
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (args.out / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    if failures:
        for failure in failures:
            print("FATAL GC reclamation blocker:", failure["engine"],
                  f"RSS grew {failure['high_minus_middle_mib']:.2f} MiB between",
                  args.counts[1], "and", args.counts[2], "allocations with 1,024 roots")
        return 2
    for reason in reasons:
        print("FATAL GC release gate incomplete:", reason)
    if reasons:
        return 3
    print("PASS GC release gate: all product modes, bounded RSS, and observed collections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
