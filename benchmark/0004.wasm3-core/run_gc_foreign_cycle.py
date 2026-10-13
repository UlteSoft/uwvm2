#!/usr/bin/env python3
"""Exact-source ASan/LSan release probes for foreign GC field leases."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from run import cgroup_preflight, sha256, verify_product_build


def invoke(command, log, env, cwd=None):
    with log.open("wb") as output:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    return {"command": command, "exit": result.returncode, "log": str(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--fixture", type=Path,
                        default=Path(__file__).with_name("gc_foreign_field_cycle.cc"))
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists; release evidence must be immutable")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    staged_runner = args.out / "run_gc_foreign_cycle.py"
    staged_fixture = args.out / "gc_foreign_field_cycle.cc"
    shutil.copyfile(__file__, staged_runner)
    shutil.copyfile(args.fixture, staged_fixture)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join(("/toolchain/lib/x86_64-unknown-linux-gnu",
        "/toolchain/lib", "/work/deps/usr/lib/x86_64-linux-gnu", env.get("LD_LIBRARY_PATH", "")))
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != expected:
        raise RuntimeError("O3 binaries do not match the explicitly requested frozen source IDs")
    metadata = {"source_builds": builds, "compiler_sha256": sha256(args.clang),
                "expected_source_ids": expected,
                "runner_sha256": sha256(staged_runner),
                "fixture_sha256": sha256(staged_fixture), "cgroup_before": before,
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    for label, build in builds.items():
        source = Path(build["source"])
        peer_fixture = source / "benchmark/0004.wasm3-core/gc_foreign_field_cycle.cc"
        if sha256(peer_fixture) != metadata["fixture_sha256"]:
            raise RuntimeError(f"{label}: source fixture differs from staged probe")
        binary = args.out / f"gc_foreign_field_cycle_{label}_asan"
        command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                   "-stdlib=libc++", "-fno-rtti", "-O1", "-g1", "-Werror",
                   "-Wno-undefined-inline", "-fsanitize=address", "-fno-omit-frame-pointer",
                   "-I" + str(source / "src"),
                   "-I" + str(source / "third-parties/fast_io/include"),
                   "-I" + str(source / "third-parties/bizwen/include"),
                   "-I" + str(source / "third-parties/boost_unordered/include"),
                   str(staged_fixture), "-fuse-ld=lld", "-rtlib=compiler-rt",
                   "-unwindlib=libunwind", "-L/work/deps/usr/lib/x86_64-linux-gnu",
                   "-pthread", "-o", str(binary)]
        row = invoke(command, args.out / f"{label}-compile.log", env, source)
        row.update(product=label, phase="compile")
        rows.append(row)
        if row["exit"]:
            raise RuntimeError(f"{label} ASan probe did not compile")
        for scenario in ("mutual-cycle", "overwritten-field", "mutual-bridge"):
            for leaks in (False, True):
                phase = "leak-sanitizer" if leaks else "weak-expiration"
                row = invoke(["taskset", "-c", "16", str(binary), scenario],
                             args.out / f"{label}-{scenario}-{phase}.log",
                             {**env, "ASAN_OPTIONS":
                              f"detect_leaks={int(leaks)}:halt_on_error=1"})
                row.update(product=label, scenario=scenario, phase=phase,
                           binary_sha256=sha256(binary))
                rows.append(row)
                (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != builds:
        raise RuntimeError("source/product changed during foreign-field lease probe")
    if sha256(staged_fixture) != metadata["fixture_sha256"] or sha256(staged_runner) != metadata["runner_sha256"]:
        raise RuntimeError("staged foreign-field probe changed during qualification")
    cgroup_after = cgroup_preflight(0)
    before_events = dict(line.split() for line in before["memory.events"].splitlines())
    after_events = dict(line.split() for line in cgroup_after["memory.events"].splitlines())
    if any(int(after_events[name]) != int(before_events[name]) for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during foreign-field lease probe")
    metadata["cgroup_after"] = cgroup_after
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    scenarios = ("mutual-cycle", "overwritten-field", "mutual-bridge")
    statuses = {label: {scenario: next(row["exit"] for row in rows
                if row["product"] == label and row.get("scenario") == scenario
                and row["phase"] == "weak-expiration") for scenario in scenarios}
                for label in builds}
    metadata["weak_expiration_exit"] = statuses
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    summary = {"weak_expiration_exit": statuses,
               "asan_runs": {label: {scenario: next(row["exit"] for row in rows
                             if row["product"] == label and row.get("scenario") == scenario
                             and row["phase"] == "leak-sanitizer") for scenario in scenarios}
                             for label in builds},
               "lsan_reported": {label: {scenario: "LeakSanitizer: detected memory leaks" in
                                 (args.out / f"{label}-{scenario}-leak-sanitizer.log").read_text(errors="replace")
                                 for scenario in scenarios} for label in builds},
               "note": "Weak-pointer expiration is decisive; static registry roots can make LSan under-report a store ownership cycle."}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if any(status.get("mutual-cycle") == 2 or status.get("overwritten-field") == 4
           or status.get("mutual-bridge") == 6
           for status in statuses.values()):
        print("FATAL GC foreign-field lease retention:", statuses,
              "2=mutual aggregate fields; 4=overwritten field; 6=mutual extern bridges")
        return 2
    if any(code != 0 for status in statuses.values() for code in status.values()):
        print("FATAL GC foreign-field lease probe unexpected exit:", statuses)
        return 3
    print("PASS mutual foreign fields and overwritten fields release stores")
    return 0


if __name__ == "__main__":
    sys.exit(main())
