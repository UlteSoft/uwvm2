#!/usr/bin/env python3
"""Source-bound sanitizer and 1P/4P A/B for the isolated GC block registry.

Build on E core 16 first, then use --run-only in an otherwise idle P-core
window. A shared-owner one-store control catches new hot-path costs; a
64-store round-robin catches TLS token-block waste. This tests a token-index
optimization, not garbage collection.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from run import LIB_PATHS, cgroup_preflight, cpu_telemetry, run_one, sha256, verify_product_build


HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")
BASE_SHA = "21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d"
CANDIDATE_SHA = {
    "v1": "fc14912df5e44af46f6e95804c755265d8a90b2496d0a62053b3a86d664e35d4",
    "v2": "8376ba6990ac854a508a0eaf3f9e941bc569671f3f824829f8920fe4642aa20b",
    "v3": "1080f56b8b723002342ac7131aea4a28d9c2f7445694a4d96d5825d502ddaeeb",
}
WORKLOADS = {"single-store": "gc_publish_scaling_shared.cc",
             "store-switch": "gc_block_switch_scaling.cc"}
SANITIZER_FIXTURES = ("gc_opaque_token_probe.cc", "gc_stripe_stress.cc",
                      "gc_ownership_transition.cc")
FIXTURES = (*WORKLOADS.values(), *SANITIZER_FIXTURES)


def invoke(command, log, env, cwd, timeout=600):
    with log.open("wb") as output:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"command exited {result.returncode}: {log}")
    return {"command": command, "log": str(log), "log_sha256": sha256(log)}


def counters(state, name):
    return dict(line.split() for line in state[name].splitlines())


def check_cgroup(before, after):
    if any(counters(before, "memory.events")[name] !=
           counters(after, "memory.events")[name] for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during block-registry A/B")
    if any(counters(before, "cpu.stat").get(name) !=
           counters(after, "cpu.stat").get(name)
           for name in ("nr_throttled", "throttled_usec")):
        raise RuntimeError("cgroup CPU throttling changed during block-registry A/B")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--baseline-product", required=True, type=Path)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--overlay-root", required=True, type=Path)
    parser.add_argument("--candidate", choices=sorted(CANDIDATE_SHA), required=True,
                        help="isolated candidate revision; v1 and v2 evidence must stay separate")
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--run-only", action="store_true")
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if args.build_only and args.run_only:
        parser.error("--build-only and --run-only conflict")
    if args.pairs < 9:
        parser.error("at least nine reversed pairs required")
    files = sorted(path.relative_to(args.overlay_root)
                   for path in args.overlay_root.rglob("*") if path.is_file())
    if files != [HEADER]:
        parser.error("overlay must contain exactly the one GC header")
    if args.run_only:
        out = args.out.resolve(strict=True)
        if (out / "summary.json").exists():
            parser.error("immutable timed output already exists")
    else:
        args.out.mkdir(parents=True, exist_ok=False)
        out = args.out.resolve()
    before = cgroup_preflight(0)
    os.sched_setaffinity(0, {16})
    product = verify_product_build("baseline", args.baseline_product, out,
                                   "run-before" if args.run_only else "build-before")
    if product["source_id"] != args.expected_source_id:
        raise RuntimeError("candidate is not compared with the intended O3 product")
    source = Path(product["source"])
    base_header = source / "src" / HEADER
    candidate_header = args.overlay_root / HEADER
    candidate_sha = CANDIDATE_SHA[args.candidate]
    if sha256(base_header) != BASE_SHA or sha256(candidate_header) != candidate_sha:
        raise RuntimeError("block registry overlay needs rebase against exact GC header")
    local = Path(__file__).parent
    fixture_paths = {name: local / name for name in FIXTURES}
    fixture_sha = {name: sha256(path) for name, path in fixture_paths.items()}
    for name in FIXTURES:
        peer = source / "benchmark/0004.wasm3-core" / name
        if sha256(peer) != fixture_sha[name]:
            raise RuntimeError(f"{name}: frozen snapshot differs from runner fixture")
    inputs = {"product": product, "baseline_header": BASE_SHA,
              "candidate_revision": args.candidate,
              "candidate_header": candidate_sha, "fixture_sha256": fixture_sha,
              "runner_sha256": sha256(Path(__file__)), "clang_sha256": sha256(args.clang),
              "compiler": str(args.clang), "overlay_root": str(args.overlay_root.resolve()),
              "source": str(source), "pairs": args.pairs}
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1"
    include = ["-I" + str(source / "src"),
               "-I" + str(source / "third-parties/fast_io/include"),
               "-I" + str(source / "third-parties/bizwen/include"),
               "-I" + str(source / "third-parties/boost_unordered/include")]
    link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind",
            "-L/work/deps/usr/lib/x86_64-linux-gnu", "-pthread"]
    binaries = {f"{workload}-{variant}": out / f"{workload}-{variant}"
                for workload in WORKLOADS for variant in ("baseline", "block-registry")}
    if args.run_only:
        built = json.loads((out / "build.json").read_text())
        if built["inputs"] != inputs:
            raise RuntimeError("block-registry build inputs changed before timing")
        if {name: sha256(path) for name, path in binaries.items()} != built["binaries"]:
            raise RuntimeError("built block-registry A/B binaries changed")
    else:
        shutil.copyfile(__file__, out / Path(__file__).name)
        for name, path in fixture_paths.items():
            shutil.copyfile(path, out / name)
        build_rows = []
        for key, binary in binaries.items():
            workload, variant = key.rsplit("-", 1) if key.endswith("-baseline") else (
                key.removesuffix("-block-registry"), "block-registry")
            selected = candidate_header if variant == "block-registry" else base_header
            overlay = (["-I" + str(args.overlay_root)] if variant == "block-registry" else [])
            depfile = out / f"{key}.d"
            command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                       "-stdlib=libc++", "-O3", "-g0", "-fno-rtti", "-Werror",
                       *overlay, *include, "-MD", "-MF", str(depfile),
                       str(out / WORKLOADS[workload]), *link, "-o", str(binary)]
            row = invoke(command, out / f"{key}-build.log", env, source)
            if str(selected) not in depfile.read_text():
                raise RuntimeError(f"{key}: wrong GC header selected by compiler")
            build_rows.append({"workload": workload, "variant": variant,
                               "binary_sha256": sha256(binary),
                               "depfile_sha256": sha256(depfile), **row})
        # Sanitizers cover exact forged/unissued/stale tokens, lease/teardown
        # races and concurrent publication. The ownership-transition check
        # first runs against the baseline to establish the existing contract.
        for name in SANITIZER_FIXTURES:
            variants = (("baseline", "block-registry") if name ==
                        "gc_ownership_transition.cc" else ("block-registry",))
            for variant in variants:
                key = f"{Path(name).stem}-{variant}-asan"
                binary = out / key
                depfile = out / (key + ".d")
                selected = candidate_header if variant == "block-registry" else base_header
                overlay = (["-I" + str(args.overlay_root)]
                           if variant == "block-registry" else [])
                command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                           "-stdlib=libc++", "-O1", "-g1", "-fno-rtti", "-Werror",
                           "-Wno-undefined-inline", "-fsanitize=address,undefined",
                           "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
                           *overlay, *include, "-MD", "-MF", str(depfile),
                           str(out / name), *link, "-o", str(binary)]
                build_row = invoke(command, out / f"{key}-build.log", env, source)
                if str(selected) not in depfile.read_text():
                    raise RuntimeError(f"{key}: sanitizer did not select intended GC header")
                run_row = invoke(["taskset", "-c", "16-19", str(binary)],
                                 out / f"{key}-run.log", env, source, timeout=300)
                build_rows.append({"sanitizer_fixture": name, "variant": variant,
                                   "binary_sha256": sha256(binary),
                                   "depfile_sha256": sha256(depfile),
                                   "build": build_row, "run": run_row})
        build_after = cgroup_preflight(0)
        check_cgroup(before, build_after)
        (out / "build.json").write_text(json.dumps({"inputs": inputs,
            "binaries": {name: sha256(path) for name, path in binaries.items()},
            "sanitizers": ["address", "undefined", "leak"],
            "sanitizer_options": {"ASAN_OPTIONS": env["ASAN_OPTIONS"],
                                  "UBSAN_OPTIONS": env["UBSAN_OPTIONS"]},
            "build_rows": build_rows, "cgroup_before": before,
            "cgroup_after": build_after,
            "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())},
            indent=2) + "\n")
    if args.build_only:
        print("PASS source-bound block-registry A/B and Linux sanitizer build; timing deferred")
        return
    logs = out / "logs"
    logs.mkdir()
    raw = []
    summaries = []
    timed_before = cgroup_preflight(0)
    timed_start = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    for workload in WORKLOADS:
        for mode in ("one", "four"):
            for variant in ("baseline", "block-registry"):
                binary = binaries[f"{workload}-{variant}"]
                run_one(["taskset", "-c", "0,2,4,6", str(binary), mode],
                        f"{workload}-{mode}-{variant}-warmup", logs, env,
                        timeout_seconds=180)
            matched_rows = []
            for pair in range(args.pairs):
                order = ("baseline", "block-registry") if pair % 2 == 0 else (
                    "block-registry", "baseline")
                telemetry_before = {str(cpu): cpu_telemetry(cpu) for cpu in (0, 2, 4, 6)}
                pair_rows = {}
                for variant in order:
                    label = f"{workload}-{mode}-pair{pair}-{variant}"
                    binary = binaries[f"{workload}-{variant}"]
                    row = run_one(["taskset", "-c", "0,2,4,6", str(binary), mode],
                                  label, logs, env, timeout_seconds=180)
                    guest = json.loads((logs / (label + ".log")).read_text())
                    workers = 1 if mode == "one" else 4
                    if workload == "store-switch":
                        expected_refs = workers * 64
                        valid = (guest["workers"] == workers and
                                 guest["stores_per_worker"] == 64 and
                                 guest["allocations"] == 1000000 and
                                 guest["final_refs"] == expected_refs and
                                 guest["field_checksum"] ==
                                     expected_refs * (expected_refs - 1) // 2 and
                                 guest["token_span"] >= guest["allocations"] and
                                 guest["token_span_method"] ==
                                     "all_store_first_last_single_writer")
                    else:
                        valid = (guest["threads"] == workers and
                                 guest["allocations"] == 4000000 and
                                 guest["field_iterations"] == 10000000 and
                                 guest["field_roots"] == 1024 and
                                 guest["field_checksum"] > 0 and
                                 guest["field_ns"] > 0)
                    if not valid or guest["allocation_ns"] <= 0 or guest["teardown_ns"] <= 0:
                        raise RuntimeError(f"{label}: native fixture self-check failed")
                    row.update(workload=workload, variant=variant, mode=mode,
                               pair=pair, order=list(order), **guest)
                    raw.append(row)
                    pair_rows[variant] = row
                    with (out / "raw.jsonl").open("a") as stream:
                        stream.write(json.dumps(row) + "\n")
                matched_rows.append(pair_rows)
                with (out / "pair-telemetry.jsonl").open("a") as stream:
                    stream.write(json.dumps({"workload": workload, "mode": mode,
                        "pair": pair, "order": order, "before": telemetry_before,
                        "after": {str(cpu): cpu_telemetry(cpu)
                                  for cpu in (0, 2, 4, 6)}}) + "\n")
            baseline = [item["baseline"] for item in matched_rows]
            candidate = [item["block-registry"] for item in matched_rows]
            speedups = [b["allocation_ns"] / c["allocation_ns"]
                        for b, c in zip(baseline, candidate)]
            record = {"workload": workload, "mode": mode, "pairs": args.pairs,
                      "median_paired_allocation_speedup": statistics.median(speedups),
                      "paired_allocation_speedups": speedups,
                      "baseline_allocation_median_ms": statistics.median(
                          item["allocation_ns"] for item in baseline) / 1e6,
                      "candidate_allocation_median_ms": statistics.median(
                          item["allocation_ns"] for item in candidate) / 1e6,
                      "baseline_teardown_median_ms": statistics.median(
                          item["teardown_ns"] for item in baseline) / 1e6,
                      "candidate_teardown_median_ms": statistics.median(
                          item["teardown_ns"] for item in candidate) / 1e6,
                      "baseline_rss_median_mib": statistics.median(
                          item["maxrss_kib"] for item in baseline) / 1024,
                      "candidate_rss_median_mib": statistics.median(
                          item["maxrss_kib"] for item in candidate) / 1024,
                      "sub_100ms_allocation_sample": any(
                          item["allocation_ns"] < 100000000 for item in baseline + candidate)}
            if workload == "store-switch":
                record.update(token_span_method="all_store_first_last_single_writer",
                    baseline_token_span_median=statistics.median(
                    item["token_span"] for item in baseline),
                    candidate_token_span_median=statistics.median(
                    item["token_span"] for item in candidate),
                    baseline_token_endpoint_span_median=statistics.median(
                    item["token_endpoint_span"] for item in baseline),
                    candidate_token_endpoint_span_median=statistics.median(
                    item["token_endpoint_span"] for item in candidate))
            else:
                field_speedups = [b["field_ns"] / c["field_ns"]
                                  for b, c in zip(baseline, candidate)]
                record.update(median_paired_local_field_speedup=statistics.median(
                    field_speedups), paired_local_field_speedups=field_speedups,
                    baseline_field_median_ms=statistics.median(
                        item["field_ns"] for item in baseline) / 1e6,
                    candidate_field_median_ms=statistics.median(
                        item["field_ns"] for item in candidate) / 1e6)
            summaries.append(record)
            (out / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    after = cgroup_preflight(0)
    check_cgroup(timed_before, after)
    final_product = verify_product_build("baseline", args.baseline_product, out, "run-after")
    if (final_product != product or sha256(base_header) != BASE_SHA or
            sha256(candidate_header) != candidate_sha or
            {name: sha256(path) for name, path in fixture_paths.items()} != fixture_sha or
            sha256(Path(__file__)) != inputs["runner_sha256"] or
            {name: sha256(path) for name, path in binaries.items()} !=
            json.loads((out / "build.json").read_text())["binaries"]):
        raise RuntimeError("block-registry A/B inputs changed during timing")
    (out / "metadata.json").write_text(json.dumps({"inputs": inputs,
        "scope": "isolated token-index A/B, not collection or same-Wasm throughput",
        "sanitizer_build_sha256": sha256(out / "build.json"),
        "cgroup_before": timed_before, "cgroup_after": after,
        "runner_affinity": [16], "guest_affinity": [0, 2, 4, 6],
        "start_utc": timed_start,
        "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}, indent=2) + "\n")
    print("PASS source-bound block-registry 1P/4P A/B; GC reclamation still unimplemented")


if __name__ == "__main__":
    main()
