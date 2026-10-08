#!/usr/bin/env python3
"""Compare executable Core 3 exnref storage/payload cases with Wasmtime.

Run this only inside the limited Linux test cgroup. The fixtures exercise
catch_ref, throw_ref, exnref globals, GC reference payloads, and bottom heaps.
"""

import argparse
import json
from pathlib import Path
import resource
import subprocess


CASES = {
    "exnref_local_global_execution": ("exceptions",),
    "exnref_struct_payload_execution": ("exceptions", "gc"),
    "exnref_noexn_global_execution": ("exceptions",),
    "exnref_select_drop_execution": ("exceptions",),
    "gc_none_global_execution": ("gc",),
}
FEATURE_FLAG = {"exceptions": "-WFE-exceptions", "gc": "-WFE-gc"}
UNRELATED_DISABLED = {
    ("exceptions",): ("-WFD-gc", "-WFD-function-references"),
    ("gc",): ("-WFD-exceptions", "-WFD-function-references"),
    ("exceptions", "gc"): ("-WFD-function-references",),
}


def run(args, timeout=20):
    try:
        return subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"timed out: {args}") from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--mode", action="append", default=[])
    parser.add_argument("--compiler", choices=("int", "llvm-jit"), default="int")
    parser.add_argument("--combine-matrix", action="store_true",
                        help="run each interpreter combine level with delay-local on/off")
    parser.add_argument("--ros", action="store_true", help="Use ROS full-only -Rint/-Raot compiler options")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    modes = args.mode or ["full"]
    if args.ros and any(mode != "full" for mode in modes):
        parser.error("ROS supports only the full compilation mode")
    if args.combine_matrix and args.compiler != "int":
        parser.error("--combine-matrix requires the interpreter")
    def compiler_flags(mode):
        if args.ros:
            return ["-Rint"] if args.compiler == "int" else ["-Raot"]
        return ["-Rcc", "int" if args.compiler == "int" else "jit", "-Rcm", mode]
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []

    for name, features in CASES.items():
        wat = root / "test/0017.runtime/fixtures" / f"{name}.wat"
        wasm = args.out / f"{name}.wasm"
        parsed = run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)])
        if parsed.returncode:
            raise RuntimeError(f"{name}: wasm-tools parse failed: {parsed.stderr}")
        oracle = run([str(args.wasmtime), "-C", "cache=n", "-W", "exceptions=y",
                      "-W", "gc=y", str(wasm)])
        rows.append(dict(case=name, engine="wasmtime", status=oracle.returncode,
                         output=oracle.stdout + oracle.stderr))
        if oracle.returncode:
            raise RuntimeError(f"{name}: Wasmtime oracle failed: {oracle.stderr}")

        for mode in modes:
            enabled = [FEATURE_FLAG[feature] for feature in features]
            enabled.extend(UNRELATED_DISABLED[features])
            product = run([str(args.uwvm), *compiler_flags(mode),
                           *enabled, "--run", str(wasm)])
            rows.append(dict(case=name, engine=f"uwvm-{args.compiler}-{mode}", status=product.returncode,
                             output=product.stdout + product.stderr))
            if product.returncode:
                raise RuntimeError(f"{name}/{args.compiler}/{mode}: uwvm failed ({product.returncode}): "
                                   f"{product.stdout}{product.stderr}")

            for disabled_feature in features:
                other_flags = [FEATURE_FLAG[feature] for feature in features if feature != disabled_feature]
                disabled = run([str(args.uwvm), *compiler_flags(mode),
                                *other_flags, "--run", str(wasm)])
                rows.append(dict(case=name, engine=f"uwvm-{args.compiler}-{mode}-{disabled_feature}-off",
                                 status=disabled.returncode,
                                 output=disabled.stdout + disabled.stderr))
                if disabled.returncode == 0:
                    raise RuntimeError(f"{name}/{mode}: {disabled_feature} feature-off accepted the module")

            no_references = run([str(args.uwvm), *compiler_flags(mode),
                                 *enabled, "-WFD-reference-types",
                                 "--run", str(wasm)])
            rows.append(dict(case=name, engine=f"uwvm-{args.compiler}-{mode}-reference-types-off",
                             status=no_references.returncode,
                             output=no_references.stdout + no_references.stderr))
            if no_references.returncode == 0:
                raise RuntimeError(f"{name}/{mode}: reference-types feature-off accepted the module")

        if args.combine_matrix:
            enabled = [FEATURE_FLAG[feature] for feature in features]
            enabled.extend(UNRELATED_DISABLED[features])
            for mode in modes:
                for level in ("disable", "soft", "heavy", "extra"):
                    for no_delay in (False, True):
                        tuning = ["-Rint-op-conbine-level", level]
                        if no_delay:
                            tuning.append("-Rint-no-delay-local")
                        label = f"{name}/int/{mode}/{level}/" + ("no-delay" if no_delay else "delay")
                        product = run([str(args.uwvm), *compiler_flags(mode), *tuning,
                                       *enabled, "--run", str(wasm)])
                        rows.append(dict(case=name, engine=label, status=product.returncode,
                                         output=product.stdout + product.stderr))
                        if product.returncode:
                            raise RuntimeError(f"{label}: uwvm failed ({product.returncode}): "
                                               f"{product.stdout}{product.stderr}")

    (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
    print(f"PASS Core 3 executable exnref: {len(rows)} checks")


if __name__ == "__main__":
    main()
