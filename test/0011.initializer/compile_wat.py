#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional


def _find_wat2wasm(explicit: Optional[str]) -> Optional[str]:
    if explicit:
        return explicit
    return shutil.which("wat2wasm")


def _compile_one(wat2wasm: str, wat_file: Path, wasm_file: Path) -> None:
    wasm_file.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [wat2wasm, str(wat_file), "-o", str(wasm_file)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile .wat files to .wasm for test/0011.initializer")
    parser.add_argument(
        "--wat-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "wat",
        help="Directory containing .wat files (default: ./wat next to this script)",
    )
    parser.add_argument(
        "--wat2wasm",
        type=str,
        default=None,
        help="Path to wat2wasm (default: search PATH)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Recompile even if .wasm is newer than .wat",
    )
    args = parser.parse_args()

    wat_dir: Path = args.wat_dir.resolve()
    wat2wasm = _find_wat2wasm(args.wat2wasm)
    if not wat2wasm:
        sys.stderr.write(
            "compile_wat.py: wat2wasm not found in PATH.\n"
            "Install wabt (e.g. macOS: brew install wabt) or pass --wat2wasm.\n"
        )
        return 2

    if not wat_dir.exists():
        sys.stderr.write(f"compile_wat.py: wat dir not found: {wat_dir}\n")
        return 2

    wat_files = sorted(wat_dir.glob("*.wat"))
    if not wat_files:
        sys.stderr.write(f"compile_wat.py: no .wat files found in: {wat_dir}\n")
        return 2

    compiled = 0
    for wat_file in wat_files:
        wasm_file = wat_file.with_suffix(".wasm")
        if (
            not args.force
            and wasm_file.exists()
            and wasm_file.stat().st_mtime >= wat_file.stat().st_mtime
        ):
            continue
        try:
            _compile_one(wat2wasm, wat_file, wasm_file)
            compiled += 1
        except subprocess.CalledProcessError as e:
            sys.stderr.write(f"compile_wat.py: failed to compile {wat_file.name}\n")
            if e.stdout:
                sys.stderr.write(e.stdout)
            if e.stderr:
                sys.stderr.write(e.stderr)
            return 1

    sys.stdout.write(f"compile_wat.py: OK (compiled {compiled}/{len(wat_files)})\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
