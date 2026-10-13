#!/usr/bin/env python3
"""Regress grouped lazy JIT validation errors using the official multi-memory case.

The fixture has local function 0 containing invalid `memory.size 2` (two memories)
and local function 1 as the exported _start caller. Background prefetch may own
function 0 and claim both; demand may own function 1 and claim both. Every failed
function must observe the actual validation error after its failed-state acquire.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess

OFFICIAL_WASM_SHA256 = "123f47d5078f4180e1578534449cd8c4a9bc0bcf0f3e20a26a4fd22fe7aa2102"
ANSI = re.compile(rb"\x1b\[[0-9;]*m")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--official-wasm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=64)
    parser.add_argument("--source-id", default="")
    parser.add_argument("--require-group-owner", action="store_true",
                        help="fail if a background local-0 group owner was not observed")
    args = parser.parse_args()
    if args.iterations < 32:
        parser.error("--iterations must be at least 32 to exercise scheduler ownership")
    if digest(args.official_wasm) != OFFICIAL_WASM_SHA256:
        raise RuntimeError("official size-out-of-range-exec.wasm changed")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    owner_seen = {"demand": {0: 0, 1: 0}, "group": {0: 0, 1: 0}}
    cross_slot_seen = 0

    for lane, workers in (("demand", 0), ("group", 2)):
        for iteration in range(args.iterations):
            command = [
                str(args.uwvm), "-Rcc", "jit", "-Rcm", "lazy",
                "-Rct", str(workers), "-Rllvm-cache-path", "disable",
                "-Rllvm-call-stack", "instruction", "-Rclog", "err",
                "-WFE-multi-memory", "--run", str(args.official_wasm),
            ]
            run = subprocess.run(command, capture_output=True, timeout=90)
            raw = run.stdout + run.stderr
            log = args.output / f"{lane}-{iteration:03d}.log"
            log.write_bytes(raw)
            plain = ANSI.sub(b"", raw).lower()
            owners = sorted({
                int(match) for match in re.findall(
                    rb"compile-start[^\r\n]*local_fn=(\d+)", plain
                )
            })
            for owner in owners:
                if owner in owner_seen[lane]:
                    owner_seen[lane][owner] += 1
            # A local-0 compile-start alone is insufficient: local 1 may already
            # have compiled independently. Only a sole local-0 owner followed by
            # local-1 demand-failed demonstrates publication into slot 1.
            cross_slot = (
                owners == [0]
                and re.search(rb"demand-failed[^\r\n]*local_fn=1", plain) is not None
            )
            cross_slot_seen += cross_slot
            valid_diagnostic = (
                run.returncode != 0
                and b"validation error in webassembly code" in plain
                and b"illegal memory index: 2" in plain
                and b"all memory count=2" in plain
                and b"there are no errors" not in plain
                and b"validator memory indication: (null)" not in plain
            )
            results.append({
                "lane": lane, "workers": workers, "iteration": iteration,
                "exit": run.returncode, "owners": owners,
                "cross_slot_propagation_observed": cross_slot,
                "valid_diagnostic": valid_diagnostic,
                "log": str(log),
            })
            if not valid_diagnostic:
                (args.output / "results.json").write_text(
                    json.dumps({"passed": False, "results": results}, indent=2) + "\n"
                )
                raise RuntimeError(
                    f"{lane} iteration {iteration} lost diagnostic; see {log}"
                )
        print(f"PASS {lane}: {args.iterations}/{args.iterations} detailed failures", flush=True)

    # With no background workers the demanded _start (local 1) owns validation.
    # With two workers, at least one prefetch run must let invalid local 0 own the
    # claimed group and publish its error into _start's separate diagnostic slot.
    ownership_covered = owner_seen["demand"][1] != 0 and cross_slot_seen != 0
    summary = {
        "passed": not args.require_group_owner or ownership_covered,
        "ownership_covered": ownership_covered,
        "cross_slot_propagation_observations": cross_slot_seen,
        "require_group_owner": args.require_group_owner,
        "source_id": args.source_id,
        "uwvm_sha256": digest(args.uwvm),
        "official_wasm_sha256": digest(args.official_wasm),
        "iterations_per_lane": args.iterations,
        "checks": len(results), "group_owner_seen": owner_seen,
        "results": results,
    }
    (args.output / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({key: value for key, value in summary.items() if key != "results"}))
    if args.require_group_owner and not ownership_covered:
        raise RuntimeError(
            f"did not observe demand local 1 and grouped local 0 owners: {owner_seen}; "
            "use targeted scheduling injection"
        )


if __name__ == "__main__":
    main()
