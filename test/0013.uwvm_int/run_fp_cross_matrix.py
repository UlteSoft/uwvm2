#!/usr/bin/env python3
"""Run independent FP regressions with explicitly configured cross compilers/runners.

The JSON config is a list of {name, cxx: [...], run: [...], flags: [...],
optimizations: ["O0", "O3"], suites: ["scalar", "boundary", "simd"]} objects.
Use a matching SDK/ABI and QEMU CPU; a build failure is never counted as a pass.
ROS configs must omit the removed SIMD suite. This tests actual opfuncs and
guards, not a full runtime/JIT process; use the production-entry tests as well.
All inputs/results for transport tests stay in integer bits. O0 is deliberate:
optimized inlining can hide the native FP ABI that quietly changes an sNaN.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import time

SUITES = {
    "scalar": ["0013.uwvm_int/uwvm_int_fp_" + name + ".cc"
               for name in ("bits", "fused_bits", "parser_bits", "provider_bits", "rounding", "nan_encoding")],
    "simd": ["0013.uwvm_int/uwvm_int_simd_" + name + ".cc" for name in ("fp_bits", "nan_encoding")],
    "tail": ["0013.uwvm_int/uwvm_int_fp_tail_chain.cc"],
    # Opt in only with a Linux SDK: the clock setter is instantiated, never run.
    "libc": ["0017.runtime/linux_rv32_libc_fallback.cc"],
    "boundary": ["0017.runtime/" + name + ".cc" for name in
                 ("llvm_wasm_fp_environment", "wasm_fp_control", "wasm_fp_arch_environment", "wasm_fp_fixed_contract", "strict_float")],
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--config", type=Path, required=True)
parser.add_argument("--build-dir", type=Path, required=True)
parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument("--only", help="Comma-separated configured target names")
parser.add_argument("--jobs", type=int, default=4)
args = parser.parse_args()
if args.jobs < 1:
    parser.error("--jobs must be positive")
repo, build = args.source_root.resolve(), args.build_dir.resolve()
build.mkdir(parents=True, exist_ok=True)
targets = json.loads(args.config.read_text())
names = [target["name"] for target in targets]
if len(names) != len(set(names)):
    parser.error("Duplicate target names")
if args.only:
    selected = set(args.only.split(","))
    if not selected <= set(names):
        parser.error("Unknown target in --only")
    targets = [target for target in targets if target["name"] in selected]
jobs = []
for target in targets:
    if not target["cxx"] or not isinstance(target["cxx"], list) or not isinstance(target["run"], list):
        parser.error("cxx and run must be argument arrays")
    for suite in target.get("suites", ["scalar", "boundary"]):
        if suite not in SUITES:
            parser.error("Unknown suite: " + suite)
        for source in SUITES[suite]:
            if not (repo / "test" / source).is_file():
                parser.error("Requested source is absent (check ROS suite selection): " + source)
            for optimization in target.get("optimizations", ["O0", "O3"]):
                if optimization not in ("O0", "O1", "O2", "O3", "Os", "Oz"):
                    parser.error("Invalid optimization: " + optimization)
                jobs.append((target, source, optimization, suite))
if not jobs:
    parser.error("No test configurations selected")

def execute(job):
    target, source, optimization, suite = job
    label = target["name"] + "-" + optimization + "-" + Path(source).stem
    if "/" in label or "\\" in label or label.startswith("."):
        raise ValueError("Unsafe target label")
    output = build / label
    command = [*target["cxx"], "-std=c++20" if suite == "boundary" else "-std=c++26", "-" + optimization,
               "-ffp-contract=off", *target.get("flags", [])]
    if suite != "boundary":
        # Match production arithmetic assumptions without enabling fast-math:
        # Wasm entry establishes RN-even; NaNs/subnormals still remain meaningful.
        command += ["-fno-math-errno", "-fno-trapping-math", "-fno-rounding-math",
                    "-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS", "-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS",
                    "-DFAST_IO_DISABLE_FLOATING_POINT", "-DUWVM_USE_UWVM_INT", "-DUWVM_DISABLE_JIT"]
    for include in ("src", "third-parties/fast_io/include", "third-parties/bizwen/include", "third-parties/boost_unordered/include"):
        command += ["-I", str(repo / include)]
    command += [str(repo / "test" / source), "-o", str(output), "-lm", *target.get("ldflags", [])]
    run_command = [*target["run"], str(output)]
    record = {"target": target["name"], "suite": suite, "source": source, "optimization": optimization,
              "build_command": command, "run_command": run_command, "source_root": str(repo)}
    environment = {**os.environ, **target.get("env", {})}
    start = time.monotonic()
    for phase, invocation, timeout in (("build", command, 600), ("run", run_command, 300)):
        try:
            result = subprocess.run(invocation, capture_output=True, text=True, env=environment, timeout=timeout)
            text = result.stdout + result.stderr
            record[phase] = result.returncode
        except subprocess.TimeoutExpired as error:
            text = str(error)
            record[phase] = "timeout"
        except OSError as error:
            text = str(error)
            record[phase] = "unavailable"
        (build / (label + "." + phase + ".log")).write_text(text)
        if record[phase] != 0:
            record["failed_phase"] = phase
            record["diagnostic"] = text[-4000:]
            break
    record["seconds"] = time.monotonic() - start
    (build / (label + ".json")).write_text(json.dumps(record, indent=2))
    passed = record.get("build") == 0 and record.get("run") == 0
    print(label + (": PASS" if passed else ": FAIL " + record.get("failed_phase", "")), flush=True)
    return record

with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
    results = list(pool.map(execute, jobs))
(build / "results.json").write_text(json.dumps(results, indent=2))
failed = [record for record in results if record.get("build") != 0 or record.get("run") != 0]
print(f"{len(results) - len(failed)}/{len(results)} passed; artifacts: {build}", flush=True)
raise SystemExit(bool(failed))
