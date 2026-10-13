#!/usr/bin/env python3
"""Build and measure native GC publication in a quiet 64 GiB cgroup.

Each process creates 4,000,000 one-field Wasm GC objects, then performs ten
million checked field set/get pairs across 1,024 retained objects. Nine
reversed-order pairs compare one versus two and one versus four P-core workers,
with immutable raw logs, compiler/source fingerprints, RSS, and separately
measured allocation, local fields, and teardown durations.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import time

from run import cgroup_preflight


LIBS = "/work/deps/usr/lib/x86_64-linux-gnu:/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_one(command, label, logs, env):
    log = logs / (label + ".log")
    with log.open("wb") as output:
        begin = time.perf_counter_ns()
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                   env=env, close_fds=True)
        _, status, usage = os.wait4(process.pid, 0)
        elapsed = time.perf_counter_ns() - begin
        process.returncode = os.waitstatus_to_exitcode(status)
    if process.returncode:
        raise RuntimeError(f"{label}: exit {process.returncode}: {log.read_text(errors='replace')[-4000:]}")
    result = json.loads(log.read_text())
    if (result["allocations"] != 4000000 or result["field_iterations"] != 10000000 or
            result["field_roots"] != 1024):
        raise RuntimeError(f"{label}: native fixture returned wrong work count")
    return {"label": label, "command": command, "elapsed_ns": elapsed,
            "maxrss_kib": usage.ru_maxrss, "user_s": usage.ru_utime,
            "system_s": usage.ru_stime, **result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--bench-source", type=Path, default=Path(__file__).with_name("gc_publish_scaling.cc"))
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--build-json", type=Path, required=True,
                        help="O3 CLI manifest for the exact source snapshot")
    parser.add_argument("--expected-source-id", required=True,
                        help="frozen src/third-parties fingerprint")
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if args.pairs < 9 or args.out.exists():
        parser.error("require at least nine pairs and a new output directory")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / "run_gc_publish_scaling.py")
    bench_snapshot = args.out / "gc_publish_scaling.cc"
    shutil.copyfile(args.bench_source, bench_snapshot)
    header = args.source_root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
    build = json.loads(args.build_json.read_text())
    product = Path(build["cli_command"][-1]).resolve(strict=True)
    if (Path(build["source"]).resolve(strict=True) != args.source_root.resolve(strict=True)
            or build["source_id"] != args.expected_source_id or
            build["binary_sha256"] != sha256(product) or
            "-O3" not in build["runtime_command"] or "-O3" not in build["cli_command"]):
        raise RuntimeError("GC publication baseline source does not match O3 build manifest")
    fingerprint = args.source_root / "tools/ci/wasm3_source_fingerprint.py"
    source_before = subprocess.check_output(
        [sys.executable, str(fingerprint), str(args.source_root),
         str(args.out / "source-before.json")], text=True).strip()
    if source_before != args.expected_source_id:
        raise RuntimeError("GC publication baseline source fingerprint changed before build")
    binary = args.out / "gc_publish_scaling"
    compile_command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                       "-stdlib=libc++", "-O3", "-g0", "-fno-rtti", "-pthread",
                       "-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
                       "-I" + str(args.source_root / "src"),
                       "-I" + str(args.source_root / "third-parties/fast_io/include"),
                       "-I" + str(args.source_root / "third-parties/bizwen/include"),
                       "-I" + str(args.source_root / "third-parties/boost_unordered/include"),
                       "-L/work/deps/usr/lib/x86_64-linux-gnu",
                       str(bench_snapshot), "-o", str(binary)]
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = LIBS + ":" + env.get("LD_LIBRARY_PATH", "")
    build_log = logs / "build.log"
    with build_log.open("wb") as output:
        subprocess.run(compile_command, cwd=args.source_root, stdout=output,
                       stderr=subprocess.STDOUT, env=env, check=True)
    metadata = {"start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "cgroup_before": before, "p_cores": [0, 2, 4, 6], "compile_core": 16,
                "pairs": args.pairs, "allocations_per_process": 4000000,
                "local_field_set_get_pairs_per_process": 10000000,
                "local_field_roots": 1024,
                "expected_source_id": args.expected_source_id,
                "build_json_path": str(args.build_json),
                "build_json_sha256": sha256(args.build_json),
                "product_path": str(product),
                "product_sha256": sha256(product),
                "compile_command": compile_command,
                "sha256": {"compiler": sha256(args.clang), "bench_source": sha256(args.bench_source),
                           "gc_header": sha256(header), "binary": sha256(binary)}}
    metadata["build_json"] = build
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    for mode in ("one", "two", "four"):
        row = run_one(["taskset", "-c", "0,2,4,6", str(binary), mode],
                      "warmup-" + mode, logs, env)
        row.update(mode=mode, phase="warmup")
        rows.append(row)
    summary = []
    for comparison in ("two", "four"):
        ratios = []
        alloc_ns = {"one": [], comparison: []}
        field_ns = {"one": [], comparison: []}
        teardown_ns = {"one": [], comparison: []}
        rss = {"one": [], comparison: []}
        for pair in range(args.pairs):
            order = ("one", comparison) if pair % 2 == 0 else (comparison, "one")
            timed = {}
            for mode in order:
                row = run_one(["taskset", "-c", "0,2,4,6", str(binary), mode],
                              f"{comparison}-pair{pair}-{mode}", logs, env)
                row.update(mode=mode, phase="timed", comparison=comparison,
                           pair=pair, order=list(order))
                rows.append(row)
                timed[mode] = row
                alloc_ns[mode].append(row["allocation_ns"])
                field_ns[mode].append(row["field_ns"])
                teardown_ns[mode].append(row["teardown_ns"])
                rss[mode].append(row["maxrss_kib"])
                (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
            ratios.append(timed["one"]["allocation_ns"] / timed[comparison]["allocation_ns"])
        record = {"comparison": comparison, "pairs": args.pairs,
                  "median_paired_speedup": statistics.median(ratios),
                  "paired_speedups": ratios,
                  "median_allocation_ns": {mode: statistics.median(values)
                                            for mode, values in alloc_ns.items()},
                  "median_field_ns": {mode: statistics.median(values)
                                       for mode, values in field_ns.items()},
                  "median_teardown_ns": {mode: statistics.median(values)
                                          for mode, values in teardown_ns.items()},
                  "median_maxrss_mib": {mode: statistics.median(values) / 1024
                                        for mode, values in rss.items()},
                  "allocation_p95_ns": {mode: statistics.quantiles(values, n=20, method="inclusive")[18]
                                        for mode, values in alloc_ns.items()},
                  "field_p95_ns": {mode: statistics.quantiles(values, n=20, method="inclusive")[18]
                                   for mode, values in field_ns.items()}}
        summary.append(record)
        (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(comparison, "paired speedup", record["median_paired_speedup"], flush=True)
    metadata["cgroup_after"] = cgroup_preflight(0)
    metadata["source_id_after"] = subprocess.check_output(
        [sys.executable, str(fingerprint), str(args.source_root),
         str(args.out / "source-after.json")], text=True).strip()
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    before_events = dict(line.split() for line in before["memory.events"].splitlines())
    after_events = dict(line.split() for line in metadata["cgroup_after"]["memory.events"].splitlines())
    before_cpu = dict(line.split() for line in before["cpu.stat"].splitlines())
    after_cpu = dict(line.split() for line in metadata["cgroup_after"]["cpu.stat"].splitlines())
    if (metadata["source_id_after"] != args.expected_source_id or
            sha256(args.bench_source) != metadata["sha256"]["bench_source"] or
            sha256(bench_snapshot) != metadata["sha256"]["bench_source"] or
            sha256(header) != metadata["sha256"]["gc_header"] or
            sha256(args.clang) != metadata["sha256"]["compiler"] or
            sha256(binary) != metadata["sha256"]["binary"] or
            sha256(product) != metadata["product_sha256"] or
            sha256(args.build_json) != metadata["build_json_sha256"] or
            any(before_events[name] != after_events[name] for name in ("oom", "oom_kill")) or
            any(before_cpu.get(name) != after_cpu.get(name)
                for name in ("nr_throttled", "throttled_usec"))):
        raise RuntimeError("GC publication baseline invalid: source, tool, OOM, or throttle drift")


if __name__ == "__main__":
    main()
