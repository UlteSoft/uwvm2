#!/usr/bin/env python3

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess


PERR_NEEDLE = "::fast_io::io::perr("


def fail(message: str) -> None:
    raise SystemExit(f"version output CPO arity: {message}")


def preprocess(source: str, *, huge_cpo: bool) -> str:
    compiler = os.environ.get("CXX", "c++")
    preamble = """\
#define UWVM_HAS_FEATURE(feature) 0
#define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 0
#define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 0
"""
    if huge_cpo:
        preamble += "#define UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT 1\n"

    completed = subprocess.run(
        [compiler, "-w", "-std=c++23", "-E", "-P", "-x", "c++", "-"],
        input=preamble + source,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        fail(f"preprocessor failed for {'huge' if huge_cpo else 'small'} CPO mode:\n{completed.stderr}")
    return completed.stdout


def perr_arities(source: str) -> list[int]:
    arities: list[int] = []
    search_from = 0
    while True:
        call_begin = source.find(PERR_NEEDLE, search_from)
        if call_begin == -1:
            return arities

        cursor = call_begin + len(PERR_NEEDLE)
        depth = 1
        comma_count = 0
        quote = ""
        escaped = False
        while cursor != len(source) and depth != 0:
            character = source[cursor]
            if quote:
                if escaped:
                    escaped = False
                elif character == "\\":
                    escaped = True
                elif character == quote:
                    quote = ""
            elif character in {'"', "'"}:
                quote = character
            elif character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
            elif character == "," and depth == 1:
                comma_count += 1
            cursor += 1

        if depth != 0:
            fail("unbalanced fast_io::io::perr call after preprocessing")
        arities.append(comma_count + 1)
        search_from = cursor


def main() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    version_header = repo_root / "src/uwvm2/uwvm/cmdline/callback/version.h"
    source = version_header.read_text(encoding="utf-8-sig")
    # The test exercises the header's own preprocessing structure without importing its dependency graph.
    source = "\n".join(line for line in source.splitlines() if not re.match(r"^\s*#\s*include\b", line))

    small_arities = perr_arities(preprocess(source, huge_cpo=False))
    huge_arities = perr_arities(preprocess(source, huge_cpo=True))
    if not small_arities or not huge_arities:
        fail("no fast_io::io::perr calls found")
    if max(small_arities) > 16:
        fail(f"ordinary mode still has a wide CPO with {max(small_arities)} arguments")
    if max(huge_arities) < 64:
        fail(f"huge-CPO opt-in no longer preserves the original wide call ({max(huge_arities)} arguments)")
    if len(small_arities) <= len(huge_arities):
        fail("ordinary mode did not expand the CPO split boundaries")

    print(
        "version output CPO arity: PASS "
        f"(small: {len(small_arities)} calls, max {max(small_arities)} args; "
        f"huge: {len(huge_arities)} calls, max {max(huge_arities)} args)"
    )


if __name__ == "__main__":
    main()
