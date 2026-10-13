#!/usr/bin/env python3
"""Source-bound GC-event diagnostics for cross-language root-ring analogues.

This is a separate traced run, never part of the untraced throughput ranking.
Java and Node traces provide pause-event distributions; the .NET public APIs
provide collection counts and total pause time, but no per-event quantiles.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time

from generate import endpoint
from run import (cgroup_preflight, cpu_telemetry, engine_order_for_pair,
                 node_runtime_environment, run_one, sha256, verify_product_build)
from run_managed import expected_root_checksum


JAVA_PAUSE = re.compile(
    r"\bGC\(\d+\)(?:\s+[A-Za-z]:)?\s+Pause\s+(.+?)\s+([0-9]+(?:\.[0-9]+)?)ms$")
NODE_PAUSE = re.compile(
    r"^\[[^]]+\]\s+[0-9]+ ms: (.+?),\s+"
    r"(?:pooled:\s*[0-9.]+ MB,\s*)?([0-9]+(?:\.[0-9]+)?)\s*/\s*"
    r"[0-9]+(?:\.[0-9]+)? ms")


def traced_region(lines):
    start = [i for i, line in enumerate(lines) if line.strip() == "GC_TELEMETRY_START"]
    end = [i for i, line in enumerate(lines) if line.strip() == "GC_TELEMETRY_END"]
    if len(start) != 1 or len(end) != 1 or start[0] >= end[0]:
        raise RuntimeError("GC trace is missing one ordered timed-loop marker pair")
    return lines[start[0] + 1:end[0]]


def parse_pauses(log, engine):
    lines = traced_region(log.splitlines())
    pattern = NODE_PAUSE if engine == "node-v8" else JAVA_PAUSE
    matches = [(match.group(1), float(match.group(2)))
               for line in lines if (match := pattern.search(line))]
    return [{"kind": kind, "duration_ms": duration} for kind, duration in matches]


def quantiles(values):
    if not values:
        return {"event_count": 0, "p50_ms": None, "p95_ms": None,
                "p95_sample_qualified": False, "p99_ms_estimate": None,
                "p99_sample_qualified": False}
    if len(values) == 1:
        return {"event_count": 1, "p50_ms": values[0], "p95_ms": None,
                "p95_sample_qualified": False, "p99_ms_estimate": None,
                "p99_sample_qualified": False}
    return {"event_count": len(values), "p50_ms": statistics.median(values),
            "p95_ms": statistics.quantiles(values, n=20, method="inclusive")[18],
            "p95_sample_qualified": len(values) >= 20,
            "p99_ms_estimate": statistics.quantiles(values, n=100, method="inclusive")[98],
            "p99_sample_qualified": len(values) >= 100}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--openjdk", required=True, type=Path)
    parser.add_argument("--graalvm", required=True, type=Path)
    parser.add_argument("--java-class-dir", required=True, type=Path)
    parser.add_argument("--dotnet", required=True, type=Path)
    parser.add_argument("--dotnet-dll", required=True, type=Path)
    parser.add_argument("--node", required=True, type=Path)
    parser.add_argument("--node-script", required=True, type=Path)
    parser.add_argument("--build-manifest", required=True, type=Path)
    parser.add_argument("--count", type=int, default=50000000)
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--affinity", default=None,
                        help="comma-separated P cores for the whole process, including GC workers; "
                             "default is the selected single --cpu")
    args = parser.parse_args()
    if args.out.exists() or not 1024 <= args.count <= 0x7fffffff or args.samples < 3:
        parser.error("fresh output, signed-i32 count >=1024 and at least three samples required")
    before = cgroup_preflight(args.cpu)
    try:
        affinity_cpus = sorted({int(part) for part in
                                (args.affinity or str(args.cpu)).split(",")})
    except ValueError:
        parser.error("affinity must be a comma-separated list of P-core numbers")
    if not affinity_cpus or args.cpu not in affinity_cpus or not set(affinity_cpus) <= {0, 2, 4, 6}:
        parser.error("affinity must contain --cpu and only the configured P cores 0,2,4,6")
    affinity = ",".join(map(str, affinity_cpus))
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    products = {label: verify_product_build(label, binary, args.out, "before")
                for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    intended = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: row["source_id"] for label, row in products.items()} != intended:
        raise RuntimeError("managed diagnostic paired with wrong frozen product IDs")
    source = Path(__file__).parent
    sources = {"java": source / "managed/java/ManagedRing.java",
               "dotnet": source / "managed/dotnet/Program.cs",
               "dotnet-project": source / "managed/dotnet/ManagedRing.csproj",
               "node": args.node_script}
    helper_sources = {name: source / (name + ".py")
                      for name in ("generate", "run", "run_managed")}
    snapshots = args.out / "sources"
    snapshots.mkdir()
    for path in sources.values():
        shutil.copyfile(path, snapshots / path.name)
    for path in helper_sources.values():
        shutil.copyfile(path, snapshots / path.name)
    build = json.loads(args.build_manifest.read_text())
    if ({name: sha256(path) for name, path in sources.items()} != build["source_sha256"] or
            build["java_release"] != 21 or
            build["outputs"]["java_class"] !=
                {"path": str(args.java_class_dir / "ManagedRing.class"),
                 "sha256": sha256(args.java_class_dir / "ManagedRing.class")} or
            build["outputs"]["dotnet_dll"] !=
                {"path": str(args.dotnet_dll), "sha256": sha256(args.dotnet_dll)}):
        raise RuntimeError("managed diagnostic source/output differs from build manifest")
    for label, folder in (("java", args.java_class_dir), ("dotnet", args.dotnet_dll.parent)):
        actual = {str(path.relative_to(folder)): sha256(path)
                  for path in sorted(folder.rglob("*")) if path.is_file()}
        if actual != build["artifact_sha256"][label]:
            raise RuntimeError(f"{label} managed build artifacts changed")
    paths = {"openjdk": args.openjdk, "graalvm": args.graalvm,
             "java_class": args.java_class_dir / "ManagedRing.class",
             "dotnet": args.dotnet, "dotnet_dll": args.dotnet_dll,
             "node": args.node, "node_script": args.node_script}
    hashes = {label: sha256(path) for label, path in paths.items()}
    env = os.environ.copy()
    env.update(DOTNET_CLI_HOME=str(args.out / "dotnet-home"),
               DOTNET_ROOT=str(args.dotnet.resolve(strict=True).parent),
               DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1", DOTNET_CLI_TELEMETRY_OPTOUT="1",
               DOTNET_TieredPGO="1", DOTNET_gcServer="0",
               DOTNET_GCHeapHardLimit="0x40000000",
               NODE_OPTIONS="--max-old-space-size=1024")
    node_env = node_runtime_environment(env)
    versions = {}
    for label, executable, flags in (("openjdk", args.openjdk, ("-version",)),
                                      ("graalvm", args.graalvm, ("-version",)),
                                      ("dotnet", args.dotnet, ("--version",)),
                                      ("node", args.node, ("--version",))):
        result = subprocess.run(["taskset", "-c", affinity, str(executable), *flags],
                                capture_output=True,
                                env=node_env if label == "node" else env, timeout=30)
        if result.returncode:
            raise RuntimeError(f"{label} version probe failed: {result.returncode}")
        versions[label] = (result.stdout + result.stderr).decode(errors="replace").strip()
    metadata = {"scope": "traced GC event diagnostic; not untraced VM throughput ranking",
                "source_builds": products, "expected_source_ids": intended,
                "build_manifest_sha256": sha256(args.build_manifest),
                "source_sha256": build["source_sha256"], "tool_sha256": hashes,
                "helper_source_sha256": {name: sha256(path)
                                         for name, path in helper_sources.items()},
                "tool_paths": {label: str(path) for label, path in paths.items()},
                "runtime_versions": versions, "cgroup_before": before,
                "cpu": args.cpu, "affinity_p_cores": affinity_cpus,
                "count": args.count, "samples": args.samples,
                "runner_sha256": sha256(Path(__file__)),
                "java_gc": "explicit OpenJDK HotSpot G1/ZGC and GraalVM CE HotSpot G1; not Native Image",
                "node_gc": "V8 as shipped by selected Node binary",
                "dotnet_gc": "selected .NET runtime; public API total pause only, no per-event quantiles",
                "runtime_flags": {"java_common": ["-Xms256m", "-Xmx1g"],
                                  "g1": ["-XX:+UseG1GC", "-Xlog:gc=info:stdout"],
                                  "zgc": ["-XX:+UseZGC",
                                          "-Xlog:gc=info,gc+phases=debug:stdout"],
                                  "node": ["--trace-gc", env["NODE_OPTIONS"]],
                                  "node_ld_library_path": node_env["LD_LIBRARY_PATH"],
                                  "dotnet": {key: env[key] for key in
                                             ("DOTNET_TieredPGO", "DOTNET_gcServer",
                                              "DOTNET_GCHeapHardLimit",
                                              "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT")}},
                "trace_sources": {
                    "java": "https://docs.oracle.com/en/java/javase/21/troubleshoot/troubleshooting-memory-leaks.html",
                    "node": "https://nodejs.org/learn/diagnostics/memory/using-gc-traces",
                    "dotnet": "https://learn.microsoft.com/dotnet/api/system.gc.gettotalpauseduration"},
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    engines = {
        "openjdk-g1": [str(args.openjdk), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                       "-Xlog:gc=info:stdout", "-cp", str(args.java_class_dir), "ManagedRing"],
        "openjdk-zgc": [str(args.openjdk), "-Xms256m", "-Xmx1g", "-XX:+UseZGC",
                        "-Xlog:gc=info,gc+phases=debug:stdout", "-cp",
                        str(args.java_class_dir), "ManagedRing"],
        "graalvm-g1": [str(args.graalvm), "-Xms256m", "-Xmx1g", "-XX:+UseG1GC",
                       "-Xlog:gc=info:stdout", "-cp", str(args.java_class_dir), "ManagedRing"],
        "dotnet": [str(args.dotnet), str(args.dotnet_dll)],
        "node-v8": [str(args.node), "--trace-gc", str(args.node_script)],
    }
    expected_state = endpoint(args.count)
    expected_roots = expected_root_checksum(args.count)
    rows = []
    for sample in range(args.samples):
        order = engine_order_for_pair(list(engines), sample)
        telemetry_before = {str(cpu): cpu_telemetry(cpu) for cpu in affinity_cpus}
        for engine in order:
            label = f"{engine}-sample{sample}"
            command = ["taskset", "-c", affinity, *engines[engine],
                       str(args.count), "--gc-telemetry"]
            process = run_one(command, label, logs,
                              node_env if engine == "node-v8" else env,
                              timeout_seconds=300)
            log = (logs / (label + ".log")).read_text(errors="replace")
            output = [json.loads(line) for line in log.splitlines() if line.startswith("{")]
            if len(output) != 1:
                raise RuntimeError(f"{label}: expected one machine-readable result")
            guest = output[0]
            runtime = "java" if engine.startswith(("openjdk-", "graalvm-")) else (
                "node" if engine == "node-v8" else "dotnet")
            if (guest.get("runtime") != runtime or guest.get("iterations") != args.count or
                    guest.get("checksum") != expected_state or
                    guest.get("root_checksum") != expected_roots):
                raise RuntimeError(f"{label}: root-ring semantic checksum mismatch")
            if engine == "dotnet" and (guest.get("server_gc") is not False or
                    guest.get("gc_heap_limit_config_bytes") != 1 << 30):
                raise RuntimeError(f"{label}: .NET GC mode or heap cap not applied")
            # A root-ring checksum alone cannot rule out allocation removal.
            # Each measured iteration publishes a fresh object into a global
            # root array; the native allocation counters must show at least
            # eight bytes per iteration before comparing collector behavior.
            if engine.startswith(("openjdk-", "graalvm-")):
                allocated = guest.get("main_thread_allocated_bytes", 0)
            elif engine == "dotnet":
                allocated = guest.get("gc_allocated_bytes", 0)
            else:
                allocated = None  # V8 only exposes pause traces here.
            if allocated is not None and allocated < args.count * 8:
                raise RuntimeError(f"{label}: allocation counter suggests scalar replacement")
            pauses = parse_pauses(log, engine) if engine != "dotnet" else []
            row = {"engine": engine, "sample": sample, "command": command,
                   "process": process, "guest": guest, "pause_events": pauses,
                   "pause_event_count": len(pauses), "log_sha256": sha256(logs / (label + ".log"))}
            rows.append(row)
            (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
        with (args.out / "pair-telemetry.jsonl").open("a") as stream:
            stream.write(json.dumps({"sample": sample, "engine_order": order,
                                     "before": telemetry_before,
                                     "after": {str(cpu): cpu_telemetry(cpu)
                                               for cpu in affinity_cpus}}) + "\n")
    summary = []
    for engine in engines:
        selected = [row for row in rows if row["engine"] == engine]
        pauses = [event["duration_ms"] for row in selected for event in row["pause_events"]]
        java_engine = engine.startswith(("openjdk-", "graalvm-"))
        counts = ([row["guest"]["gc_collection_count"] for row in selected]
                  if java_engine else
                  [row["guest"]["gc_gen0_count"] for row in selected]
                  if engine == "dotnet" else [row["pause_event_count"] for row in selected])
        record = {"engine": engine, "process_count": len(selected),
                  "collection_counts_by_process": counts,
                  "observed_collection_count": sum(counts),
                  "collection_count_definition": (
                      "sum of collector MXBean collection-count deltas; may not equal unique GC cycles"
                      if java_engine else "generation-0 collection-count delta"
                      if engine == "dotnet" else "timed-region V8 GC trace events"),
                  "pause_distribution": quantiles(pauses) if engine != "dotnet" else None,
                  "dotnet_total_pause_ns_by_process": (
                      [row["guest"]["gc_total_pause_ns"] for row in selected]
                      if engine == "dotnet" else None),
                  "allocated_bytes_by_process": (
                      [row["guest"]["main_thread_allocated_bytes"] for row in selected]
                      if java_engine else
                      [row["guest"]["gc_allocated_bytes"] for row in selected]
                      if engine == "dotnet" else None),
                  "minimum_observed_bytes_per_iteration": 8 if engine != "node-v8" else None,
                  "dotnet_gc_mode": (
                      "workstation" if engine == "dotnet" else None),
                  "dotnet_gc_heap_limit_config_bytes_by_process": (
                      [row["guest"]["gc_heap_limit_config_bytes"] for row in selected]
                      if engine == "dotnet" else None),
                  "dotnet_gc_heap_budget_bytes_by_process": (
                      [row["guest"]["gc_heap_budget_bytes"] for row in selected]
                      if engine == "dotnet" else None),
                  "peak_rss_mib": max(row["process"]["maxrss_kib"] for row in selected) / 1024,
                  "diagnostic_not_comparable_to_untraced_throughput": True}
        summary.append(record)
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    after = cgroup_preflight(args.cpu)
    products_after = {label: verify_product_build(label, binary, args.out, "after")
                      for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    metadata["cgroup_after"] = after
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    old_events = dict(line.split() for line in before["memory.events"].splitlines())
    new_events = dict(line.split() for line in after["memory.events"].splitlines())
    old_cpu = dict(line.split() for line in before["cpu.stat"].splitlines())
    new_cpu = dict(line.split() for line in after["cpu.stat"].splitlines())
    if (products_after != products or
            any(old_events[name] != new_events[name] for name in ("oom", "oom_kill")) or
            any(old_cpu.get(name) != new_cpu.get(name)
                for name in ("nr_throttled", "throttled_usec")) or
            sha256(args.build_manifest) != metadata["build_manifest_sha256"] or
            any(sha256(path) != hashes[label] for label, path in paths.items()) or
            {name: sha256(path) for name, path in sources.items()} != build["source_sha256"] or
            any(sha256(snapshots / path.name) != build["source_sha256"][name]
                for name, path in sources.items()) or
            {name: sha256(path) for name, path in helper_sources.items()} !=
                metadata["helper_source_sha256"] or
            any(sha256(snapshots / path.name) != metadata["helper_source_sha256"][name]
                for name, path in helper_sources.items())):
        raise RuntimeError("managed GC diagnostic inputs/cgroup changed")
    if any(record["observed_collection_count"] <= 0 for record in summary):
        raise RuntimeError("one managed runtime had no observed GC in the traced workload")
    if any(record["pause_distribution"]["event_count"] <= 0
           for record in summary if record["pause_distribution"] is not None):
        raise RuntimeError("a traced managed runtime had no timed-loop GC pause event")
    print("PASS managed GC event diagnostic; pause quantiles are separate from throughput")


if __name__ == "__main__":
    main()
