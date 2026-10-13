#!/usr/bin/env python3
"""Source-bound semantic GC-root survival matrix inside the 64 GiB cgroup.

This runner does not certify reclamation: a successful monotonic store also
keeps every sentinel alive. Pair it with release_check_gc.py and a measured
collection event when a collector exists.
"""

import argparse
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import time

from generate_gc_root_survival import ROOTS, module
from run import (LIB_PATHS, cgroup_preflight, command, run_one, sha256,
                 verify_product_build)


ORDINARY_MODES = {
    "int-full": ("-Rcc", "int", "-Rcm", "full"),
    "int-lazy": ("-Rcc", "int", "-Rcm", "lazy"),
    "int-lazy-verified": ("-Rcc", "int", "-Rcm", "lazy+verification"),
    "jit-full-unwind": ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-full-policy", "pb-o3",
                        "-Rllvm-call-stack", "unwind"),
    "jit-full-instruction": ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-full-policy", "pb-o3",
                             "-Rllvm-call-stack", "instruction"),
    "jit-lazy-unwind": ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-lazy-policy", "balanced",
                        "-Rllvm-call-stack", "unwind"),
    "jit-lazy-verified": ("-Rcc", "jit", "-Rcm", "lazy+verification",
                          "-Rllvm-lazy-policy", "balanced", "-Rllvm-call-stack", "unwind"),
    "tiered-lazy-unwind": ("-Rcc", "tiered", "-Rcm", "lazy",
                           "-Rllvm-call-stack", "unwind"),
    "tiered-lazy-instruction": ("-Rcc", "tiered", "-Rcm", "lazy",
                                "-Rllvm-call-stack", "instruction"),
    "tiered-lazy-verified": ("-Rcc", "tiered", "-Rcm", "lazy+verification",
                             "-Rllvm-call-stack", "unwind"),
    "tiered-t0-t1-unwind": ("-Rcc", "tiered", "-Rcm", "lazy",
                            "-Rtiered-disable-t2", "-Rllvm-call-stack", "unwind"),
    "tiered-t1-unwind": ("-Rcc", "tiered", "-Rcm", "lazy",
                          "-Rtiered-disable-t0", "-Rllvm-call-stack", "unwind"),
}
ROS_MODES = {
    "int-full": ("-Rint",),
    "jit-full-unwind": ("-Raot", "-Rllvm-full-policy", "pb-o3",
                        "-Rllvm-call-stack", "unwind"),
    "jit-full-instruction": ("-Raot", "-Rllvm-full-policy", "pb-o3",
                             "-Rllvm-call-stack", "instruction"),
}
FEATURES = ("gc", "exceptions", "function-references")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--ordinary", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--wasmtime", type=Path,
                        help="optional same-Wasm copying-collector oracle")
    parser.add_argument("--allocations", type=int, default=100000)
    parser.add_argument("--warmup-calls", type=int, default=0,
                        help="run an indirect-call warmup before each root check to probe tier promotion")
    parser.add_argument("--root", action="append", choices=("all", *ROOTS),
                        help="repeat to test separate root locations; default all")
    parser.add_argument("--mode", action="append",
                        help="repeat to restrict product modes, e.g. ordinary:int-full")
    parser.add_argument("--cpu", type=int, default=0)
    args = parser.parse_args()
    if (args.out.exists() or not 0 < args.allocations <= 0x7FFFFFFF or
            not 0 <= args.warmup_calls <= 0x3FFFFFFF):
        parser.error("require fresh output and valid allocation/warmup counts")
    roots = tuple(dict.fromkeys(args.root or ("all",)))
    valid_modes = {**{"ordinary:" + name: ("ordinary", options)
                     for name, options in ORDINARY_MODES.items()},
                   **{"ros:" + name: ("ros", options)
                     for name, options in ROS_MODES.items()}}
    if args.mode and any(mode not in valid_modes for mode in args.mode):
        parser.error("unknown --mode; use ordinary:<mode> or ros:<mode>")
    modes = tuple(dict.fromkeys(args.mode or tuple(valid_modes)))
    before = cgroup_preflight(args.cpu)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True)
    shutil.copyfile(__file__, args.out / "run_gc_root_survival.py")
    shutil.copyfile(Path(__file__).with_name("generate_gc_root_survival.py"),
                    args.out / "generate_gc_root_survival.py")
    logs = args.out / "logs"
    logs.mkdir()
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if (builds["ordinary"]["source_id"] != args.ordinary_source_id or
            builds["ros"]["source_id"] != args.ros_source_id):
        raise RuntimeError("product source IDs differ from intended frozen source")
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    result = {"start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "allocations_per_root": args.allocations, "warmup_calls": args.warmup_calls,
              "roots": roots,
              "modes": modes, "features": FEATURES, "source_builds": builds,
              "binary_sha256": {"ordinary": sha256(args.ordinary),
                                "ros": sha256(args.ros)},
              "wasm_tools_sha256": sha256(args.wasm_tools),
              "wasmtime_sha256": sha256(args.wasmtime) if args.wasmtime else None,
              "runner_sha256": sha256(Path(__file__)),
              "generator_sha256": sha256(Path(__file__).with_name("generate_gc_root_survival.py")),
              "cgroup_before": before, "fixture_sha256": {}, "rows": [],
              "semantic_only_no_collector_event_proof": True,
              "unsupported_mode_probe": None}
    for root in roots:
        wat = args.out / f"root-{root}.wat"
        wasm = wat.with_suffix(".wasm")
        wat.write_text(module(args.allocations, root, args.warmup_calls))
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
        result["fixture_sha256"][root] = {"wat": sha256(wat), "wasm": sha256(wasm)}
        for mode in modes:
            label, options = valid_modes[mode]
            binary = args.ordinary if label == "ordinary" else args.ros
            tiered_log = logs / f"{mode}-{root}.compile.log" if "tiered" in mode else None
            invocation = ["taskset", "-c", str(args.cpu), str(binary), *options,
                          "-Rllvm-cache-path", "disable", "-Rct", "0",
                          *(["-Rclog", "file", str(tiered_log)] if tiered_log else []),
                          *("-WFE-" + feature for feature in FEATURES),
                          "--run", str(wasm)]
            row = run_one(invocation, f"{mode}-{root}", logs, env)
            row.update(mode=mode, root=root, wasm_sha256=sha256(wasm))
            if tiered_log:
                if not tiered_log.exists():
                    raise RuntimeError(f"{mode}/{root}: missing tiered compiler log")
                log = tiered_log.read_text(errors="replace")
                row["tiered_compile_log_sha256"] = sha256(tiered_log)
                row["tiered_events"] = {marker: len(re.findall(re.escape(marker), log))
                                        for marker in ("[uwvm-int-lazy] demand-request",
                                                       "[llvm-jit-lazy] compile-end",
                                                       "tiered-osr-enter", "tiered-full-request",
                                                       "tiered-full-ready", "tiered-full-enter")}
                row["tiered_counters"] = {name: max((int(value) for value in
                    re.findall(r"\b" + name + r"=(\d+)", log)), default=0)
                    for name in ("tiered_switches", "tiered_osr_ready",
                                 "tiered_full_requests", "tiered_full_ready",
                                 "tiered_full_failed")}
                # A mode name describes the configured tier pipeline, not
                # proof that this particular run entered T1/T2 native code.
            result["rows"].append(row)
            (args.out / "progress.json").write_text(json.dumps(result, indent=2) + "\n")
        if args.wasmtime:
            invocation = command("wasmtime-copying", args.wasmtime, wasm,
                                 FEATURES, "unwind", args.cpu)
            row = run_one(invocation, f"wasmtime-copying-{root}", logs, env)
            row.update(mode="wasmtime-copying", root=root, wasm_sha256=sha256(wasm))
            result["rows"].append(row)
            (args.out / "progress.json").write_text(json.dumps(result, indent=2) + "\n")
        if result["unsupported_mode_probe"] is None:
            # The CLI has a dedicated fatal for -Rcc tiered -Rcm full; do
            # not count this nonexistent combination as a tested backend.
            probe = ["taskset", "-c", str(args.cpu), str(args.ordinary),
                     "-Rcc", "tiered", "-Rcm", "full", "-Rct", "0",
                     *("-WFE-" + feature for feature in FEATURES),
                     "--run", str(wasm)]
            row = run_one(probe, "ordinary-tiered-full-unsupported", logs, env,
                          allow_failure=True)
            diagnostic = (logs / "ordinary-tiered-full-unsupported.log").read_text(errors="replace")
            if row["exit"] == 0 or "Tiered compilation conflicts with full compilation" not in diagnostic:
                raise RuntimeError("tiered/full CLI behavior differs from explicit unsupported-mode contract")
            result["unsupported_mode_probe"] = row
            (args.out / "progress.json").write_text(json.dumps(result, indent=2) + "\n")
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != builds:
        raise RuntimeError("product source or binary changed during root-survival run")
    result["cgroup_after"] = cgroup_preflight(args.cpu)
    first = dict(line.split() for line in before["memory.events"].splitlines())
    last = dict(line.split() for line in result["cgroup_after"]["memory.events"].splitlines())
    if any(first[name] != last[name] for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM during root-survival run")
    result["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS root-survival semantic preflight; collector/reclamation still unproven")


if __name__ == "__main__":
    main()
