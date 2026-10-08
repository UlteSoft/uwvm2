#!/usr/bin/env python3
"""Check indexed and legacy memory syntax in every interpreter tuning build.

This is a correctness matrix. Timings belong to check_memory_performance.py and
must be taken separately while the machine is idle.
"""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

from memory_performance_cases import checksum


COMBINATIONS = ("none", "soft", "heavy", "extra")
DELAYS = ("none", "soft", "heavy")
KERNELS = (
    "scalar-aligned", "scalar-unaligned", "simd-aligned",
    "load-scalar-aligned", "load-scalar-unaligned", "load-simd-aligned",
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=1003)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if not 1 <= args.iterations <= 100000000:
        parser.error("--iterations must be in [1, 100000000]")
    build = args.build.resolve(strict=True)
    fixtures = args.fixtures.resolve(strict=True)
    reference = json.loads((fixtures / "reference.json").read_text())
    fixture_hashes = {}
    for kernel in KERNELS:
        for syntax in ("legacy", "indexed"):
            name = f"{kernel}-{syntax}"
            wasm = fixtures / f"{name}.wasm"
            fixture_hashes[name] = digest(wasm)
            witnesses = [row for row in reference if row["fixture"] == name]
            if len(witnesses) < 2 or any(not row["passed"] or row["wasm_sha256"] != fixture_hashes[name]
                                       for row in witnesses):
                raise RuntimeError(f"fixture lacks independent Wasmtime qualification: {name}")
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    binary_hashes = {}
    expected_configurations = {f"combine-{combine}-delay-{delay}"
                               for combine in COMBINATIONS for delay in DELAYS}
    actual_configurations = {path.name for path in build.iterdir() if path.is_dir()}
    if not expected_configurations.issubset(actual_configurations):
        raise RuntimeError(f"missing tuning configurations: {sorted(expected_configurations - actual_configurations)}")
    for configuration in sorted(expected_configurations):
        for version in ("baseline", "current"):
            binary = build / configuration / version
            binary_hashes[f"{configuration}/{version}"] = digest(binary)
            for kernel in KERNELS:
                for syntax in (("legacy",) if version == "baseline" else ("legacy", "indexed")):
                    wasm = fixtures / f"{kernel}-{syntax}.wasm"
                    command = [str(binary), str(wasm), str(args.iterations), "1"]
                    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
                    output = result.stdout.strip()
                    try:
                        measured = json.loads(output)
                    except json.JSONDecodeError:
                        measured = {}
                    expected = checksum(kernel, args.iterations)
                    passed = (result.returncode == 0 and measured.get("iterations") == args.iterations and
                              measured.get("checksum") == expected and len(measured.get("nanoseconds", [])) == 1)
                    rows.append({"configuration": configuration, "version": version, "kernel": kernel,
                                 "syntax": syntax, "command": command, "exit": result.returncode,
                                 "expected_checksum": expected, "observed_checksum": measured.get("checksum"),
                                 "passed": passed, "stderr": result.stderr[-500:]})
                    if not passed:
                        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
                        raise RuntimeError(f"failed {configuration}/{version}/{kernel}-{syntax}: {output} {result.stderr}")
    (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
    summary = {"passed": True, "checks": len(rows), "iterations": args.iterations,
               "configurations": sorted(expected_configurations), "binary_sha256": binary_hashes,
               "fixture_sha256": fixture_hashes, "runner_sha256": digest(Path(__file__))}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"PASS Core 3 memory combination/delay matrix: {len(rows)}/{len(rows)}")


if __name__ == "__main__":
    main()
