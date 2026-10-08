#!/usr/bin/env python3
"""Bind current O3 products and original fixtures without launching a VM.

This is a plan producer, not a performance runner or a quiet-window guard.
The keeper must independently close source fingerprints, validate fixtures,
and obtain the existing process-ownership and thermal receipts before timing.
"""

import argparse
import hashlib
import json
from pathlib import Path


EXPERIMENTS = (
    "COMPACT_NUMERIC", "MANAGED_NUMERIC_PAGE", "SEALED_COMPACT_CURSOR",
    "SEALED_LOCAL_TABLE", "PENDING_NUMERIC_FUSED_CATCH", "PACKED_NUMERIC_ARRAYS",
)
WT49_SHA = "c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94"
FIXTURES = {
    "gc-allocation-ring-2000000": {
        "family": "gc", "iterations": 2000000, "checksum": 750095765,
        "sha256": "f1b1637cf4b7c1380ba6320065cf534818c6c154d8eabe63a7b2eafd8013c316"},
    "gc-allocation-ring-16000000": {
        "family": "gc", "iterations": 16000000, "checksum": 493211925,
        "sha256": "66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e"},
    "plain_normal": {
        "family": "eh", "iterations": 200000000, "checksum": 1231817216,
        "catches": 0,
        "sha256": "03c56518a1d2ed9d0149791e3a241ddff773395e51bd48aeb67a9160d5bdba01"},
    "eh_normal": {
        "family": "eh", "iterations": 200000000, "checksum": 1231817216,
        "catches": 0,
        "sha256": "f2b9df696835bdd0f759885b77862e4a3de56b506a96f3edc41e54b20a63be8a"},
    "eh_throws": {
        "family": "eh", "iterations": 8000000, "checksum": 2464256,
        "catches": 500000,
        "sha256": "560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560"},
    "memory64-random-store-5000000": {
        "family": "memory64", "iterations": 5000000,
        "sha256": "5ed7090a0cbafe866be3eae0bbfc76ceeed6b1cb607dc0bc6392f4a829269fd9"},
    "memory64-random-store-50000000": {
        "family": "memory64", "iterations": 50000000,
        "sha256": "322641f90c7b80225e942031988918da63e925e725a72dd2dd1f6c4b28cb36cc"},
}
# These are the independently assembled archived siblings of the original
# 1024-root ring. Selecting them binds bytes; it does not certify a new product
# execution or waive the keeper's current-build semantic receipts.
LONG_GC_FIXTURES = {
    "gc-allocation-ring-128000000": {
        "family": "gc", "iterations": 128000000, "checksum": 2259414293,
        "sha256": "5eb8cb2ab852164f339636ac21fb7b0284b3d6dee0236fe9cd11e2f04e27fd76"},
    "gc-allocation-ring-512000000": {
        "family": "gc", "iterations": 512000000, "checksum": 1687964949,
        "sha256": "4344aec55a95e51d86433ab6d57305528866f18583947a1def97e81b69d26d79"},
}
ALL_FIXTURES = FIXTURES | LONG_GC_FIXTURES


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def build_binding(directory, expected_id):
    directory = directory.resolve(strict=True)
    manifest_path = directory / "build.json"
    manifest = json.loads(manifest_path.read_text())
    require(manifest.get("passed") is True, "O3 product build did not pass")
    require(manifest.get("source_id") == expected_id == manifest.get("source_id_after"),
            "requested source identity did not close during the build")
    require(manifest.get("fresh_runtime") is True and
            manifest.get("old_object_reused") is False,
            "this current-build lane requires a genuinely fresh runtime object")
    commands = {}
    for kind in ("runtime", "cli"):
        command_path = directory / (kind + ".command.json")
        argv = json.loads(command_path.read_text())
        require(isinstance(argv, list) and all(isinstance(arg, str) for arg in argv),
                "build command sidecar must contain an argv array")
        require("-O3" in argv, "both translation units must be compiled at O3")
        actual = sorted(arg for arg in argv if arg.startswith("-DUWVM_EXPERIMENTAL_"))
        expected = sorted("-DUWVM_EXPERIMENTAL_" + item + "=1" for item in EXPERIMENTS)
        require(actual == expected, "current candidate must contain exactly six reviewed experiments")
        require(any(arg.startswith("-DUWVM2_BUILD_SOURCE_ID=") and expected_id in arg
                    for arg in argv), "translation unit does not embed the requested source ID")
        commands[kind] = {"path": str(command_path), "sha256": digest(command_path),
                          "argv": argv}
    runtime_argv = commands["runtime"]["argv"]
    cli_argv = commands["cli"]["argv"]
    require(runtime_argv.count("-o") == 1 and cli_argv.count("-o") == 1,
            "ambiguous actual build output")
    runtime = Path(runtime_argv[runtime_argv.index("-o") + 1]).resolve(strict=True)
    binary = Path(cli_argv[cli_argv.index("-o") + 1]).resolve(strict=True)
    require(runtime.parent == directory and binary.parent == directory and
            cli_argv.count(str(runtime)) == 1,
            "CLI must link exactly its own fresh runtime object")
    require(digest(binary) == manifest["binary_sha256"], "product ELF differs from build receipt")
    for kind, argv in (("runtime", runtime_argv), ("cli", cli_argv)):
        require(not any(arg.startswith("-DUWVM_ENABLE_UWVM_INT_") and arg not in argv
                        for arg in commands["runtime" if kind == "cli" else "cli"]["argv"]),
                "combine/delay compilation options differ between RT and CLI")
    return {"source": manifest["source"], "source_id": expected_id,
            "build_json": str(manifest_path), "build_json_sha256": digest(manifest_path),
            "runtime": str(runtime), "runtime_sha256": digest(runtime),
            "binary": str(binary), "binary_sha256": digest(binary), "commands": commands}


