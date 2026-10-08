#!/usr/bin/env python3
"""Exact-O3-source Linux ASan/LSan qualification of opaque aggregate IDs."""

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
                                stderr=subprocess.STDOUT, timeout=300)
    return {"command": command, "exit": result.returncode,
            "log": str(log), "log_sha256": sha256(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists; source-bound evidence must be immutable")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    fixture = Path(__file__).with_name("gc_opaque_token_probe.cc")
    staged_runner = args.out / Path(__file__).name
    staged_fixture = args.out / fixture.name
    shutil.copyfile(__file__, staged_runner)
    shutil.copyfile(fixture, staged_fixture)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join(("/toolchain/lib/x86_64-unknown-linux-gnu",
        "/toolchain/lib", "/work/deps/usr/lib/x86_64-linux-gnu", env.get("LD_LIBRARY_PATH", "")))
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != expected:
        raise RuntimeError("O3 products do not match intended frozen source IDs")
    metadata = {"source_builds": builds, "expected_source_ids": expected,
                "compiler_sha256": sha256(args.clang),
                "fixture_sha256": sha256(staged_fixture), "runner_sha256": sha256(staged_runner),
                "cgroup_before": before,
                "scope": "native source-header four-thread/two-store token identity, forged and stale handle rejection; not Wasm JIT semantics or GC reclamation",
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    for label, build in builds.items():
        source = Path(build["source"])
        peer = source / "benchmark/0004.wasm3-core/gc_opaque_token_probe.cc"
        if sha256(peer) != metadata["fixture_sha256"]:
            raise RuntimeError(f"{label}: source fixture differs from runner")
        binary = args.out / f"gc_opaque_token_{label}_asan"
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
        compiled = invoke(command, args.out / f"{label}-compile.log", env, source)
        compiled.update(product=label, phase="compile")
        rows.append(compiled)
        if compiled["exit"]:
            raise RuntimeError(f"{label}: ASan opaque token probe did not compile")
        for leaks in (False, True):
            phase = "asan-lsan" if leaks else "asan-only"
            row = invoke(["taskset", "-c", "0,2,4,6", str(binary)],
                         args.out / f"{label}-{phase}.log",
                         {**env, "ASAN_OPTIONS": f"detect_leaks={int(leaks)}:halt_on_error=1"}, source)
            row.update(product=label, phase=phase,
                       binary_sha256=sha256(binary),
                       pass_marker="PASS opaque GC token" in
                           (args.out / f"{label}-{phase}.log").read_text(errors="replace"))
            rows.append(row)
            (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
            if row["exit"] or not row["pass_marker"]:
                raise RuntimeError(f"{label}: {phase} token qualification failed")
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != builds:
        raise RuntimeError("source/product changed during opaque token qualification")
    if sha256(staged_fixture) != metadata["fixture_sha256"] or sha256(staged_runner) != metadata["runner_sha256"]:
        raise RuntimeError("staged opaque token probe changed during qualification")
    final = cgroup_preflight(0)
    before_events = dict(line.split() for line in before["memory.events"].splitlines())
    after_events = dict(line.split() for line in final["memory.events"].splitlines())
    if any(before_events[name] != after_events[name] for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during token qualification")
    metadata["cgroup_after"] = final
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (args.out / "summary.json").write_text(json.dumps({"passed": True,
        "product_binary_sha256": {label: build["binary_sha256"] for label, build in builds.items()},
        "runs": [row for row in rows if row["phase"] != "compile"],
        "collector_qualified": False}, indent=2) + "\n")
    print("PASS both products opaque GC token Linux ASan/LSan semantic qualification")


if __name__ == "__main__":
    sys.exit(main())
