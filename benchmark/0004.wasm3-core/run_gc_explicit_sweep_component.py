#!/usr/bin/env python3
"""Qualify and compare an explicitly collected native GC component on P0.

An isolated, explicitly hashed header replaces only a direct native include.
The product parser/JIT/root inventory is not exercised, and no automatic GC or
VM release qualification is inferred. The monotonic product header stays the
baseline and reports zero collections. Timed comparison needs an explicit quiet-
window handoff. Build-only qualification uses E16 and is not a performance result;
it may run only in a separately authorized bounded slot. The shared cgroup guard
is not a substitute for agent coordination.
"""

import argparse
import functools
import json
import math
import os
from pathlib import Path
import platform
import resource
import shlex
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time

from run import LIB_PATHS, cgroup_preflight, cpu_telemetry, sha256, verify_product_build


HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")
BASE_HEADER_SHA = "a24fa96e3e45eb1ec2e70936ce2da06d1439cf269a0d175bfa7ed38661017c49"
OWNER_CHECKED_HEADER_SHA = "16fcee47d451c614b222db978ff2fc2570d8c3ada94c782a0505b0ae1407b1f5"
FIELD_ITERATIONS = 1_000_000
CHECKPOINT_EVERY = 1_000_000
RING_SIZE = 1024
SCOPE = ("source-bound native hash-membership store with explicit canonical cohort and native roots; "
         "single mutator, caller-quiescent, no activation-root discovery, full Wasm execution or automatic collector")
NATIVE_DEFINES = ["-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT", "-DUWVM_DISABLE_JIT",
                  "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL", "-DUWVM_VERSION_X=2",
                  "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0", "-DNDEBUG",
                   "-DUWVM_MODE_RELEASE"]


def process_tree_rss_bytes(root_pid, page_bytes, proc_root=Path("/proc")):
    """Conservative sum of every live descendant's RSS, including Clang cc1."""
    pending = [root_pid]
    visited = set()
    total = 0
    while pending:
        pid = pending.pop()
        if pid in visited:
            continue
        visited.add(pid)
        proc = proc_root / str(pid)
        try:
            total += int((proc / "statm").read_text().split()[1]) * page_bytes
            for task in (proc / "task").iterdir():
                try:
                    pending.extend(int(child) for child in (task / "children").read_text().split())
                except (FileNotFoundError, ProcessLookupError):
                    pass
        except (FileNotFoundError, ProcessLookupError):
            pass
    return total


def expected_collection_count(allocations, interval):
    complete, remainder = divmod(allocations, CHECKPOINT_EVERY)
    return complete * math.ceil(CHECKPOINT_EVERY / interval) + math.ceil(remainder / interval) + 1


def collection_points(allocations, interval):
    points = []
    current = 0
    while current != allocations:
        current += min(allocations - current, interval, CHECKPOINT_EVERY - current % CHECKPOINT_EVERY)
        points.append(current)
    return points


def lcg_advance(count, initial=123456789):
    # Affine exponentiation over the exact modulo-2**32 recurrence. This is an
    # independent fixture oracle, evaluated outside every native timed interval.
    multiplier, addend = 1664525, 1013904223
    combined_multiplier, combined_addend = 1, 0
    mask = 0xFFFFFFFF
    while count:
        if count & 1:
            combined_multiplier = combined_multiplier * multiplier & mask
            combined_addend = (combined_addend * multiplier + addend) & mask
        addend = (multiplier * addend + addend) & mask
        multiplier = multiplier * multiplier & mask
        count >>= 1
    return (combined_multiplier * initial + combined_addend) & mask


@functools.lru_cache(maxsize=16)
def expected_checksums(allocations, fields=FIELD_ITERATIONS):
    roots = [0] * RING_SIZE
    state = lcg_advance(allocations - RING_SIZE)
    for index in range(allocations - RING_SIZE, allocations):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        roots[index & (RING_SIZE - 1)] = state
    field_checksum = 0
    for _ in range(fields):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        roots[(state >> 20) & (RING_SIZE - 1)] = state
        field_checksum += state
    return field_checksum, sum(roots)


