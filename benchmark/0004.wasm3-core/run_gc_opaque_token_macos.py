#!/usr/bin/env python3
"""Source-bound Darwin ASan preflight for concurrent opaque GC identities."""

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from run_gc_foreign_cycle_macos import LIMIT, invoke, sha256, source_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary-root", type=Path, required=True)
    parser.add_argument("--ros-root", type=Path, required=True)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != "Darwin" or args.out.exists():
        parser.error("Darwin and a fresh output directory are required")
    roots = {"ordinary": args.ordinary_root.resolve(strict=True),
             "ros": args.ros_root.resolve(strict=True)}
    intended = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    fixture = Path(__file__).with_name("gc_opaque_token_probe.cc")
    helper = Path(__file__).with_name("run_gc_foreign_cycle_macos.py")
    compiler = Path(subprocess.check_output(["xcrun", "--find", "clang++"],
                                            text=True).strip())
    args.out.mkdir(parents=True)
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    shutil.copyfile(fixture, args.out / fixture.name)
    shutil.copyfile(helper, args.out / helper.name)
    result = {"start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "max_allowed_rss_bytes": LIMIT, "compiler": str(compiler),
              "compiler_sha256": sha256(compiler), "fixture_sha256": sha256(fixture),
              "runner_sha256": sha256(Path(__file__)),
              "helper_sha256": sha256(helper),
              "asan_options": "detect_leaks=0:halt_on_error=1",
              "expected_source_ids": intended,
              "scope": "four-thread/two-store token uniqueness, forged 1/max rejection, store unload, stale token rejection; source-header semantics only, not product JIT or collector performance",
              "products": {}}
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    for label, root in roots.items():
        peer_fixture = root / "benchmark/0004.wasm3-core/gc_opaque_token_probe.cc"
        if sha256(peer_fixture) != result["fixture_sha256"]:
            raise RuntimeError(f"{label}: fixture differs from mirrored peer")
        header = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
        before = source_id(root, args.out / f"{label}-source-before.json")
        if before != intended[label]:
            raise RuntimeError(f"{label}: wrong frozen source ID")
        header_before = sha256(header)
        binary = args.out / f"{label}-asan"
        build_command = ["xcrun", "clang++", "-std=c++2c", "-stdlib=libc++",
                         "-fno-rtti", "-O1", "-g1", "-Werror",
                         "-Wno-undefined-inline", "-fsanitize=address",
                         "-fno-omit-frame-pointer", "-pthread",
                         "-I" + str(root / "src"),
                         "-I" + str(root / "third-parties/fast_io/include"),
                         "-I" + str(root / "third-parties/bizwen/include"),
                         "-I" + str(root / "third-parties/boost_unordered/include"),
                         str(peer_fixture), "-o", str(binary)]
        build = invoke(build_command, args.out / f"{label}-compile.log",
                       os.environ.copy(), root)
        if build["exit"]:
            raise RuntimeError(f"{label}: token probe failed to compile")
        run = invoke([str(binary)], args.out / f"{label}-run.log",
                     {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1"}, root)
        after = source_id(root, args.out / f"{label}-source-after.json")
        result["products"][label] = {
            "source_id_before": before, "source_id_after": after,
            "header_sha256_before": header_before,
            "header_sha256_after": sha256(header),
            "binary_sha256": sha256(binary), "build": build, "run": run}
        (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
        if before != after or header_before != result["products"][label]["header_sha256_after"]:
            raise RuntimeError(f"{label}: source changed during token probe")
        if run["exit"] or "PASS opaque GC token" not in (args.out / f"{label}-run.log").read_text():
            raise RuntimeError(f"{label}: token semantic probe failed")
    result["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS both products opaque GC token source-header ASan preflight")


if __name__ == "__main__":
    sys.exit(main())
