#!/usr/bin/env python3
"""Record a qualified Win64 PE import table and reject unavailable GCC DLLs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


DLL_PATTERN = re.compile(r"^\s*DLL Name:\s*(\S+)\s*$", re.MULTILINE)
FORBIDDEN_DLLS = frozenset({"libgcc_s_seh-1.dll"})


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def contains(path: Path, needle: bytes) -> bool:
    overlap = b""
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            data = overlap + chunk
            if needle in data:
                return True
            overlap = data[-(len(needle) - 1):]
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pe", type=Path, required=True)
    parser.add_argument("--source-id")
    parser.add_argument("--expected-sha256")
    parser.add_argument("--objdump", default="objdump")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pe = args.pe.resolve(strict=True)
    if not pe.is_file() or not (args.source_id or args.expected_sha256):
        raise ValueError("expected a regular PE and source ID or expected SHA-256")
    if args.source_id and re.fullmatch(r"sha256:[0-9a-f]{64}", args.source_id) is None:
        raise ValueError("invalid source ID")
    if args.expected_sha256 and re.fullmatch(r"[0-9a-f]{64}", args.expected_sha256) is None:
        raise ValueError("invalid expected PE SHA-256")
    pe_sha256 = digest(pe)
    if args.expected_sha256 and pe_sha256 != args.expected_sha256:
        raise ValueError("PE differs from expected build artifact SHA-256")
    if args.source_id and not contains(pe, args.source_id.encode("ascii")):
        raise ValueError("PE does not embed the requested source ID")

    objdump_name = shutil.which(args.objdump)
    if objdump_name is None:
        raise ValueError("objdump executable was not found")
    objdump = Path(objdump_name).resolve(strict=True)
    result = subprocess.run([str(objdump), "-p", str(pe)],
                            check=True, capture_output=True, text=True)
    imports = sorted({name.lower() for name in DLL_PATTERN.findall(result.stdout)})
    if not imports:
        raise ValueError("PE import table is empty or objdump output was not parsed")
    forbidden = sorted(FORBIDDEN_DLLS.intersection(imports))
    record = {"schema": 1, "source_id": args.source_id,
              "pe_sha256": pe_sha256, "imports": imports,
              "objdump_sha256": digest(objdump),
              "forbidden_imports": forbidden,
              "passed": not forbidden}
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(json.dumps(record, sort_keys=True))
    if forbidden:
        raise SystemExit("Win64 PE still imports an unstaged GCC runtime: " +
                         ", ".join(forbidden))


if __name__ == "__main__":
    main()