def product_command(product, binary, policy, name, fixture, dispatch):
    mode = (["-Rcc", "jit", "-Rcm", "full"] if product == "ordinary" else ["-Raot"])
    flags = (["-WFE-gc"] if ALL_FIXTURES[name]["family"] == "gc" else
             ["-WFE-memory64"] if ALL_FIXTURES[name]["family"] == "memory64" else
             ["-WFD-exceptions"] if name == "plain_normal" else ["-WFE-exceptions"])
    return ["taskset", "-c", "0", binary, *mode, "-Rllvm-full-policy", "pb-o3",
            "-Rllvm-call-stack", policy, "-Rllvm-exception-dispatch", dispatch,
            "-Rllvm-cache-path", "disable", "-Rct", "0", *flags, "--run", fixture]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary-build", type=Path, required=True)
    parser.add_argument("--ros-build", type=Path, required=True)
    parser.add_argument("--ordinary-source-id", required=True)
    parser.add_argument("--ros-source-id", required=True)
    parser.add_argument("--gc-fixture-dir", type=Path, required=True)
    parser.add_argument("--long-gc-fixture-dir", type=Path,
                        help="bind archived 128M/512M siblings as the explicit GC timing pair")
    parser.add_argument("--eh-fixture-dir", type=Path, required=True)
    parser.add_argument("--memory-fixture-dir", type=Path)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--include-native-eh", action="store_true",
                        help="add explicit native-unwind EH dispatch controls")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    require(not args.out.exists(), "measurement plans must have a new output path")
    products = {"ordinary": build_binding(args.ordinary_build, args.ordinary_source_id),
                "ros": build_binding(args.ros_build, args.ros_source_id)}
    reference = args.wasmtime.resolve(strict=True)
    require(digest(reference) == WT49_SHA, "reference must be the actual pinned Wasmtime 49.0.1 ELF")
    fixtures, commands = {}, []
    definitions = ALL_FIXTURES if args.long_gc_fixture_dir is not None else FIXTURES
    for name, definition in definitions.items():
        family = definition["family"]
        if family == "memory64" and args.memory_fixture_dir is None:
            continue
        folder = (args.long_gc_fixture_dir if name in LONG_GC_FIXTURES else
                  args.gc_fixture_dir if family == "gc" else
                  args.eh_fixture_dir if family == "eh" else args.memory_fixture_dir)
        fixture = (folder / (name + ".wasm")).resolve(strict=True)
        require(digest(fixture) == definition["sha256"], "original Wasm changed: " + name)
        fixtures[name] = dict(definition, path=str(fixture))
        for product, binding in products.items():
            for policy in ("instruction", "unwind"):
                for dispatch in (("auto", "native-unwind") if
                                 args.include_native_eh and family == "eh" else ("auto",)):
                    argv = product_command(product, binding["binary"], policy, name, str(fixture), dispatch)
                    commands.append({"fixture": name, "profile": product + "/" + policy + "/" + dispatch,
                                     "argv": argv,
                                     "diagnostic_argv": [*argv[:-2], "-Rclog", "err", *argv[-2:]]})
        features = ("all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,"
                    "function-references=y,gc=y" if family == "gc" else
                    "memory64=y" if family == "memory64" else
                    "exceptions=n" if name == "plain_normal" else "exceptions=y")
        commands.append({"fixture": name, "profile": "wasmtime49/copying" if family == "gc" else "wasmtime49/native",
                         "argv": ["taskset", "-c", "0", str(reference), "-C",
                                  "cache=n,collector=copying" if family == "gc" else "cache=n",
                                  "-W", features, str(fixture)]})
    plan = {"schema": "uwvm-current-pcore-gc-eh-plan-v1", "execute_ready": False,
            "runner_launched": False, "source_fingerprint_after_build_checked": False,
            "products": products, "wasmtime": {"path": str(reference), "sha256": WT49_SHA},
            "fixtures": fixtures, "commands": commands,
            "required_external_gate": "unchanged keeper cgroup/process-ownership/thermal guard",
            "timing": {"guest_cpu": 0, "controller_cpu": 16, "pairs": 9,
                       "gc_fixture_pair": list(LONG_GC_FIXTURES) if args.long_gc_fixture_dir is not None
                       else ["gc-allocation-ring-2000000", "gc-allocation-ring-16000000"],
                       "engine_order": "rotate by pair//2 and reverse on odd pairs",
                       "low_high_order": "AB on even pairs; BA on odd pairs",
                       "maximum_start_millicelsius": 75000, "maximum_peak_millicelsius": 90000,
                       "maximum_pair_start_delta_millicelsius": 5000,
                       "maximum_pair_median_frequency_relative_delta": 0.1,
                       "minimum_whole_process_sample_ns": 100000000,
                       "memory_max_bytes": 64 << 30, "swap_max_bytes": 0},
            "caveats": ["All guest checks, current source closure and quiet receipts remain external prerequisites.",
                        "The original 500K-catch case may be too short; retain its result and use an independently checked longer sibling before ranking.",
                        "Optional long GC siblings require current-product cold semantic receipts before timing; original 2M/16M commands remain anchors.",
                        "GC reclamation must come from actual cold receipts and RSS gates, never from experiment macros.",
                        "Nine process samples do not qualify a service p99."]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(plan, indent=2, allow_nan=False) + "\n")
    print("Bound current products and original fixtures; no VM or benchmark was launched.")


if __name__ == "__main__":
    main()
