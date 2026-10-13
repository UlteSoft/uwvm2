#!/usr/bin/env python3
"""Paired P-core Java/.NET/Node thread-start analogues, never a same-Wasm ranking."""

import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from run import (cgroup_preflight, cpu_telemetry, engine_order_for_pair,
                 node_runtime_environment, run_one, sha256, verify_product_build)


def kernel(worker):
    mask = 0xFFFFFFFF
    memory = [0] * 1024
    seed = 0x12345 + worker
    for index in range(1024):
        memory[index] = index + seed
    for step in range(32):
        for index in range(1024):
            value = memory[index]
            rotated = ((value << 7) | (value >> 25)) & mask
            memory[index] = ((rotated ^ (seed + step)) + 0x9E3779B9) & mask
    checksum = 0
    for value in memory:
        checksum = (((checksum << 5) | (checksum >> 27)) + value) & mask
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
    parser.add_argument("--build-manifest", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--affinity", choices=("single", "four"), required=True)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--pairs", type=int, default=9)
    parser.add_argument("--rounds", type=int, default=32)
    args = parser.parse_args()
    if args.out.exists() or args.pairs < 9 or not 1 <= args.rounds <= 1024:
        parser.error("fresh output, >=9 pairs and 1..1024 rounds required")
    if args.affinity == "four" and args.cpu != 0:
        parser.error("four-core run must use --cpu 0")
    before = cgroup_preflight(args.cpu)
    affinity = str(args.cpu) if args.affinity == "single" else "0,2,4,6"
    args.out.mkdir(parents=True)
    products = {label: verify_product_build(label, binary, args.out, "before")
                for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected_source_ids = {"ordinary": args.ordinary_source_id,
                           "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in products.items()} != expected_source_ids:
        raise RuntimeError("managed thread analogue paired with the wrong product source IDs")
    native_fixtures = {label: Path(product["source"]) / "test/0017.runtime/wasm_thread_performance.cc"
                       for label, product in products.items()}
    native_fixture_sha256 = {label: sha256(path) for label, path in native_fixtures.items()}
    if len(set(native_fixture_sha256.values())) != 1:
        raise RuntimeError("ordinary/ROS reference thread kernels differ")
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / "run_managed_threads.py")
    base = Path(__file__).parent / "managed_threads"
    sources = {"java": base / "java/ManagedThreads.java",
               "dotnet": base / "dotnet/Program.cs",
               "dotnet_project": base / "dotnet/ManagedThreads.csproj",
               "node": args.node_script}
    build = json.loads(args.build_manifest.read_text())
    if {name: sha256(path) for name, path in sources.items()} != build["source_sha256"]:
        raise RuntimeError("managed thread analogue sources changed since build")
    outputs = {"java_class": args.java_class_dir / "ManagedThreads.class",
               "dotnet_dll": args.dotnet_dll}
    if any(build["outputs"][name] != {"path": str(path), "sha256": sha256(path)}
           for name, path in outputs.items()):
        raise RuntimeError("managed thread analogue build output changed")
    for kind, directory in (("java", args.java_class_dir), ("dotnet", args.dotnet_dll.parent)):
        actual = {str(path.relative_to(directory)): sha256(path)
                  for path in sorted(directory.rglob("*")) if path.is_file()}
        if actual != build["artifact_sha256"][kind]:
            raise RuntimeError(f"{kind} build artifacts changed")
    snapshots = args.out / "sources"
    snapshots.mkdir()
    for source in sources.values():
        shutil.copyfile(source, snapshots / source.name)
    paths = {"openjdk": args.openjdk, "graalvm": args.graalvm,
             "dotnet": args.dotnet, "node": args.node,
             "java_class": outputs["java_class"], "dotnet_dll": args.dotnet_dll,
             "node_script": args.node_script}
    hashes = {name: sha256(path) for name, path in paths.items()}
    env = os.environ.copy()
    env.update(DOTNET_CLI_HOME=str(args.out / "dotnet-home"),
               DOTNET_ROOT=str(args.dotnet.resolve(strict=True).parent),
               DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", DOTNET_CLI_TELEMETRY_OPTOUT="1",
               DOTNET_TieredPGO="1", DOTNET_gcServer="0",
               DOTNET_GCHeapHardLimit="0x40000000",
               NODE_OPTIONS="--max-old-space-size=1024")
    node_env = node_runtime_environment(env)
    engines = {
        "openjdk-g1": [str(args.openjdk), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                       "-cp", str(args.java_class_dir), "ManagedThreads"],
        "graalvm-g1": [str(args.graalvm), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                       "-cp", str(args.java_class_dir), "ManagedThreads"],
        "dotnet-thread": [str(args.dotnet), str(args.dotnet_dll)],
        "node-worker-isolate": [str(args.node), str(args.node_script)],
    }
    expected = {workers: args.rounds * sum(kernel(worker) for worker in range(workers))
                for workers in (1, 4)}
    meta = {"affinity": affinity, "cpu": args.cpu, "pairs": args.pairs,
            "rounds": args.rounds, "programs_are_cross_language_analogues": True,
            "node_worker_starts_new_v8_isolate": True,
            "java_and_dotnet_use_platform_threads": True,
            "scope": "start, same integer kernel, join; does not execute identical Wasm code",
            "source_sha256": build["source_sha256"], "build_manifest_sha256": sha256(args.build_manifest),
            "product_builds": products, "expected_source_ids": expected_source_ids,
            "native_fixture_sha256": native_fixture_sha256,
            "paths": {name: {"path": str(path), "sha256": hashes[name]}
                      for name, path in paths.items()},
            "runtime_commands": engines, "expected_checksum": expected,
            "dotnet_gc_mode": "workstation (DOTNET_gcServer=0)",
            "dotnet_gc_heap_hard_limit": env["DOTNET_GCHeapHardLimit"],
            "node_ld_library_path": node_env["LD_LIBRARY_PATH"],
            "cgroup_before": before, "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    meta["runtime_versions"] = {}
    for label, executable, flags in (("openjdk", args.openjdk, ("-version",)),
                                      ("graalvm", args.graalvm, ("-version",)),
                                      ("dotnet", args.dotnet, ("--version",)),
                                      ("node", args.node, ("--version",))):
        probe = subprocess.run(["taskset", "-c", str(args.cpu), str(executable), *flags],
                               capture_output=True,
                               env=node_env if label == "node" else env, timeout=30)
        if probe.returncode:
            raise RuntimeError(f"{label} version probe exited {probe.returncode}")
        meta["runtime_versions"][label] = (probe.stdout + probe.stderr).decode(errors="replace").strip()
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    rows = []
    for pair in range(args.pairs):
        order = engine_order_for_pair(list(engines), pair)
        telemetry_before = {str(cpu): cpu_telemetry(cpu)
                            for cpu in ((args.cpu,) if args.affinity == "single" else (0, 2, 4, 6))}
        for engine in order:
            label = f"{engine}-pair{pair}-{args.affinity}"
            invocation = ["taskset", "-c", affinity, *engines[engine], "1", str(args.rounds),
                          str(pair & 1)]
            process = run_one(invocation, label, logs,
                              node_env if engine == "node-worker-isolate" else env,
                              timeout_seconds=300)
            outputs = [json.loads(line) for line in (logs / (label + ".log")).read_text().splitlines()
                       if line.startswith("{")]
            if len(outputs) != 4:
                raise RuntimeError(f"{label}: expected four timed rows; saw {len(outputs)}")
            keyed = {(item["workers"], item["threaded"]): item for item in outputs}
            if set(keyed) != {(1, False), (1, True), (4, False), (4, True)}:
                raise RuntimeError(f"{label}: wrong worker/path cells")
            for item in outputs:
                workers = item["workers"]
                if (item["sample"] != 0 or item["rounds"] != args.rounds or
                        item["checksum"] != expected[workers] or
                        item["requested_threads"] != (workers * args.rounds if item["threaded"] else 0) or
                        item["wall_ns"] <= 0):
                    raise RuntimeError(f"{label}: semantic/self-check failed: {item}")
            rows.append({"engine": engine, "pair": pair, "process": process,
                         "program_rows": outputs, "order": order})
            (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
        telemetry_after = {str(cpu): cpu_telemetry(cpu)
                           for cpu in ((args.cpu,) if args.affinity == "single" else (0, 2, 4, 6))}
        with (args.out / "pair-telemetry.jsonl").open("a") as stream:
            stream.write(json.dumps({"pair": pair, "engine_order": order,
                                     "before": telemetry_before, "after": telemetry_after}) + "\n")
    summaries = []
    for engine in engines:
        for workers in (1, 4):
            engine_rows = [row for row in rows if row["engine"] == engine]
            threaded = [next(item["wall_ns"] for item in row["program_rows"]
                             if item["workers"] == workers and item["threaded"])
                        for row in engine_rows]
            direct = [next(item["wall_ns"] for item in row["program_rows"]
                           if item["workers"] == workers and not item["threaded"])
                      for row in engine_rows]
            delta = [new - old for new, old in zip(threaded, direct)]
            median_threaded = statistics.median(threaded)
            summaries.append({"engine": engine, "workers": workers,
                              "median_threaded_wall_ns_per_round": median_threaded,
                              "median_direct_wall_ns_per_round": statistics.median(direct),
                              "median_threaded_minus_direct_wall_ns_per_round": statistics.median(delta),
                              "p95_threaded_minus_direct_wall_ns_per_round": statistics.quantiles(
                                  delta, n=20, method="inclusive")[18],
                              "raw_threaded_minus_direct_wall_ns_per_round": delta,
                              "end_to_end_threaded_updates_per_second":
                                  workers * 1024 * 32 * 1e9 / median_threaded,
                              "delta_includes_parallel_speedup_and_is_not_pure_spawn_latency": True,
                              "maxrss_kib": max(row["process"]["maxrss_kib"] for row in engine_rows)})
    (args.out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    after = cgroup_preflight(args.cpu)
    products_after = {label: verify_product_build(label, binary, args.out, "after")
                      for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    meta["cgroup_after"] = after
    meta["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    old_events = dict(line.split() for line in before["memory.events"].splitlines())
    new_events = dict(line.split() for line in after["memory.events"].splitlines())
    old_cpu = dict(line.split() for line in before["cpu.stat"].splitlines())
    new_cpu = dict(line.split() for line in after["cpu.stat"].splitlines())
    if (products_after != products or
            any(old_events[name] != new_events[name] for name in ("oom", "oom_kill")) or
            any(old_cpu.get(name) != new_cpu.get(name)
                for name in ("nr_throttled", "throttled_usec")) or
            sha256(args.build_manifest) != meta["build_manifest_sha256"] or
            any(sha256(path) != hashes[name] for name, path in paths.items()) or
            {name: sha256(path) for name, path in sources.items()} != build["source_sha256"] or
            {label: sha256(path) for label, path in native_fixtures.items()} != native_fixture_sha256):
        raise RuntimeError("cgroup OOM/throttling or benchmark input changed")
    print(f"PASS managed-thread analogue: {len(rows)} checked processes, affinity {affinity}")


if __name__ == "__main__":
    main()
