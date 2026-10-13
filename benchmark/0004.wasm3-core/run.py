#!/usr/bin/env python3
"""P-core, cgroup-contained paired Core 3 CLI benchmark with raw evidence.

This is a whole-process benchmark. Each engine executes the same validated
Wasm at both iteration counts; the high-minus-low slope estimates guest work
without incorrectly calling JIT startup a guest-instruction cost.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import statistics
import subprocess
import sys
import time

from generate import CASES, call_ref_trace, gc_cast_trace, module, page_memory_trace, swizzle_trace, table_index_trace


LIB_PATHS = ("/toolchain/lib/x86_64-unknown-linux-gnu", "/toolchain/lib",
             "/work/deps/usr/lib/x86_64-linux-gnu",
             "/work/wasm3-resume-20260924/tools/host-libs")
NODE_LIBATOMIC_PATH = "/work/deps/usr/lib/x86_64-linux-gnu"
HIGH_MEMORY_HEADROOM_BYTES = 8 * 1024**3
WASMER_FLAGS = {"memory64": "--enable-memory64", "table64": "--enable-memory64",
                "threads": "--enable-threads", "multi-memory": "--enable-multi-memory",
                "tail-call": "--enable-tail-call", "exceptions": "--enable-exceptions",
                "function-references": "--enable-reference-types", "simd": "--enable-simd",
                "relaxed-simd": "--enable-relaxed-simd",
                "extended-const": "--enable-extended-const"}
# `all-proposals=n` turns off Wasm 2.0 proposals as well. Restore the
# baseline that both product VMs already support, then opt into only the
# Core 3 proposals required by this particular module. In Wasmtime's CLI,
# 64-bit tables are gated by memory64 rather than a separate table64 switch.
WASMTIME_BASE_FLAGS = ("all-proposals=n", "bulk-memory=y", "multi-value=y",
                       "reference-types=y", "simd=y")
WASMTIME_FLAGS = {"gc": ("function-references=y", "gc=y"),
                  "memory64": ("memory64=y",),
                  "table64": ("memory64=y",),
                  "threads": ("threads=y", "shared-memory=y"),
                  "multi-memory": ("multi-memory=y",),
                  "tail-call": ("tail-call=y",),
                  "exceptions": ("exceptions=y",),
                  "function-references": ("function-references=y",),
                  "relaxed-simd": ("relaxed-simd=y",),
                  "extended-const": ("extended-const=y",),
                  "simd": ()}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def node_runtime_environment(base):
    """Expose the container's libatomic to Node only, preserving peer VMs' loader setup."""
    environment = base.copy()
    previous = environment.get("LD_LIBRARY_PATH", "")
    environment["LD_LIBRARY_PATH"] = NODE_LIBATOMIC_PATH + (":" + previous if previous else "")
    return environment


def disable_core_dumps():
    # A comparator may reject or crash on a proposal. Keep that evidence in
    # the text log without writing an unbounded core into the shared cgroup.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def cpus_from_list(spec):
    allowed = set()
    for part in spec.split(","):
        if "-" in part:
            low, high = map(int, part.split("-", 1))
            allowed.update(range(low, high + 1))
        else:
            allowed.add(int(part))
    return allowed


def engine_order_for_pair(names, pair):
    if not names:
        raise ValueError("engine order requires at least one approved engine")
    # One forward/reverse pair shares the same rotation; otherwise even
    # engine counts would repeatedly put only half the engines first.
    start = (pair // 2) % len(names)
    order = list(names[start:]) + list(names[:start])
    if pair % 2:
        order.reverse()
    return order


def cpu_telemetry(cpu):
    frequency = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq")
    sensors = {}
    for zone in sorted(Path("/sys/class/thermal").glob("thermal_zone*")):
        try:
            sensors[f"{zone.name}:{(zone / 'type').read_text().strip()}"] = int(
                (zone / "temp").read_text())
        except (FileNotFoundError, PermissionError, ValueError):
            continue
    result = {"timestamp_ns": time.time_ns(), "cpu": cpu,
              "cgroup_cpu_stat": Path("/sys/fs/cgroup/cpu.stat").read_text().strip(),
              "thermal_millicelsius_by_zone_and_type": sensors}
    for name in ("scaling_cur_freq", "cpuinfo_cur_freq", "scaling_governor",
                 "scaling_min_freq", "scaling_max_freq"):
        try:
            result[name] = (frequency / name).read_text().strip()
        except (FileNotFoundError, PermissionError):
            result[name] = None
    return result


def cgroup_preflight(cpu):
    root = Path("/sys/fs/cgroup")
    state = {name: (root / name).read_text().strip() for name in
             ("memory.max", "memory.swap.max", "memory.current", "memory.peak",
              "memory.events", "cpuset.cpus.effective", "cpu.stat", "cpu.pressure")}
    if state["memory.max"] != str(64 * 1024**3) or state["memory.swap.max"] != "0":
        raise RuntimeError("benchmark requires the established 64 GiB swap-free cgroup")
    actual_cpus = cpus_from_list(state["cpuset.cpus.effective"])
    required_p = {0, 2, 4, 6}
    required_e = set(range(16, 32))
    if actual_cpus != required_p | required_e:
        raise RuntimeError(f"unexpected 4P+16E cpuset: {state['cpuset.cpus.effective']}")
    if cpu not in required_p:
        raise RuntimeError(f"CPU {cpu} is not an allowed P core in the cgroup")
    state["cpu_topology"] = {}
    for logical_cpu in sorted(actual_cpus):
        topology = Path(f"/sys/devices/system/cpu/cpu{logical_cpu}/topology")
        entry = {}
        for key in ("core_type", "core_id", "physical_package_id", "thread_siblings_list"):
            try:
                entry[key] = (topology / key).read_text().strip()
            except (FileNotFoundError, PermissionError):
                entry[key] = None
        try:
            entry["cpuinfo_max_freq_khz"] = int((Path(f"/sys/devices/system/cpu/cpu{logical_cpu}") /
                                                  "cpufreq/cpuinfo_max_freq").read_text().strip())
        except (FileNotFoundError, PermissionError, ValueError):
            entry["cpuinfo_max_freq_khz"] = None
        state["cpu_topology"][str(logical_cpu)] = entry
    observed_types = {cpu_id: entry["core_type"]
                      for cpu_id, entry in state["cpu_topology"].items()
                      if entry["core_type"] is not None}
    if observed_types and (any(observed_types.get(str(p)) != "2" for p in required_p)
                           or any(observed_types.get(str(e)) != "1" for e in required_e)):
        raise RuntimeError(f"hybrid CPU topology differs from established 4P+16E map: {observed_types}")
    p_max = [state["cpu_topology"][str(p)]["cpuinfo_max_freq_khz"] for p in required_p]
    e_max = [state["cpu_topology"][str(e)]["cpuinfo_max_freq_khz"] for e in required_e]
    if all(value is not None for value in (*p_max, *e_max)) and min(p_max) <= max(e_max):
        raise RuntimeError(f"P-core max frequencies do not exceed E-core maxima: {p_max}, {e_max}")
    state["cpu_telemetry"] = cpu_telemetry(cpu)
    return state


def require_sparse_high_memory_headroom(state):
    # The guest declaration reserves >4 GiB of address space. Keep enough
    # physical-cgroup slack for an engine that unexpectedly commits pages,
    # even though the separate watchdog also limits observed RSS growth.
    available = int(state["memory.max"]) - int(state["memory.current"])
    if available < HIGH_MEMORY_HEADROOM_BYTES:
        raise RuntimeError(f"sparse memory64 preflight needs >=8 GiB cgroup headroom; "
                           f"current headroom is {available / 1024**3:.2f} GiB")


def verify_product_build(label, binary, out, stage):
    """Bind a measurement to the exact O3 product source and binary."""
    manifest = binary.parent / "build.json"
    build = json.loads(manifest.read_text())
    source = Path(build["source"]).resolve(strict=True)
    if (build["binary_sha256"] != sha256(binary) or
            Path(build["cli_command"][-1]).resolve(strict=True) != binary.resolve(strict=True) or
            "-O3" not in build["runtime_command"] or "-O3" not in build["cli_command"]):
        raise RuntimeError(f"{label}: binary or O3 build manifest mismatch")
    fingerprint = source / "tools/ci/wasm3_source_fingerprint.py"
    snapshot = out / f"{label}-source-{stage}.json"
    source_id = subprocess.check_output([sys.executable, str(fingerprint), str(source),
                                         str(snapshot)], text=True).strip()
    if source_id != build["source_id"]:
        raise RuntimeError(f"{label}: live source differs from O3 build")
    return {"build_json_path": str(manifest), "build_json_sha256": sha256(manifest),
            "source": str(source), "source_id": source_id,
            "binary_sha256": sha256(binary)}


def run_one(command, label, logs, env, allow_failure=False, timeout_seconds=180):
    disable_core_dumps()
    path = logs / (label + ".log")
    with path.open("wb") as output:
        start = time.perf_counter_ns()
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                   env=env, close_fds=True, start_new_session=True)
        timed_out = False

        def deadline_expired(_number, _frame):
            raise TimeoutError(label)

        previous_handler = signal.signal(signal.SIGALRM, deadline_expired)
        signal.setitimer(signal.ITIMER_REAL, timeout_seconds)
        try:
            try:
                _, raw_status, usage = os.wait4(process.pid, 0)
            except TimeoutError:
                timed_out = True
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                _, raw_status, usage = os.wait4(process.pid, 0)
        finally:
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, previous_handler)
        elapsed = time.perf_counter_ns() - start
        process.returncode = 124 if timed_out else os.waitstatus_to_exitcode(raw_status)
    row = {"label": label, "command": command, "elapsed_ns": elapsed,
           "exit": process.returncode, "timed_out": timed_out,
           "timeout_seconds": timeout_seconds, "maxrss_kib": usage.ru_maxrss,
           "user_s": usage.ru_utime, "system_s": usage.ru_stime}
    if process.returncode and not allow_failure:
        raise RuntimeError(f"{label}: exit {process.returncode}; {path.read_text(errors='replace')[-4000:]}")
    return row


def command(engine, vm, wasm, features, trace, cpu):
    pinned = ["taskset", "-c", str(cpu)]
    if engine == "wasmtime" or engine.startswith("wasmtime-"):
        collector = engine.removeprefix("wasmtime-") if engine != "wasmtime" else None
        codegen = "cache=n" + (",collector=" + collector if collector else "")
        wasm_flags = dict.fromkeys((*WASMTIME_BASE_FLAGS,
                                    *(flag for feature in features
                                      for flag in WASMTIME_FLAGS[feature])))
        return pinned + [str(vm), "-C", codegen, "-W",
                         ",".join(wasm_flags), str(wasm)]
    if engine in ("wasmedge-jit", "wasmedge-int"):
        mode = "jit" if engine == "wasmedge-jit" else "interpreter"
        return pinned + [str(vm), "run", "--wasm-3", "--run-mode", mode,
                         "--enable-gc", "--enable-threads", str(wasm)]
    if engine == "wavm":
        return pinned + [str(vm), "run", "--enable", "memory64", "--enable", "atomics",
                         "--enable", "table64", "--enable", "multi-memory",
                         "--enable", "exception-handling", "--enable", "simd",
                         "--nocache", "--function=_start", str(wasm)]
    if engine == "wasmer":
        flags = list(dict.fromkeys(WASMER_FLAGS[feature] for feature in features
                                   if feature in WASMER_FLAGS))
        return pinned + [str(vm), "run", *flags, "--invoke", "_start", str(wasm)]
    feature_args = ["-WFE-" + feature for feature in features]
    if engine == "ordinary-jit":
        mode = ["-Rcc", "jit", "-Rcm", "full", "-Rllvm-full-policy", "pb-o3",
                "-Rllvm-call-stack", trace, "-Rllvm-cache-path", "disable"]
    elif engine == "ordinary-int":
        mode = ["-Rcc", "int", "-Rcm", "full"]
    elif engine == "ros-jit":
        mode = ["-Raot", "-Rllvm-full-policy", "pb-o3",
                "-Rllvm-call-stack", trace, "-Rllvm-cache-path", "disable"]
    elif engine == "ros-int":
        mode = ["-Rint"]
    else:
        raise ValueError(engine)
    return pinned + [str(vm), *mode, "-Rct", "0", *feature_args, "--run", str(wasm)]


def main():
    disable_core_dumps()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--ordinary", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--wasmedge", type=Path)
    parser.add_argument("--wavm", type=Path)
    parser.add_argument("--wasmer", type=Path)
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--low", type=int, default=250000)
    parser.add_argument("--high", type=int, default=1000000)
    parser.add_argument("--pairs", type=int, default=9)
    parser.add_argument("--trace", choices=("instruction", "unwind", "none"), default="unwind")
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--allow-high-memory", action="store_true",
                        help="explicitly permit the sparse >4 GiB case after bounded preflight")
    parser.add_argument("--high-memory-preflight", type=Path,
                        help="summary.json from the bounded sparse high-address preflight")
    parser.add_argument("--engine", action="append",
                        help="run only the named engine/mode; repeat for a selected subset")
    parser.add_argument("--include-interpreters", action="store_true")
    parser.add_argument("--include-gc-collector-matrix", action="store_true")
    parser.add_argument("--require-exact-source", action="store_true",
                        help="recompute both product source fingerprints before and after timing")
    args = parser.parse_args()
    if not (0 < args.low < args.high <= 0xffffffff) or args.pairs < 9:
        parser.error("require positive low < high <= u32 max and at least nine pairs")
    if args.out.exists():
        parser.error("output directory already exists; measurements must be immutable")
    selected_cases = args.case or [case for case in CASES
                                   if case != "memory64-high-random-store"]
    high_case = "memory64-high-random-store" in selected_cases
    if high_case and selected_cases != ["memory64-high-random-store"]:
        parser.error("sparse >4 GiB memory64 must run alone under its bounded watchdog")
    if high_case and not (args.allow_high_memory and args.high_memory_preflight and
                          args.require_exact_source):
        parser.error("memory64-high-random-store requires --allow-high-memory, "
                     "--high-memory-preflight and --require-exact-source")
    state = cgroup_preflight(args.cpu)
    if high_case:
        require_sparse_high_memory_headroom(state)
    # Keep fixture generation and the sparse-memory polling watchdog off the
    # measured P core. Every engine command selects that P core explicitly.
    os.sched_setaffinity(0, {16})
    args.out.mkdir(parents=True)
    fixtures = args.out / "fixtures"
    logs = args.out / "logs"
    fixtures.mkdir()
    logs.mkdir()
    shutil.copyfile(__file__, args.out / "run.py")
    shutil.copyfile(Path(__file__).with_name("generate.py"), args.out / "generate.py")
    paths = {"wasm-tools": args.wasm_tools, "wasmtime": args.wasmtime,
             "ordinary": args.ordinary, "ros": args.ros}
    if args.wasmedge:
        paths["wasmedge"] = args.wasmedge
    if args.wavm:
        paths["wavm"] = args.wavm
    if args.wasmer:
        paths["wasmer"] = args.wasmer
    metadata = {"paths": {key: {"path": str(value), "sha256": sha256(value)}
                           for key, value in paths.items()},
                "reference_specification": {
                    "edition": "WebAssembly 3.0 (2026-09-21)",
                    "change_history_url": "https://webassembly.github.io/spec/core/appendix/changes.html",
                    "scope": "representative performance fixtures, not the official conformance suite"},
                "cgroup_before": state, "cpu": args.cpu, "low": args.low,
                "harness_affinity": sorted(os.sched_getaffinity(0)),
                "high": args.high, "pairs": args.pairs, "trace": args.trace,
                "runner_sha256": sha256(Path(__file__)),
                "generator_sha256": sha256(Path(__file__).with_name("generate.py")),
                "fixtures": {},
                "include_gc_collector_matrix": args.include_gc_collector_matrix,
                "feature_policy": {
                    "ordinary_ros": "case-required -WFE flags; direct-call-step also enables function-references to match call-ref-step codegen configuration",
                    "wasmtime": "Wasm2 baseline plus case-required Core3 flags; direct-call-step also enables function-references as a matched control",
                    "wasmer": "case-required supported --enable flags",
                    "wasmedge": "--wasm-3 plus threads enabled for every case",
                    "wavm": "fixed available --enable list for every case",
                },
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    if args.require_exact_source:
        metadata["product_builds"] = {
            label: verify_product_build(label, binary, args.out, "before")
            for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    env = os.environ.copy()
    runtime_libs = [str(path.parent.parent / "lib") for path in (args.wasmedge, args.wavm)
                    if path is not None]
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, *runtime_libs)) + ":" + env.get("LD_LIBRARY_PATH", "")
    if args.wasmer:
        config = str(args.wasmer.parent.parent / "config")
        env.update(XDG_CONFIG_HOME=config, WASMER_DIR=config)
    metadata["competitor_versions"] = {}
    for name in ("wasmtime", "wasmedge", "wavm", "wasmer"):
        if name in paths:
            version_command = ["taskset", "-c", str(args.cpu), str(paths[name]), "--version"]
            version = subprocess.run(version_command, capture_output=True, timeout=30, env=env)
            metadata["competitor_versions"][name] = {
                "command": version_command, "exit": version.returncode,
                "output": (version.stdout + version.stderr).decode(errors="replace")[-2000:]}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    summary = []
    failures = []

    def record_row(row):
        rows.append(row)
        # Keep crash-recoverable samples without rewriting an ever-growing
        # JSON array between timed subprocesses. The array is saved at exit.
        with (args.out / "raw.jsonl").open("a") as stream:
            stream.write(json.dumps(row) + "\n")

    engine_paths = {"ordinary-jit": args.ordinary, "ros-jit": args.ros,
                    "wasmtime": args.wasmtime}
    if args.wasmedge:
        engine_paths.update({"wasmedge-jit": args.wasmedge, "wasmedge-int": args.wasmedge})
    if args.wavm:
        engine_paths["wavm"] = args.wavm
    if args.wasmer:
        engine_paths["wasmer"] = args.wasmer
    if args.include_interpreters:
        engine_paths.update({"ordinary-int": args.ordinary, "ros-int": args.ros})
    if args.engine:
        unknown = set(args.engine) - set(engine_paths)
        if unknown:
            parser.error(f"--engine names unavailable modes: {sorted(unknown)}")
        engine_paths = {name: vm for name, vm in engine_paths.items()
                        if name in args.engine}
    if high_case:
        preflight = json.loads(args.high_memory_preflight.read_text())
        preflight_metadata = json.loads(
            (args.high_memory_preflight.parent / "metadata.json").read_text())
        approved = set(preflight["approved_engines"])
        if (not set(engine_paths) <= approved or
                preflight_metadata["source_builds"] != metadata["product_builds"] or
                preflight_metadata.get("runner_sha256") != sha256(
                    Path(__file__).with_name("preflight_high_memory.py")) or
                preflight_metadata["generator_sha256"] != sha256(Path(__file__).with_name("generate.py")) or
                preflight_metadata["wasm_tools_sha256"] != sha256(args.wasm_tools) or
                preflight_metadata["iterations"] < 10000 or
                preflight_metadata["guest_footprint"]["final_expected"] == 0 or
                any(preflight_metadata["engine_sha256"].get(name) != sha256(vm)
                    for name, vm in engine_paths.items())):
            raise RuntimeError("sparse high-memory engine/source/tool preflight mismatch")
        first_events = dict(line.split() for line in
                            preflight_metadata["cgroup_before"]["memory.events"].splitlines())
        last_events = dict(line.split() for line in
                           preflight_metadata["cgroup_after"]["memory.events"].splitlines())
        if any(first_events[key] != last_events[key] for key in ("oom", "oom_kill")):
            raise RuntimeError("sparse high-memory preflight had cgroup OOM")
        metadata["high_memory_preflight"] = {
            "path": str(args.high_memory_preflight),
            "sha256": sha256(args.high_memory_preflight),
            "metadata_sha256": sha256(args.high_memory_preflight.parent / "metadata.json"),
            "approved_engines": sorted(approved),
            "guest_footprint": preflight_metadata["guest_footprint"]}
        watchdog_source = Path(__file__).with_name("preflight_high_memory.py")
        metadata["high_memory_watchdog_source_sha256"] = sha256(watchdog_source)
        (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        from preflight_high_memory import MAX_RSS_KIB, bounded_run

    def measured_run(instruction, label, allow_failure):
        if not high_case:
            return run_one(instruction, label, logs, env, allow_failure=allow_failure)
        # The preflight covered one shorter run. Every longer timing process
        # must also stay under its cgroup-growth, RSS and 45-second limits.
        row = bounded_run(instruction, label, logs, env)
        row["rss_limited"] = row["rss_limited"] or row["maxrss_kib"] > MAX_RSS_KIB
        if row["rss_limited"]:
            row["exit"] = 125
        if row["exit"] and not allow_failure:
            log = logs / (label + ".log")
            raise RuntimeError(f"{label}: bounded high-memory run exited {row['exit']}; "
                               f"{log.read_text(errors='replace')[-4000:]}")
        return row
    for case in selected_cases:
        generated = {}
        metadata["fixtures"][case] = {}
        for count in (args.low, args.high):
            source, features = module(case, count)
            wat = fixtures / f"{case}-{count}.wat"
            wasm = wat.with_suffix(".wasm")
            wat.write_text(source)
            subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
            subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
            generated[count] = (wasm, features)
            fixture = {"wat_sha256": sha256(wat), "wasm_sha256": sha256(wasm),
                       "features": features, "iterations": count}
            if "-page-" in case:
                fixture["boundary_trace"] = page_memory_trace(count, "-aligned-" in case)
            if case == "gc-cast":
                fixture["cast_trace"] = gc_cast_trace(count)
                if not all(fixture["cast_trace"].values()):
                    raise RuntimeError("gc-cast fixture missed one aggregate heap type")
            if case in ("table32-indirect", "table64-indirect"):
                fixture["table_index_trace"] = table_index_trace(count)
                trace = fixture["table_index_trace"]
                if (not trace["first_target_calls"] or not trace["second_target_calls"] or
                        (count >= 8192 and trace["distinct_indices_in_prefix"] != 1024)):
                    raise RuntimeError("indirect-call fixture missed a target or table slot")
            if case in ("call-ref-step", "direct-call-step"):
                fixture["call_ref_trace"] = call_ref_trace(count)
                trace = fixture["call_ref_trace"]
                if not trace["first_target_calls"] or not trace["second_target_calls"]:
                    raise RuntimeError("typed call_ref fixture missed one dynamic target")
            if case in ("strict-swizzle", "relaxed-swizzle"):
                fixture["swizzle_trace"] = swizzle_trace(count)
                trace = fixture["swizzle_trace"]
                if (trace["distinct_bitmasks_in_prefix"] < 2 or
                        (count >= 8192 and trace["distinct_indices_in_prefix"] != 16)):
                    raise RuntimeError("swizzle fixture missed dynamic SIMD indices/results")
            metadata["fixtures"][case][str(count)] = fixture
        (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        case_engines = dict(engine_paths)
        if (args.include_gc_collector_matrix and "gc" in generated[args.low][1]
                and "wasmtime" in case_engines):
            case_engines.pop("wasmtime")
            case_engines.update({f"wasmtime-{collector}": args.wasmtime
                                 for collector in ("copying", "drc", "null")})
        prepared = {}
        for engine, vm in case_engines.items():
            baseline = {}
            semantic_failed = False
            for count in (args.low, args.high):
                wasm, features = generated[count]
                instruction = command(engine, vm, wasm, features, args.trace, args.cpu)
                baseline[count] = instruction
                row = measured_run(instruction, f"{case}-{engine}-{count}-semantic",
                                   allow_failure=engine not in ("ordinary-jit", "ordinary-int",
                                                                "ros-jit", "ros-int"))
                row.update(case=case, engine=engine, count=count, phase="semantic")
                record_row(row)
                if row["exit"]:
                    failures.append({"case": case, "engine": engine, "count": count,
                                     "phase": "semantic", "exit": row["exit"],
                                     "log": row["label"] + ".log"})
                    semantic_failed = True
                    break
            if semantic_failed:
                (args.out / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
                print(case, engine, "semantic failure; excluded", flush=True)
                continue
            prepared[engine] = baseline
        # Rotate and reverse engine order as well as low/high order. This
        # spreads thermal and clock drift across VMs instead of granting one
        # engine every early sample and another every late sample.
        engine_names = list(prepared)
        if not engine_names:
            raise RuntimeError(f"{case}: no engine passed semantic preflight")
        timings = {engine: {"slopes": [], "high_times": []} for engine in engine_names}
        invalid_timed_engines = set()
        for pair in range(args.pairs):
            pair_start = cpu_telemetry(args.cpu)
            engine_order = engine_order_for_pair(engine_names, pair)
            order = (args.low, args.high) if pair % 2 == 0 else (args.high, args.low)
            for engine in engine_order:
                if engine in invalid_timed_engines:
                    continue
                timed = {}
                for count in order:
                    row = measured_run(prepared[engine][count],
                                       f"{case}-{engine}-pair{pair}-{count}",
                                       allow_failure=engine not in ("ordinary-jit", "ordinary-int",
                                                                    "ros-jit", "ros-int"))
                    row.update(case=case, engine=engine, count=count,
                               phase="timed", pair=pair, order=list(order),
                               engine_order=engine_order)
                    record_row(row)
                    if row["exit"]:
                        failures.append({"case": case, "engine": engine, "count": count,
                                         "phase": "timed", "pair": pair,
                                         "exit": row["exit"], "log": row["label"] + ".log"})
                        (args.out / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
                        invalid_timed_engines.add(engine)
                        print(case, engine, "timed failure; all timing samples excluded", flush=True)
                        break
                    timed[count] = row["elapsed_ns"]
                    if count == args.high:
                        timings[engine]["high_times"].append(row["elapsed_ns"])
                if engine in invalid_timed_engines:
                    continue
                timings[engine]["slopes"].append(
                    (timed[args.high] - timed[args.low]) / (args.high - args.low))
            telemetry = {"case": case, "pair": pair, "engine_order": engine_order,
                         "before": pair_start, "after": cpu_telemetry(args.cpu)}
            with (args.out / "pair-telemetry.jsonl").open("a") as output:
                output.write(json.dumps(telemetry) + "\n")
        for engine in engine_names:
            if engine in invalid_timed_engines:
                continue
            slopes = timings[engine]["slopes"]
            high_times = timings[engine]["high_times"]
            slope_median = statistics.median(slopes)
            record = {"case": case, "engine": engine,
                      "comparison_role": ("no-reclamation-throughput-bound"
                                          if engine == "wasmtime-null" else "same-wasm-engine"),
                      "repeated_feature_exercised": case != "extended-const-init",
                      "metric_caveat": (
                          "extended-const evaluated once at instantiation; slope measures the scalar control loop"
                          if case == "extended-const-init" else
                          "nonblocking mismatch/empty fast path only; no parked-thread wake latency"
                          if "atomic-wait-mismatch" in case or "atomic-notify-empty" in case else None),
                      "gc_reclamation_status": ({
                          "ordinary-jit": "absent-release-blocker",
                          "ordinary-int": "absent-release-blocker",
                          "ros-jit": "absent-release-blocker",
                          "ros-int": "absent-release-blocker",
                          "wasmtime-copying": "copying-selected-rss-gate-still-required",
                          "wasmtime-drc": "drc-selected-cycles-not-collected",
                          "wasmtime-null": "none-throughput-bound-only",
                      }.get(engine, "unproven-by-throughput")
                          if "gc" in generated[args.low][1] else None),
                      "median_delta_ns_per_iteration": slope_median,
                      "mad_delta_ns_per_iteration": statistics.median(
                          abs(slope - slope_median) for slope in slopes),
                      "min_delta_ns_per_iteration": min(slopes),
                      "max_delta_ns_per_iteration": max(slopes),
                      "slopes_ns_per_iteration": slopes,
                      "negative_slope_sample": any(slope < 0 for slope in slopes),
                      "high_median_ms": statistics.median(high_times) / 1e6,
                      "high_min_ms": min(high_times) / 1e6,
                      "high_p95_ms": statistics.quantiles(high_times, n=20, method="inclusive")[18] / 1e6,
                      "high_p99_ms": statistics.quantiles(high_times, n=100, method="inclusive")[98] / 1e6,
                      "high_max_ms": max(high_times) / 1e6,
                      "tail_sample_count": len(high_times),
                      "sub_100ms_sample": min(high_times) < 100_000_000,
                      "low_median_rss_mib": statistics.median(
                          row["maxrss_kib"] for row in rows
                          if row["case"] == case and row["engine"] == engine
                          and row["count"] == args.low and row["phase"] == "timed") / 1024,
                      "high_median_rss_mib": statistics.median(
                          row["maxrss_kib"] for row in rows
                          if row["case"] == case and row["engine"] == engine
                          and row["count"] == args.high and row["phase"] == "timed") / 1024,
                      "high_maxrss_kib": max(row["maxrss_kib"] for row in rows
                                              if row["case"] == case and row["engine"] == engine
                                              and row["count"] == args.high)}
            summary.append(record)
            (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
            print(case, engine, round(record["median_delta_ns_per_iteration"], 3),
                  "ns/iteration", flush=True)
    if args.require_exact_source:
        after = {label: verify_product_build(label, binary, args.out, "after")
                 for label, binary in (("ordinary", args.ordinary), ("ros", args.ros))}
        if after != metadata["product_builds"]:
            raise RuntimeError("product source or build changed during benchmark")
    if high_case and (sha256(Path(__file__).with_name("preflight_high_memory.py")) !=
                      metadata["high_memory_watchdog_source_sha256"] or
                      sha256(args.high_memory_preflight) !=
                      metadata["high_memory_preflight"]["sha256"] or
                      sha256(args.high_memory_preflight.parent / "metadata.json") !=
                      metadata["high_memory_preflight"]["metadata_sha256"]):
        raise RuntimeError("bounded high-memory preflight or watcher changed during timing")
    (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
    metadata["cgroup_after"] = cgroup_preflight(args.cpu)
    metadata["competitor_failures"] = failures
    metadata["competitor_semantic_failures"] = [row for row in failures if row["phase"] == "semantic"]
    metadata["competitor_timed_failures"] = [row for row in failures if row["phase"] == "timed"]
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    before_events = dict(line.split() for line in state["memory.events"].splitlines())
    after_events = dict(line.split() for line in metadata["cgroup_after"]["memory.events"].splitlines())
    if any(before_events[key] != after_events[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM occurred during paired benchmark; discard samples")
    before_cpu = dict(line.split() for line in state["cpu.stat"].splitlines())
    after_cpu = dict(line.split() for line in metadata["cgroup_after"]["cpu.stat"].splitlines())
    if any(before_cpu.get(key) != after_cpu.get(key)
           for key in ("nr_throttled", "throttled_usec")):
        raise RuntimeError("cgroup CPU throttled during paired benchmark; discard samples")


if __name__ == "__main__":
    main()
