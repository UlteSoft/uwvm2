#!/usr/bin/env python3
"""Check official Core3 uncaught-exception call stacks against interpreter frames.

Run the product inside the remote 64 GiB Linux test cgroup. The input manifest is
the already Wasmtime-qualified official try_table/throw_ref subset.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess


ANSI = re.compile(r"\x1b\[[0-9;]*m")
FRAME = re.compile(r"#\d+[^\n]*?func_idx=(\d+)")
DEFAULT_MODES = (
    "tiered-lazy-instruction",
    "tiered-lazy-unwind",
    "tiered-lazy+verification-unwind",
    "int-full",
    "jit-full-instruction",
    "jit-full-unwind",
)


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def no_core_dump():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def key(run):
    return run["file"], run["group"], run["line"]


def frames(raw):
    return [int(value) for value in FRAME.findall(ANSI.sub("", raw))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--modes", nargs="+", default=DEFAULT_MODES)
    args = parser.parse_args()
    source_root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(source_root / "tools/ci/require_wasm3_test_cgroup.sh")],
                   check=True)
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.manifest.read_text())
    expected = {}
    for run in manifest["runs"]:
        if run["product"] == "ordinary" and run["mode"] == "int-full":
            expected[key(run)] = frames(Path(run["log"]).read_text())
    if len(expected) != 11 or any(not sequence for sequence in expected.values()):
        raise RuntimeError("official reference stacks are incomplete")

    results = []
    for run in manifest["runs"]:
        if run["product"] != "ordinary" or run["mode"] not in args.modes:
            continue
        command = list(run["command"])
        command[0] = str(args.product)
        completed = subprocess.run(command, capture_output=True, timeout=30,
                                   preexec_fn=no_core_dump, check=False)
        raw = (completed.stdout + completed.stderr).decode("utf-8", "replace")
        stem = f"{run['file']}-{run['group']}-line{run['line']}-{run['mode']}"
        log = args.output / f"{stem}.log"
        log.write_text(raw)
        plain = ANSI.sub("", raw)
        found = frames(raw)
        want = expected[key(run)]
        ok = (completed.returncode != 0
              and "Uncaught WebAssembly exception" in plain
              and "Wasm call stack captured at throw" in plain
              and "no snapshot captured" not in plain
              and found == want)
        results.append(dict(case=list(key(run)), mode=run["mode"],
                            exit=completed.returncode, frames=found,
                            expected=want, pass_=ok, log=str(log)))
        print("PASS" if ok else "FAIL", stem, "frames", found, flush=True)
    needed = 11 * len(args.modes)
    summary = dict(product=str(args.product), product_sha256=digest(args.product),
                   manifest=str(args.manifest), manifest_sha256=digest(args.manifest),
                   cgroup={name: Path("/sys/fs/cgroup", name).read_text().strip()
                           for name in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")},
                   expected_count=needed, actual_count=len(results),
                   passed=sum(item["pass_"] for item in results), results=results)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if len(results) != needed or summary["passed"] != needed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
