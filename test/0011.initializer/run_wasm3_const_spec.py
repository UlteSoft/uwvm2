#!/usr/bin/env python3
"""Compare the constant-initializer subset of Core 3 global.wast with Wasmtime.

Generate global.json with: wast2json --enable-all --no-check global.wast -o global.json
This is a parser test, not a runner for the entire spec suite. The module at
line 634 additionally exercises explicit table initialization.
"""

import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec_directory", type=pathlib.Path)
    parser.add_argument("uwvm_parser", type=pathlib.Path)
    parser.add_argument("wasmtime", type=pathlib.Path)
    args = parser.parse_args()
    root = args.spec_directory.resolve()
    digest = hashlib.sha256((root / "global.wast").read_bytes()).hexdigest()
    if digest != "0899df8c814d953d56a22b04d518fda6c7e6aab5a0d0472367a830e31821ee40":
        raise SystemExit("global.wast changed; review the selected initializer cases before updating the fingerprint")
    rows = []
    with tempfile.TemporaryDirectory(dir=root) as temp:
        for command in json.loads((root / "global.json").read_text())["commands"]:
            line = command["line"]
            if not (295 <= line <= 377 or line in (634, 667)):
                continue
            if command["type"] not in ("module", "assert_invalid"):
                continue
            expected = 0 if command["type"] == "module" else 1
            filename = root / command["filename"]
            ours = subprocess.run([str(args.uwvm_parser.resolve()), "--parse", str(filename)], capture_output=True, timeout=30)
            # Earlier Wasmtime versions gate previous-local-global reads on GC as well.
            oracle = subprocess.run([
                str(args.wasmtime.resolve()), "compile", "-C", "cache=n", "-W",
                "extended-const=y,gc=y,function-references=y", "-o", str(pathlib.Path(temp) / "oracle.cwasm"), str(filename),
            ], capture_output=True, timeout=30)
            row = {"line": line, "module": command["filename"], "expected_exit": expected,
                   "uwvm_exit": ours.returncode, "wasmtime_exit": oracle.returncode}
            rows.append(row)
            if ours.returncode != expected or oracle.returncode != expected:
                raise RuntimeError(f"{row}\n{ours.stderr.decode(errors='replace')}\n{oracle.stderr.decode(errors='replace')}")
            if expected and b"failed to parse WebAssembly module" not in oracle.stderr:
                raise RuntimeError(f"Oracle failed before validating input: {oracle.stderr!r}")
    if len(rows) != 22:
        raise RuntimeError(f"Expected 22 reviewed constant-expression cases, got {len(rows)}")
    print(json.dumps({"source_sha256": digest, "passed": len(rows), "cases": rows}, indent=2))


if __name__ == "__main__":
    main()
