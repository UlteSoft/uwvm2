#!/usr/bin/env python3
"""Keeper-only actual reset/source-lifetime component; no synthetic epoch."""
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


def bounded_cgroup() -> tuple[str, int]:
    if platform.system() != "Linux":
        raise RuntimeError("only the SSH Linux cgroup keeper may execute this component")
    membership = next((line.split(":", 2)[2] for line in Path("/proc/self/cgroup").read_text().splitlines()
                       if line.startswith("0::")), None)
    if membership is None:
        raise RuntimeError("cgroup-v2 sandbox required")
    root = Path("/sys/fs/cgroup")
    current = root / membership.lstrip("/")
    limits = []
    while True:
        bound = (current / "memory.max").read_text().strip()
        if bound != "max":
            limits.append(int(bound))
        if current == root:
            break
        if root not in current.parents:
            raise RuntimeError("invalid cgroup ancestry")
        current = current.parent
    if not limits or min(limits) <= 0 or min(limits) > 64 * 1024**3:
        raise RuntimeError("actual enforced ancestor memory.max <=64 GiB is required")
    return membership, min(limits)


def execute(argv: list[str]) -> dict:
    started = time.monotonic()
    run = subprocess.run(argv, capture_output=True, text=True, timeout=45, check=False)
    return {"argv": argv, "returncode": run.returncode, "stdout": run.stdout, "stderr": run.stderr,
            "seconds": time.monotonic() - started}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", choices=["uwvm2", "uwvm2-ros"], required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--scalar-manifest", type=Path, required=True)
    parser.add_argument("--fixture-manifest", type=Path, required=True)
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
    for manifest_path in (args.scalar_manifest, args.fixture_manifest):
        manifest = json.loads(manifest_path.read_text())
        for relative, expected in manifest["after"][args.repo].items():
            if sha256(source / relative) != expected:
                raise RuntimeError(f"exact current-source mismatch: {relative}")
    cgroup, limit = bounded_cgroup()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture = source / "test/0031.compiler_generation_lifecycle/actual_source_core3.wat"
    wasm = args.output / "actual_source_core3.wasm"
    records = [execute([str(oracle), "parse", str(fixture), "-o", str(wasm)])]
    if records[-1]["returncode"] != 0:
        raise RuntimeError("official parser did not accept the exact Core3 input")
    records.append(execute([str(oracle), "validate", "--features", "all", str(wasm)]))
    if records[-1]["returncode"] != 0:
        raise RuntimeError("official validation did not accept the exact Core3 input")
    records.append(execute([str(binary), str(wasm)]))
    result = records[-1]
    marker = ("COMPILER_GENERATION actual_reset_counter=1 selected_owner_retained=1 original_metadata_retained=1"
              " explicit_drained_source_retirement=1 final_weak_expired=1 empty_reset_counter=1 concurrent_reset_qualified=0")
    passed = result["returncode"] == 0 and marker in result["stdout"]
    record = {"schema": 1, "repo": args.repo, "passed": passed, "concurrent_reset_qualified": False,
              "default_lazy_complete": False, "compiled_cache_qualified": False, "source_only_before_execution": True,
              "scalar_manifest_sha256": sha256(args.scalar_manifest), "fixture_manifest_sha256": sha256(args.fixture_manifest),
              "build_record_sha256": sha256(args.build_record), "binary_sha256": sha256(binary), "oracle_sha256": sha256(oracle),
              "cgroup": cgroup, "memory_limit": limit, "affinity": sorted(os.sched_getaffinity(0)), "records": records}
    (args.output / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    if not passed:
        raise RuntimeError("real reset/source ownership coverage was missing or failed")
    print("COMPILER_GENERATION_LIFECYCLE actual_component_pass=1 default_lazy_complete=0 compiled_cache_qualified=0 concurrent_reset_qualified=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
