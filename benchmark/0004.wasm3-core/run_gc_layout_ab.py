#!/usr/bin/env python3
"""Exact-source native A/B for an isolated GC header-only layout candidate.

The same source tree, fixture and Clang flags compile both binaries. The
candidate adds one include overlay containing only gc_object.h. Nine reversed
pairs compare allocation, checked local field access, teardown and RSS for
1/2/4 P-core workers. This is not a collector test and cannot clear the GC
reclamation release gate.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import time

from run import cgroup_preflight, cpu_telemetry, run_one, sha256, verify_product_build


HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")
BASELINE_HEADER_SHA256 = "21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d"
CANDIDATE_HEADER_SHA256 = "7eca359a5d989c814967df49e9030e5e1402adb97a0154a82b2591a52ad9a602"
LIBS = "/work/deps/usr/lib/x86_64-linux-gnu:/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib"


def counters(state, name):
    return dict(line.split() for line in state[name].splitlines())


def timing(binary, mode, label, logs, environment):
    command = ["taskset", "-c", "0,2,4,6", str(binary), mode]
    row = run_one(command, label, logs, environment, timeout_seconds=120)
    guest = json.loads((logs / (label + ".log")).read_text())
    if (guest["allocations"] != 4000000 or guest["field_iterations"] != 10000000 or
            guest["field_roots"] != 1024 or
            guest["threads"] != {"one": 1, "two": 2, "four": 4}[mode]):
        raise RuntimeError(f"{label}: native fixture returned wrong work count")
    row.update(guest)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--baseline-product", required=True, type=Path,
                        help="exact-source O3 UWVM CLI with adjacent build.json")
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--overlay-root", required=True, type=Path,
                        help="candidate include root containing only uwvm2/.../gc_object.h")
    parser.add_argument("--bench-source", type=Path,
                        default=Path(__file__).with_name("gc_publish_scaling.cc"))
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--objdump", type=Path, default=Path("/toolchain/bin/llvm-objdump"),
                        help="save exact native baseline/candidate machine code before timing")
    parser.add_argument("--nm", type=Path, default=Path("/toolchain/bin/llvm-nm"))
    parser.add_argument("--pairs", type=int, default=9)
    args = parser.parse_args()
    if args.out.exists() or args.pairs < 9:
        parser.error("fresh output and at least nine paired samples required")
    source_files = sorted(path.relative_to(args.overlay_root)
                          for path in args.overlay_root.rglob("*") if path.is_file())
    if source_files != [HEADER]:
        parser.error("overlay must contain exactly uwvm2/uwvm/runtime/storage/gc_object.h")
    state_before = cgroup_preflight(0)
    os.sched_setaffinity(0, {16})
    args.out.mkdir(parents=True)
    logs = args.out / "logs"
    logs.mkdir()
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    fixture = args.out / "gc_publish_scaling.cc"
    shutil.copyfile(args.bench_source, fixture)
    build = verify_product_build("baseline", args.baseline_product, args.out, "before")
    if build["source_id"] != args.expected_source_id:
        raise RuntimeError("native A/B baseline is not the intended frozen O3 source")
    source = Path(build["source"])
    baseline_header = source / "src" / HEADER
    candidate_header = args.overlay_root / HEADER
    hashes = {"compiler": sha256(args.clang), "fixture": sha256(fixture),
              "baseline_header": sha256(baseline_header),
              "candidate_header": sha256(candidate_header)}
    if (hashes["baseline_header"] != BASELINE_HEADER_SHA256 or
            hashes["candidate_header"] != CANDIDATE_HEADER_SHA256):
        raise RuntimeError("single-block overlay needs rebase against this exact GC header")
    environment = os.environ.copy()
    environment["LD_LIBRARY_PATH"] = LIBS + ":" + environment.get("LD_LIBRARY_PATH", "")
    compiler_flags = ["-std=c++26", "-stdlib=libc++", "-O3", "-g0", "-fno-rtti",
                      "-pthread", "-fuse-ld=lld", "-rtlib=compiler-rt",
                      "-unwindlib=libunwind"]
    binaries = {}
    builds = {}
    for variant in ("baseline", "single-block"):
        binary = args.out / ("gc-publish-" + variant)
        depfile = args.out / (variant + ".d")
        includes = (["-I" + str(args.overlay_root)] if variant == "single-block" else [])
        includes += ["-I" + str(source / "src"),
                     "-I" + str(source / "third-parties/fast_io/include"),
                     "-I" + str(source / "third-parties/bizwen/include"),
                     "-I" + str(source / "third-parties/boost_unordered/include"),
                     "-L/work/deps/usr/lib/x86_64-linux-gnu"]
        command = ["taskset", "-c", "16", str(args.clang), *compiler_flags,
                   *includes, "-MD", "-MF", str(depfile), str(fixture), "-o", str(binary)]
        build_log = logs / (variant + "-build.log")
        with build_log.open("wb") as output:
            completed = subprocess.run(command, cwd=source, stdout=output,
                                       stderr=subprocess.STDOUT, env=environment, timeout=600)
        if completed.returncode:
            raise RuntimeError(f"{variant} native build failed: {build_log}")
        selected = candidate_header if variant == "single-block" else baseline_header
        if str(selected) not in depfile.read_text():
            raise RuntimeError(f"{variant} build did not include expected GC header")
        codegen = {}
        for label, tool, options in (("assembly", args.objdump,
                                      ("-drC", "--no-show-raw-insn")),
                                     ("symbols", args.nm, ("-C", "--defined-only"))):
            path = args.out / f"{variant}-{label}.txt"
            with path.open("wb") as output:
                result = subprocess.run([str(tool), *options, str(binary)], cwd=source,
                                        stdout=output, stderr=subprocess.STDOUT,
                                        env=environment, timeout=180)
            if result.returncode:
                raise RuntimeError(f"{variant} native {label} capture failed: {path}")
            codegen[label] = {"path": str(path), "sha256": sha256(path)}
        binaries[variant] = binary
        builds[variant] = {"command": command, "build_log": str(build_log),
                           "build_log_sha256": sha256(build_log),
                           "binary_sha256": sha256(binary),
                           "depfile_sha256": sha256(depfile),
                           "selected_header": str(selected), "codegen": codegen}
    metadata = {"scope": "isolated GC single-block allocation A/B, not collection",
                "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "source_build": build, "expected_source_id": args.expected_source_id,
                "baseline_product": str(args.baseline_product),
                "overlay_root": str(args.overlay_root),
                "runner_sha256": sha256(Path(__file__)),
                "compiler_version": subprocess.check_output(
                    [str(args.clang), "--version"], text=True,
                    env=environment).splitlines()[0],
                "source_hashes": hashes, "builds": builds,
                "codegen_tools_sha256": {"objdump": sha256(args.objdump),
                                         "nm": sha256(args.nm)},
                "cgroup_before": state_before, "pairs": args.pairs,
                "harness_affinity": sorted(os.sched_getaffinity(0)),
                "p_cores": [0, 2, 4, 6], "allocations_per_process": 4000000,
                "local_field_set_get_pairs_per_process": 10000000,
                "local_field_roots": 1024}
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    summary = []
    for mode in ("one", "two", "four"):
        for variant, binary in binaries.items():
            row = timing(binary, mode, f"{mode}-{variant}-warmup", logs, environment)
            row.update(mode=mode, variant=variant, phase="warmup")
            rows.append(row)
        values = {variant: {field: [] for field in
                            ("allocation_ns", "field_ns", "teardown_ns", "maxrss_kib")}
                  for variant in binaries}
        ratios = []
        field_ratios = []
        for pair in range(args.pairs):
            order = ("baseline", "single-block") if pair % 2 == 0 else ("single-block", "baseline")
            frequency_before = {str(cpu): cpu_telemetry(cpu) for cpu in (0, 2, 4, 6)}
            matched = {}
            for variant in order:
                row = timing(binaries[variant], mode, f"{mode}-pair{pair}-{variant}",
                             logs, environment)
                row.update(mode=mode, variant=variant, phase="timed", pair=pair,
                           order=list(order))
                rows.append(row)
                matched[variant] = row
                for field in values[variant]:
                    values[variant][field].append(row[field])
                with (args.out / "raw.jsonl").open("a") as stream:
                    stream.write(json.dumps(row) + "\n")
            ratios.append(matched["baseline"]["allocation_ns"] /
                          matched["single-block"]["allocation_ns"])
            field_ratios.append(matched["baseline"]["field_ns"] /
                                matched["single-block"]["field_ns"])
            with (args.out / "pair-telemetry.jsonl").open("a") as stream:
                stream.write(json.dumps({"mode": mode, "pair": pair, "order": order,
                                         "before": frequency_before,
                                         "after": {str(cpu): cpu_telemetry(cpu)
                                                   for cpu in (0, 2, 4, 6)}}) + "\n")
        record = {"mode": mode, "pairs": args.pairs,
                  "median_paired_allocation_speedup": statistics.median(ratios),
                  "paired_allocation_speedups": ratios,
                  "median_paired_local_field_speedup": statistics.median(field_ratios),
                  "paired_local_field_speedups": field_ratios}
        for variant in binaries:
            samples = values[variant]
            record[variant] = {
                "allocation_median_ms": statistics.median(samples["allocation_ns"]) / 1e6,
                "allocation_p95_ms": statistics.quantiles(samples["allocation_ns"],
                                                            n=20, method="inclusive")[18] / 1e6,
                "local_field_median_ms": statistics.median(samples["field_ns"]) / 1e6,
                "local_field_p95_ms": statistics.quantiles(samples["field_ns"],
                                                            n=20, method="inclusive")[18] / 1e6,
                "teardown_median_ms": statistics.median(samples["teardown_ns"]) / 1e6,
                "rss_median_mib": statistics.median(samples["maxrss_kib"]) / 1024}
        summary.append(record)
        (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(mode, "allocation speedup", record["median_paired_allocation_speedup"], flush=True)
    (args.out / "raw.json").write_text(json.dumps(rows, indent=2) + "\n")
    state_after = cgroup_preflight(0)
    product_after = verify_product_build("baseline", args.baseline_product, args.out, "after")
    metadata["cgroup_after"] = state_after
    metadata["source_build_after"] = product_after
    metadata["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (args.out / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if (product_after != build or
            sha256(Path(__file__)) != metadata["runner_sha256"] or
            any(sha256(path) != hashes[name] for name, path in
                (("compiler", args.clang), ("fixture", fixture),
                 ("baseline_header", baseline_header), ("candidate_header", candidate_header))) or
            sha256(args.objdump) != metadata["codegen_tools_sha256"]["objdump"] or
            sha256(args.nm) != metadata["codegen_tools_sha256"]["nm"] or
            any(sha256(Path(record["path"])) != record["sha256"]
                for build_record in builds.values() for record in build_record["codegen"].values()) or
            any(sha256(binaries[name]) != builds[name]["binary_sha256"] for name in binaries) or
            any(counters(state_before, "memory.events")[name] !=
                counters(state_after, "memory.events")[name] for name in ("oom", "oom_kill")) or
            any(counters(state_before, "cpu.stat").get(name) !=
                counters(state_after, "cpu.stat").get(name)
                for name in ("nr_throttled", "throttled_usec"))):
        raise RuntimeError("GC layout A/B invalid: source, binary, OOM, or throttling drift")
    print("PASS source-bound native GC allocation-layout A/B; reclamation remains untested")


if __name__ == "__main__":
    main()
