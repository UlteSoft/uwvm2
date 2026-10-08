#!/usr/bin/env python3
"""Compare the same GC-store test compiled against old and new exnref roots."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import sys


ROOT_TIME = re.compile(r"exn_root_reread_ns=(\d+) tokens=(\d+) repetitions=(\d+)")
IMMUTABLE_GET = re.compile(r"checked immutable get ([0-9.]+) ns/op")


def sample(path: Path) -> dict:
    # Each library needs its own process: their inline static registries must
    # never interpose on one another in the same dynamic-linker namespace.
    code = (
        "import ctypes, sys; "
        "library = ctypes.CDLL(sys.argv[1]); "
        "result = library.main(); "
        "sys.exit(result)"
    )
    result = subprocess.run([sys.executable, "-c", code, str(path)],
                            capture_output=True, text=True, timeout=20, check=True)
    root = ROOT_TIME.search(result.stdout)
    immutable = IMMUTABLE_GET.search(result.stdout)
    if not root or not immutable or "gc_object_store: PASS" not in result.stdout:
        raise RuntimeError(f"unexpected GC-store output from {path}: {result.stdout!r}")
    if (int(root[2]), int(root[3])) != (4096, 4):
        raise RuntimeError(f"wrong exnref fixture cardinality from {path}: {root.group()}")
    return {"exn_root_reread_ns": int(root[1]),
            "checked_immutable_get_ns_per_op": float(immutable[1])}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if not 1 <= args.rounds <= 20:
        parser.error("--rounds must be in [1, 20]")
    paths = {"baseline": args.baseline.resolve(strict=True),
             "candidate": args.candidate.resolve(strict=True)}
    results: dict[str, list[dict]] = {"baseline": [], "candidate": []}
    for round_number in range(args.rounds):
        # Reverse pair order on alternate rounds to limit drift from warmup.
        for name in (("baseline", "candidate") if round_number % 2 == 0
                     else ("candidate", "baseline")):
            results[name].append(sample(paths[name]))
    summary = {
        name: {
            "library": str(paths[name]),
            "library_sha256": hashlib.sha256(paths[name].read_bytes()).hexdigest(),
            "samples": results[name],
            "median_exn_root_reread_ns": statistics.median(
                row["exn_root_reread_ns"] for row in results[name]),
            "median_checked_immutable_get_ns_per_op": statistics.median(
                row["checked_immutable_get_ns_per_op"] for row in results[name]),
        } for name in paths
    }
    summary["root_reread_speedup"] = (
        summary["baseline"]["median_exn_root_reread_ns"] /
        summary["candidate"]["median_exn_root_reread_ns"])
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return 0 if summary["root_reread_speedup"] > 10 else 1


if __name__ == "__main__":
    sys.exit(main())
