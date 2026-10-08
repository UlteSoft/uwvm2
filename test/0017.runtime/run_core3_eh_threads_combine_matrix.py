#!/usr/bin/env python3
"""Run Core3 try_table + shared atomic memory under every int combine/delay level."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


MODES = {"int-full": ["-Rcc", "int", "-Rcm", "full"],
         "int-lazy": ["-Rcc", "int", "-Rcm", "lazy"],
         "int-lazy-verified": ["-Rcc", "int", "-Rcm", "lazy+verification"]}
ANSI = re.compile(rb"\x1b\[[0-9;]*m")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    source = root / "test/0017.runtime/fixtures/core3_eh_threads_shared_atomic.wat"
    wasm = args.out / "core3_eh_threads_shared_atomic.wasm"
    rows = []

    def check(label, command, success=True, fragment=None):
        result = subprocess.run([str(arg) for arg in command], capture_output=True,
                                timeout=30, check=False)
        raw = result.stdout + result.stderr
        (args.out / (label + ".log")).write_bytes(raw)
        plain = ANSI.sub(b"", raw).decode("utf-8", "replace")
        passed = ((result.returncode == 0) == success
                  and (fragment is None or fragment in plain))
        rows.append(dict(label=label, command=[str(arg) for arg in command],
                         exit=result.returncode, passed=passed))
        if not passed:
            raise RuntimeError(f"{label}: exit={result.returncode}, output={plain[-800:]}")

    check("parse", [args.wasm_tools, "parse", source, "-o", wasm])
    check("validate", [args.wasm_tools, "validate", wasm])
    check("wasmtime", [args.wasmtime, "-C", "cache=n", "-W", "exceptions=y",
                       "-W", "threads=y", "-W", "shared-memory=y", wasm])
    for mode, backend in MODES.items():
        for level in ("disable", "soft", "heavy", "extra"):
            for no_delay in (False, True):
                variant = f"{mode}-{level}-" + ("no-delay" if no_delay else "delay")
                tuning = ["-Rint-op-conbine-level", level]
                if no_delay:
                    tuning += ["-Rint-no-delay-local"]
                prefix = [args.uwvm, *backend, *tuning]
                check(variant + "-enabled", [*prefix, "-WFE-exceptions", "-WFE-threads", "--run", wasm])
                check(variant + "-exceptions-off",
                      [*prefix, "-WFD-exceptions", "-WFE-threads", "--run", wasm],
                      False, "--wasm-feature-enable-exceptions")
                check(variant + "-threads-off",
                      [*prefix, "-WFE-exceptions", "-WFD-threads", "--run", wasm],
                      False, "--wasm-feature-enable-threads")
    summary = dict(passed=True, checks=len(rows), rows=rows,
                   source_sha256=digest(source), wasm_sha256=digest(wasm),
                   product_sha256=digest(args.uwvm),
                   wasmtime_sha256=digest(args.wasmtime),
                   wasm_tools_sha256=digest(args.wasm_tools),
                   cgroup={name: Path("/sys/fs/cgroup", name).read_text().strip()
                           for name in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")})
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Core3 EH/shared atomic combine matrix: {len(rows)}/{len(rows)} PASS")


if __name__ == "__main__":
    main()
