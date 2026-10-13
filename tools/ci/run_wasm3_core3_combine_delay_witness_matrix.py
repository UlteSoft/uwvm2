#!/usr/bin/env python3
"""Build all 4 x 3 interpreter configurations and execute Core 3 witnesses.

The existing strict fusion matrix checks two older C++ translation tests. A
single all-enabled VM can select four combine levels at runtime, but it cannot
distinguish the soft and heavy delay-local *compiled* paths. This separate
matrix rebuilds runtime and CLI with each exact macro set, then executes the
same self-checking Wasm bytes through every selected interpreter mode.

Run inside the project's 64 GiB, swap-free Linux cgroup. --config and --case
permit short checkpoints; a complete result requires all 12 builds and every
witness, and the script never labels a partial run as complete.
"""

import argparse
from collections import Counter
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shlex
import signal
import subprocess
import time

from requalify_wasm3_cli_from_provenance import (
    bind_source_paths, embedded_source_ids, output_index,
)
from wasm3_compile_fatal_policy import negative_classification, prove_compile_fatal_trap


COMBINE = {
    "none": (),
    "soft": ("UWVM_ENABLE_UWVM_INT_COMBINE_OPS",),
    "heavy": ("UWVM_ENABLE_UWVM_INT_COMBINE_OPS",
              "UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS"),
    "extra": ("UWVM_ENABLE_UWVM_INT_COMBINE_OPS",
              "UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS",
              "UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS"),
}
DELAY = {
    "none": (),
    "soft": ("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT",),
    "heavy": ("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT",
              "UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY"),
}
CONFIGS = tuple(f"{combine}-{delay}" for combine in COMBINE for delay in DELAY)
ALL_DEFINES = {f"-D{macro}" for values in (*COMBINE.values(), *DELAY.values())
               for macro in values}
CASES = (
    "call-ref-step", "gc-struct-heap-update", "gc-array-heap-update",
    "gc-cast", "memory64-random-store", "table64-indirect",
    "multi-memory-random-store", "extended-const-init", "tail-call-step",
    "eh-caught-step", "memory64-atomic-rmw",
    "memory32-atomic-wait-mismatch", "memory32-atomic-notify-empty",
    "memory64-atomic-wait-mismatch", "memory64-atomic-notify-empty",
    "relaxed-swizzle", "strict-swizzle",
)
ORDINARY_MODES = {
    "int-full": ("-Rcc", "int", "-Rcm", "full"),
    "int-lazy": ("-Rcc", "int", "-Rcm", "lazy"),
    "int-lazy-verified": ("-Rcc", "int", "-Rcm", "lazy+verification"),
}
ROS_MODES = {"int-full": ("-Rint",)}
WASMTIME_BASE = ("all-proposals=n", "bulk-memory=y", "multi-value=y",
                 "reference-types=y", "simd=y")
WASMTIME_FEATURES = {
    "gc": ("function-references=y", "gc=y"),
    "function-references": ("function-references=y",),
    "memory64": ("memory64=y",),
    "table64": ("memory64=y",),
    "multi-memory": ("multi-memory=y",),
    "extended-const": ("extended-const=y",),
    "tail-call": ("tail-call=y",),
    "threads": ("threads=y", "shared-memory=y"),
    "exceptions": ("exceptions=y",),
    "relaxed-simd": ("relaxed-simd=y",),
    "simd": (),
}
# Stop this runner well before the 64 GiB hard limit. The cgroup also holds
# compiler page cache and any qualified tools, so leave at least 12 GiB slack.
MAX_CGROUP_BYTES = 56_000_000_000


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path: Path, value: object) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def cgroup() -> dict[str, str]:
    root = Path("/sys/fs/cgroup")
    data = {name: (root / name).read_text().strip() for name in
            ("memory.max", "memory.swap.max", "memory.current", "memory.peak",
             "memory.events", "cpuset.cpus.effective")}
    if (data["memory.max"], data["memory.swap.max"],
            data["cpuset.cpus.effective"]) != (
                "68719476736", "0", "0,2,4,6,16-31"):
        raise RuntimeError(f"wrong 64 GiB/4P+16E Linux cgroup: {data}")
    return data


