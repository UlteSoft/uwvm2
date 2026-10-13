#!/usr/bin/env python3
"""Actual Core 3 uncaught tag/throw_ref/name diagnostics; Linux cgroup only.

SOURCE fixture until a keeper executes it. This driver grants no source, root,
restore, native-address or checkpoint authority to the runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys

ANSI = re.compile(rb"\x1b\[[0-9;]*m")
TAG = re.compile(r'tag module_id=(\d+) module="(?:[^"\\]|\\.)*" tag_idx=(\d+) type_idx=(\d+)')
FRAME = re.compile(r'\[info\]\s+#(\d+) module_id=\d+ module="(?:[^"\\]|\\.)*" func_idx=(\d+)')
CASES = (("uncaught_tag_alpha", 0, 0), ("uncaught_tag_beta", 1, 0),
         ("uncaught_tag_throw_ref", 1, 2))


def run(command):
    return subprocess.run(command, capture_output=True, timeout=30)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", type=Path, help="Optional independent uncaught-exception oracle")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--compiler", choices=("int", "llvm-jit"), default="llvm-jit")
    parser.add_argument("--mode", choices=("full", "lazy"), action="append", default=[])
    parser.add_argument("--policy", choices=("instruction", "unwind"), action="append", default=[])
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("Run only through the SSH Linux keeper inside the existing <=64 GiB cgroup")
    if args.ros and any(mode != "full" for mode in (args.mode or ["full"])):
        parser.error("ROS supports only full mode")
    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True, timeout=10)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []
    versions = {}
    for name, executable in (("uwvm", args.uwvm), ("wasm-tools", args.wasm_tools),
                             ("wasmtime", args.wasmtime)):
        if executable is not None:
            result = run([str(executable), "--version"])
            versions[name] = dict(path=str(executable), sha256=sha(executable),
                                  version=(result.stdout + result.stderr).decode(errors="replace"),
                                  version_exit=result.returncode)
    (args.out / "tools.json").write_text(json.dumps(versions, indent=2) + "\n")
    modes = args.mode or ["full"]
    policies = ((args.policy or ["instruction", "unwind"])
                if args.compiler == "llvm-jit" else [None])
    for name, expected_tag, expected_entry in CASES:
        wat = root / "test/0017.runtime/fixtures" / (name + ".wat")
        wasm = args.out / (name + ".wasm")
        parsed = run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)])
        if parsed.returncode:
            raise RuntimeError(f"{name}: official wasm-tools parse failed: {parsed.stderr.decode(errors='replace')}")
        validated = run([str(args.wasm_tools), "validate", str(wasm)])
        if validated.returncode:
            raise RuntimeError(f"{name}: official wasm-tools validate failed: {validated.stderr.decode(errors='replace')}")
        if args.wasmtime is not None:
            oracle = run([str(args.wasmtime), "-C", "cache=n", "-W", "exceptions=y", str(wasm)])
            if oracle.returncode == 0 or b"wasm backtrace" not in oracle.stderr:
                raise RuntimeError(f"{name}: Wasmtime did not produce a real uncaught exception")
            rows.append(dict(case=name, engine="wasmtime", exit=oracle.returncode,
                             wat_sha256=sha(wat), wasm_sha256=sha(wasm)))
        for mode in modes:
            selected = (["-Rint"] if args.compiler == "int" else ["-Raot"]) if args.ros else [
                "-Rcc", "int" if args.compiler == "int" else "jit", "-Rcm", mode]
            for policy in policies:
                extra = (["-Rllvm-call-stack", policy, "-Rllvm-cache-path", "disable"]
                         if policy is not None else [])
                for color in ("enable", "disable"):
                    command = [str(args.uwvm), *selected, *extra, "-WFE-exceptions",
                               "-WFD-function-references", "-WFD-gc", "--log-color", color,
                               "--run", str(wasm)]
                    result = run(command)
                    raw = result.stdout + result.stderr
                    plain_bytes = ANSI.sub(b"", raw)
                    plain = plain_bytes.decode(errors="strict")
                    tags = TAG.findall(plain)
                    frames = FRAME.findall(plain)
                    label = f"{name}-{args.compiler}-{mode}-{policy or 'logical'}-{color}"
                    log = args.out / (label + ".log")
                    log.write_bytes(raw)
                    checks = dict(
                        nonzero_exit=result.returncode != 0,
                        genuine_fatal="Uncaught WebAssembly exception" in plain,
                        exact_tag=len(tags) == 1 and int(tags[0][1]) == expected_tag and int(tags[0][2]) == 0,
                        real_entry=f"entry_func_idx={expected_entry}" in plain,
                        same_payload="payload[0] i32 bits=0x" in plain and "signed=42" in plain,
                        throw_trace="Wasm call stack captured at throw" in plain and bool(frames),
                        original_throw=bool(frames) and int(frames[0][1]) == 0,
                        old_trace_preserved=name != "uncaught_tag_throw_ref" or all(int(f[1]) != 3 for f in frames),
                        color_switch=(bool(ANSI.search(raw)) if color == "enable" else b"\x1b" not in raw),
                        no_guest_raw_esc=b"\x1b" not in plain_bytes,
                        escaped_newline=bool(re.search(r'func_name="throw\\x(?:0[xX])?0*[aA]', plain)),
                        escaped_esc=bool(re.search(r'\\x(?:0[xX])?0*1[bB]', plain)),
                        escaped_quote=r'\"' in plain,
                        escaped_backslash=r'\\escaped' in plain,
                        no_unavailable="metadata unavailable" not in plain and "frame unavailable" not in plain,
                        no_validation_failure="Validation error" not in plain,
                    )
                    passed = all(checks.values())
                    rows.append(dict(case=name, engine=f"uwvm-{args.compiler}-{mode}", policy=policy,
                                     color=color, exit=result.returncode, passed=passed, checks=checks,
                                     command=command, product_sha256=sha(args.uwvm), wat_sha256=sha(wat),
                                     wasm_sha256=sha(wasm), log=str(log)))
                    (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
                    if not passed:
                        raise RuntimeError(f"{label}: malformed actual uncaught diagnostic: {checks}\n{plain}")
    print(f"PASS real Core 3 uncaught diagnostics: {sum(r.get('passed', False) for r in rows)} product cells")


if __name__ == "__main__":
    main()
