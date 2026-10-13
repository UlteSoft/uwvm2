#!/usr/bin/env python3
"""Pinned managed-language analogue for the GC allocation-ring workload.

The Java, C# and JavaScript programs warm their own JITs before reporting
guest-loop time. This is an analogue, never a same-bytecode Wasm VM ranking.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from generate import A, C, MASK, endpoint
from run import (cgroup_preflight, cpu_telemetry, engine_order_for_pair,
                 node_runtime_environment, run_one, sha256, verify_product_build)


def expected_root_checksum(count):
    state = endpoint(count - 1024)
    checksum = 0
    for _ in range(1024):
        state = (state * A + C) & MASK
        checksum ^= state
    return checksum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--openjdk", required=True, type=Path)
    parser.add_argument("--graalvm", required=True, type=Path)
    parser.add_argument("--java-class-dir", required=True, type=Path)
    parser.add_argument("--dotnet", required=True, type=Path)
    parser.add_argument("--dotnet-dll", required=True, type=Path)
    parser.add_argument("--node", required=True, type=Path)
    parser.add_argument("--node-script", required=True, type=Path)
    parser.add_argument("--build-manifest", required=True, type=Path,
                        help="build.json from build_managed.py for the exact analogue sources")
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--low", type=int, default=5000000)
    parser.add_argument("--high", type=int, default=50000000)
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if not (1024 <= args.low < args.high <= 0x7fffffff) or args.pairs < 9:
        parser.error("require 1024 <= low < high <= signed i32 max and at least nine pairs")
    if args.out.exists():
        parser.error("output directory already exists")
    state = cgroup_preflight(args.cpu)
    args.out.mkdir(parents=True)
    products = {label: verify_product_build(label, binary, args.out, "before")
                for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected_source_ids = {"ordinary": args.ordinary_source_id,
                           "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in products.items()} != expected_source_ids:
        raise RuntimeError("managed analogue paired with the wrong product source IDs")
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / "run_managed.py")
    paths = {"openjdk": args.openjdk, "graalvm": args.graalvm,
             "java-class": args.java_class_dir / "ManagedRing.class",
             "dotnet": args.dotnet, "dotnet-dll": args.dotnet_dll,
             "node": args.node, "node-script": args.node_script}
    source_files = (Path(__file__).parent / "managed/java/ManagedRing.java",
                    Path(__file__).parent / "managed/dotnet/Program.cs",
                    Path(__file__).parent / "managed/dotnet/ManagedRing.csproj",
                    args.node_script)
    source_snapshots = args.out / "sources"
    source_snapshots.mkdir()
    for path in source_files:
        shutil.copyfile(path, source_snapshots / path.name)
    build = json.loads(args.build_manifest.read_text())
    expected_sources = {"java": source_files[0], "dotnet": source_files[1],
                        "dotnet-project": source_files[2], "node": source_files[3]}
    if ({name: sha256(path) for name, path in expected_sources.items()} != build["source_sha256"] or
            build["java_release"] != 21 or
            build["tool_sha256"]["dotnet"] != sha256(args.dotnet) or
            Path(build["outputs"]["java_class"]["path"]).resolve() !=
                paths["java-class"].resolve() or
            Path(build["outputs"]["dotnet_dll"]["path"]).resolve() !=
                args.dotnet_dll.resolve()):
        raise RuntimeError("managed analogue source/compiler artifact manifest mismatch")
    for name, folder in (("java", args.java_class_dir),
                         ("dotnet", args.dotnet_dll.parent)):
        actual = {str(path.relative_to(folder)): sha256(path)
                  for path in sorted(folder.rglob("*")) if path.is_file()}
        if actual != build["artifact_sha256"][name]:
            raise RuntimeError(f"{name} analogue build artifacts changed")
    meta = {"paths": {key: {"path": str(path), "sha256": sha256(path)}
                      for key, path in paths.items()},
            "sources": {str(path): sha256(path) for path in source_files},
            "build_manifest": {"path": str(args.build_manifest),
                               "sha256": sha256(args.build_manifest)},
            "cpu": args.cpu, "low": args.low, "high": args.high,
            "pairs": args.pairs, "cgroup_before": state,
            "product_builds": products, "expected_source_ids": expected_source_ids,
            "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    env = os.environ.copy()
    env.update(DOTNET_CLI_HOME=str(args.out / "dotnet-home"),
               DOTNET_ROOT=str(args.dotnet.resolve(strict=True).parent),
               DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", DOTNET_CLI_TELEMETRY_OPTOUT="1",
               DOTNET_TieredPGO="1", DOTNET_gcServer="0",
               DOTNET_GCHeapHardLimit="0x40000000",
               NODE_OPTIONS="--max-old-space-size=1024")
    node_env = node_runtime_environment(env)
    meta["runtime_versions"] = {}
    for name, executable, version_args in (("openjdk", args.openjdk, ("-version",)),
                                           ("graalvm", args.graalvm, ("-version",)),
                                           ("dotnet", args.dotnet, ("--version",)),
                                           ("node", args.node, ("--version",))):
        result = subprocess.run(["taskset", "-c", str(args.cpu), str(executable),
                                 *version_args], capture_output=True,
                                env=node_env if name == "node" else env, timeout=30)
        if result.returncode:
            raise RuntimeError(f"{name} version command exited {result.returncode}")
        meta["runtime_versions"][name] = (result.stdout + result.stderr).decode(errors="replace").strip()
    meta["runtime_flags"] = {"java": ["-Xms256m", "-Xmx1g", "-XX:+UseG1GC"],
                             "dotnet": {name: env[name] for name in
                                        ("DOTNET_TieredPGO", "DOTNET_gcServer",
                                         "DOTNET_GCHeapHardLimit",
                                         "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT")},
                             "node": env["NODE_OPTIONS"],
                             "node_ld_library_path": node_env["LD_LIBRARY_PATH"]}
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    engines = {
        "openjdk-g1": [str(args.openjdk), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                        "-cp", str(args.java_class_dir), "ManagedRing"],
        "graalvm-g1": [str(args.graalvm), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                        "-cp", str(args.java_class_dir), "ManagedRing"],
        "dotnet": [str(args.dotnet), str(args.dotnet_dll)],
        "node-v8": [str(args.node), str(args.node_script)],
    }
    runtime_names = {"openjdk-g1": "java", "graalvm-g1": "java",
                     "dotnet": "dotnet", "node-v8": "node"}
    rows = []
    summaries = []
    timings = {engine: {"guest_slopes": [], "process_slopes": [], "high_times": []}
               for engine in engines}
    engine_names = list(engines)
    for pair in range(args.pairs):
        telemetry_before = cpu_telemetry(args.cpu)
        engine_order = engine_order_for_pair(engine_names, pair)
        order = (args.low, args.high) if pair % 2 == 0 else (args.high, args.low)
        for engine in engine_order:
            base = engines[engine]
            samples = {}
            for count in order:
                label = f"{engine}-pair{pair}-{count}"
                row = run_one(["taskset", "-c", str(args.cpu), *base, str(count)], label, logs,
                              node_env if engine == "node-v8" else env)
                output = (logs / (label + ".log")).read_text().strip().splitlines()
                guest = json.loads(output[-1])
                if (guest["runtime"] != runtime_names[engine] or
                        not isinstance(guest["guest_ns"], int) or guest["guest_ns"] <= 0 or
                        guest["iterations"] != count or guest["checksum"] != endpoint(count) or
                        guest["root_checksum"] != expected_root_checksum(count)):
                    raise RuntimeError(label + ": guest count/final state/root ring mismatch")
                if engine == "dotnet" and (guest.get("server_gc") is not False or
                        guest.get("gc_heap_limit_config_bytes") != 1 << 30):
                    raise RuntimeError(label + ": .NET GC mode or heap cap not applied")
                row.update(engine=engine, pair=pair, count=count, phase="timed",
                           engine_order=engine_order, order=list(order),
                           guest_ns=guest["guest_ns"], checksum=guest["checksum"],
                           root_checksum=guest["root_checksum"],
                           server_gc=guest.get("server_gc"),
                           gc_heap_limit_config_bytes=guest.get("gc_heap_limit_config_bytes"),
                           gc_heap_budget_bytes=guest.get("gc_heap_budget_bytes"))
                samples[count] = row
                rows.append(row)
                (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
                if count == args.high:
                    timings[engine]["high_times"].append(row["guest_ns"])
            if samples[args.low]["checksum"] == samples[args.high]["checksum"]:
                raise RuntimeError("low and high produced identical checksum; check test counts")
            timings[engine]["guest_slopes"].append(
                (samples[args.high]["guest_ns"] - samples[args.low]["guest_ns"])
                / (args.high - args.low))
            timings[engine]["process_slopes"].append(
                (samples[args.high]["elapsed_ns"] - samples[args.low]["elapsed_ns"])
                / (args.high - args.low))
        with (args.out / "pair-telemetry.jsonl").open("a") as output:
            output.write(json.dumps({"pair": pair, "engine_order": engine_order,
                                     "before": telemetry_before,
                                     "after": cpu_telemetry(args.cpu)}) + "\n")
    for engine in engine_names:
        slopes_guest = timings[engine]["guest_slopes"]
        slopes_process = timings[engine]["process_slopes"]
        high_times = timings[engine]["high_times"]
        record = {"engine": engine, "guest_median_ns_per_iteration": statistics.median(slopes_guest),
                  "process_median_ns_per_iteration": statistics.median(slopes_process),
                  "guest_slopes_ns_per_iteration": slopes_guest,
                  "process_slopes_ns_per_iteration": slopes_process,
                  "guest_high_median_ms": statistics.median(high_times) / 1e6,
                  "guest_high_p95_ms": statistics.quantiles(high_times, n=20, method="inclusive")[18] / 1e6,
                  "sub_100ms_sample": min(high_times) < 100_000_000,
                  "high_maxrss_kib": max(row["maxrss_kib"] for row in rows
                                         if row["engine"] == engine and row["count"] == args.high)}
        summaries.append(record)
        (args.out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
        print(engine, round(record["guest_median_ns_per_iteration"], 3), "guest ns/iteration",
              round(record["high_maxrss_kib"] / 1024), "peak MiB", flush=True)
    meta["cgroup_after"] = cgroup_preflight(args.cpu)
    products_after = {label: verify_product_build(label, binary, args.out, "after")
                      for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    meta["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    if (products_after != products or
            sha256(args.build_manifest) != meta["build_manifest"]["sha256"] or
            any(sha256(path) != build["source_sha256"][name]
                for name, path in expected_sources.items()) or
            any(sha256(path) != meta["paths"][name]["sha256"]
                for name, path in paths.items())):
        raise RuntimeError("managed analogue source, runtime, or build manifest changed")
    for name, folder in (("java", args.java_class_dir),
                         ("dotnet", args.dotnet_dll.parent)):
        actual = {str(path.relative_to(folder)): sha256(path)
                  for path in sorted(folder.rglob("*")) if path.is_file()}
        if actual != build["artifact_sha256"][name]:
            raise RuntimeError(f"{name} analogue artifacts changed during measurement")
    before_events = dict(line.split() for line in state["memory.events"].splitlines())
    after_events = dict(line.split() for line in meta["cgroup_after"]["memory.events"].splitlines())
    if any(before_events[key] != after_events[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM occurred during managed analogue; discard samples")
    before_cpu = dict(line.split() for line in state["cpu.stat"].splitlines())
    after_cpu = dict(line.split() for line in meta["cgroup_after"]["cpu.stat"].splitlines())
    if any(before_cpu.get(key) != after_cpu.get(key)
           for key in ("nr_throttled", "throttled_usec")):
        raise RuntimeError("cgroup CPU throttled during managed analogue; discard samples")


if __name__ == "__main__":
    main()
