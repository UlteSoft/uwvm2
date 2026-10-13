#!/usr/bin/env python3
"""Bounded macOS semantic smoke for the two exnref reclamation workloads.

This exercises throwing, catching, payload cycles and their checksums. It does
not run a collector, measure throughput or clear the GC release gate.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from generate_exnref_reclamation import CASES, expected_after, module


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_id(source, out, stage):
    fingerprint = source / "tools/ci/wasm3_source_fingerprint.py"
    return subprocess.check_output(
        [sys.executable, str(fingerprint), str(source),
         str(out / f"source-{stage}.json")], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--expected-binary-sha256", required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--count", type=int, default=1000)
    args = parser.parse_args()
    if sys.platform != "darwin" or args.out.exists() or not 0 < args.count < 1_000_000:
        parser.error("requires macOS, a fresh output directory and 1..999999 iterations")
    args.source = args.source.resolve(strict=True)
    args.uwvm = args.uwvm.resolve(strict=True)
    args.wasm_tools = args.wasm_tools.resolve(strict=True)
    args.out.mkdir(parents=True)
    (args.out / "logs").mkdir()
    (args.out / "fixtures").mkdir()
    for name in (Path(__file__).name, "generate_exnref_reclamation.py"):
        shutil.copyfile(Path(__file__).with_name(name), args.out / name)
    original = source_id(args.source, args.out, "before")
    binary_sha = sha256(args.uwvm)
    if original != args.expected_source_id or binary_sha != args.expected_binary_sha256:
        raise RuntimeError("candidate source ID or product binary SHA changed")
    watcher = args.source / "test/0017.runtime/macos_rss_limit.py"
    staged_watcher = args.out / "macos_rss_limit.py"
    shutil.copyfile(watcher, staged_watcher)
    watcher_sha = sha256(staged_watcher)
    modes = (("int-full", ("-Rint",)) if args.ros else
             ("int-full", ("-Rcc", "int", "-Rcm", "full")))
    selected = [("jit-instruction", "instruction"), ("jit-unwind", "unwind")]
    if args.ros:
        selected.insert(0, modes)
    rows = []
    for case in CASES:
        text, features = module(case, args.count)
        wat = args.out / "fixtures" / f"{case}-{args.count}.wat"
        wasm = wat.with_suffix(".wasm")
        wat.write_text(text)
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
        for label, mode in selected:
            if isinstance(mode, tuple):
                policy = list(mode)
            else:
                policy = (["-Raot"] if args.ros else
                          ["-Rcc", "jit", "-Rcm", "full"])
                policy += ["-Rllvm-call-stack", mode, "-Rllvm-cache-path", "disable"]
            invocation = [str(args.uwvm), *policy, "-Rct", "0",
                          *("-WFE-" + feature for feature in features),
                          "--run", str(wasm)]
            command = [sys.executable, str(staged_watcher), "--limit-bytes", str(4 * 1024**3),
                       "--", *invocation]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=120)
            log = args.out / "logs" / f"{case}-{label}.log"
            log.write_text(result.stdout)
            peak = next((int(line.partition("=")[2]) for line in result.stdout.splitlines()
                         if line.startswith("PEAK_PROCESS_TREE_RSS_BYTES=")), None)
            passed = (result.returncode == 0 and "COMMAND_EXIT=0" in result.stdout and
                      peak is not None and peak <= 4 * 1024**3)
            rows.append({"case": case, "mode": label, "command": command,
                         "exit": result.returncode, "passed": passed,
                         "peak_process_tree_rss_bytes": peak,
                         "wasm_sha256": sha256(wasm), "wat_sha256": sha256(wat),
                         "expected_lcg": expected_after(args.count),
                         "log": str(log)})
            (args.out / "rows.json").write_text(json.dumps(rows, indent=2) + "\n")
            if not passed:
                raise RuntimeError(f"{case}/{label}: failed semantic smoke; see {log}")
    final = source_id(args.source, args.out, "after")
    if (final != original or sha256(args.uwvm) != binary_sha or
            sha256(staged_watcher) != watcher_sha):
        raise RuntimeError("product source, binary or watcher changed during macOS smoke")
    summary = {"scope": f"{args.count}-step semantic smoke only; no collection count verified",
               "release_gate_passed": False, "source_id": original,
               "product_binary_sha256": binary_sha,
               "wasm_tools_sha256": sha256(args.wasm_tools),
               "runner_sha256": sha256(args.out / Path(__file__).name),
               "generator_sha256": sha256(args.out / "generate_exnref_reclamation.py"),
               "watcher_sha256": watcher_sha,
               "source_after": final,
               "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
               "rows": rows}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"PASS {len(rows)} exnref semantics; GC reclamation release gate remains FAIL")


if __name__ == "__main__":
    main()
