#!/usr/bin/env python3
"""Check Core 3 uncaught managed-reference payload diagnostics in Linux cgroup."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


CASES = {
    "exnref_uncaught_struct_execution": ("struct", "-WFE-gc"),
    "exnref_uncaught_exn_execution": ("exn", "-WFD-gc"),
}


def run(command):
    return subprocess.run(command, capture_output=True, timeout=30)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--compiler", choices=("int", "llvm-jit"), default="int")
    parser.add_argument("--mode", action="append", default=[])
    parser.add_argument("--ros", action="store_true", help="Use ROS full-only -Rint/-Raot compiler options")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if args.ros and any(mode != "full" for mode in (args.mode or ["full"])):
        parser.error("ROS supports only the full compilation mode")
    def compiler_flags(mode):
        if args.ros:
            return ["-Rint"] if args.compiler == "int" else ["-Raot"]
        return ["-Rcc", "int" if args.compiler == "int" else "jit", "-Rcm", mode]
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, (kind, gc_flag) in CASES.items():
        wat = root / "test/0017.runtime/fixtures" / f"{name}.wat"
        wasm = args.out / f"{name}.wasm"
        parsed = run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)])
        if parsed.returncode:
            raise RuntimeError(f"{name}: wasm-tools parse failed: {parsed.stderr.decode(errors='replace')}")
        oracle = run([str(args.wasmtime), "-C", "cache=n", "-W", "exceptions=y",
                      "-W", "gc=y", str(wasm)])
        if oracle.returncode == 0 or b"wasm backtrace" not in oracle.stderr:
            raise RuntimeError(f"{name}: Wasmtime did not report an uncaught throw")
        rows.append(dict(case=name, engine="wasmtime", exit=oracle.returncode,
                         wat_sha256=sha(wat), wasm_sha256=sha(wasm)))
        for mode in args.mode or ["full"]:
            command = [str(args.uwvm), *compiler_flags(mode),
                       "-WFE-exceptions", "-WFD-function-references", gc_flag,
                       "--log-color", "enable", "--run", str(wasm)]
            result = run(command)
            raw = result.stdout + result.stderr
            log = args.out / f"{name}-{args.compiler}-{mode}.log"
            log.write_bytes(raw)
            text = re.sub(rb"\x1b\[[0-9;]*m", b"", raw).decode(errors="replace")
            passed = (result.returncode != 0 and b"\x1b[" in raw and
                      "Uncaught WebAssembly exception" in text and
                      f"payload[0] wasm_reference kind={kind} opaque_token=0x" in text and
                      "entry_func_idx=0" in text and "func_idx=0" in text and
                      "Wasm call stack captured at throw" in text and
                      "Validation error" not in text and "truncated" not in text.lower())
            rows.append(dict(case=name, engine=f"uwvm-{args.compiler}-{mode}",
                             exit=result.returncode, passed=passed, command=command,
                             product_sha256=sha(args.uwvm), log=str(log)))
            (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
            if not passed:
                raise RuntimeError(f"{name}/{args.compiler}/{mode}: malformed uncaught diagnostic:\n{text}")
    print(f"PASS Core 3 uncaught managed references: {len(rows)} oracle/product checks")


if __name__ == "__main__":
    main()
