#!/usr/bin/env python3
"""Prove which simple reference carriers LLVM 23 exposes in stack maps.

This compiles independent LLVM IR, not UWVM JIT output. It deliberately
cannot qualify a GC collector or serve as evidence of product-root coverage.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


DEFAULT_TARGETS = (
    "aarch64-apple-darwin",
    "x86_64-unknown-linux-gnu",
    "x86_64-w64-windows-gnu",
    "riscv64-unknown-linux-gnu",
)
FUNCTIONS = ("typed_reference", "integer_carrier", "tagged_carrier")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(argv, stdout_path, stderr_path):
    result = subprocess.run(argv, capture_output=True, check=False)
    stdout_path.write_bytes(result.stdout)
    stderr_path.write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{argv[0]} exited {result.returncode}; see {stderr_path}")
    return result.stdout.decode("utf-8", "replace")


def function_body(ir, name):
    match = re.search(r"(?ms)^define\s+[^\n]*\s@" + re.escape(name)
                      + r"\([^\n]*\)\s+gc\s+\"statepoint-example\"\s*\{(.*?)^\}", ir)
    if not match:
        raise RuntimeError(f"rewritten LLVM IR lacks {name}")
    return match.group(1)


def stackmap_records(output):
    if "LLVM StackMap Version: 3" not in output or "Num Records: 3" not in output:
        raise RuntimeError("expected three LLVM v3 stack-map records")
    records = re.findall(r"(?ms)^  Record ID: [^\n]+\n"
                         r"    (\d+) locations:\n(.*?)(?=^  Record ID: |\Z)", output)
    if len(records) != 3:
        raise RuntimeError("missing a stack-map record")
    return [(int(count), body) for count, body in records]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True,
                        help="new evidence directory; never overwrites an earlier probe")
    parser.add_argument("--opt", default="opt")
    parser.add_argument("--llc", default="llc")
    parser.add_argument("--readobj", default="llvm-readobj")
    parser.add_argument("--target", action="append", dest="targets",
                        help="LLVM target triple; default is four known 64-bit formats")
    args = parser.parse_args()
    source = Path(__file__).with_name("carriers.ll")
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    targets = args.targets or DEFAULT_TARGETS
    tools = {}
    for name in ("opt", "llc", "readobj"):
        path = shutil.which(getattr(args, name))
        if path is None:
            raise RuntimeError(f"{name} tool unavailable")
        tools[name] = str(Path(path).resolve())
        command([tools[name], "--version"], output / f"{name}.version.txt",
                output / f"{name}.version.err.txt")

    rewritten = output / "carriers.rewritten.ll"
    command([tools["opt"], "-passes=rewrite-statepoints-for-gc", "-S",
             str(source), "-o", str(rewritten)], output / "opt.stdout.txt",
            output / "opt.stderr.txt")
    ir = rewritten.read_text()
    bodies = {name: function_body(ir, name) for name in FUNCTIONS}
    if not all("statepoint" in body for body in bodies.values()):
        raise RuntimeError("a call was not rewritten to a statepoint")
    if '"gc-live"' not in bodies["typed_reference"]:
        raise RuntimeError("typed managed pointer lost its GC root")
    if any('"gc-live"' in bodies[name] for name in FUNCTIONS[1:]):
        raise RuntimeError("integer/tagged carrier unexpectedly gained an implicit GC root")

    results = []
    for triple in targets:
        if not re.fullmatch(r"[A-Za-z0-9_.+-]+", triple):
            raise ValueError("unsafe LLVM target triple")
        obj = output / f"{triple}.o"
        command([tools["llc"], "-filetype=obj", f"-mtriple={triple}",
                 str(rewritten), "-o", str(obj)], output / f"{triple}.llc.stdout.txt",
                output / f"{triple}.llc.stderr.txt")
        stackmap = command([tools["readobj"], "--stackmap", str(obj)],
                           output / f"{triple}.stackmap.txt",
                           output / f"{triple}.readobj.stderr.txt")
        records = stackmap_records(stackmap)
        if records[0][0] != 5 or "Indirect" not in records[0][1]:
            raise RuntimeError(f"{triple}: typed reference has no expected root location")
        if any(count != 3 or "Indirect" in body or "Register" in body
               for count, body in records[1:]):
            raise RuntimeError(f"{triple}: scalar/tagged carrier unexpectedly has a root location")
        results.append({"target": triple, "format": stackmap.split("Format: ", 1)[1].splitlines()[0],
                        "object_sha256": digest(obj),
                        "locations": [count for count, _ in records]})

    summary = {"scope": "exploratory LLVM IR only; no UWVM JIT code or GC collection",
               "fixture_sha256": digest(source), "rewritten_ir_sha256": digest(rewritten),
               "tools": tools, "targets": results}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
