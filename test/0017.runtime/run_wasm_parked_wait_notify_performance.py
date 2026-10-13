#!/usr/bin/env python3
"""Source-bound Linux parked guest wait/notify latency on two P cores.

The fixture samples the waiter's /proc task state as S around a futex wchan
read, then times a second host entry executing guest notify that returns one.
Proc qualification overhead is reported separately from the wake interval.
The rendezvous fixture and mismatch/empty CLI fast paths remain separate.
Build and measure only in the established 64 GiB swap-free Linux cgroup.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import statistics
import subprocess
import sys
import time


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def percentile(values, centile):
    ordered = sorted(values)
    index = (len(ordered) - 1) * centile / 100
    low = int(index)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (index - low)


def cgroup_state():
    root = Path("/sys/fs/cgroup")
    names = ("memory.max", "memory.swap.max", "memory.current", "memory.events",
             "cpuset.cpus.effective", "cpu.stat", "cpu.pressure")
    state = {name: (root / name).read_text().strip() for name in names}
    if (state["memory.max"] != str(64 * 1024**3) or state["memory.swap.max"] != "0" or
            state["cpuset.cpus.effective"] != "0,2,4,6,16-31"):
        raise RuntimeError("expected the established 64 GiB, swap-free, 4P+16E cgroup")
    state["p_core_frequency"] = {}
    for cpu in (0, 2):
        directory = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq")
        state["p_core_frequency"][str(cpu)] = {}
        for name in ("scaling_cur_freq", "scaling_governor"):
            try:
                state["p_core_frequency"][str(cpu)][name] = (directory / name).read_text().strip()
            except (FileNotFoundError, PermissionError):
                state["p_core_frequency"][str(cpu)][name] = None
    state["thermal_millicelsius_by_zone_and_type"] = {}
    for zone in sorted(Path("/sys/class/thermal").glob("thermal_zone*")):
        try:
            kind = (zone / "type").read_text().strip()
            state["thermal_millicelsius_by_zone_and_type"][f"{zone.name}:{kind}"] = int(
                (zone / "temp").read_text().strip())
        except (FileNotFoundError, PermissionError, ValueError):
            continue
    return state


def counters(state, key):
    return dict(line.split() for line in state[key].splitlines())


def check_clean(before, after):
    first, last = counters(before, "memory.events"), counters(after, "memory.events")
    if any(first[key] != last[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during parked wait/notify qualification")
    first, last = counters(before, "cpu.stat"), counters(after, "cpu.stat")
    if any(first.get(key) != last.get(key) for key in ("nr_throttled", "throttled_usec")):
        raise RuntimeError("cgroup CPU throttling changed during parked wait/notify timing")


def verify_source(base, runtime, expected, out, stage):
    build = json.loads((runtime / "build.json").read_text())
    fingerprint = base / "tools/ci/wasm3_source_fingerprint.py"
    observed = subprocess.check_output([sys.executable, str(fingerprint), str(base),
                                        str(out / f"source-{stage}.json")], text=True).strip()
    binary = Path(build["cli_command"][-1])
    if (observed != expected or build["source_id"] != expected or
            Path(build["source"]).resolve() != base or
            build["runtime_object_sha256"] != sha(runtime / "runtime.o") or
            build["binary_sha256"] != sha(binary)):
        raise RuntimeError("wait/notify fixture and O3 product do not match intended source")
    return {"source_id": observed, "product_binary_sha256": sha(binary),
            "runtime_object_sha256": sha(runtime / "runtime.o"),
            "build_json_sha256": sha(runtime / "build.json")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-source-root", type=Path, required=True)
    parser.add_argument("--runtime-build", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True,
                        help="exact wasm-tools executable used to validate the dumped guest binary")
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--run-only", action="store_true")
    parser.add_argument("--policy", action="append", choices=("instruction", "unwind"))
    parser.add_argument("--pairs", type=int, default=9,
                        help="process-level AB/BA pairs; use one only for a semantic smoke")
    args = parser.parse_args()
    if args.build_only and args.run_only:
        parser.error("--build-only and --run-only conflict")
    if args.pairs < 1:
        parser.error("--pairs must be positive")
    root = Path(__file__).resolve().parents[2]
    base, runtime = args.base_source_root.resolve(strict=True), args.runtime_build.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    if root != base:
        raise RuntimeError("the fixture and frozen runtime must come from the same checkout")
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if args.run_only:
        out = args.out.resolve(strict=True)
        if (out / "summary.json").exists():
            raise RuntimeError("immutable wait/notify output already contains a timed result")
    else:
        args.out.mkdir(parents=True, exist_ok=False)
        out = args.out.resolve()
    product = verify_source(base, runtime, args.expected_source_id, out, "before")
    source = root / "test/0017.runtime/wasm_parked_wait_notify_performance.cc"
    inputs = (source, Path(__file__).resolve(), runtime / "runtime.o",
              runtime / "runtime.command", runtime / "test.command",
              runtime / "build.json", base / "tools/ci/wasm3_source_fingerprint.py",
              base / "test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h",
              wasm_tools)
    before = {str(path): sha(path) for path in inputs}
    binary = out / "fixture"
    wasm = out / "fixture.wasm"
    if args.run_only:
        if json.loads((out / "input-sha256.json").read_text()) != before:
            raise RuntimeError("the frozen harness/build inputs changed after compilation")
        built = json.loads((out / "build-binaries.json").read_text())
        if sha(binary) != built["fixture"] or sha(wasm) != built["wasm"]:
            raise RuntimeError("the wait/notify executable or guest Wasm changed after compilation")
    else:
        (out / "input-sha256.json").write_text(json.dumps(before, indent=2) + "\n")
        command = shlex.split((runtime / "test.command").read_text())
        configured = [flag for flag in command if flag in
                      {"-O0", "-O1", "-O2", "-O3", "-Os", "-Oz", "-Og", "-Ofast"}]
        runtime_command = shlex.split((runtime / "runtime.command").read_text())
        runtime_configured = [flag for flag in runtime_command if flag in
                              {"-O0", "-O1", "-O2", "-O3", "-Os", "-Oz", "-Og", "-Ofast"}]
        if (not configured or configured[-1] != "-O3" or
                not runtime_configured or runtime_configured[-1] != "-O3" or
                "-DUWVM2TEST_RUNNER_USE_LLVM_JIT" not in command):
            raise RuntimeError("real guest wake latency requires the O3 LLVM-full test/runtime template")
        fixtures = [index for index, item in enumerate(command) if item.endswith(".cc") and
                    (item.startswith("test/") or Path(item).is_relative_to(base / "test"))]
        if len(fixtures) != 1 or "-o" not in command:
            raise RuntimeError("unexpected frozen test.command shape")
        command[fixtures[0]] = str(source)
        command[command.index("-o") + 1] = str(binary)
        command.append("-I" + str(base / "test/0013.uwvm_int/strict"))
        (out / "build.command").write_text(shlex.join(["taskset", "-c", "16", *command]) + "\n")
        with (out / "build.log").open("wb") as stream:
            result = subprocess.run(["taskset", "-c", "16", *command], cwd=base,
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        if result.returncode:
            raise RuntimeError(f"wait/notify fixture build failed: {result.returncode}")
        with (out / "dump-wasm.log").open("wb") as stream:
            subprocess.run(["taskset", "-c", "16", str(binary), "--dump-wasm", str(wasm)],
                           stdout=stream, stderr=subprocess.STDOUT, timeout=30, check=True)
        with (out / "validate-wasm.log").open("wb") as stream:
            subprocess.run([str(wasm_tools), "validate", str(wasm)], stdout=stream,
                           stderr=subprocess.STDOUT, timeout=30, check=True)
        (out / "build-binaries.json").write_text(json.dumps({"fixture": sha(binary),
                                                                "wasm": sha(wasm)}, indent=2) + "\n")
    if args.build_only:
        print("PASS source-bound O3 Linux parked wait/notify built; timing deferred", flush=True)
        return

    policies = args.policy or ["instruction", "unwind"]
    if len(policies) != len(set(policies)):
        parser.error("each --policy may be selected only once")
    state_before = cgroup_state()
    raw = []
    processes = []
    for pair in range(args.pairs):
        telemetry_before = cgroup_state()
        order = policies if pair % 2 == 0 else list(reversed(policies))
        for policy in order:
            command = ["taskset", "-c", "0,2", str(binary), policy]
            label = f"pair-{pair:02d}-{policy}"
            (out / f"{label}.command").write_text(shlex.join(command) + "\n")
            begin = time.perf_counter_ns()
            with (out / f"{label}.log").open("wb") as stream:
                process = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
            if process.returncode:
                raise RuntimeError(f"{label}: guest wait/notify returned {process.returncode}")
            lines = (out / f"{label}.log").read_text().splitlines()
            if not any("PASS Linux parked guest wait32 -> guest notify" in line for line in lines):
                raise RuntimeError(f"{label}: completion marker missing")
            rows = [json.loads(line) for line in lines if line.startswith("{")]
            if (len(rows) != 9 * 128 or
                    {(row["sample"], row["round"]) for row in rows} !=
                    {(sample, round_) for sample in range(9) for round_ in range(128)} or
                    any(row["policy"] != policy or row["wake_ns"] < row["notify_call_ns"] or
                        row.get("parked_task_state") != "S" or
                        "futex" not in row.get("parked_wchan", "") or
                        row.get("notify_return") != 1 or row.get("waiter_tid", 0) <= 0 or
                        row.get("qualification_ns", 0) <= 0 or
                        row.get("qualification_probes", 0) <= 0
                        for row in rows)):
                raise RuntimeError(f"{label}: malformed or incomplete wakeup samples")
            for row in rows:
                row["pair"] = pair
                raw.append(row)
            processes.append({"pair": pair, "policy": policy, "wakes": len(rows),
                              "elapsed_ns": time.perf_counter_ns() - begin,
                              "wake_p50_ns": percentile([row["wake_ns"] for row in rows], 50),
                              "wake_p95_ns": percentile([row["wake_ns"] for row in rows], 95),
                              "wake_p99_ns": percentile([row["wake_ns"] for row in rows], 99),
                              "log_sha256": sha(out / f"{label}.log")})
            print(label, "wakes", len(rows), flush=True)
        with (out / "pair-telemetry.jsonl").open("a") as stream:
            stream.write(json.dumps({"pair": pair, "policy_order": order,
                                     "before": telemetry_before,
                                     "after": cgroup_state()}) + "\n")
    with (out / "raw.jsonl").open("w") as stream:
        for row in raw:
            stream.write(json.dumps(row) + "\n")
    summaries = []
    for policy in policies:
        rows = [row for row in raw if row["policy"] == policy]
        wake = [row["wake_ns"] for row in rows]
        notify = [row["notify_call_ns"] for row in rows]
        summaries.append({"policy": policy, "wakes": len(rows),
                          "wake_p50_ns": percentile(wake, 50),
                          "wake_p95_ns": percentile(wake, 95),
                          "wake_p99_ns": percentile(wake, 99),
                          "notify_call_p50_ns": percentile(notify, 50),
                          "notify_call_p95_ns": percentile(notify, 95),
                          "qualification_p50_ns": percentile(
                              [row["qualification_ns"] for row in rows], 50),
                          "qualification_p95_ns": percentile(
                              [row["qualification_ns"] for row in rows], 95),
                          "qualification_probe_median": statistics.median(
                              row["qualification_probes"] for row in rows),
                          "empty_notify_retries_total": sum(
                              row["empty_notify_retries"] for row in rows),
                          "observed_futex_wchans": sorted({row["parked_wchan"] for row in rows}),
                          "parked_qualified_wakes": len(rows)})
        print(policy, "guest wake p50/p95/p99 ns",
              summaries[-1]["wake_p50_ns"], summaries[-1]["wake_p95_ns"],
              summaries[-1]["wake_p99_ns"], flush=True)
    state_after = cgroup_state()
    check_clean(state_before, state_after)
    if before != {str(path): sha(path) for path in inputs}:
        raise RuntimeError("benchmark inputs changed during guest wake timing")
    after_product = verify_source(base, runtime, args.expected_source_id, out, "after")
    if after_product != product:
        raise RuntimeError("product source or binary changed during guest wake timing")
    report = {"source": product, "expected_source_id": args.expected_source_id,
              "input_sha256": before, "binary_sha256": sha(binary),
              "guest_wasm_sha256": sha(wasm), "wasm_tools_sha256": sha(wasm_tools),
              "runner_sha256": sha(Path(__file__)), "affinity": "0,2 P cores",
              "scope": "Linux two-host-thread guest wait32/notify full JIT; task state S sampled around futex wchan before successful notify=1; includes VM host-entry, wake and host completion observation, excludes proc qualification and thread creation from per-wake interval",
              "qualification_source": "https://docs.kernel.org/filesystems/proc.html",
              "qualification_limit": "proc state/wchan are sequential samples rather than an atomic kernel snapshot; unavailable/zero/non-futex wchan fails qualification",
              "warmup_samples_per_process": 2, "timed_samples_per_process": 9,
              "rounds_per_sample": 128, "ab_ba_pairs": args.pairs,
              "formally_paired": len(policies) == 2 and args.pairs >= 9,
              "cgroup_before": state_before, "cgroup_after": state_after,
              "policies": summaries, "processes": processes,
              "raw_jsonl_sha256": sha(out / "raw.jsonl"),
              "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    print("PASS source-bound Linux parked guest wait/notify timing", flush=True)


if __name__ == "__main__":
    main()