def oom_counts(state: dict[str, str]) -> Counter[str]:
    return Counter({name: int(value) for name, value in
                    (line.split() for line in state["memory.events"].splitlines())})


def fingerprint(source: Path, manifest: Path) -> str:
    tool = source / "tools/ci/wasm3_source_fingerprint.py"
    return subprocess.check_output(["python3", str(tool), str(source),
                                    str(manifest)], text=True).strip()


def checked_run(command: list[str], log: Path, cwd: Path, timeout: int) -> dict:
    """Keep a bounded log and stop only this command's process group on a cap."""
    start = time.monotonic()
    with log.open("wb") as stream:
        process = subprocess.Popen(command, cwd=cwd, stdout=stream,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        reason = None
        while process.poll() is None:
            if time.monotonic() - start >= timeout:
                reason = "timeout"
            elif int(Path("/sys/fs/cgroup/memory.current").read_text()) >= MAX_CGROUP_BYTES:
                reason = "cgroup-memory-cap"
            if reason is not None:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                break
            time.sleep(0.2)
        exit_code = process.wait()
    return {"exit": exit_code, "reason": reason, "seconds": time.monotonic() - start,
            "log": str(log), "log_sha256": digest(log)}


def load_generator(path: Path):
    spec = importlib.util.spec_from_file_location("core3_witness_generator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load fixture generator: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not set(CASES) <= set(module.CASES):
        raise RuntimeError("fixture generator lacks a selected Core 3 witness")
    return module


def feature_flags(features: list[str]) -> list[str]:
    return ["-WFE-" + feature for feature in features]


def oracle_flags(features: list[str]) -> list[str]:
    flags = dict.fromkeys((*WASMTIME_BASE,
                           *(flag for feature in features
                             for flag in WASMTIME_FEATURES[feature])))
    return ["-W", ",".join(flags)]


def checked_result(command: list[str], label: str, logs: Path, cwd: Path,
                   timeout: int, success: bool, diagnostic: str | None = None,
                   debugger: Path | None = None) -> dict:
    result = checked_run(command, logs / f"{label}.log", cwd, timeout)
    output = Path(result["log"]).read_text(errors="replace").lower()
    # A diagnostic alone never makes a signal acceptable. Optional classification
    # replays this exact executable/argv under GDB and proves the actual fatal PC.
    proof = None
    if (not success and result["reason"] is None and debugger is not None and
            result["exit"] == -4 and diagnostic is not None and diagnostic.lower() in output):
        proof = prove_compile_fatal_trap(
            command, diagnostic, debugger, logs / f"{label}.fatal-pc.log",
            binary_sha256=digest(Path(command[0])), timeout=45)
    if success:
        passed = result["reason"] is None and result["exit"] == 0
        classification = None
    else:
        classification, passed = negative_classification(
            result["exit"], diagnostic is None or diagnostic.lower() in output, proof)
        passed = passed and result["reason"] is None
    return {**result, "command": command, "passed": passed,
            "expected_success": success, "required_diagnostic": diagnostic,
            "negative_classification": classification, "fatal_trap_proof": proof,
            "strict_regular_diagnostic_exit_passed": (not success and result["reason"] is None and
                                                      result["exit"] > 0 and diagnostic is not None and
                                                      diagnostic.lower() in output)}


def normal_build(source: Path, manifest: Path, source_id: str) -> tuple[dict, list[str], list[str]]:
    data = json.loads(manifest.read_text())
    runtime = Path(data["runtime_command"][-1]).resolve(strict=True)
    product = Path(data["cli_command"][-1]).resolve(strict=True)
    if (data["source_id"] != source_id or
            Path(data["source"]).resolve(strict=True) != source or
            digest(runtime) != data["runtime_object_sha256"] or
            digest(product) != data["binary_sha256"] or
            embedded_source_ids(product) != {source_id}):
        raise RuntimeError("normal O3 build does not match exact source and product")
    runtime_command = list(data["runtime_command"])
    cli_command = list(data["cli_command"])
    prior = bind_source_paths(runtime_command, cli_command, source)
    if prior is not None and Path(prior).resolve(strict=True) != source:
        raise RuntimeError("normal O3 command refers to a different source tree")
    if "-O3" not in runtime_command or "-O3" not in cli_command:
        raise RuntimeError("normal product compiler commands are not O3")
    for command in (runtime_command, cli_command):
        if "-fstack-clash-protection" not in command or "-mstack-probe-size=4096" not in command:
            raise RuntimeError("normal x86-64 product command lacks Xmake's host stack-probe flags")
    if "-c" not in runtime_command or "-c" in cli_command:
        raise RuntimeError("expected runtime object and CLI link compiler commands")
    if not Path(runtime_command[0]).is_file() or not Path(cli_command[0]).is_file():
        raise RuntimeError("qualified compiler executable is missing")
    return data, runtime_command, cli_command


def config_commands(runtime: list[str], cli: list[str], combine: str,
                    delay: str, directory: Path) -> tuple[list[str], list[str]]:
    definitions = [f"-D{macro}" for macro in (*COMBINE[combine], *DELAY[delay])]
    base_runtime = [item for item in runtime if item not in ALL_DEFINES]
    base_cli = [item for item in cli if item not in ALL_DEFINES]
    old_object = base_runtime[output_index(base_runtime)]
    if base_cli.count(old_object) != 1:
        raise RuntimeError("CLI must link the normal runtime object once")
    runtime_command = [base_runtime[0], *definitions, *base_runtime[1:]]
    cli_command = [base_cli[0], *definitions, *base_cli[1:]]
    runtime_command[output_index(runtime_command)] = str(directory / "runtime.o")
    cli_command[cli_command.index(old_object)] = str(directory / "runtime.o")
    cli_command[output_index(cli_command)] = str(directory / "uwvm")
    for command in (runtime_command, cli_command):
        actual = {item for item in command if item in ALL_DEFINES}
        if actual != set(definitions):
            raise RuntimeError(f"wrong compile-time interpreter configuration: {actual}")
    return runtime_command, cli_command


def ensure_build(source: Path, source_id: str, base_runtime: list[str],
                 base_cli: list[str], output: Path, config: str) -> dict:
    combine, delay = config.split("-", 1)
    directory = output / "builds" / config
    directory.mkdir(parents=True, exist_ok=True)
    runtime, cli = config_commands(base_runtime, base_cli, combine, delay, directory)
    manifest = directory / "build.json"
    if manifest.exists():
        value = json.loads(manifest.read_text())
        if (value.get("source_id") != source_id or value.get("runtime_command") != runtime
                or value.get("cli_command") != cli):
            raise RuntimeError(f"{config}: existing build manifest differs")
        if value.get("complete"):
            if (digest(directory / "runtime.o") != value["runtime_object_sha256"] or
                    digest(directory / "uwvm") != value["binary_sha256"] or
                    embedded_source_ids(directory / "uwvm") != {source_id}):
                raise RuntimeError(f"{config}: completed build changed")
            return value
    else:
        value = {"source_id": source_id, "source": str(source), "configuration": config,
                 "combine": combine, "delay": delay,
                 "compile_definitions": [item for item in runtime if item in ALL_DEFINES],
                 "runtime_command": runtime, "cli_command": cli, "complete": False}
        save(manifest, value)
    for label, command in (("runtime", runtime), ("cli", cli)):
        row = checked_result(command, label, directory, source, 3600, True)
        value[label + "_build"] = row
        save(manifest, value)
        if not row["passed"]:
            raise RuntimeError(f"{config}: {label} compilation failed; see {row['log']}")
    value["runtime_object_sha256"] = digest(directory / "runtime.o")
    value["binary_sha256"] = digest(directory / "uwvm")
    value["binary_embedded_source_ids"] = sorted(embedded_source_ids(directory / "uwvm"))
    if value["binary_embedded_source_ids"] != [source_id]:
        raise RuntimeError(f"{config}: binary embeds an incorrect source ID")
    value["complete"] = True
    save(manifest, value)
    return value


def ensure_fixture(generator, case: str, count: int, fixture_dir: Path,
                   wasm_tools: Path, wasmtime: Path, source: Path) -> dict:
    fixture_dir.mkdir(parents=True, exist_ok=True)
    wat = fixture_dir / f"{case}-{count}.wat"
    wasm = wat.with_suffix(".wasm")
    manifest = fixture_dir / f"{case}-{count}.json"
    contents, features = generator.module(case, count)
    if wat.exists() and wat.read_text() != contents:
        raise RuntimeError(f"{case}: existing WAT differs from generator")
    wat.write_text(contents)
    if manifest.exists():
        data = json.loads(manifest.read_text())
        if (data["wat_sha256"] != digest(wat) or
                data["wasm_sha256"] != digest(wasm) or
                data["features"] != features or
                data["wasm_tools_sha256"] != digest(wasm_tools) or
                data["wasmtime_sha256"] != digest(wasmtime)):
            raise RuntimeError(f"{case}: fixture or oracle changed during resume")
        return data
    for phase, command in (
        ("parse", [str(wasm_tools), "parse", str(wat), "-o", str(wasm)]),
        ("validate", [str(wasm_tools), "validate", "--features", "all", str(wasm)]),
        ("wasmtime49", [str(wasmtime), "-C", "cache=n", *oracle_flags(features), str(wasm)]),
    ):
        row = checked_result(command, f"{case}-{count}-{phase}", fixture_dir,
                             source, 120, True)
        if not row["passed"]:
            raise RuntimeError(f"{case}: {phase} failed; see {row['log']}")
    data = {"case": case, "count": count, "features": features,
            "expected": generator.expected_for_case(case, count),
            "wat": str(wat), "wat_sha256": digest(wat),
            "wasm": str(wasm), "wasm_sha256": digest(wasm),
            "wasm_tools_sha256": digest(wasm_tools),
            "wasmtime_sha256": digest(wasmtime)}
    if case == "call-ref-step":
        data["dynamic_target_trace"] = generator.call_ref_trace(count)
        if not all(data["dynamic_target_trace"][key] > 0 for key in
                   ("first_target_calls", "second_target_calls")):
            raise RuntimeError("call_ref fixture does not exercise both targets")
    if case == "relaxed-swizzle":
        data["dynamic_swizzle_trace"] = generator.swizzle_trace(count)
        if data["dynamic_swizzle_trace"]["distinct_indices_in_prefix"] != 16:
            raise RuntimeError("SIMD fixture does not exercise all indices")
    save(manifest, data)
    return data


def run_product(source: Path, source_id: str, build: dict, config: str,
                case: str, fixture: dict, modes: dict, output: Path,
                debugger: Path | None = None) -> None:
    binary = Path(build["cli_command"][-1])
    rows_path = output / "runs.json"
    rows = json.loads(rows_path.read_text()) if rows_path.exists() else {}
    combine = build["combine"]
    tuning = (["-Rint-op-conbine-level", "disable" if combine == "none" else combine]
              if combine != "none" else [])
    flags = feature_flags(fixture["features"])
    for mode, backend in modes.items():
        key = f"{config}/{case}/{mode}/enabled"
        command = [str(binary), *backend, *tuning, *flags, "--run", fixture["wasm"]]
        if key not in rows:
            row = checked_result(command, key.replace("/", "-"), output / "logs",
                                 source, 120, True)
            rows[key] = {**row, "source_id": source_id, "binary_sha256": digest(binary),
                         "wasm_sha256": fixture["wasm_sha256"]}
            save(rows_path, rows)
            if not row["passed"]:
                raise RuntimeError(f"Core 3 witness failed: {key}; see {row['log']}")
        elif (rows[key]["command"] != command or
              rows[key]["binary_sha256"] != digest(binary) or
              rows[key]["wasm_sha256"] != fixture["wasm_sha256"] or
              not rows[key]["passed"]):
            raise RuntimeError(f"{key}: saved execution differs")
    # A feature gate is validator policy, so one int-full run per compiled
    # configuration proves each independently switchable proposal is rejected.
    for feature in dict.fromkeys(fixture["features"]):
        key = f"{config}/{case}/int-full/{feature}-disabled"
        off_flags = ["-WFD-" + feature if enabled == feature else "-WFE-" + enabled
                     for enabled in fixture["features"]]
        command = [str(binary), *modes["int-full"], *tuning, *off_flags,
                   "--run", fixture["wasm"]]
        if key not in rows:
            row = checked_result(command, key.replace("/", "-"), output / "logs",
                                 source, 120, False,
                                 # SIMD is already a Wasm 1.1 baseline feature;
                                 # its older validator says "requires simd"
                                 # before reaching relaxed_swizzle. Other
                                 # proposal gates name their exact CLI switch.
                                 "requires simd" if feature == "simd" else
                                 "wasm-feature-enable-" + feature, debugger)
            rows[key] = {**row, "source_id": source_id, "binary_sha256": digest(binary),
                         "wasm_sha256": fixture["wasm_sha256"]}
            save(rows_path, rows)
            if not row["passed"]:
                raise RuntimeError(f"Core 3 feature gate failed: {key}; see {row['log']}")
        elif (rows[key]["command"] != command or
              rows[key]["binary_sha256"] != digest(binary) or
              rows[key]["wasm_sha256"] != fixture["wasm_sha256"] or
              not rows[key]["passed"]):
            raise RuntimeError(f"{key}: saved execution differs")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--normal-build-json", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--count", type=int, default=8192)
    parser.add_argument("--config", action="append", choices=CONFIGS,
                        help="select a short build checkpoint; omit for all 12")
    parser.add_argument("--case", action="append", choices=CASES,
                        help="select a short witness checkpoint; omit for all 17")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--gdb", type=Path,
                        help="individually prove intentional compile-fatal SIGILL PCs; otherwise signals fail")
    args = parser.parse_args()
    if args.count < 8192:
        parser.error("count must be >=8192 to exercise both call_ref targets and all SIMD indices")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = cgroup()
    source = args.source.resolve(strict=True)
    normal_manifest = args.normal_build_json.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    wasmtime_version = subprocess.check_output([str(wasmtime), "--version"], text=True).strip()
    if not wasmtime_version.startswith("wasmtime 49.0.1 "):
        raise RuntimeError(f"Wasmtime 49.0.1 required: {wasmtime_version}")
    output = args.output.absolute()
    output.mkdir(parents=True, exist_ok=args.resume)
    (output / "logs").mkdir(exist_ok=True)
    source_id = fingerprint(source, output / "source-before.json")
    if source_id != args.expected_source_id:
        raise RuntimeError("source fingerprint differs from the explicitly requested candidate")
    debugger = args.gdb.resolve(strict=True) if args.gdb is not None else None
    normal, runtime, cli = normal_build(source, normal_manifest, source_id)
    generator_path = source / "benchmark/0004.wasm3-core/generate.py"
    generator = load_generator(generator_path)
    runner_sha = digest(Path(__file__))
    plan = {"source": str(source), "source_id": source_id,
            "normal_build_json": str(normal_manifest),
            "normal_build_json_sha256": digest(normal_manifest),
            "normal_binary_sha256": normal["binary_sha256"],
            "normal_runtime_object_sha256": normal["runtime_object_sha256"],
            "wasm_tools_sha256": digest(wasm_tools),
            "wasmtime_sha256": digest(wasmtime),
            "wasmtime_version": wasmtime_version,
            "generator_sha256": digest(generator_path), "runner_sha256": runner_sha,
            "negative_policy_sha256": digest(Path(__file__).with_name("wasm3_compile_fatal_policy.py")),
            "debugger": str(debugger) if debugger else None,
            "debugger_sha256": digest(debugger) if debugger else None,
            "count": args.count, "configurations": CONFIGS, "cases": CASES,
            "ros": args.ros, "cgroup_before": before}
    plan_path = output / "plan.json"
    if plan_path.exists():
        saved = json.loads(plan_path.read_text())
        if {key: value for key, value in saved.items() if key != "cgroup_before"} != {
                key: value for key, value in plan.items() if key != "cgroup_before"}:
            raise RuntimeError("resume plan differs from original source, tools or fixtures")
    else:
        save(plan_path, plan)
    selected_configs = tuple(dict.fromkeys(args.config or CONFIGS))
    selected_cases = tuple(dict.fromkeys(args.case or CASES))
    modes = ROS_MODES if args.ros else ORDINARY_MODES
    fixtures = {case: ensure_fixture(generator, case, args.count,
                                     output / "fixtures", wasm_tools, wasmtime, source)
                for case in selected_cases}
    for config in selected_configs:
        build = ensure_build(source, source_id, runtime, cli, output, config)
        for case in selected_cases:
            run_product(source, source_id, build, config, case, fixtures[case], modes, output, debugger)
        if fingerprint(source, output / f"source-after-{config}.json") != source_id:
            raise RuntimeError(f"source changed after {config}")
        if any(oom_counts(cgroup())[key] != oom_counts(before)[key]
               for key in ("oom", "oom_kill")):
            raise RuntimeError("cgroup recorded an OOM during Core 3 matrix")
        print(f"PASS {config}: {len(selected_cases)} self-checking Core 3 fixtures", flush=True)
    after_id = fingerprint(source, output / "source-after.json")
    after = cgroup()
    if after_id != source_id or any(oom_counts(after)[key] != oom_counts(before)[key]
                                 for key in ("oom", "oom_kill")):
        raise RuntimeError("source changed or cgroup OOM during Core 3 matrix")
    for path, expected_sha in (
        (normal_manifest, plan["normal_build_json_sha256"]),
        (Path(__file__), runner_sha),
        (generator_path, plan["generator_sha256"]),
        (wasm_tools, plan["wasm_tools_sha256"]),
        (wasmtime, plan["wasmtime_sha256"]),
        (Path(__file__).with_name("wasm3_compile_fatal_policy.py"), plan["negative_policy_sha256"]),
    ):
        if digest(path) != expected_sha:
            raise RuntimeError(f"matrix input changed during execution: {path}")
    if debugger is not None and digest(debugger) != plan["debugger_sha256"]:
        raise RuntimeError("debugger changed during execution")
    rows = json.loads((output / "runs.json").read_text())
    expected = {f"{config}/{case}/{mode}/enabled"
                for config in CONFIGS for case in CASES for mode in modes}
    for case in CASES:
        # The complete-coverage denominator is independent of a checkpoint's
        # selected cases. Unselected fixtures need no parse or product run here.
        _, features = generator.module(case, args.count)
        expected.update(f"{config}/{case}/int-full/{feature}-disabled"
                        for config in CONFIGS for feature in features)
    complete = (all((output / "builds" / config / "build.json").exists()
                    for config in CONFIGS)
                and set(rows) == expected)
    summary = {"passed": all(row["passed"] for row in rows.values()),
               "complete": complete, "scope": "ros" if args.ros else "ordinary",
               "source_id": source_id, "selected_configs": selected_configs,
               "selected_cases": selected_cases, "total_expected_configs": 12,
               "total_expected_cases": len(CASES), "completed_executions": len(rows),
               "expected_executions": len(expected), "runs_sha256": digest(output / "runs.json"),
               "plan_sha256": digest(plan_path),
               "source_before_sha256": digest(output / "source-before.json"),
               "source_after_sha256": digest(output / "source-after.json"),
               "cgroup_before": before, "cgroup_after": after}
    save(output / "summary.json", summary)
    print(f"Core 3 combine/delay witnesses: {len(rows)}/{len(expected)} "
          f"executions; complete={complete}", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
