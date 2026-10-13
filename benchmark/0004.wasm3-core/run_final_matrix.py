#!/usr/bin/env python3
"""Source-bound Core 3.0 performance matrix on the remote P core.

Every group invokes run.py, which validates the exact same Wasm bytecode for
each supporting VM and records nine reversed-order high/low pairs. Split
groups permit windows between other users of the shared 64 GiB cgroup.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from generate import CASES
from run import cgroup_preflight, sha256, verify_product_build


GROUPS = {
    "gc-allocation": {"counts": (2000000, 16000000),
                      "cases": ("gc-allocation-ring",), "collectors": True},
    "gc-heap-fields": {"counts": (1000000, 10000000),
                       "cases": ("gc-struct-heap-update", "gc-array-heap-update"),
                       "collectors": True},
    "gc-local-and-cast": {"counts": (1000000, 10000000),
                          "cases": ("gc-struct-update", "gc-array-update", "gc-cast",
                                    "gc-i31-box-unbox"), "collectors": True},
    "memory32": {"counts": (5000000, 50000000),
                 "cases": ("memory32-random-store", "memory32-page-aligned-store",
                           "memory32-page-unaligned-store"), "collectors": False},
    "memory64": {"counts": (5000000, 50000000),
                 "cases": ("memory64-random-store", "memory64-page-aligned-store",
                           "memory64-page-unaligned-store"), "collectors": False},
    "multi-memory": {"counts": (1000000, 10000000),
                     "cases": ("multi-memory-random-store",), "collectors": False},
    "atomics": {"counts": (1000000, 10000000),
                "cases": ("memory32-atomic-rmw", "memory64-atomic-rmw"),
                "collectors": False},
    "wait-notify-fast": {"counts": (1000000, 10000000),
                         "cases": ("memory32-atomic-wait-mismatch",
                                   "memory64-atomic-wait-mismatch",
                                   "memory32-atomic-notify-empty",
                                   "memory64-atomic-notify-empty"),
                         "collectors": False},
    "high-memory": {"counts": (5000000, 50000000),
                    "cases": ("memory64-high-random-store",), "collectors": False},
    "tables-and-calls": {"counts": (1000000, 10000000),
                         "cases": ("table32-indirect", "table64-indirect",
                                   "plain-call-step", "tail-call-step",
                                   "direct-call-step", "call-ref-step"), "collectors": False},
    "exceptions": {"counts": (50000, 500000),
                   "cases": ("eh-plain-step", "eh-caught-step"), "collectors": False},
    "simd": {"counts": (1000000, 10000000),
             "cases": ("strict-swizzle", "relaxed-swizzle"), "collectors": False},
    "extended-const": {"counts": (1000000, 10000000),
                       "cases": ("extended-const-init",), "collectors": False},
}
assert sorted(case for group in GROUPS.values() for case in group["cases"]) == sorted(CASES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True,
                        help="full frozen sha256:... source ID; rejects historical O3 binaries")
    parser.add_argument("--ros-source-id", required=True,
                        help="full frozen sha256:... source ID; rejects historical O3 binaries")
    parser.add_argument("--wasmedge", type=Path)
    parser.add_argument("--wavm", type=Path)
    parser.add_argument("--wasmer", type=Path)
    parser.add_argument("--group", action="append", choices=GROUPS)
    parser.add_argument("--high-memory-preflight", type=Path,
                        help="required for high-memory timing; approved engine/mode list and hashes")
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if args.out.exists() or args.pairs < 9:
        parser.error("require new output directory and at least nine pairs")
    before = cgroup_preflight(args.cpu)
    os.sched_setaffinity(0, {16})
    args.out.mkdir(parents=True)
    product_builds = {label: verify_product_build(label, binary, args.out, "before")
                      for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in product_builds.items()} != expected:
        raise RuntimeError("O3 binaries do not match the explicitly requested frozen source IDs")
    selections = args.group or list(GROUPS)
    high_approved = None
    high_preflight = None
    if "high-memory" in selections:
        if args.high_memory_preflight is None:
            parser.error("high-memory timing requires --high-memory-preflight")
        high_preflight = json.loads(args.high_memory_preflight.read_text())
        high_metadata = json.loads((args.high_memory_preflight.parent / "metadata.json").read_text())
        if (high_metadata["source_builds"] != product_builds or
                high_metadata["expected_source_ids"] != expected or
                high_metadata["wasm_tools_sha256"] != sha256(args.wasm_tools) or
                high_metadata["generator_sha256"] != sha256(Path(__file__).with_name("generate.py")) or
                high_metadata["iterations"] < 10000 or
                high_metadata["guest_footprint"]["final_expected"] == 0):
            raise RuntimeError("high-memory preflight used different product sources")
        high_approved = set(high_preflight["approved_engines"])
        required = {"ordinary-jit", "ordinary-int", "ros-jit", "ros-int"}
        if not required <= high_approved:
            raise RuntimeError("a product engine/mode failed sparse high-memory preflight")
        high_binaries = {"ordinary-jit": args.ordinary, "ordinary-int": args.ordinary,
                         "ros-jit": args.ros, "ros-int": args.ros, "wasmtime": args.wasmtime}
        if args.wasmedge:
            high_binaries.update({"wasmedge-jit": args.wasmedge, "wasmedge-int": args.wasmedge})
        if args.wavm:
            high_binaries["wavm"] = args.wavm
        if args.wasmer:
            high_binaries["wasmer"] = args.wasmer
        if any(high_metadata["engine_sha256"].get(name) != sha256(path)
               for name, path in high_binaries.items() if name in high_approved):
            raise RuntimeError("an approved high-memory engine binary changed after preflight")
    paths = {name: path for name, path in (("wasm-tools", args.wasm_tools),
             ("wasmtime", args.wasmtime), ("ordinary", args.ordinary),
             ("ros", args.ros), ("wasmedge", args.wasmedge),
             ("wavm", args.wavm), ("wasmer", args.wasmer)) if path}
    omitted_comparators = [name for name in ("wasmedge", "wavm", "wasmer")
                           if name not in paths]
    metadata = {"runner_sha256": sha256(Path(__file__)),
                "run_py_sha256": sha256(Path(__file__).with_name("run.py")),
                "tool_sha256": {name: sha256(path) for name, path in paths.items()},
                "selected_groups": selections, "group_definitions": GROUPS,
                "product_builds": product_builds, "expected_source_ids": expected,
                "omitted_same_wasm_comparators": omitted_comparators,
                "all_requested_same_wasm_comparators_supplied": not omitted_comparators,
                "high_memory_preflight": ({"path": str(args.high_memory_preflight),
                    "sha256": sha256(args.high_memory_preflight),
                    "approved_engines": sorted(high_approved)} if high_approved else None),
                "cpu": args.cpu, "pairs": args.pairs, "cgroup_before": before,
                "harness_affinity": sorted(os.sched_getaffinity(0)),
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "matrix-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if omitted_comparators:
        print("PARTIAL same-Wasm comparator scope; not supplied:",
              ", ".join(omitted_comparators), flush=True)
    inconclusive = {}
    for name in selections:
        group = GROUPS[name]
        command = [sys.executable, str(Path(__file__).with_name("run.py")),
                   "--out", str(args.out / name), "--wasm-tools", str(args.wasm_tools),
                   "--wasmtime", str(args.wasmtime), "--ordinary", str(args.ordinary),
                   "--ros", str(args.ros), "--low", str(group["counts"][0]),
                   "--high", str(group["counts"][1]), "--pairs", str(args.pairs),
                   "--cpu", str(args.cpu), "--trace", "unwind",
                   "--include-interpreters", "--require-exact-source"]
        for case in group["cases"]:
            command.extend(("--case", case))
        if name == "high-memory":
            command.extend(("--allow-high-memory", "--high-memory-preflight",
                            str(args.high_memory_preflight)))
            for engine in sorted(high_approved & set(high_binaries)):
                command.extend(("--engine", engine))
        if group["collectors"]:
            command.append("--include-gc-collector-matrix")
        for option, path in (("--wasmedge", args.wasmedge), ("--wavm", args.wavm),
                             ("--wasmer", args.wasmer)):
            if (name == "high-memory" and option == "--wasmedge" and
                    not {"wasmedge-jit", "wasmedge-int"} & high_approved):
                continue
            if name == "high-memory" and option == "--wavm" and "wavm" not in high_approved:
                continue
            if name == "high-memory" and option == "--wasmer" and "wasmer" not in high_approved:
                continue
            if path:
                command.extend((option, str(path)))
        (args.out / f"{name}.command.json").write_text(json.dumps(command, indent=2) + "\n")
        with (args.out / f"{name}.runner.log").open("wb") as log:
            completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        if completed.returncode:
            raise RuntimeError(f"{name}: run.py exited {completed.returncode}; see group log")
        rows = json.loads((args.out / name / "summary.json").read_text())
        weak = [{"case": row["case"], "engine": row["engine"],
                 "sub_100ms_sample": row["sub_100ms_sample"],
                 "negative_slope_sample": row["negative_slope_sample"]}
                for row in rows if row["sub_100ms_sample"] or row["negative_slope_sample"]]
        if weak:
            inconclusive[name] = weak
            (args.out / f"{name}.inconclusive.json").write_text(json.dumps(weak, indent=2) + "\n")
            print(name, "product semantic PASS; performance INCONCLUSIVE for",
                  len(weak), "engine/case rows", flush=True)
        else:
            print(name, "product semantic PASS; timing threshold met", flush=True)
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != product_builds:
        raise RuntimeError("O3 product source or binary changed during matrix")
    metadata["cgroup_after"] = cgroup_preflight(args.cpu)
    metadata["performance_inconclusive"] = inconclusive
    metadata["all_selected_group_timings_conclusive"] = not inconclusive
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "matrix-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
