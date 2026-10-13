#!/usr/bin/env python3
"""Compare 90 converted modules and the one equivalent invalid subtype binary."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path


WASMTIME_FEATURES = (
    "bulk-memory", "multi-memory", "multi-value", "reference-types", "simd",
    "relaxed-simd", "tail-call", "threads", "shared-memory", "memory64",
    "function-references", "gc", "extended-const", "exceptions",
)
UWVM_FEATURES = (
    "bulk-memory", "exceptions", "extended-const", "function-references",
    "gc", "memory64", "multi-memory", "multi-value", "multiple-tables",
    "reference-types", "relaxed-simd", "simd", "table-initializer",
    "table-instructions", "table64", "tail-call", "threads",
)
# Exact equivalent of the pinned official multiple-supertypes assertion:
# three struct types, with the third declaring type indices 0 and 1 as parents.
# Core 3 binary syntax encodes a list of parent indices; validation rejects two.
MULTIPLE_SUPERTYPE_BINARY = bytes.fromhex(
    "0061736d01000000010f0350005f0050005f00500200015f00"
)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command):
    start = time.monotonic()
    try:
        result = subprocess.run(command, capture_output=True, timeout=45)
        return result.returncode, result.stdout + result.stderr, round(time.monotonic() - start, 3)
    except subprocess.TimeoutExpired as exc:
        return "timeout", (exc.stdout or b"") + (exc.stderr or b""), round(time.monotonic() - start, 3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case-dir", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if Path("/sys/fs/cgroup/memory.max").read_text().strip() != str(64 * 1024**3):
        raise SystemExit("all Linux test runs require the 64 GiB cgroup")
    if Path("/sys/fs/cgroup/memory.swap.max").read_text().strip() != "0":
        raise SystemExit("swap must be disabled in the test cgroup")
    args.out.mkdir(parents=True, exist_ok=False)
    commands = json.loads((args.case_dir / "commands.json").read_text())["commands"]
    names = sorted({command["filename"] for command in commands if command.get("filename", "").endswith(".wasm")})
    equivalent = args.out / "multiple-supertypes.wasm"
    equivalent.write_bytes(MULTIPLE_SUPERTYPE_BINARY)
    rows = []
    for name, wasm in [(name, args.case_dir / name) for name in names] + [(equivalent.name, equivalent)]:
        reference = [str(args.wasmtime), "compile", "-C", "cache=n", "-W", "all-proposals=n"]
        reference.extend(argument for feature in WASMTIME_FEATURES for argument in ("-W", feature + "=y"))
        reference.extend((str(wasm), "-o", "/dev/null"))
        product = [str(args.binary), "-m", "validation", *("-WFE-" + feature for feature in UWVM_FEATURES), "--run", str(wasm)]
        expected_exit, expected_log, expected_time = run(reference)
        actual_exit, actual_log, actual_time = run(product)
        matched = expected_exit != "timeout" and actual_exit != "timeout" and (expected_exit == 0) == (actual_exit == 0)
        row = {"module": name, "wasm_sha256": sha256(wasm), "wasmtime_exit": expected_exit,
               "uwvm_exit": actual_exit, "wasmtime_seconds": expected_time,
               "uwvm_seconds": actual_time, "match": matched}
        rows.append(row)
        if not matched:
            (args.out / f"{name}-wasmtime.log").write_bytes(expected_log)
            (args.out / f"{name}-uwvm.log").write_bytes(actual_log)
    result = {"source_id": args.source_id, "binary_sha256": sha256(args.binary),
              "wasmtime_sha256": sha256(args.wasmtime),
              "sanitization_sha256": sha256(args.case_dir / "sanitize-manifest.json"),
              "modules": len(rows), "matches": sum(row["match"] for row in rows),
              "mismatches": [row for row in rows if not row["match"]], "rows": rows}
    (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("source_id", "modules", "matches")}), flush=True)
    if result["modules"] != 91 or result["mismatches"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
