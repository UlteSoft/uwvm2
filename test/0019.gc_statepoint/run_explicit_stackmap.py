#!/usr/bin/env python3
"""Test explicit root locations for UWVM's integer reference carrier.

The object files are standalone LLVM IR, not UWVM product output. A collector
would still need exact live-value enumeration, register recovery, safepoint
coordination, stale-handle protection, barriers, and reachability sweeping.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


TARGETS = (
    "aarch64-apple-darwin",
    "x86_64-unknown-linux-gnu",
    "x86_64-w64-windows-gnu",
    "riscv64-unknown-linux-gnu",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(argv, stdout_path, stderr_path):
    result = subprocess.run(argv, capture_output=True, check=False)
    stdout_path.write_bytes(result.stdout)
    stderr_path.write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{argv[0]} exited {result.returncode}; see {stderr_path}")
    return result.stdout.decode("utf-8", "replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--llc", default="llc")
    parser.add_argument("--readobj", default="llvm-readobj")
    parser.add_argument("--objdump", default="llvm-objdump")
    parser.add_argument("--target", action="append", dest="targets")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).with_name("explicit_stackmap.ll")
    tool_paths = {}
    for name in ("llc", "readobj", "objdump"):
        path = shutil.which(getattr(args, name))
        if path is None:
            raise RuntimeError(f"{name} unavailable")
        tool_paths[name] = str(Path(path).resolve())
        command([tool_paths[name], "--version"], out / (name + ".version.txt"),
                out / (name + ".version.err.txt"))

    rows = []
    for triple in args.targets or TARGETS:
        if not re.fullmatch(r"[A-Za-z0-9_.+-]+", triple):
            raise ValueError("unsafe target triple")
        obj = out / (triple + ".o")
        command([tool_paths["llc"], "-O3", "-filetype=obj", f"-mtriple={triple}",
                 str(source), "-o", str(obj)], out / (triple + ".llc.stdout.txt"),
                out / (triple + ".llc.stderr.txt"))
        stackmap = command([tool_paths["readobj"], "--stackmap", str(obj)],
                           out / (triple + ".stackmap.txt"), out / (triple + ".readobj.stderr.txt"))
        disassembly = command([tool_paths["objdump"], "--disassemble", "--no-show-raw-insn", str(obj)],
                              out / (triple + ".disassembly.txt"), out / (triple + ".objdump.stderr.txt"))
        if "LLVM StackMap Version: 3" not in stackmap or "Num Records: 1" not in stackmap or "Record ID: 9001" not in stackmap:
            raise RuntimeError(f"{triple}: explicit stackmap record missing")
        match = re.search(r"Record ID: 9001, instruction offset: (\d+)\n    (\d+) locations:\n((?:      #[^\n]+\n)+)", stackmap)
        if not match or int(match.group(2)) != 2:
            raise RuntimeError(f"{triple}: payload and kind need two locations")
        locations = match.group(3).splitlines()
        if not all(re.search(r"(?:Register|Indirect) R#\d+, size: 8", line) for line in locations):
            raise RuntimeError(f"{triple}: root half is not recoverable")
        # Mach-O may label an unexported IR function as ltmp0 in the object.
        if "Disassembly of section" not in disassembly or not re.search(r"\b(?:call|bl|jal)\w*\b", disassembly):
            raise RuntimeError(f"{triple}: helper call missing from object")
        rows.append({"target": triple,
                     "format": stackmap.split("Format: ", 1)[1].splitlines()[0],
                     "object_sha256": digest(obj),
                     "stackmap_sha256": digest(out / (triple + ".stackmap.txt")),
                     "disassembly_sha256": digest(out / (triple + ".disassembly.txt")),
                     "instruction_offset": int(match.group(1)),
                     "locations": locations})
    summary = {"scope": "exploratory LLVM IR; explicit i64 halves only, no product collector",
               "fixture_sha256": digest(source), "runner_sha256": digest(Path(__file__)),
               "tools": tool_paths, "targets": rows}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    sys.exit(main())