@functools.lru_cache(maxsize=16)
def expected_checkpoint_checksum(allocations):
    return expected_checksums(allocations, 0)[1]


def rows_from_log(log, prefix):
    return [json.loads(line.removeprefix(prefix + " ")) for line in log.splitlines()
            if line.startswith(prefix + " ")]


def validate_ring(log, variant, allocations, interval, checksums):
    summaries = rows_from_log(log, "GC_SWEEP_SUMMARY")
    pauses = rows_from_log(log, "GC_SWEEP_COLLECTION")
    checkpoints = rows_from_log(log, "GC_SWEEP_CHECKPOINT")
    if len(summaries) != 1:
        raise RuntimeError("expected exactly one complete ring summary")
    result = summaries[0]
    expected_variant = "monotonic" if variant == "baseline" else "explicit-component"
    if (result["variant"] != expected_variant or result["allocations"] != allocations or
        result["ring_roots"] != RING_SIZE or result["collect_every"] != interval or
        result["field_iterations"] != FIELD_ITERATIONS or not result["explicit_native_roots"] or
        result["vm_qualified"] or result["automatic_gc"] or
        (result["field_checksum"], result["root_checksum"]) != checksums):
        raise RuntimeError("native ring count, data, explicit-root contract or scope mismatch")
    if any(result[name] <= 0 for name in ("allocation_ns", "local_fields_ns", "ring_wall_ns")):
        raise RuntimeError("missing positive measured native time")
    expected_checkpoints = list(range(CHECKPOINT_EVERY, allocations + 1, CHECKPOINT_EVERY))
    if allocations % CHECKPOINT_EVERY:
        expected_checkpoints.append(allocations)
    if [row["allocated"] for row in checkpoints] != expected_checkpoints:
        raise RuntimeError("missing, repeated or reordered actual RSS/checksum checkpoints")
    for row in checkpoints:
        if (row["root_count"] != RING_SIZE or row["rss_bytes"] <= 0 or
            row["checksum"] != expected_checkpoint_checksum(row["allocated"])):
            raise RuntimeError("checkpoint did not inspect a full actual resident root ring")
        expected_reclaimed = 0 if variant == "baseline" else row["allocated"] - RING_SIZE
        if row["reclaimed_total"] != expected_reclaimed or row["live_expected"] != (
            row["allocated"] if variant == "baseline" else RING_SIZE):
            raise RuntimeError("checkpoint object accounting differs from the explicit root graph")
    if variant == "baseline":
        if (pauses or result["collections"] or result["reclaimed"] or result["final_reclaimed"] or
            result["collection_ns"] or result["maximum_pause_ns"]):
            raise RuntimeError("a monotonic baseline must never report collection")
    else:
        count = expected_collection_count(allocations, interval)
        if len(pauses) != count or result["collections"] != count or result["reclaimed"] != allocations or result["final_reclaimed"] != RING_SIZE:
            raise RuntimeError("real collection/reclamation count missing or inconsistent")
        if [row["allocated"] for row in pauses] != collection_points(allocations, interval) + [allocations]:
            raise RuntimeError("missing or fabricated collection boundary")
        total = 0
        previous_allocated = 0
        for number, row in enumerate(pauses, start=1):
            total += row["reclaimed"]
            if (row["collections"] != number or row["allocated"] < previous_allocated or
                row["allocated"] > allocations or row["reclaimed_total"] != total or row["pause_ns"] <= 0):
                raise RuntimeError("repeated/invalid collection record")
            final = number == count
            if row["live_expected"] != (0 if final else RING_SIZE) or bool(row.get("final_drop", False)) != final:
                raise RuntimeError("final precise-root drop is not separate from ring collection")
            previous_allocated = row["allocated"]
        if (total != allocations or pauses[-1]["allocated"] != allocations or
            sum(row["pause_ns"] for row in pauses) != result["collection_ns"] or
            max(row["pause_ns"] for row in pauses) != result["maximum_pause_ns"]):
            raise RuntimeError("pause totals and returned reclaim totals differ")
    return {"summary": result, "pauses": pauses, "checkpoints": checkpoints}


