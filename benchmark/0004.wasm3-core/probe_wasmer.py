#!/usr/bin/env python3
"""Record per-feature Wasmer support without treating rejected modules as benchmarks."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from generate import CASES, module
from run import cgroup_preflight, disable_core_dumps, sha256


FLAGS = {"memory64": "--enable-memory64", "table64": "--enable-memory64",
         "threads": "--enable-threads", "multi-memory": "--enable-multi-memory",
         "tail-call": "--enable-tail-call", "exceptions": "--enable-exceptions",
         "function-references": "--enable-reference-types", "simd": "--enable-simd",
         "relaxed-simd": "--enable-relaxed-simd",
         "extended-const": "--enable-extended-const"}


def main():
    disable_core_dumps()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wasmer", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    cgroup_preflight(0)
    env = os.environ.copy()
    config = str(args.wasmer.parent.parent / "config")
    env.update(XDG_CONFIG_HOME=config, WASMER_DIR=config)
    rows = []
    for case in CASES:
        if case == "memory64-high-random-store":
            rows.append({"case": case, "result": "separate bounded preflight required"})
            continue
        _, features = module(case, 1000)
        flags = list(dict.fromkeys(FLAGS[feature] for feature in features if feature in FLAGS))
        instruction = ["taskset", "-c", "0", str(args.wasmer), "run", *flags,
                       "--invoke", "_start", str(args.fixtures / f"{case}.wasm")]
        result = subprocess.run(instruction, capture_output=True, env=env, timeout=30)
        row = {"case": case, "features": features, "command": instruction,
               "exit": result.returncode,
               "output": (result.stdout + result.stderr).decode(errors="replace")[-2000:]}
        rows.append(row)
        print(case, "pass" if result.returncode == 0 else "rejected", flush=True)
    args.out.write_text(json.dumps({"wasmer_sha256": sha256(args.wasmer),
                                    "high_memory": "excluded; use bounded preflight_high_memory.py",
                                    "results": rows}, indent=2) + "\n")


if __name__ == "__main__":
    main()
