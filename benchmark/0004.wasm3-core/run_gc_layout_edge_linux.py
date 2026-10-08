#!/usr/bin/env python3
"""Source-bound Linux ASan/UBSan/LSan edge probe for a GC header overlay.

Compile the same zero-length/overflow/alignment/failure/lease fixture against
ordinary and ROS baseline headers and one isolated candidate header. This is
correctness evidence for a layout experiment, not collector qualification.
"""

import argparse
import json
import os
from pathlib import Path
import resource
import shutil
import subprocess
import time

from run import cgroup_preflight, sha256, verify_product_build


HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")
BASELINE_HEADER_SHA256 = "21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d"
CANDIDATE_HEADER_SHA256 = "7eca359a5d989c814967df49e9030e5e1402adb97a0154a82b2591a52ad9a602"


def invoke(command, log, environment, cwd=None):
    with log.open("wb") as output:
        result = subprocess.run(command, cwd=cwd, env=environment, stdout=output,
                                stderr=subprocess.STDOUT, timeout=300)
    return {"command": command, "exit": result.returncode,
            "log": str(log), "log_sha256": sha256(log)}


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--overlay-root", required=True, type=Path)
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    args = parser.parse_args()
    if args.out.exists():
        parser.error("fresh output directory required")
    files = sorted(path.relative_to(args.overlay_root)
                   for path in args.overlay_root.rglob("*") if path.is_file())
    if files != [HEADER]:
        parser.error("overlay must contain exactly uwvm2/uwvm/runtime/storage/gc_object.h")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    fixture = Path(__file__).with_name("gc_layout_edge_cases.cc")
    staged_fixture = args.out / fixture.name
    staged_runner = args.out / Path(__file__).name
    shutil.copyfile(fixture, staged_fixture)
    shutil.copyfile(__file__, staged_runner)
    overlay_header = args.overlay_root / HEADER
    sources = {"ordinary": args.ordinary, "ros": args.ros}
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in sources.items()}
    intended = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != intended:
        raise RuntimeError("GC edge probe product source IDs differ from frozen builds")
    header_hashes = {label: sha256(Path(build["source"]) / "src" / HEADER)
                     for label, build in builds.items()}
    if (set(header_hashes.values()) != {BASELINE_HEADER_SHA256} or
            sha256(overlay_header) != CANDIDATE_HEADER_SHA256):
        raise RuntimeError("GC edge overlay needs rebase against these exact product headers")
    environment = os.environ.copy()
    environment["LD_LIBRARY_PATH"] = ":".join((
        "/toolchain/lib/x86_64-unknown-linux-gnu", "/toolchain/lib",
        "/work/deps/usr/lib/x86_64-linux-gnu", environment.get("LD_LIBRARY_PATH", "")))
    environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1"
    meta = {"scope": "native GC layout edge correctness; no collector claim",
            "source_builds": builds, "expected_source_ids": intended,
            "baseline_header_sha256": header_hashes,
            "overlay_header_sha256": sha256(overlay_header),
            "fixture_sha256": sha256(staged_fixture),
            "runner_sha256": sha256(staged_runner),
            "compiler_sha256": sha256(args.clang),
            "sanitizers": ["address", "undefined", "leak"],
            "oom_fault_injection": {"mechanism": "ELF --wrap on scalar/array nothrow new",
                                    "minimum_failed_allocation_bytes": 4096,
                                    "baseline_expected": "array allocation fails after header",
                                    "single_block_expected": "one scalar allocation fails"},
            "asan_options": environment["ASAN_OPTIONS"],
            "ubsan_options": environment["UBSAN_OPTIONS"],
            "cgroup_before": before,
            "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    rows = []
    for label, build in builds.items():
        source = Path(build["source"])
        peer_fixture = source / "benchmark/0004.wasm3-core" / fixture.name
        if sha256(peer_fixture) != meta["fixture_sha256"]:
            raise RuntimeError(f"{label}: snapshot edge fixture differs from staged copy")
        for variant in ("baseline", "single-block"):
            binary = args.out / f"gc-layout-{label}-{variant}-asan"
            depfile = args.out / f"{label}-{variant}.d"
            includes = (["-I" + str(args.overlay_root)] if variant == "single-block" else [])
            includes += ["-I" + str(source / "src"),
                         "-I" + str(source / "third-parties/fast_io/include"),
                         "-I" + str(source / "third-parties/bizwen/include"),
                         "-I" + str(source / "third-parties/boost_unordered/include")]
            fault_flags = ["-DGC_LAYOUT_FAULT_INJECT",
                           "-Wl,--wrap=_ZnwmRKSt9nothrow_t",
                           "-Wl,--wrap=_ZnamRKSt9nothrow_t"]
            if variant == "single-block":
                fault_flags.append("-DGC_LAYOUT_EXPECT_SINGLE_BLOCK")
            command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                       "-stdlib=libc++", "-O1", "-g1", "-fno-rtti", "-Werror",
                       "-Wno-undefined-inline", "-fsanitize=address,undefined",
                       "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
                       *fault_flags,
                       *includes, "-MD", "-MF", str(depfile), str(staged_fixture),
                       "-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
                       "-L/work/deps/usr/lib/x86_64-linux-gnu", "-pthread", "-o", str(binary)]
            compiled = invoke(command, args.out / f"{label}-{variant}-build.log",
                              environment, source)
            compiled.update(product=label, variant=variant, phase="build")
            rows.append(compiled)
            if compiled["exit"]:
                raise RuntimeError(f"{label}/{variant} sanitizer fixture did not compile")
            selected = overlay_header if variant == "single-block" else source / "src" / HEADER
            if str(selected) not in depfile.read_text():
                raise RuntimeError(f"{label}/{variant} sanitizer build selected wrong header")
            row = invoke(["taskset", "-c", "16", str(binary)],
                         args.out / f"{label}-{variant}-run.log", environment, source)
            row.update(product=label, variant=variant, phase="run",
                       binary_sha256=sha256(binary),
                       pass_marker="PASS GC layout edge cases" in
                           (args.out / f"{label}-{variant}-run.log").read_text(errors="replace"))
            rows.append(row)
            (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
            if row["exit"] or not row["pass_marker"]:
                raise RuntimeError(f"{label}/{variant} edge ASan/LSan qualification failed")
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in sources.items()}
    state_after = cgroup_preflight(0)
    first_events = dict(line.split() for line in before["memory.events"].splitlines())
    last_events = dict(line.split() for line in state_after["memory.events"].splitlines())
    if (after != builds or
            any(sha256(Path(build["source"]) / "src" / HEADER) != header_hashes[label]
                for label, build in builds.items()) or
            sha256(overlay_header) != meta["overlay_header_sha256"] or
            sha256(staged_fixture) != meta["fixture_sha256"] or
            sha256(staged_runner) != meta["runner_sha256"] or
            sha256(args.clang) != meta["compiler_sha256"] or
            any(first_events[name] != last_events[name] for name in ("oom", "oom_kill"))):
        raise RuntimeError("GC layout sanitizer evidence invalidated by source/tool/OOM drift")
    meta["source_builds_after"] = after
    meta["cgroup_after"] = state_after
    meta["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    (args.out / "summary.json").write_text(json.dumps({"passed": True,
        "source_ids": intended, "overlay_header_sha256": meta["overlay_header_sha256"],
        "runs": [row for row in rows if row["phase"] == "run"],
        "collector_qualified": False}, indent=2) + "\n")
    print("PASS ordinary/ROS baseline and isolated layout candidate ASan/UBSan/LSan")


if __name__ == "__main__":
    main()
