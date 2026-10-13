#!/usr/bin/env python3
"""Keeper-only source-owned typed parsing before any initialization effect."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def kernel_text(path: Path, limit: int) -> str:
    with path.open("rb") as stream:
        contents = stream.read(limit + 1)
    if len(contents) > limit:
        raise RuntimeError("kernel observation exceeds the bounded probe")
    return contents.decode("ascii", errors="strict")


def clean_kernel_path(value: str) -> bool:
    return (value.startswith("/") and "\\" not in value
            and all(32 <= ord(c) < 127 for c in value)
            and not any(part in {".", ".."} for part in value.split("/")))


def bounded_cgroup() -> dict:
    # This is a cold self-check, not a launcher/guardian capability. The sole
    # keeper must additionally bind each subprocess PIDFD to the SAME actual
    # leaf, limits, fresh artifacts and reviewed literal argv before execution.
    if platform.system() != "Linux":
        raise RuntimeError("only the SSH Linux cgroup keeper may execute this component")
    memberships = [line.split(":", 2)[2]
                   for line in kernel_text(Path("/proc/self/cgroup"), 65536).splitlines()
                   if line.startswith("0::")]
    if len(memberships) != 1 or not clean_kernel_path(memberships[0]):
        raise RuntimeError("one unambiguous actual unified cgroup path required")
    membership = memberships[0]
    mount_roots = []
    for line in kernel_text(Path("/proc/self/mountinfo"), 1048576).splitlines():
        fields = line.split()
        if len(fields) < 8 or fields[4] != "/sys/fs/cgroup":
            continue
        try:
            separator = fields.index("-", 6)
        except ValueError:
            continue
        if separator + 1 < len(fields) and fields[separator + 1] == "cgroup2":
            mount_roots.append(fields[3])
    if len(mount_roots) != 1 or not clean_kernel_path(mount_roots[0]):
        raise RuntimeError("one unambiguous visible cgroup-v2 mount required")
    mount_root = mount_roots[0]
    candidates = [Path("/sys/fs/cgroup" + membership.rstrip("/"))]
    if membership == mount_root:
        candidates.append(Path("/sys/fs/cgroup"))
    elif mount_root != "/" and membership.startswith(mount_root + "/"):
        candidates.append(Path("/sys/fs/cgroup" + membership[len(mount_root):]))
    actual = []
    pid = str(os.getpid())
    for candidate in dict.fromkeys(candidates):
        try:
            pids = kernel_text(candidate / "cgroup.procs", 1048576).splitlines()
        except FileNotFoundError:
            continue
        if not all(item.isascii() and item.isdecimal() for item in pids):
            raise RuntimeError("malformed kernel cgroup PID observation")
        if pid in pids:
            actual.append(candidate)
    if len(actual) != 1:
        raise RuntimeError("actual SELF PID leaf membership required; ancestors cannot admit execution")
    leaf = actual[0]
    memory = kernel_text(leaf / "memory.max", 64).strip()
    swap = kernel_text(leaf / "memory.swap.max", 64).strip()
    cpus = kernel_text(leaf / "cpuset.cpus.effective", 256).strip()
    expected = os.environ.get("UWVM_TEST_CPUSET", "")
    if (not memory.isascii() or not memory.isdecimal() or len(memory) > 11
            or not 0 < int(memory) <= 64 * 1024**3 or swap != "0"):
        raise RuntimeError("actual SELF leaf requires memory.max<=64 GiB and swap.max=0")
    if expected != "0,2,4,6,16-31" or cpus != expected:
        raise RuntimeError("actual SELF leaf must match the verified 16 E-core plus 4 P-core topology")
    allowed = {0, 2, 4, 6, *range(16, 32)}
    affinity = set(os.sched_getaffinity(0))
    if not affinity or not affinity <= allowed:
        raise RuntimeError("actual SELF affinity exceeds the verified cgroup CPU set")
    return {"unified_path": membership, "visible_mount_root": mount_root,
            "actual_leaf": str(leaf), "self_pid": os.getpid(), "memory_max": int(memory),
            "swap_max": 0, "effective_cpuset": cpus, "affinity": sorted(affinity),
            "self_pid_membership_verified": True, "guardian_qualification": False}


def execute(argv: list[str]) -> dict:
    started = time.monotonic()
    run = subprocess.run(argv, capture_output=True, text=True, timeout=45, check=False)
    return {"argv": argv, "returncode": run.returncode, "stdout": run.stdout, "stderr": run.stderr,
            "seconds": time.monotonic() - started}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", choices=["uwvm2", "uwvm2-ros"], required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source-manifest", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--build-record", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    binary = args.binary.resolve(strict=True)
    oracle = args.wasm_tools.resolve(strict=True)
    if args.build_record.stat().st_size == 0:
        raise RuntimeError("a nonempty actual fresh runtime/support/own-main closure record is mandatory")
    for manifest_path in (args.source_manifest,):
        manifest = json.loads(manifest_path.read_text())
        for relative, expected in manifest["after"][args.repo].items():
            if sha256(source / relative) != expected:
                raise RuntimeError(f"exact current-source mismatch: {relative}")
    cgroup = bounded_cgroup()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture = source / "test/0035.parsed_record_quota"
    records = []
    inputs = []
    for name in ["invalid_last_after_limit", "invalid_last_after_unsupported", "many_nop_operations",
                 "unsupported_then_many_nop", "module_operation_sum", "module_owner_sum"]:
        wasm = args.output / f"{name}.wasm"
        records.append(execute([str(oracle), "parse", str(fixture / f"{name}.wat"), "-o", str(wasm)]))
        if records[-1]["returncode"] != 0:
            raise RuntimeError("official parser did not accept the exact original Core3 syntax")
        records.append(execute([str(oracle), "validate", "--features", "all", str(wasm)]))
        valid = not name.startswith("invalid_last_")
        if (records[-1]["returncode"] == 0) != valid:
            raise RuntimeError("official validation disagrees with the exact intended case")
        if not valid:
            reason = (records[-1]["stdout"] + records[-1]["stderr"]).lower()
            if not any(word in reason for word in ["uninitialized", "uninitialised", "local is not initialized", "unset"]):
                raise RuntimeError("oracle must identify the unused unset non-null Core3 local")
        inputs.append(wasm)
    # The native fixture runs INVALID first internally before any valid initialization.
    records.append(execute([str(binary), *(str(wasm) for wasm in inputs)]))
    result = records[-1]
    markers = [
        "invalid_last_after=module_limit rejected_by_actual_validator=1 private_plan_minted=0 runtime_registry_empty=1",
        "invalid_last_after=unsupported_instruction rejected_by_actual_validator=1 private_plan_minted=0 runtime_registry_empty=1",
    ]
    markers += [f"case={case} valid_guest_accepted=1 all_body_validation_completed=1 payload_discarded=1 no_raw_fallback=1 runtime_effects=0"
                for case in ["function_operations", "function_bytes", "unsupported_then_many_nop", "module_operations", "module_fixed_metadata"]]
    passed = result["returncode"] == 0 and all(marker in result["stdout"] for marker in markers)
    record = {"schema": 1, "repo": args.repo, "passed": passed,
              "whole_default_pipeline_complete": False, "concurrent_reset_qualified": False,
              "all_record_lowerings_complete": False, "llvm_retained_consumer_complete": False,
              "physical_lowering_qualified": False, "retained_payload_budget_qualified": passed,
              "source_manifest_sha256": sha256(args.source_manifest),
              "build_record_sha256": sha256(args.build_record), "binary_sha256": sha256(binary), "oracle_sha256": sha256(oracle),
              "cgroup_self_observation": cgroup, "guardian_qualification": False,
              "affinity": sorted(os.sched_getaffinity(0)), "records": records}
    (args.output / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    if not passed:
        raise RuntimeError("real before-effects admission/initialization/record consumer coverage was missing or failed")
    print("RETAINED_QUOTA actual_component_pass=1 physical_lowering_qualified=0 whole_default_pipeline_complete=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
