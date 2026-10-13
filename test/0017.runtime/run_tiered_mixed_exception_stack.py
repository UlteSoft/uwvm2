#!/usr/bin/env python3
"""Prove cross-module EH frames, including ordinary native-tier/T0 mixing."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


ANSI = re.compile(r"\x1b\[[0-9;]*m")
FRAME = re.compile(r"#\d+[^\n]*?module_id=(\d+) module=\"([^\"]+)\" func_idx=(\d+)")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command):
    return subprocess.run([str(arg) for arg in command], capture_output=True,
                          timeout=60, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--wasmtime", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ros", action="store_true", help="Run ROS int/full and JIT/full instead of ordinary tiered")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)

    fixtures = root / "test/0017.runtime/fixtures"
    binaries = {}
    for stem in ("tiered_t1_to_t0_provider", "tiered_t1_to_t0_import_uncaught"):
        wat = fixtures / (stem + ".wat")
        wasm = args.out / (stem + ".wasm")
        result = run([args.wasm_tools, "parse", wat, "-o", wasm])
        if result.returncode:
            raise RuntimeError(f"{stem} parse: {result.stderr.decode(errors='replace')}")
        result = run([args.wasm_tools, "validate", wasm])
        if result.returncode:
            raise RuntimeError(f"{stem} validate: {result.stderr.decode(errors='replace')}")
        binaries[stem] = wasm

    provider = binaries["tiered_t1_to_t0_provider"]
    main_wasm = binaries["tiered_t1_to_t0_import_uncaught"]
    oracle = run([args.wasmtime, "-C", "cache=n", "-W", "exceptions=y",
                  "--preload", "p=" + str(provider), main_wasm])
    (args.out / "wasmtime.log").write_bytes(oracle.stdout + oracle.stderr)
    if oracle.returncode == 0 or b"thrown Wasm exception" not in oracle.stderr:
        raise RuntimeError("Wasmtime did not confirm the expected uncaught Core3 exception")

    # Module IDs depend on preload registration order. The module display
    # names and per-module function indices are the stable identity here.
    expected = [("p", 0), ("main", 1), ("main", 2)]
    rows = []
    modes = (("int-full", None, ["-Rint"]),
             ("jit-full-instruction", "instruction", ["-Raot"]),
             ("jit-full-unwind", "unwind", ["-Raot"])) if args.ros else (
             ("instruction", "instruction", ["-Rcc", "tiered", "-Rcm", "lazy", "-Rtiered-disable-t2"]),
             ("unwind", "unwind", ["-Rcc", "tiered", "-Rcm", "lazy", "-Rtiered-disable-t2"]))
    for label, policy, backend in modes:
        compile_log = args.out / (label + ".compile.log")
        command = [args.uwvm, *backend, "-Rct", "0", "-Rllvm-cache-path", "disable"]
        if policy is not None:
            command += ["-Rllvm-call-stack", policy]
        if not args.ros:
            command += ["-Rclog", "file", compile_log]
        command += ["-WFE-exceptions", "-Wpre", provider, "p", "--run", main_wasm]
        result = run(command)
        raw = result.stdout + result.stderr
        (args.out / (label + ".run.log")).write_bytes(raw)
        plain = ANSI.sub("", raw.decode("utf-8", "replace"))
        parsed = [(int(module_id), module, int(function))
                  for module_id, module, function in FRAME.findall(plain)]
        actual = [("p" if module == "p" else "main" if module == str(main_wasm) else module,
                   function) for _, module, function in parsed]
        compile_text = compile_log.read_text() if compile_log.exists() else ""
        # The preloaded module's runtime name is the import alias "p", not its
        # file path. Its module_id/fn pair proves the throwing callee stayed T0.
        cold_t0 = bool(re.search(r"\[uwvm-int-lazy\] compile-end module=\"p\" module_id=\d+ [^\n]*? fn=0 ",
                                 compile_text)) if not args.ros else None
        hot_t1 = bool(re.search(r"\[llvm-jit-lazy\] compile-end module=\""
                                 + re.escape(str(main_wasm)) + r"\"[^\n]*? fn=1 ", compile_text)) if not args.ros else None
        passed = (result.returncode != 0 and "Uncaught WebAssembly exception" in plain
                  and "Wasm call stack captured at throw" in plain
                  and actual == expected and (args.ros or (cold_t0 and (policy != "unwind" or hot_t1))))
        rows.append(dict(label=label, policy=policy, command=[str(arg) for arg in command],
                         frames=actual, expected=expected, cold_t0=cold_t0,
                         hot_t1=hot_t1, passed=passed, exit=result.returncode))
        print("PASS" if passed else "FAIL", label, actual,
              "cold-T0", cold_t0, "hot-T1", hot_t1, flush=True)

    summary = dict(product_sha256=digest(args.uwvm), ros=args.ros,
                   wasmtime_sha256=digest(args.wasmtime),
                   wasm_tools_sha256=digest(args.wasm_tools),
                   provider_sha256=digest(provider), main_sha256=digest(main_wasm),
                   cgroup={name: Path("/sys/fs/cgroup", name).read_text().strip()
                           for name in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")},
                   runs=rows, passed=all(row["passed"] for row in rows))
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
