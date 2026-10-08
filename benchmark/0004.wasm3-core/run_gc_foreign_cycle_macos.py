#!/usr/bin/env python3
"""Low-RSS Darwin ASan preflight for foreign GC arena ownership cycles.

This is supplemental Mac evidence. The release qualification remains the
exact-product-source Linux cgroup ASan/LSan runner in run_gc_foreign_cycle.py.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import shutil
import subprocess
import sys
import time


LIMIT = 4 * 1024**3
SCENARIOS = ("mutual-cycle", "overwritten-field", "mutual-bridge")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def invoke(command, log, env, cwd):
    with log.open("wb") as output:
        process = subprocess.Popen(["/usr/bin/time", "-l", *command], cwd=cwd, env=env,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        deadline = time.monotonic() + 180
        while process.poll() is None:
            if time.monotonic() > deadline:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise TimeoutError(f"Mac preflight timed out: {log}")
            processes = subprocess.check_output(["ps", "-axo", "pgid=,rss="], text=True)
            group_kib = sum(int(columns[1]) for line in processes.splitlines()
                            if len(columns := line.split()) == 2 and
                            int(columns[0]) == process.pid)
            # Kill well below the user's 4 GiB cap; the post-run peak RSS is
            # also checked because the process may finish between polls.
            if group_kib * 1024 > 3 * 1024**3:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise MemoryError(f"Mac preflight exceeded 3 GiB watchdog: {log}")
            time.sleep(0.05)
        exit_code = process.wait()
    raw = log.read_text(errors="replace")
    match = re.search(r"(\d+)\s+maximum resident set size", raw)
    if not match:
        raise RuntimeError(f"maximum RSS absent from {log}")
    rss = int(match.group(1))
    if rss > LIMIT:
        raise RuntimeError(f"4 GiB Mac test limit exceeded: {log}: {rss} bytes")
    return {"command": command, "log": str(log), "exit": exit_code,
            "maxrss_bytes": rss}


def source_id(root, path):
    return subprocess.check_output([sys.executable,
        str(root / "tools/ci/wasm3_source_fingerprint.py"), str(root), str(path)],
        text=True).strip()


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
    args.out.mkdir(parents=True)
    roots = {"ordinary": args.ordinary_root.resolve(strict=True),
             "ros": args.ros_root.resolve(strict=True)}
    expected_source_ids = {"ordinary": args.ordinary_source_id,
                           "ros": args.ros_source_id}
    fixture = Path(__file__).with_name("gc_foreign_field_cycle.cc")
    compiler = Path(subprocess.check_output(["xcrun", "--find", "clang++"],
                 text=True).strip())
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    shutil.copyfile(fixture, args.out / fixture.name)
    result = {"start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "max_allowed_rss_bytes": LIMIT, "compiler": str(compiler),
              "compiler_sha256": sha256(compiler), "fixture_sha256": sha256(fixture),
              "asan_options": "detect_leaks=0:halt_on_error=1",
              "expected_source_ids": expected_source_ids,
              "note": "Weak-pointer expiration is decisive; macOS ASan leak detection is not the Linux LSan release qualification.",
              "products": {}}
    for label, root in roots.items():
        peer_fixture = root / "benchmark/0004.wasm3-core/gc_foreign_field_cycle.cc"
        if sha256(peer_fixture) != result["fixture_sha256"]:
            raise RuntimeError(f"{label}: fixture differs from mirrored peer")
        header = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
        before = source_id(root, args.out / f"{label}-source-before.json")
        if before != expected_source_ids[label]:
            raise RuntimeError(f"{label}: wrong frozen source ID for Mac preflight")
        header_before = sha256(header)
        binary = args.out / f"{label}-asan"
        command = ["xcrun", "clang++", "-std=c++23", "-stdlib=libc++", "-fno-rtti",
                   "-O1", "-g1", "-Werror", "-Wno-undefined-inline",
                   "-fsanitize=address", "-fno-omit-frame-pointer",
                   "-I" + str(root / "src"),
                   "-I" + str(root / "third-parties/fast_io/include"),
                   "-I" + str(root / "third-parties/bizwen/include"),
                   "-I" + str(root / "third-parties/boost_unordered/include"),
                   str(peer_fixture), "-o", str(binary)]
        build = invoke(command, args.out / f"{label}-compile.log", os.environ.copy(), root)
        if build["exit"]:
            raise RuntimeError(f"{label}: ASan fixture failed to compile")
        runs = {}
        for scenario in SCENARIOS:
            runs[scenario] = invoke([str(binary), scenario],
                args.out / f"{label}-{scenario}.log",
                {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1"}, root)
        after = source_id(root, args.out / f"{label}-source-after.json")
        row = {"source_id_before": before, "source_id_after": after,
               "header_sha256_before": header_before,
               "header_sha256_after": sha256(header),
               "binary_sha256": sha256(binary), "build": build, "runs": runs}
        result["products"][label] = row
        (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
        if before != after or header_before != row["header_sha256_after"]:
            raise RuntimeError(f"{label}: source changed during Mac preflight")
    result["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    exits = {label: {name: row["exit"] for name, row in value["runs"].items()}
             for label, value in result["products"].items()}
    print(json.dumps(exits, sort_keys=True))
    if any(status.get("mutual-cycle") == 2 or status.get("overwritten-field") == 4
           or status.get("mutual-bridge") == 6 for status in exits.values()):
        print("FATAL GC foreign arena ownership retention")
        return 2
    if any(code != 0 for status in exits.values() for code in status.values()):
        print("FATAL unexpected foreign lease probe exit")
        return 3
    print("PASS foreign arena lease lifetimes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
