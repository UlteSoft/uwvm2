#!/usr/bin/env python3
"""Execute Core 3 try_table plus shared atomics in ordinary full/lazy/tiered VMs."""

import argparse
import hashlib
import json
import re
import resource
import subprocess
from pathlib import Path


CONFIGURATIONS = (
    ("int-full", ("-Rcc", "int", "-Rcm", "full")),
    ("int-lazy", ("-Rcc", "int", "-Rcm", "lazy")),
    ("int-lazy-verified", ("-Rcc", "int", "-Rcm", "lazy+verification")),
    ("jit-full-instruction", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "instruction")),
    ("jit-full-unwind", ("-Rcc", "jit", "-Rcm", "full", "-Rllvm-call-stack", "unwind")),
    ("jit-lazy-instruction", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "instruction")),
    ("jit-lazy-unwind", ("-Rcc", "jit", "-Rcm", "lazy", "-Rllvm-call-stack", "unwind")),
    ("tiered-lazy-instruction", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0", "-Rllvm-call-stack", "instruction")),
    ("tiered-lazy-unwind", ("-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0", "-Rllvm-call-stack", "unwind")),
)
ANSI = re.compile(rb"\x1b\[[0-9;]*m")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(("bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")), check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=True)
    wat = root / "test/0017.runtime/fixtures/core3_eh_threads_shared_atomic.wat"
    wasm = args.out / "core3_eh_threads_shared_atomic.wasm"
    rows = []

    def check(label, command, success=True, fragment=None):
        result = subprocess.run(tuple(str(part) for part in command), capture_output=True, timeout=60)
        raw = result.stdout + result.stderr
        (args.out / f"{label}.log").write_bytes(raw)
        plain = ANSI.sub(b"", raw).decode(errors="replace")
        passed = (result.returncode == 0) == success and (fragment is None or fragment in plain)
        rows.append({"label": label, "command": [str(part) for part in command],
                     "exit": result.returncode, "passed": passed})
        if not passed:
            raise RuntimeError(f"{label}: exit={result.returncode}; {plain[-800:]}")

    check("parse", (args.wasm_tools, "parse", wat, "-o", wasm))
    check("validate", (args.wasm_tools, "validate", wasm))
    check("wasmtime", (args.wasmtime, "-C", "cache=n", "-W", "exceptions=y",
                       "-W", "threads=y", "-W", "shared-memory=y", wasm))
    for label, flags in CONFIGURATIONS:
        prefix = (args.uwvm, *flags, "-Rllvm-cache-path", "disable")
        check(label + "-enabled", (*prefix, "-WFE-exceptions", "-WFE-threads", "--run", wasm))
        check(label + "-exceptions-off", (*prefix, "-WFD-exceptions", "-WFE-threads", "--run", wasm),
              False, "--wasm-feature-enable-exceptions")
        check(label + "-threads-off", (*prefix, "-WFE-exceptions", "-WFD-threads", "--run", wasm),
              False, "--wasm-feature-enable-threads")

    summary = {
        "passed": len(rows) == 3 + 3 * len(CONFIGURATIONS) and all(row["passed"] for row in rows),
        "checks": len(rows), "source_id": args.source_id, "rows": rows,
        "wat_sha256": digest(wat), "wasm_sha256": digest(wasm),
        "binary_sha256": digest(args.uwvm), "wasm_tools_sha256": digest(args.wasm_tools),
        "wasmtime_sha256": digest(args.wasmtime),
        "cgroup": {name: Path("/sys/fs/cgroup", name).read_text().strip()
                   for name in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")},
    }
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Core3 EH/shared-atomic backends: {sum(row['passed'] for row in rows)}/{len(rows)}", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
