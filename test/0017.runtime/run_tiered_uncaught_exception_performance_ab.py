#!/usr/bin/env python3
"""Paired, CPU-pinned timing for one official uncaught Core3 exception case.

This measures whole-process latency. Startup dominates a single throw, so it
must not be interpreted as a per-throw microbenchmark.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import statistics
import subprocess
import time


ANSI = re.compile(r"\x1b\[[0-9;]*m")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cpu", type=int, default=16)
    parser.add_argument("--pairs", type=int, default=15)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    if args.cpu not in os.sched_getaffinity(0) or not 3 <= args.pairs <= 99:
        parser.error("CPU must be inside test cpuset and pairs must be 3..99")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.manifest.read_text())
    reference = next(run for run in manifest["runs"]
                     if run["product"] == "ordinary"
                     and run["mode"] == "tiered-lazy-instruction"
                     and (run["file"], run["group"], run["line"]) == ("try_table", 1, 292))
    base_command = list(reference["command"])
    rows = []
    for pair in range(args.pairs + 1):
        # Alternate AB/BA to reduce drift from CPU frequency and page cache.
        for label in (("baseline", "candidate") if pair % 2 == 0 else ("candidate", "baseline")):
            command = ["taskset", "-c", str(args.cpu),
                       str(args.baseline if label == "baseline" else args.candidate),
                       *base_command[1:]]
            start = time.perf_counter_ns()
            result = subprocess.run(command, capture_output=True, timeout=30, check=False)
            elapsed = time.perf_counter_ns() - start
            plain = ANSI.sub("", (result.stdout + result.stderr).decode("utf-8", "replace"))
            expected = ("no snapshot captured" if label == "baseline"
                        else "Wasm call stack captured at throw")
            if result.returncode == 0 or "Uncaught WebAssembly exception" not in plain or expected not in plain:
                raise RuntimeError(f"{label} pair {pair} failed semantic check: {plain[-1200:]}")
            if pair == 0:  # Untimed warm-up and exact representative diagnostic.
                (args.output / f"{label}-warmup.log").write_text(plain)
                continue
            rows.append(dict(pair=pair - 1, product=label, wall_ns=elapsed,
                             exit=result.returncode))
    groups = {label: [row["wall_ns"] for row in rows if row["product"] == label]
              for label in ("baseline", "candidate")}
    summary = dict(case="official try_table group1 line292 uncaught",
                   baseline_sha256=digest(args.baseline), candidate_sha256=digest(args.candidate),
                   manifest_sha256=digest(args.manifest), runner_sha256=digest(Path(__file__)),
                   cgroup={name: Path("/sys/fs/cgroup", name).read_text().strip()
                           for name in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")},
                   cpu=args.cpu, pairs=args.pairs, groups={
                       label: dict(median_wall_ns=statistics.median(values),
                                   minimum_wall_ns=min(values), maximum_wall_ns=max(values))
                       for label, values in groups.items()}, rows=rows,
                   limitation="Whole-process latency; cannot isolate one throw from startup.")
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS paired uncaught whole-process timing", args.pairs, "pairs")


if __name__ == "__main__":
    main()
