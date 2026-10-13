#!/usr/bin/env python3
"""Nine reversed-order A/B pairs for exact-source GC stripe publication."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from run import cgroup_preflight


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sample(binary, mode, label, logs, env):
    command = ["taskset", "-c", "0,2,4,6", str(binary), mode]
    log = logs / (label + ".log")
    with log.open("wb") as output:
        start = time.perf_counter_ns()
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, env=env)
        _, status, usage = os.wait4(process.pid, 0)
        elapsed = time.perf_counter_ns() - start
    exit_code = os.waitstatus_to_exitcode(status)
    if exit_code:
        raise RuntimeError(f"{label}: exit {exit_code}, {log.read_text(errors='replace')[-2000:]}")
    result = json.loads(log.read_text())
    return {"label": label, "command": command, "exit": exit_code,
            "elapsed_ns": elapsed, "maxrss_kib": usage.ru_maxrss,
            "user_s": usage.ru_utime, "system_s": usage.ru_stime, **result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--baseline-header", required=True, type=Path)
    parser.add_argument("--candidate-header", required=True, type=Path)
    parser.add_argument("--bench-source", required=True, type=Path)
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if args.pairs < 9 or args.out.exists():
        parser.error("require nine or more pairs and a fresh output directory")
    state = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / "run_gc_stripe_ab.py")
    hashes = {key: {"path": str(value), "sha256": digest(value)} for key, value in {
        "baseline": args.baseline, "candidate": args.candidate,
        "baseline_header": args.baseline_header,
        "candidate_header": args.candidate_header,
        "bench_source": args.bench_source}.items()}
    metadata = {"inputs": hashes, "cgroup_before": state, "pairs": args.pairs,
                "p_cores": [0, 2, 4, 6], "allocations_per_process": 4000000,
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join(("/toolchain/lib/x86_64-unknown-linux-gnu",
        "/toolchain/lib", "/work/deps/usr/lib/x86_64-linux-gnu", env.get("LD_LIBRARY_PATH", "")))
    binaries = {"baseline": args.baseline, "stripe": args.candidate}
    rows = []
    summary = []
    for mode in ("one", "two", "four"):
        for version, binary in binaries.items():
            row = sample(binary, mode, f"{mode}-{version}-warmup", logs, env)
            row.update(mode=mode, version=version, phase="warmup")
            rows.append(row)
        ratios = []
        values = {name: {"allocation_ns": [], "teardown_ns": [], "maxrss_kib": []}
                  for name in binaries}
        for pair in range(args.pairs):
            order = ("baseline", "stripe") if pair % 2 == 0 else ("stripe", "baseline")
            pair_rows = {}
            for version in order:
                row = sample(binaries[version], mode, f"{mode}-pair{pair}-{version}", logs, env)
                row.update(mode=mode, version=version, phase="timed", pair=pair, order=list(order))
                rows.append(row)
                pair_rows[version] = row
                for metric in values[version]:
                    values[version][metric].append(row[metric])
                (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
            ratios.append(pair_rows["baseline"]["allocation_ns"] /
                          pair_rows["stripe"]["allocation_ns"])
        result = {"mode": mode, "paired_speedups": ratios,
                  "median_paired_speedup": statistics.median(ratios)}
        for version in binaries:
            result[version] = {
                "median_allocation_ms": statistics.median(values[version]["allocation_ns"]) / 1e6,
                "p95_allocation_ms": statistics.quantiles(values[version]["allocation_ns"],
                    n=20, method="inclusive")[18] / 1e6,
                "median_teardown_ms": statistics.median(values[version]["teardown_ns"]) / 1e6,
                "median_rss_mib": statistics.median(values[version]["maxrss_kib"]) / 1024}
        summary.append(result)
        (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(mode, "paired speedup", round(result["median_paired_speedup"], 4), flush=True)
    metadata["cgroup_after"] = cgroup_preflight(0)
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
