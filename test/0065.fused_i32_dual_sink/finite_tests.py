#!/usr/bin/env python3
"""Finite actual official-oracle + real dual physical artifact component run.
Never run on the development Mac. Keeper must bind fresh tools/source/provider
closure and execute this helper under the existing original 64 GiB cgroup.
"""
import argparse
import json
import pathlib
import re
import subprocess


def invoke(argv, *, negative=False):
    completed = subprocess.run(argv, stdin=subprocess.DEVNULL, capture_output=True, timeout=120, check=False)
    if len(completed.stdout) + len(completed.stderr) > 1048576:
        raise RuntimeError("bounded actual test output exceeded one MiB")
    output = completed.stdout.decode("utf-8", "replace") + completed.stderr.decode("utf-8", "replace")
    if negative:
        forbidden = re.compile(r"unknown (?:option|feature|argument)|unexpected argument|unrecognized option|no such file|permission denied|failed to (?:open|read)|not enabled|(?:support|feature).*disabled|requires.*feature", re.I)
        accepted = completed.returncode > 0 and re.search(r"\(at offset 0x[0-9a-fA-F]+\)", output) and re.search(r"type mismatch|expected[^\n]*(?:i32|i64)", output, re.I) and not forbidden.search(output)
    else:
        accepted = completed.returncode == 0
    if not accepted:
        raise RuntimeError(json.dumps({"argv": argv, "returncode": completed.returncode, "output": output}))
    return {"argv": argv, "returncode": completed.returncode, "negative_code_diagnostic": bool(negative), "output": output}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--wasm-tools", required=True, type=pathlib.Path)
    parser.add_argument("--component", required=True, type=pathlib.Path)
    parser.add_argument("--source-dir", required=True, type=pathlib.Path)
    parser.add_argument("--work-dir", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if not args.wasm_tools.is_file() or not args.component.is_file() or not args.source_dir.is_dir():
        raise RuntimeError("actual pinned tool/component/source required")
    args.work_dir.mkdir(exist_ok=False)
    names = ["all18-modern", "unused-memory64-invalid", "unused-i31-invalid"]
    receipts = []
    targets = []
    for name in names:
        source = args.source_dir / (name + ".wat")
        if not source.is_file():
            raise RuntimeError("exact official WAT source missing")
        target = args.work_dir / (name + ".wasm")
        receipts.append(invoke([str(args.wasm_tools), "parse", str(source), "-o", str(target)]))
        receipts.append(invoke([str(args.wasm_tools), "validate", str(target)], negative=name != names[0]))
        targets.append(target)
    receipts.append(invoke([str(args.component), *(str(path) for path in targets)]))
    print(json.dumps({"actual_steps": receipts, "whole_tiered_single_walk": False,
                      "product_startup_admission": False, "native_debug_permission": False,
                      "performance_qualified": False}, indent=2))


if __name__ == "__main__":
    main()
