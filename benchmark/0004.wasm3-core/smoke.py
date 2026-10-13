#!/usr/bin/env python3
"""Run the exact Core 3 benchmark modules once before collecting timings."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from generate import CASES, module
from run import cgroup_preflight, command, disable_core_dumps, LIB_PATHS


def main():
    disable_core_dumps()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--ordinary", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--wasmedge", type=Path, required=True)
    parser.add_argument("--wavm", type=Path, required=True)
    parser.add_argument("--count", type=int, default=1000)
    parser.add_argument("--cpu", type=int, default=0)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory already exists")
    cgroup_preflight(args.cpu)
    args.out.mkdir(parents=True)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, str(args.wasmedge.parent.parent / "lib"),
                                        str(args.wavm.parent.parent / "lib"))) + ":" + env.get("LD_LIBRARY_PATH", "")
    paths = {"wasmtime": args.wasmtime, "ordinary-jit": args.ordinary,
             "ros-jit": args.ros, "wasmedge-jit": args.wasmedge,
             "wasmedge-int": args.wasmedge, "wavm": args.wavm}
    metadata = {name: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                for name, path in paths.items()}
    (args.out / "metadata.json").write_text(json.dumps({"engines": metadata,
        "count": args.count, "high_memory": "excluded; use bounded preflight_high_memory.py",
        "start_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}, indent=2) + "\n")
    rows = []
    failures = []
    for case in CASES:
        if case == "memory64-high-random-store":
            rows.append({"case": case, "result": "separate bounded preflight required"})
            continue
        source, features = module(case, args.count)
        wat = args.out / f"{case}.wat"
        wasm = wat.with_suffix(".wasm")
        wat.write_text(source)
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
        for engine, executable in paths.items():
            if engine == "wavm" and any(feature in features for feature in
                                         ("gc", "tail-call", "function-references", "relaxed-simd",
                                          "exceptions", "extended-const")):
                rows.append({"case": case, "engine": engine, "result": "unsupported"})
                continue
            invocation = command(engine, executable, wasm, features, "unwind", args.cpu)
            result = subprocess.run(invocation, capture_output=True, env=env, timeout=60)
            row = {"case": case, "engine": engine, "result": "pass" if result.returncode == 0 else "fail",
                   "exit": result.returncode, "command": invocation,
                   "output": (result.stdout + result.stderr).decode(errors="replace")[-1000:]}
            rows.append(row)
            if result.returncode:
                failures.append(row)
            (args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            print(case, engine, row["result"], flush=True)
    if failures:
        print(json.dumps(failures, indent=2))
        raise SystemExit(1)


if __name__ == "__main__":
    main()
