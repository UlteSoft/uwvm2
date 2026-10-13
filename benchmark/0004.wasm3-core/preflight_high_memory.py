#!/usr/bin/env python3
"""One-shot, source-bound sparse >4 GiB memory64 semantic/RSS preflight.

Run in an idle Linux 64 GiB cgroup before the separate high-memory timing
group. An engine that rejects the exact module or commits >2 GiB RSS is not
eligible for repeated timing; its full error remains in the output directory.
"""

import argparse
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import time

from generate import memory_footprint, module
from run import (LIB_PATHS, cgroup_preflight, command, disable_core_dumps, require_sparse_high_memory_headroom, sha256,
                 verify_product_build)


MAX_RSS_KIB = 2 * 1024**2
MAX_CGROUP_DELTA_BYTES = 4 * 1024**3
PRECHECK_ITERATIONS = 10000


def proc_memory_kib(pid):
    try:
        lines = Path(f"/proc/{pid}/status").read_text().splitlines()
    except (FileNotFoundError, ProcessLookupError):
        return {}
    result = {}
    for line in lines:
        key, _, value = line.partition(":")
        if key in ("VmPeak", "VmSize", "VmHWM", "VmRSS"):
            result[key] = int(value.split()[0])
    return result


def bounded_run(command_line, label, logs, env):
    disable_core_dumps()
    log = logs / f"{label}.log"
    cgroup = Path("/sys/fs/cgroup")
    cgroup_start = int((cgroup / "memory.current").read_text())
    cgroup_max = cgroup_start
    proc_max = {}
    with log.open("wb") as output:
        start = time.perf_counter_ns()
        process = subprocess.Popen(command_line, stdout=output, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True, close_fds=True)
        deadline = time.monotonic() + 45
        timed_out = False
        cgroup_limited = False
        rss_limited = False
        while True:
            for key, value in proc_memory_kib(process.pid).items():
                proc_max[key] = max(proc_max.get(key, 0), value)
            rss_limited = max(proc_max.get("VmRSS", 0),
                              proc_max.get("VmHWM", 0)) > MAX_RSS_KIB
            cgroup_max = max(cgroup_max, int((cgroup / "memory.current").read_text()))
            cgroup_limited = cgroup_max - cgroup_start > MAX_CGROUP_DELTA_BYTES
            pid, status, usage = os.wait4(process.pid, os.WNOHANG)
            if pid:
                break
            if rss_limited or cgroup_limited or time.monotonic() > deadline:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                _, status, usage = os.wait4(process.pid, 0)
                timed_out = not (rss_limited or cgroup_limited)
                break
            time.sleep(0.005)
    process.returncode = os.waitstatus_to_exitcode(status)
    return {"label": label, "command": command_line, "log": str(log),
            "exit": 125 if rss_limited or cgroup_limited else 124 if timed_out else process.returncode,
            "elapsed_ns": time.perf_counter_ns() - start,
            "maxrss_kib": usage.ru_maxrss, "user_s": usage.ru_utime,
            "system_s": usage.ru_stime, "timed_out": timed_out,
            "rss_limited": rss_limited,
            "cgroup_memory_limited": cgroup_limited,
            "cgroup_memory_before_bytes": cgroup_start,
            "cgroup_memory_observed_peak_bytes": cgroup_max,
            "proc_status_observed_peak_kib": proc_max}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--ordinary", required=True, type=Path)
    parser.add_argument("--ros", required=True, type=Path)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--wasmedge", type=Path)
    parser.add_argument("--wavm", type=Path)
    parser.add_argument("--wasmer", type=Path)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists; evidence must be immutable")
    before = cgroup_preflight(0)
    require_sparse_high_memory_headroom(before)
    os.sched_setaffinity(0, {16})
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    for name in ("preflight_high_memory.py", "generate.py", "run.py"):
        shutil.copyfile(Path(__file__).with_name(name), args.out / name)
    builds = {label: verify_product_build(label, binary, args.out, "before")
              for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    expected = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    if {label: build["source_id"] for label, build in builds.items()} != expected:
        raise RuntimeError("wrong frozen product source IDs")
    source, features = module("memory64-high-random-store", PRECHECK_ITERATIONS)
    wat = args.out / "memory64-high-random-store.wat"
    wasm = wat.with_suffix(".wasm")
    wat.write_text(source)
    for action in ([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)],
                   [str(args.wasm_tools), "validate", str(wasm)]):
        subprocess.run(["taskset", "-c", "16", *action], check=True, timeout=30)
    env = os.environ.copy()
    runtime_libs = [str(path.parent.parent / "lib") for path in (args.wasmedge, args.wavm)
                    if path is not None]
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, *runtime_libs)) + ":" + env.get("LD_LIBRARY_PATH", "")
    if args.wasmer:
        config = str(args.wasmer.parent.parent / "config")
        env.update(XDG_CONFIG_HOME=config, WASMER_DIR=config)
    engines = {"ordinary-jit": args.ordinary, "ordinary-int": args.ordinary,
               "ros-jit": args.ros, "ros-int": args.ros, "wasmtime": args.wasmtime}
    if args.wasmedge:
        engines.update({"wasmedge-jit": args.wasmedge, "wasmedge-int": args.wasmedge})
    if args.wavm:
        engines["wavm"] = args.wavm
    if args.wasmer:
        engines["wasmer"] = args.wasmer
    metadata = {"source_builds": builds, "expected_source_ids": expected,
                "runner_sha256": sha256(Path(__file__)),
                "engine_sha256": {name: sha256(path) for name, path in engines.items()},
                "wasm_tools_sha256": sha256(args.wasm_tools),
                "generator_sha256": sha256(Path(__file__).with_name("generate.py")),
                "wat_sha256": sha256(wat), "wasm_sha256": sha256(wasm),
                "case": "memory64-high-random-store", "iterations": PRECHECK_ITERATIONS,
                "declared_wasm_pages_64k": 65537,
                "declared_wasm_bytes": 65537 * 65536,
                "guest_footprint": memory_footprint(PRECHECK_ITERATIONS, 1 << 32),
                "max_approved_rss_kib": MAX_RSS_KIB, "cgroup_before": before,
                "max_cgroup_delta_bytes": MAX_CGROUP_DELTA_BYTES,
                "harness_affinity": sorted(os.sched_getaffinity(0)),
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    for name, binary in engines.items():
        inner = command(name, binary, wasm, features, "unwind", 0)
        row = bounded_run(inner, f"{name}-semantic", logs, env)
        row.update(engine=name, approved=row["exit"] == 0 and
                   row["maxrss_kib"] <= MAX_RSS_KIB and
                   not row["cgroup_memory_limited"])
        rows.append(row)
        (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(name, "approved" if row["approved"] else "excluded",
              row["exit"], row["maxrss_kib"] // 1024, "MiB", flush=True)
    after = {label: verify_product_build(label, binary, args.out, "after")
             for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    if after != builds:
        raise RuntimeError("product source or binary changed during preflight")
    end = cgroup_preflight(0)
    first_events = dict(line.split() for line in before["memory.events"].splitlines())
    last_events = dict(line.split() for line in end["memory.events"].splitlines())
    if any(first_events[key] != last_events[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM occurred during sparse high-memory preflight")
    metadata.update(cgroup_after=end,
                    end_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    summary = {"approved_engines": [row["engine"] for row in rows if row["approved"]],
               "excluded_engines": [{"engine": row["engine"], "exit": row["exit"],
                                     "maxrss_kib": row["maxrss_kib"]}
                                    for row in rows if not row["approved"]]}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if not all(row["approved"] for row in rows if row["engine"] in
               ("ordinary-jit", "ordinary-int", "ros-jit", "ros-int")):
        print("FATAL product sparse >4 GiB memory64 preflight")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
