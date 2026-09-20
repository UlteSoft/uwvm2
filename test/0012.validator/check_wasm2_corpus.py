#!/usr/bin/env python3
"""Check every binary module emitted from the pinned Wasm 2.0 core WAST suite.

Text-format-only malformed assertions are reported separately: UWVM consumes
binary modules, not WAT. Every binary has an explicit upstream validity oracle.
Usage: --suite /path/to/spec/test/core --checker /path/to/wasm2_corpus_check
"""
import argparse
import collections
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", required=True, type=Path)
    parser.add_argument("--checker", required=True, type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--wast2json", default="wast2json")
    args = parser.parse_args()
    out = args.out or Path(tempfile.mkdtemp(prefix="uwvm-wasm2-corpus-", dir="/tmp"))
    out.mkdir(parents=True, exist_ok=True)
    cases, excluded, conversion_failures = [], [], []
    for wast in sorted(args.suite.rglob("*.wast")):
        relative = wast.relative_to(args.suite)
        folder = out / relative.with_suffix("")
        folder.mkdir(parents=True, exist_ok=True)
        target = folder / "script.json"
        converted = subprocess.run([args.wast2json, str(wast), "-o", str(target)], capture_output=True, timeout=60)
        (folder / "conversion.log").write_bytes(converted.stdout + converted.stderr)
        if converted.returncode:
            conversion_failures.append(str(relative))
            continue
        for command in json.loads(target.read_text())["commands"]:
            if "filename" not in command:
                continue
            if command.get("module_type") == "text":
                excluded.append(dict(source=str(relative), line=command["line"], reason="text-format assertion"))
                continue
            path = folder / command["filename"]
            if command["type"] not in ("module", "assert_invalid", "assert_malformed", "assert_unlinkable", "assert_uninstantiable", "assert_trap"):
                raise ValueError(command)
            cases.append(dict(source=str(relative), line=command["line"], kind=command["type"],
                              expected=command["type"] not in ("assert_invalid", "assert_malformed"),
                              path=str(path.resolve())))
    result = subprocess.run([str(args.checker.resolve())], input="".join(c["path"] + "\n" for c in cases),
                            text=True, capture_output=True, timeout=300)
    (out / "checker.stdout").write_text(result.stdout)
    (out / "checker.stderr").write_text(result.stderr)
    lines = result.stdout.splitlines()
    if result.returncode or len(lines) != len(cases):
        raise RuntimeError(f"checker failed: exit={result.returncode}, records={len(lines)}/{len(cases)}; {out}")
    for case, actual in zip(cases, lines):
        case["actual"] = actual
        case["pass"] = (actual == "ok 0 0") == case["expected"] and not actual.startswith("io ")
    failures = [c for c in cases if not c["pass"]]
    report = dict(suite=str(args.suite.resolve()), checker=str(args.checker.resolve()), cases=cases,
                  excluded=excluded, conversion_failures=conversion_failures, failures=failures,
                  counts=dict(collections.Counter(c["kind"] for c in cases)))
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(binary_cases=len(cases), failed=len(failures), text_only=len(excluded),
                          conversion_failures=conversion_failures, output=str(out))))
    for case in failures:
        print(json.dumps(case))
    return bool(failures or conversion_failures)


if __name__ == "__main__":
    raise SystemExit(main())
