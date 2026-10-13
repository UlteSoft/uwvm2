#!/usr/bin/env python3
"""Qualify a layout-only, explicitly collected native component on SSH Linux.

Both variants override the same direct gc_object.h include: the precise 16fcee
collector baseline versus its one-block allocation candidate. They never link
the context product's runtime.o, mix class definitions across TUs, or claim a
full VM collector. Build-only uses E16. Formal timing requires an explicitly
handed-off quiet P-core window, with at least nine reversed pairs.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
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
from run_gc_explicit_sweep_component import (NATIVE_DEFINES, FIELD_ITERATIONS,
    expected_checksums, quantiles, rows_from_log, validate_ring)

HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")
BASE_SHA = "16fcee47d451c614b222db978ff2fc2570d8c3ada94c782a0505b0ae1407b1f5"
CANDIDATE_SHA = "0c17afb7976fb6d22c6708f0187952f638cac9c19f8e7f23c100223b5e31feeb"
WRAP_SYMBOLS = ("_ZnwmRKSt9nothrow_t", "_ZnamRKSt9nothrow_t", "_ZdlPv", "_ZdlPvm", "_ZdaPv", "_ZdaPvm")
SYMBOLS = ("block_struct_get", "block_struct_set", "block_array_get", "block_array_set")
FIXTURES = {
    "edge": "gc_single_block_collect_edge_cases.cc",
    "ring": "gc_explicit_sweep_ring.cc",
    "publication": "gc_publish_scaling.cc",
    "opaque": "gc_opaque_token_probe.cc",
    "field_assembly": "gc_single_block_field_codegen.cc",
}
SCOPE = ("layout-only native component; both variants use the identical exclusive closed-cohort collector, "
         "explicit native roots and same source context; no actual Wasm lowering, automatic VM collection, "
         "multithreaded safepoint or managed-language ranking")


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def checked_process_tree_rss_bytes(root_pid, page_bytes, proc_root=Path("/proc")):
    """Fail on unreadable live accounting; only actually retired tasks disappear.

    This is an inner RSS guard. The independently coordinated host controller
    owns PID/birth/pidfd admission and peer accounting; this helper never kills
    a numerically discovered child or infers ownership from its environment.
    """
    pending, visited, total = [root_pid], set(), 0
    while pending:
        pid = pending.pop()
        if pid in visited:
            continue
        visited.add(pid)
        proc = proc_root / str(pid)
        try:
            first = (proc / "stat").read_text().rpartition(")")[2].split()
            birth = first[19]
            pages = int((proc / "statm").read_text().split()[1])
            for task in (proc / "task").iterdir():
                try:
                    pending.extend(int(child) for child in (task / "children").read_text().split())
                except (FileNotFoundError, ProcessLookupError):
                    if task.exists():
                        raise RuntimeError("live task accounting disappeared: " + str(task))
            last = (proc / "stat").read_text().rpartition(")")[2].split()
            if last[19] != birth:
                raise RuntimeError("RSS process identity changed: " + str(pid))
            total += pages * page_bytes
        except (FileNotFoundError, ProcessLookupError):
            if proc.exists():
                raise RuntimeError("live process accounting disappeared: " + str(pid))
    return total


def monitor_owned_process(process, record, read_memory, read_rss, tree_limit, shared_limit, timeout, started):
    """Always reap our unreaped start_new_session Popen, including reader faults."""
    try:
        while process.poll() is None:
            current, rss = read_memory(), read_rss(process.pid)
            record["tree_rss_peak"] = max(record["tree_rss_peak"], rss)
            record["cgroup_peak"] = max(record["cgroup_peak"], current)
            record["guard"] = current >= shared_limit or rss >= tree_limit
            record["timeout"] = time.monotonic() - started >= timeout
            if record["guard"] or record["timeout"]:
                break
            try:
                process.wait(timeout=.05)
            except subprocess.TimeoutExpired:
                pass
    except BaseException as error:
        record["accounting_error"] = type(error).__name__ + ": " + str(error)
        raise
    finally:
        # The session leader is still our unreaped Popen, so its PID cannot have
        # been recycled into a peer group. Kill only this owned PGID and wait;
        # never scan/kill child PID numbers or turn an observer error into RSS0.
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
                record["owned_group_killed"] = True
            except ProcessLookupError:
                record["owned_group_killed"] = False
            process.wait(timeout=4)


def normalized_assembly(text, symbol):
    match = re.search(r"^" + re.escape(symbol) + r":.*?^\s*\.size\s+" +
                      re.escape(symbol) + r",[^\n]*", text, re.M | re.S)
    if not match:
        raise RuntimeError("actual native field wrapper missing: " + symbol)
    labels = {}
    def label(match):
        value = match[0]
        return labels.setdefault(value, ".Lnative" + str(len(labels)))
    result = []
    for line in match[0].splitlines():
        line = line.split("#", 1)[0].strip()
        if line and not line.startswith((".loc", ".file")):
            result.append(re.sub(r"\.L[A-Za-z_][A-Za-z_0-9.$]*", label, line))
    return "\n".join(result) + "\n"


def publication_checksum(count=10_000_000):
    state, checksum = 123456789, 0
    for _ in range(count):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        checksum += state
    return checksum


def validate_publication(text, mode, checksum):
    data = json.loads(text)
    if (data["threads"] != {"one": 1, "two": 2, "four": 4}[mode] or
        data["allocations"] != 4_000_000 or data["field_iterations"] != 10_000_000 or
        data["field_roots"] != 1024 or data["field_checksum"] != checksum or
        any(data[key] <= 0 for key in ("allocation_ns", "field_ns", "teardown_ns"))):
        raise RuntimeError("publication work counts/data differ from the independent recurrence")
    return data


def validate_edge(text, single_block):
    results = rows_from_log(text, "GC_SINGLE_BLOCK_EDGE")
    if len(results) != 1:
        raise RuntimeError("expected one complete native edge proof")
    result = results[0]
    # The identical graph has twelve explicit collection calls. The candidate
    # additionally publishes the object in the injected second-allocation test:
    # its one allocation succeeds, whereas the baseline's second one fails.
    if (result.get("single_block") is not single_block or
        result.get("native_component_only") is not True or
        result.get("automatic_vm_gc") is not False or
        type(result.get("checks")) is not int or result["checks"] <= 512 or
        result.get("collections") != 12 or
        result.get("reclaimed") != (16 if single_block else 15)):
        raise RuntimeError("native edge allocation/graph/collection witnesses are incomplete")
    return result


def self_test():
    first = "block_array_get:\n .cfi_startproc\n.LBB8_3:\n jmp .LBB8_3 # note\n .cfi_endproc\n .size block_array_get, .-block_array_get\n"
    second = first.replace(".LBB8_3", ".LBB91_2")
    assert normalized_assembly(first, "block_array_get") == normalized_assembly(second, "block_array_get")
    assert normalized_assembly(first, "block_array_get") != normalized_assembly(first.replace("jmp", "call"), "block_array_get")
    failed = False
    try:
        normalized_assembly(first, "block_struct_get")
    except RuntimeError:
        failed = True
    assert failed
    state, checksum = 123456789, 0
    for _ in range(17):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        checksum += state
    assert publication_checksum(17) == checksum
    record = {"threads": 1, "allocations": 4_000_000, "field_iterations": 10_000_000,
              "field_roots": 1024, "field_checksum": 17,
              "allocation_ns": 1, "field_ns": 1, "teardown_ns": 1}
    assert validate_publication(json.dumps(record), "one", 17) == record
    failures = 0
    for key, value in (("threads", 4), ("field_checksum", 19), ("allocations", 1), ("allocation_ns", 0)):
        try:
            validate_publication(json.dumps({**record, key: value}), "one", 17)
        except RuntimeError:
            failures += 1
    assert failures == 4
    edge = {"checks": 600, "collections": 12, "reclaimed": 16,
            "single_block": True, "native_component_only": True, "automatic_vm_gc": False}
    assert validate_edge("GC_SINGLE_BLOCK_EDGE " + json.dumps(edge), True) == edge
    edge_failures = 0
    for key, value in (("checks", 512), ("collections", 0), ("reclaimed", 15),
                       ("native_component_only", False), ("automatic_vm_gc", True),
                       ("single_block", False)):
        try:
            validate_edge("GC_SINGLE_BLOCK_EDGE " + json.dumps({**edge, key: value}), True)
        except RuntimeError:
            edge_failures += 1
    assert edge_failures == 6
    with tempfile.TemporaryDirectory(prefix="uwvm-gc-accounting-control-") as directory:
        proc = Path(directory) / "100"
        (proc / "task" / "100").mkdir(parents=True)
        stat = "100 (control name) " + " ".join(["S", "1", *["0"] * 17, "123", *["0"] * 8])
        (proc / "stat").write_text(stat)
        (proc / "statm").write_text("9 3 0\n")
        (proc / "task" / "100" / "children").write_text("")
        assert checked_process_tree_rss_bytes(100, 4096, Path(directory)) == 12288
        (proc / "statm").unlink()
        try:
            checked_process_tree_rss_bytes(100, 4096, Path(directory))
        except RuntimeError:
            pass
        else:
            raise AssertionError("unreadable live accounting was accepted")
    for failure in ("reader", "budget"):
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        record = {"tree_rss_peak": 0, "cgroup_peak": 0, "guard": False, "timeout": False}
        def reader():
            if failure == "reader":
                raise PermissionError("injected unreadable live accounting")
            return 200
        try:
            monitor_owned_process(child, record, reader, lambda pid: 1, 100, 100, 10, time.monotonic())
        except PermissionError:
            assert failure == "reader" and record.get("accounting_error")
        assert child.returncode == -signal.SIGKILL and record["owned_group_killed"]
    print("PASS auxiliary accounting/owned-child cleanup controls only; no C++ qualification or performance timing")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary", type=Path)
    parser.add_argument("--ros", type=Path)
    parser.add_argument("--ordinary-source-id")
    parser.add_argument("--ros-source-id")
    parser.add_argument("--baseline-root", type=Path)
    parser.add_argument("--candidate-root", type=Path)
    parser.add_argument("--expected-baseline-sha256", default=BASE_SHA)
    parser.add_argument("--expected-candidate-sha256", default=CANDIDATE_SHA)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--samples", type=int, default=9)
    parser.add_argument("--allocations", type=int, default=10_000_000)
    parser.add_argument("--collect-every", type=int, default=65536)
    parser.add_argument("--tree-rss-bytes", type=int, default=2 * 1024**3)
    parser.add_argument("--stop-memory-bytes", type=int, default=63_000_000_000)
    parser.add_argument("--start-temperature-max-millicelsius", type=int, default=0)
    parser.add_argument("--thermal-wait-seconds", type=int, default=180)
    parser.add_argument("--quiet-window-confirmed", action="store_true")
    phase = parser.add_mutually_exclusive_group()
    phase.add_argument("--build-only", action="store_true")
    phase.add_argument("--run-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    for name in ("ordinary", "ros", "ordinary_source_id", "ros_source_id", "baseline_root", "candidate_root", "out"):
        if getattr(args, name) is None:
            parser.error("--" + name.replace("_", "-") + " is required")
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("native qualification and formal timing require SSH Linux x86-64")
    if not args.build_only and not args.quiet_window_confirmed:
        raise RuntimeError("formal P-core timing requires an explicitly handed-off quiet window")
    if (args.samples < 9 or not 1_000_000 <= args.allocations <= 10_000_000 or
        not 1024 <= args.collect_every <= 1_000_000 or
        not 0 < args.tree_rss_bytes <= 4 * 1024**3 or
        not 0 < args.stop_memory_bytes < 64 * 1024**3 or args.thermal_wait_seconds < 0):
        raise ValueError("invalid pair, work-count, memory or thermal limit")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    os.sched_setaffinity(0, {16})
    before = cgroup_preflight(0)
    if before["cpuset.cpus.effective"] != "0,2,4,6,16-31":
        raise RuntimeError("exact 4P+16E cgroup cpuset changed")
    prefixes = {"baseline": args.baseline_root.resolve(strict=True), "single-block": args.candidate_root.resolve(strict=True)}
    expected_headers = {"baseline": args.expected_baseline_sha256, "single-block": args.expected_candidate_sha256}
    for variant, prefix in prefixes.items():
        if sorted(path.relative_to(prefix) for path in prefix.rglob("*") if path.is_file()) != [HEADER]:
            raise RuntimeError("each include overlay must contain only its reviewed gc_object.h")
        if sha256(prefix / HEADER) != expected_headers[variant]:
            raise RuntimeError("reviewed " + variant + " header SHA mismatch")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=args.run_only)
    if args.run_only and not (out / "native-build.json").is_file():
        raise RuntimeError("run-only requires a successfully qualified immutable build")
    products = {"ordinary": args.ordinary.resolve(strict=True), "ros": args.ros.resolve(strict=True)}
    expected_ids = {"ordinary": args.ordinary_source_id, "ros": args.ros_source_id}
    contexts = {name: verify_product_build(name, binary, out, "run-before" if args.run_only else "build-before")
                for name, binary in products.items()}
    if {name: context["source_id"] for name, context in contexts.items()} != expected_ids:
        raise RuntimeError("context source/product O3 manifests differ from the intended IDs")
    own = Path(__file__).resolve().parent
    fixture_paths = {name: own / file for name, file in FIXTURES.items()}
    inputs = {"scope": SCOPE, "contexts": contexts, "intended_source_ids": expected_ids,
              "context_product_headers_sha256": {name: sha256(Path(context["source"]) / "src" / HEADER) for name, context in contexts.items()},
              "baseline_header_sha256": expected_headers["baseline"], "candidate_header_sha256": expected_headers["single-block"],
              "overlays": {name: str(prefix) for name, prefix in prefixes.items()},
              "runner_sha256": sha256(Path(__file__)), "fixture_sha256": {name: sha256(path) for name, path in fixture_paths.items()},
              "component_helper_sha256": sha256(own / "run_gc_explicit_sweep_component.py"),
              "common_runner_sha256": sha256(own / "run.py"), "compiler_sha256": sha256(args.clang),
              "native_defines": NATIVE_DEFINES, "samples": args.samples, "allocations": args.allocations,
              "collect_every": args.collect_every, "tree_rss_bytes": args.tree_rss_bytes,
              "stop_memory_bytes": args.stop_memory_bytes,
              "start_temperature_max_millicelsius": args.start_temperature_max_millicelsius,
              "thermal_wait_seconds": args.thermal_wait_seconds,
              "page_bytes": resource.getpagesize(), "p_cores": [0, 2, 4, 6],
              "asan_options": "detect_leaks=1:halt_on_error=1", "ubsan_options": "halt_on_error=1"}
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    env["ASAN_OPTIONS"] = inputs["asan_options"]; env["UBSAN_OPTIONS"] = inputs["ubsan_options"]
    summary = {"passed": False, "scope": SCOPE, "full_vm_qualified": False,
               "automatic_vm_gc": False, "formal_native_performance_complete": False,
               "product_source_edited": False, "edge_qualification": [], "field_assembly": []}
    rows, binaries, samples = [], {}, []
    logs = out / "logs"; logs.mkdir(exist_ok=args.run_only)

    def run(label, argv, cwd, timeout=600, formal=False):
        state = cgroup_preflight(0)
        if int(state["memory.current"]) + args.tree_rss_bytes >= args.stop_memory_bytes:
            raise RuntimeError("insufficient shared cgroup reserve before " + label)
        save(out / (label + ".command.json"), argv)
        log = logs / (label + ".log")
        record = {"label": label, "argv": argv, "resources_before": state,
                  "telemetry_before": {str(cpu): cpu_telemetry(cpu) for cpu in (0, 2, 4, 6)} if formal else cpu_telemetry(16),
                  "tree_rss_peak": 0, "cgroup_peak": int(state["memory.current"]), "guard": False, "timeout": False}
        started = time.monotonic()
        failure = None
        with log.open("wb") as output:
            process = subprocess.Popen(argv, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                monitor_owned_process(process, record,
                    lambda: int(Path("/sys/fs/cgroup/memory.current").read_text()),
                    lambda pid: checked_process_tree_rss_bytes(pid, inputs["page_bytes"]),
                    args.tree_rss_bytes, args.stop_memory_bytes, timeout, started)
            except BaseException as error:
                failure = error
        record.update(exit=process.returncode, elapsed_seconds=time.monotonic() - started,
                      log_sha256=sha256(log))
        try:
            record.update(resources_after=cgroup_preflight(0),
                telemetry_after={str(cpu): cpu_telemetry(cpu) for cpu in (0, 2, 4, 6)} if formal else cpu_telemetry(16))
        except BaseException as error:
            record["after_accounting_error"] = type(error).__name__ + ": " + str(error)
            if failure is None:
                failure = error
        rows.append(record); save(out / "commands.json", rows)
        if failure is not None:
            raise RuntimeError(label + " accounting failed; owned group reaped and original log retained") from failure
        if record["exit"] or record["guard"] or record["timeout"]:
            raise RuntimeError(label + " failed; original log retained")
        return log.read_text(errors="replace"), record

    def thermal_admission():
        started = time.monotonic(); probes = []
        while True:
            data = cpu_telemetry(0); probes.append(data)
            temperatures = list(data["thermal_millicelsius_by_zone_and_type"].values())
            if not args.start_temperature_max_millicelsius or not temperatures or max(temperatures) <= args.start_temperature_max_millicelsius:
                return {"probes": probes, "temperature_gate_requested": bool(args.start_temperature_max_millicelsius),
                        "temperature_available": bool(temperatures), "seconds": time.monotonic() - started}
            if time.monotonic() - started >= args.thermal_wait_seconds:
                raise RuntimeError("temperature admission timed out; no small-regression claim permitted")
            time.sleep(.25)

    def check_depfile(path, variant, source):
        text = path.read_text().replace("\\\n", " ")
        selected = str(prefixes[variant] / HEADER)
        other = str(prefixes["single-block" if variant == "baseline" else "baseline"] / HEADER)
        if selected not in text or other in text or str(source / "src" / HEADER) in text:
            raise RuntimeError("native TU did not use exactly its reviewed one-header override")

    try:
        if args.run_only:
            saved = json.loads((out / "native-build.json").read_text())
            if saved["inputs"] != inputs:
                raise RuntimeError("run-only inputs differ from the qualified build")
            binaries = {key: Path(record["path"]) for key, record in saved["binaries"].items()}
            if any(sha256(path) != saved["binaries"][key]["sha256"] for key, path in binaries.items()):
                raise RuntimeError("qualified native binary changed")
            summary.update(edge_qualification=saved["edge_qualification"], field_assembly=saved["field_assembly"])
        else:
            save(out / "inputs.json", inputs)
            for path in (Path(__file__), *fixture_paths.values()): shutil.copy2(path, out / path.name)
            run("compiler-version", [str(args.clang), "--version"], own)
            for product, context in contexts.items():
                source = Path(context["source"])
                common = [str(args.clang), "-std=c++26", "-stdlib=libc++", "-fno-rtti", "-Werror",
                          "-Wno-undefined-inline", *NATIVE_DEFINES]
                includes = ["-I" + str(source / "src"), "-I" + str(source / "third-parties/fast_io/include"),
                            "-I" + str(source / "third-parties/bizwen/include"), "-I" + str(source / "third-parties/boost_unordered/include")]
                link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
                        "-L/work/deps/usr/lib/x86_64-linux-gnu", "-pthread"]
                assembly = {}
                for variant in prefixes:
                    override = ["-I" + str(prefixes[variant])]
                    variant_macro = ["-DUWVM2TEST_GC_SINGLE_BLOCK_CANDIDATE=1"] if variant == "single-block" else []
                    run("syntax-" + product + "-" + variant, ["taskset", "-c", "16", *common,
                        *variant_macro, *override, *includes, "-DUWVM2TEST_GC_BLOCK_FAULT_INJECT=1",
                        "-fsyntax-only", str(out / FIXTURES["edge"])], source)
                    for fixture_name in ("edge", "ring", "opaque", "publication"):
                        profiles = ("o3", "asan") if fixture_name in ("edge", "ring") else (("asan",) if fixture_name == "opaque" else ("o3",))
                        for profile in profiles:
                            key = f"{product}-{variant}-{fixture_name}-{profile}"
                            binary, dep = out / key, out / (key + ".d")
                            flags = ["-O3", "-g0"] if profile == "o3" else ["-O1", "-g1", "-fno-omit-frame-pointer",
                                "-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
                            extra = variant_macro.copy()
                            if fixture_name == "edge":
                                extra += ["-DUWVM2TEST_GC_BLOCK_FAULT_INJECT=1", *["-Wl,--wrap=" + symbol for symbol in WRAP_SYMBOLS]]
                            if fixture_name == "ring":
                                extra += ["-DUWVM2TEST_GC_EXPLICIT_SWEEP=1"]
                                if profile == "asan": extra += ["-DUWVM2TEST_GC_SWEEP_FAULT_INJECT=1", "-Wl,--wrap=_ZnamRKSt9nothrow_t"]
                            run("build-" + key, ["taskset", "-c", "16", *common, *flags, *extra, *override, *includes,
                                "-MD", "-MF", str(dep), str(out / FIXTURES[fixture_name]), *link, "-o", str(binary)], source)
                            check_depfile(dep, variant, source); binaries[key] = binary
                            if fixture_name == "publication": continue
                            text, command = run("proof-" + key, ["taskset", "-c", "16", str(binary), *(["proof"] if fixture_name == "ring" else [])], source)
                            if fixture_name == "edge":
                                result = validate_edge(text, variant == "single-block")
                            elif fixture_name == "ring":
                                result = rows_from_log(text, "GC_SWEEP_PROOF")
                                if (len(result) != 1 or result[0]["collections"] != 4 or result[0]["reclaimed"] != 8 or
                                    not all(result[0][name] for name in ("missing_empty_store_rejected", "stale_rejected", "foreign_cycle_reclaimed", "overwrite_lease_released")) or
                                    result[0]["work_oom_injected"] != (profile == "asan") or result[0]["vm_qualified"]):
                                    raise RuntimeError("existing closed-cohort graph/work-OOM proof did not complete")
                            else:
                                if "PASS opaque GC token; workers=4 stores=2 initial_objects=32768 new_objects=8192" not in text:
                                    raise RuntimeError("four-thread/two-store token uniqueness/stale proof missing")
                                result = {"four_thread_two_store_identity": True, "native_component_only": True}
                            summary["edge_qualification"].append({"key": key, "result": result, "binary_sha256": sha256(binary),
                                                                 "depfile_sha256": sha256(dep), "command": command})
                    assembly_file = out / f"{product}-{variant}-field.s"
                    run("assembly-" + product + "-" + variant, ["taskset", "-c", "16", *common, "-O3", "-g0",
                        *override, *includes, "-S", str(out / FIXTURES["field_assembly"]), "-o", str(assembly_file)], source)
                    assembly[variant] = assembly_file.read_text()
                for symbol in SYMBOLS:
                    base, candidate = (normalized_assembly(assembly[variant], symbol) for variant in prefixes)
                    row = {"product": product, "symbol": symbol, "identical": base == candidate,
                           "baseline_sha256": hashlib.sha256(base.encode()).hexdigest(),
                           "candidate_sha256": hashlib.sha256(candidate.encode()).hexdigest()}
                    summary["field_assembly"].append(row)
                    (out / f"{product}-{symbol}-baseline.normalized.s").write_text(base)
                    (out / f"{product}-{symbol}-candidate.normalized.s").write_text(candidate)
                    if base != candidate:
                        raise RuntimeError("field wrapper assembly differs; stop for source-bound review before timing")
            save(out / "native-build.json", {"inputs": inputs,
                "binaries": {key: {"path": str(path), "sha256": sha256(path)} for key, path in binaries.items()},
                "edge_qualification": summary["edge_qualification"], "field_assembly": summary["field_assembly"]})
        summary["native_component_semantics_qualified"] = True
        if not args.build_only:
            checksums, checksum = expected_checksums(args.allocations), publication_checksum()
            for pair in range(args.samples):
                product_order = ("ros", "ordinary") if (pair // 2) % 2 == 0 else ("ordinary", "ros")
                variant_order = ("baseline", "single-block") if pair % 2 == 0 else ("single-block", "baseline")
                for workload in ("ring", "one", "two", "four"):
                    for product in product_order:
                        for variant in variant_order:
                            admission = thermal_admission()
                            fixture_name = "ring" if workload == "ring" else "publication"
                            key = f"{product}-{variant}-{fixture_name}-o3"
                            argv = ["taskset", "-c", "0" if workload == "ring" else "0,2,4,6", str(binaries[key])]
                            argv += ["ring", str(args.allocations), str(args.collect_every), str(inputs["page_bytes"])] if workload == "ring" else [workload]
                            text, command = run(f"pair-{pair:02}-{product}-{variant}-{workload}", argv,
                                Path(contexts[product]["source"]), formal=True)
                            result = validate_ring(text, "collector", args.allocations, args.collect_every, checksums) if workload == "ring" else validate_publication(text, workload, checksum)
                            samples.append({"product": product, "variant": variant, "workload": workload,
                                "pair": pair, "product_order": product_order, "variant_order": variant_order,
                                "result": result, "command": command, "native_binary_sha256": sha256(binaries[key]),
                                "thermal_admission": admission})
                            save(out / "raw-samples.json", samples)
            groups = {}
            for product in contexts:
                groups[product] = {}
                for workload in ("ring", "one", "two", "four"):
                    cells = {variant: [row for row in samples if row["product"] == product and row["workload"] == workload and row["variant"] == variant]
                             for variant in prefixes}
                    metrics = ("allocation_ns", "collection_ns", "local_fields_ns", "ring_wall_ns") if workload == "ring" else ("allocation_ns", "field_ns", "teardown_ns")
                    metric_rows = {}
                    for metric in metrics:
                        values = {variant: [row["result"]["summary"][metric] if workload == "ring" else row["result"][metric] for row in cells[variant]] for variant in prefixes}
                        ratios = [new / old for old, new in zip(values["baseline"], values["single-block"])]
                        metric_rows[metric] = {"median_ns": {variant: statistics.median(data) for variant, data in values.items()},
                            "candidate_over_baseline": ratios, "median_paired_candidate_over_baseline": statistics.median(ratios),
                            "sub_100ms_samples": {variant: sum(value < 100_000_000 for value in data) for variant, data in values.items()},
                            "scope": "sub-100ms cells cannot establish small regressions; nine process samples are not service p99"}
                    cell = {"pairs": args.samples, "metrics": metric_rows}
                    if workload == "ring":
                        cell["gc"] = {variant: {"actual_collections": sum(row["result"]["summary"]["collections"] for row in cells[variant]),
                            "actual_reclaimed": sum(row["result"]["summary"]["reclaimed"] for row in cells[variant]),
                            "rss_checkpoints": [row["result"]["checkpoints"] for row in cells[variant]],
                            "empirical_native_pause_ns": quantiles([pause["pause_ns"] for row in cells[variant] for pause in row["result"]["pauses"]]),
                            "pause_scope": "dependent native exclusive API pauses; no guest stop-time or service-tail claim"} for variant in prefixes}
                    groups[product][workload] = cell
            summary.update(groups=groups, formal_native_performance_complete=True, paired_samples=args.samples)
        after = {name: verify_product_build(name, binary, out, "source-after") for name, binary in products.items()}
        final = cgroup_preflight(0)
        event = lambda state, name: int(dict(line.split() for line in state["memory.events"].splitlines())[name])
        if (after != contexts or any(sha256(prefix / HEADER) != expected_headers[variant] for variant, prefix in prefixes.items()) or
            any(sha256(path) != inputs["fixture_sha256"][name] for name, path in fixture_paths.items()) or
            sha256(Path(__file__)) != inputs["runner_sha256"] or sha256(args.clang) != inputs["compiler_sha256"] or
            sha256(own / "run_gc_explicit_sweep_component.py") != inputs["component_helper_sha256"] or
            sha256(own / "run.py") != inputs["common_runner_sha256"] or
            any(event(before, name) != event(final, name) for name in ("oom", "oom_kill"))):
            raise RuntimeError("source/header/tool/fixture or OOM drift invalidates native qualification")
        summary.update(passed=True, build_only=args.build_only, inputs=inputs, resources_before=before, resources_after=final)
    except Exception as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["commands"] = len(rows)
        save(out / "summary.json", summary)
    print("PASS isolated single-block native component; full-VM automatic GC/performance remain unqualified")


if __name__ == "__main__":
    main()