def quantiles(values):
    ordered = sorted(values)
    return {label: ordered[min(len(ordered) - 1, math.ceil(len(ordered) * probability) - 1)]
            for label, probability in (("p50", 0.50), ("p95", 0.95), ("p99", 0.99))} if ordered else {}


def self_test():
    slow = 123456789
    for count in range(129):
        if count:
            slow = (slow * 1664525 + 1013904223) & 0xFFFFFFFF
        assert lcg_advance(count) == slow
    assert expected_collection_count(1_000_000, 65536) == 17
    assert expected_collection_count(10_000_000, 65536) == 161
    assert expected_collection_count(1_000_001, 65536) == 18
    assert len(collection_points(10_000_000, 65536)) == 160
    assert expected_checksums(1024, 0)[1] == sum(lcg_advance(index) for index in range(1, 1025))
    assert quantiles([1, 2, 3, 4]) == {"p50": 2, "p95": 4, "p99": 4}
    # Synthetic JSON below tests only rejection/accounting in the runner. It
    # does not mock a GC store, LLVM module, or claim native execution.
    allocations, interval = 1_000_000, 65536
    checkpoints = [{"allocated": allocations, "collections": 16, "reclaimed_total": allocations - RING_SIZE,
                    "live_expected": RING_SIZE, "root_count": RING_SIZE, "rss_bytes": 100,
                    "checksum": expected_checkpoint_checksum(allocations)}]
    total = 0
    pauses = []
    for number, point in enumerate(collection_points(allocations, interval), start=1):
        reclaimed = point - total - RING_SIZE
        total += reclaimed
        pauses.append({"allocated": point, "collections": number, "reclaimed": reclaimed,
                       "reclaimed_total": total, "live_expected": RING_SIZE, "pause_ns": 1})
    pauses.append({"allocated": allocations, "collections": 17, "reclaimed": RING_SIZE,
                   "reclaimed_total": allocations, "live_expected": 0, "final_drop": True, "pause_ns": 1})
    result = {"variant": "explicit-component", "allocations": allocations, "ring_roots": RING_SIZE,
              "collect_every": interval, "collections": 17, "reclaimed": allocations, "final_reclaimed": RING_SIZE,
              "field_iterations": FIELD_ITERATIONS, "explicit_native_roots": True, "vm_qualified": False,
              "automatic_gc": False, "field_checksum": 17, "root_checksum": 23, "collection_ns": 17, "maximum_pause_ns": 1,
              "allocation_ns": 1, "local_fields_ns": 1, "ring_wall_ns": 1}
    def encode(summary, pause_rows, checkpoint_rows):
        return "\n".join(["GC_SWEEP_SUMMARY " + json.dumps(summary)] +
                          ["GC_SWEEP_COLLECTION " + json.dumps(row) for row in pause_rows] +
                          ["GC_SWEEP_CHECKPOINT " + json.dumps(row) for row in checkpoint_rows])
    validate_ring(encode(result, pauses, checkpoints), "collector", allocations, interval, (17, 23))
    failures = 0
    for summary, pause_rows, checkpoint_rows in (
        ({**result, "collections": 0}, pauses, checkpoints),
        (result, pauses[:-1], checkpoints),
        (result, pauses, [{**checkpoints[0], "checksum": 0}]),
        ({**result, "vm_qualified": True}, pauses, checkpoints),
        (result, [{**pauses[0], "allocated": 1}, *pauses[1:]], checkpoints),
        ({**result, "allocation_ns": 0}, pauses, checkpoints)):
        try:
            validate_ring(encode(summary, pause_rows, checkpoint_rows), "collector", allocations, interval, (17, 23))
        except RuntimeError:
            failures += 1
    assert failures == 6
    with tempfile.TemporaryDirectory(prefix="uwvm-gc-tree-control-") as directory:
        proc_root = Path(directory)
        for pid, pages, children in ((11, 2, "12 12 99"), (12, 3, "13"), (13, 5, "11")):
            proc = proc_root / str(pid)
            task = proc / "task" / str(pid)
            task.mkdir(parents=True)
            (proc / "statm").write_text(f"20 {pages} 0 0 0 0 0\n")
            (task / "children").write_text(children)
        assert process_tree_rss_bytes(11, 4096, proc_root) == 10 * 4096
        assert process_tree_rss_bytes(100, 4096, proc_root) == 0
    print("PASS Python recurrence/accounting controls only; native C++/collection has not run")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary", type=Path)
    parser.add_argument("--ros", type=Path)
    parser.add_argument("--ordinary-source-id")
    parser.add_argument("--ros-source-id")
    parser.add_argument("--overlay-root", type=Path)
    parser.add_argument("--expected-candidate-sha256", default=OWNER_CHECKED_HEADER_SHA)
    parser.add_argument("--expected-baseline-sha256", default=BASE_HEADER_SHA)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--samples", type=int, default=9)
    parser.add_argument("--max-allocations", type=int, default=10_000_000)
    parser.add_argument("--collect-every", type=int, default=65536)
    parser.add_argument("--max-process-rss-bytes", type=int, default=4 * 1024**3)
    parser.add_argument("--stop-memory-bytes", type=int, default=60_000_000_000)
    parser.add_argument("--start-temperature-max-millicelsius", type=int, default=0,
                        help="optional bounded pre-sample thermal admission; zero records without imposing a gate")
    parser.add_argument("--thermal-wait-seconds", type=int, default=180)
    parser.add_argument("--component-rss-growth-limit-bytes", type=int, default=32 * 1024**2)
    phases = parser.add_mutually_exclusive_group()
    phases.add_argument("--build-only", action="store_true")
    phases.add_argument("--run-only", action="store_true")
    parser.add_argument("--syntax-first", action="store_true",
                        help="preflight both native header contexts with -fsyntax-only before linking each variant")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    for name in ("ordinary", "ros", "ordinary_source_id", "ros_source_id", "overlay_root", "out"):
        if getattr(args, name) is None:
            parser.error("--" + name.replace("_", "-") + " is required")
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("formal native component qualification requires SSH Linux x86-64")
    if args.samples < 9 or not CHECKPOINT_EVERY <= args.max_allocations <= 10_000_000 or not RING_SIZE <= args.collect_every <= CHECKPOINT_EVERY:
        parser.error("at least nine pairs; 1M..10M allocations; 1024..1M collection interval required")
    if not 0 < args.stop_memory_bytes < 64 * 1024**3 or args.max_process_rss_bytes <= 0:
        parser.error("memory watchdog must stop below the hard cgroup limit")
    if args.thermal_wait_seconds < 0 or args.component_rss_growth_limit_bytes < 0:
        parser.error("invalid thermal/RSS limit")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    if args.run_only and not out.is_dir():
        raise RuntimeError("run-only requires the existing source-bound build directory")
    out.mkdir(parents=True, exist_ok=args.run_only)
    os.sched_setaffinity(0, {16})
    before = cgroup_preflight(0)
    overlay = args.overlay_root.resolve(strict=True)
    if sorted(path.relative_to(overlay) for path in overlay.rglob("*") if path.is_file()) != [HEADER]:
        raise RuntimeError("native overlay must contain only the explicitly reviewed gc_object.h")
    if sha256(overlay / HEADER) != args.expected_candidate_sha256:
        raise RuntimeError("candidate header changed since explicit review")
    products = {"ordinary": args.ordinary.resolve(strict=True), "ros": args.ros.resolve(strict=True)}
    intended = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    builds = {label: verify_product_build(label, path, out, "run-before" if args.run_only else "build-before")
              for label, path in products.items()}
    if {label: item["source_id"] for label, item in builds.items()} != intended:
        raise RuntimeError("exact product/context source IDs differ from the selected O3 manifests")
    for item in builds.values():
        if sha256(Path(item["source"]) / "src" / HEADER) != args.expected_baseline_sha256:
            raise RuntimeError("collector candidate must be rebased against this exact baseline header")
    runtime_hashes = {}
    for product, binary in products.items():
        metadata = json.loads((binary.parent / "build.json").read_text())
        runtime_hashes[product] = sha256(binary.parent / "runtime.o")
        if runtime_hashes[product] != metadata["runtime_object_sha256"]:
            raise RuntimeError("baseline context's qualified runtime object changed")
    own = Path(__file__).resolve().parent
    fixture = own / "gc_explicit_sweep_ring.cc"
    proof_fixture = own.parents[1] / "test/0019.gc_statepoint/product_cohort_sweep.cc"
    inputs = {"scope": SCOPE, "products": builds, "source_ids": intended, "runtime_object_sha256": runtime_hashes,
              "runner_sha256": sha256(Path(__file__)), "fixture_sha256": sha256(fixture),
              "companion_proof_sha256": sha256(proof_fixture), "clang_sha256": sha256(args.clang),
              "overlay_root": str(overlay), "candidate_header_sha256": args.expected_candidate_sha256,
              "baseline_header_sha256": args.expected_baseline_sha256, "native_defines": NATIVE_DEFINES,
              "samples": args.samples, "max_allocations": args.max_allocations,
              "syntax_first": args.syntax_first,
              "collect_every": args.collect_every, "page_bytes": resource.getpagesize(),
              "stop_memory_bytes": args.stop_memory_bytes, "max_process_rss_bytes": args.max_process_rss_bytes,
              "start_temperature_max_millicelsius": args.start_temperature_max_millicelsius,
              "thermal_wait_seconds": args.thermal_wait_seconds,
              "rss_growth_limit_bytes": args.component_rss_growth_limit_bytes,
              "sampling_scope": "native API pauses; dependent serial samples, not service p99"}
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1"
    rows = []
    binaries = {f"{product}-{variant}-{profile}": out / f"{product}-{variant}-{profile}"
                for product in builds for variant in ("baseline", "collector") for profile in ("o3", "asan")}
    companion = {product: out / f"{product}-cohort-asan" for product in builds}
    cgroup_root = Path("/sys/fs/cgroup")
    summary = {"passed": False, "scope": SCOPE, "product_release_qualified": False,
                "full_vm_qualified": False, "automatic_gc": False,
                "performance_qualified": False,
                "execution_scope": ("E16 compile/sanitizer/graph semantics only; no formal performance measurement" if args.build_only else
                                    "source-bound native component API timing; not VM release or industry performance qualification"),
               "release_blocker": "VM activation roots, mutator safepoints and complete exn/extern/host graph are not integrated"}

    def run(label, command, cwd, timeout=300):
        cgroup_preflight(0)
        (out / (label + ".command")).write_text(shlex.join(command) + "\n")
        row = {"label": label, "command": command, "resources_before": cgroup_preflight(0),
               "telemetry_before": cpu_telemetry(16 if args.build_only else 0), "timeout": False, "memory_guard": False,
               "rss_guard_scope": "conservative sum of process tree RSS, including compiler descendants",
               "process_tree_peak_rss_bytes": 0}
        if int(row["resources_before"]["memory.current"]) + args.max_process_rss_bytes >= args.stop_memory_bytes:
            raise RuntimeError("insufficient cgroup headroom for " + label)
        started = time.monotonic()
        peak = int(row["resources_before"]["memory.current"])
        log = out / (label + ".log")
        with log.open("wb") as output:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
            while True:
                pid, raw_status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid:
                    break
                current = int((cgroup_root / "memory.current").read_text())
                peak = max(peak, current)
                rss = process_tree_rss_bytes(process.pid, inputs["page_bytes"])
                row["process_tree_peak_rss_bytes"] = max(row["process_tree_peak_rss_bytes"], rss)
                row["memory_guard"] = current >= args.stop_memory_bytes or rss >= args.max_process_rss_bytes
                row["timeout"] = time.monotonic() - started >= timeout
                if row["memory_guard"] or row["timeout"]:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    _, raw_status, usage = os.wait4(process.pid, 0)
                    break
                time.sleep(0.05)
        process.returncode = os.waitstatus_to_exitcode(raw_status)
        row.update(exit=process.returncode, elapsed_seconds=time.monotonic() - started,
                   maxrss_kib=usage.ru_maxrss, maxrss_scope="wait4 includes fork/exec inherited floor; checkpoint self RSS is direct",
                   cgroup_peak_observed_bytes=peak, log_sha256=sha256(log), telemetry_after=cpu_telemetry(16 if args.build_only else 0),
                   resources_after=cgroup_preflight(0))
        rows.append(row)
        (out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
        if row["exit"] or row["timeout"] or row["memory_guard"]:
            raise RuntimeError(f"{label}: command failed, timed out or exceeded memory guard")
        return log.read_text(errors="replace"), row

    def thermal_admission():
        started = time.monotonic()
        probes = []
        while True:
            observed = cpu_telemetry(0)
            probes.append(observed)
            sensors = list(observed["thermal_millicelsius_by_zone_and_type"].values())
            if not args.start_temperature_max_millicelsius or not sensors or max(sensors) <= args.start_temperature_max_millicelsius:
                return {"probes": probes, "elapsed_seconds": time.monotonic() - started,
                        "gate_requested": bool(args.start_temperature_max_millicelsius), "sensors_available": bool(sensors)}
            if time.monotonic() - started >= args.thermal_wait_seconds:
                raise RuntimeError("requested comparable pre-sample thermal range was not reached")
            time.sleep(0.25)

    try:
        if args.run_only:
            built = json.loads((out / "build.json").read_text())
            if built["inputs"] != inputs or built["binaries"] != {key: sha256(path) for key, path in {**binaries, **companion}.items()}:
                raise RuntimeError("native component build inputs or compiled binaries changed")
        else:
            (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
            shutil.copy2(__file__, out / Path(__file__).name)
            shutil.copy2(fixture, out / fixture.name)
            shutil.copy2(proof_fixture, out / proof_fixture.name)
            run("compiler-version", [str(args.clang), "--version"], own)
            for product, item in builds.items():
                source = Path(item["source"])
                common = [str(args.clang), "-std=c++26", "-stdlib=libc++", "-fno-rtti", "-Werror",
                          "-Wno-undefined-inline", *NATIVE_DEFINES]
                include = ["-I" + str(source / "src"), "-I" + str(source / "third-parties/fast_io/include"),
                           "-I" + str(source / "third-parties/bizwen/include"), "-I" + str(source / "third-parties/boost_unordered/include")]
                link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
                        "-L/work/deps/usr/lib/x86_64-linux-gnu", "-pthread"]
                sanitizer = ["-O1", "-g1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
                for variant in ("baseline", "collector"):
                    selected = source / "src" / HEADER if variant == "baseline" else overlay / HEADER
                    override = [] if variant == "baseline" else ["-I" + str(overlay), "-DUWVM2TEST_GC_EXPLICIT_SWEEP=1"]
                    if args.syntax_first:
                        run("syntax-" + product + "-" + variant,
                            ["taskset", "-c", "16", *common, *override, *include, "-fsyntax-only", str(out / fixture.name)],
                            source, timeout=300)
                    for profile in ("o3", "asan"):
                        key = f"{product}-{variant}-{profile}"
                        depfile = out / (key + ".d")
                        flags = ["-O3", "-g0"] if profile == "o3" else sanitizer + ["-DUWVM2TEST_GC_SWEEP_FAULT_INJECT=1", "-Wl,--wrap=_ZnamRKSt9nothrow_t"]
                        command = ["taskset", "-c", "16", *common, *flags, *override, *include,
                                   "-MD", "-MF", str(depfile), str(out / fixture.name), *link, "-o", str(binaries[key])]
                        run("build-" + key, command, source, timeout=600)
                        if str(selected) not in depfile.read_text():
                            raise RuntimeError("native fixture did not select the explicitly reviewed header")
                        if variant == "collector" and str(source / "src" / HEADER) in depfile.read_text():
                            raise RuntimeError("candidate compile unexpectedly included the baseline GC definition")
                        proof_log, _ = run("proof-" + key, ["taskset", "-c", "16", str(binaries[key]), "proof"], source)
                        proof_rows = rows_from_log(proof_log, "GC_SWEEP_PROOF")
                        if len(proof_rows) != 1 or not proof_rows[0]["component_semantics"] or proof_rows[0]["vm_qualified"]:
                            raise RuntimeError("missing native proof or incorrect VM scope")
                        proof_result = proof_rows[0]
                        if variant == "collector" and (proof_result["collections"] != 4 or proof_result["reclaimed"] != 8 or
                            not all(proof_result[name] for name in ("missing_empty_store_rejected", "stale_rejected", "foreign_cycle_reclaimed", "overwrite_lease_released")) or
                            proof_result["work_oom_injected"] != (profile == "asan")):
                            raise RuntimeError("collector graph/stale/lease/OOM proof incomplete")
                        if variant == "baseline" and (proof_result["collections"] or proof_result["reclaimed"]):
                            raise RuntimeError("baseline proof mislabeled as collection")
                key = product + "-cohort"
                depfile = out / (key + ".d")
                run("build-" + key, ["taskset", "-c", "16", *common, *sanitizer, "-I" + str(overlay), *include,
                    "-DUWVM_GC_PRODUCT_COHORT_PROBE=1", "-MD", "-MF", str(depfile), str(out / proof_fixture.name),
                    *link, "-o", str(companion[product])], source, timeout=600)
                if str(overlay / HEADER) not in depfile.read_text():
                    raise RuntimeError("closed-cohort proof compiled the wrong header")
                text, _ = run("proof-" + key, ["taskset", "-c", "16", str(companion[product])], source)
                if "PASS product cohort sweep:" not in text or "7 actual collections; 6 reclaimed;" not in text:
                    raise RuntimeError("closed-cohort/canonical-pin/extern-exn rejection proof did not complete")
            (out / "build.json").write_text(json.dumps({"inputs": inputs,
                "binaries": {key: sha256(path) for key, path in {**binaries, **companion}.items()},
                "proof_commands": rows, "sanitizers": ["ASan", "UBSan", "LSan"],
                "asan_options": env["ASAN_OPTIONS"], "ubsan_options": env["UBSAN_OPTIONS"],
                "scope": SCOPE, "full_vm_qualified": False}, indent=2) + "\n")
        samples = []
        summary["native_component_semantics_qualified"] = True
        if not args.build_only:
            checksums = expected_checksums(args.max_allocations)
            for pair in range(args.samples):
                order = ("baseline", "collector") if pair % 2 == 0 else ("collector", "baseline")
                product_order = ("ros", "ordinary") if (pair // 2) % 2 == 0 else ("ordinary", "ros")
                for product in product_order:
                    for variant in order:
                        admission = thermal_admission()
                        key = f"{product}-{variant}-o3"
                        label = f"pair-{pair:02}-{key}"
                        log, command_row = run(label, ["taskset", "-c", "0", str(binaries[key]), "ring",
                            str(args.max_allocations), str(args.collect_every), str(inputs["page_bytes"])], Path(builds[product]["source"]), timeout=300)
                        record = validate_ring(log, variant, args.max_allocations, args.collect_every, checksums)
                        record.update(product=product, variant=variant, pair=pair, command=command_row,
                                      native_binary_sha256=sha256(binaries[key]), thermal_admission=admission)
                        samples.append(record)
                        (out / "raw-samples.json").write_text(json.dumps(samples, indent=2) + "\n")
            groups = {}
            for product in builds:
                groups[product] = {}
                for variant in ("baseline", "collector"):
                    selected = [sample for sample in samples if sample["product"] == product and sample["variant"] == variant]
                    pauses = [pause["pause_ns"] for sample in selected for pause in sample["pauses"]]
                    rss_growth = [sample["checkpoints"][-1]["rss_bytes"] - sample["checkpoints"][0]["rss_bytes"] for sample in selected]
                    groups[product][variant] = {"samples": len(selected),
                        "allocation_ns_per_object_median": statistics.median(sample["summary"]["allocation_ns"] / args.max_allocations for sample in selected),
                        "allocation_plus_collection_ns_per_object_median": statistics.median((sample["summary"]["allocation_ns"] + sample["summary"]["collection_ns"]) / args.max_allocations for sample in selected),
                        "field_set_get_ns_median": statistics.median(sample["summary"]["local_fields_ns"] / FIELD_ITERATIONS for sample in selected),
                        "sub_100ms_allocation_samples": sum(sample["summary"]["allocation_ns"] < 100_000_000 for sample in selected),
                        "sub_100ms_field_samples": sum(sample["summary"]["local_fields_ns"] < 100_000_000 for sample in selected),
                        "short_sample_limit": "timing cells below 100ms are exploratory; do not infer a small regression or industry ranking",
                        "rss_growth_1m_to_final_bytes": rss_growth, "rss_growth_median_bytes": statistics.median(rss_growth),
                        "pause_count": len(pauses), "empirical_pause_ns": quantiles(pauses),
                        "pause_p99_count_qualified": len(pauses) >= 100,
                        "pause_scope": "pooled dependent serial native API calls; no service-tail or mutator stop-time claim",
                        "total_collections": sum(sample["summary"]["collections"] for sample in selected),
                        "total_reclaimed": sum(sample["summary"]["reclaimed"] for sample in selected)}
                ratios = {}
                for metric in ("allocation_ns", "local_fields_ns"):
                    ratios[metric] = [next(sample["summary"][metric] for sample in samples if sample["product"] == product and sample["variant"] == "collector" and sample["pair"] == pair) /
                                     next(sample["summary"][metric] for sample in samples if sample["product"] == product and sample["variant"] == "baseline" and sample["pair"] == pair)
                                     for pair in range(args.samples)]
                groups[product]["paired_candidate_over_baseline"] = {metric: {"raw": values, "median": statistics.median(values)} for metric, values in ratios.items()}
            enough_range = args.max_allocations >= 2_000_000
            rss_gate = enough_range and all(max(groups[product]["collector"]["rss_growth_1m_to_final_bytes"]) <= args.component_rss_growth_limit_bytes for product in groups)
            summary.update(groups=groups, paired_samples=args.samples,
                           component_rss_plateau_checked=enough_range, component_rss_plateau_passed=rss_gate,
                           native_component_semantics_qualified=True,
                           native_component_performance_complete=True,
                           performance_scope="component API, not same-Wasm VM or managed-language ranking")
            if enough_range and not rss_gate:
                raise RuntimeError("explicit component RSS plateau exceeds the requested bound")
        after = {label: verify_product_build(label, path, out, "source-after") for label, path in products.items()}
        final = cgroup_preflight(0)
        event = lambda state, key: int(dict(line.split() for line in state["memory.events"].splitlines())[key])
        if (after != builds or sha256(fixture) != inputs["fixture_sha256"] or sha256(proof_fixture) != inputs["companion_proof_sha256"] or
            sha256(Path(__file__)) != inputs["runner_sha256"] or sha256(args.clang) != inputs["clang_sha256"] or
            sha256(overlay / HEADER) != inputs["candidate_header_sha256"] or
            any(sha256(binary.parent / "runtime.o") != runtime_hashes[product] for product, binary in products.items()) or
            any(event(before, name) != event(final, name) for name in ("oom", "oom_kill"))):
            raise RuntimeError("source/header/tool/OOM drift invalidated native component evidence")
        summary.update(passed=True, build_only=args.build_only, inputs=inputs, resources_before=before, resources_after=final)
    except Exception as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["commands"] = len(rows)
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS explicit native component only; full-VM automatic GC release remains unqualified")


if __name__ == "__main__":
    main()
