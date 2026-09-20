#!/usr/bin/env python3
"""
Detect includes that escape the header/module split after `#pragma once`.

Rule:
- Headers with a `#ifndef UWVM_MODULE` compatibility guard must not include
  dependencies between `#pragma once` and that guard. Other directives (for
  example a C++ feature-level check) are permitted before the guard.
- Plain implementation-detail headers without a `UWVM_MODULE` compatibility
  guard are out of scope; their includes are intentionally textual in both
  default and module translation units.

Scope:
- Scans the local `src` tree (two levels up from this test directory).
- Only checks project headers ("*.h"). Third-party code is not scanned.

Exit codes:
- 0: No problems found
- 1: Found violations
"""

from __future__ import annotations

import os
import re
import sys
from typing import List, Optional


THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_ROOT = os.path.abspath(os.path.join(THIS_DIR, "..", "..", "src"))


RE_PRAGMA_ONCE = re.compile(r"^\s*#\s*pragma\s+once\b")
RE_UWVM_MODULE_GUARD = re.compile(r"^\s*#\s*ifndef\s+UWVM_MODULE\b")


def list_header_files(root: str) -> List[str]:
    out: List[str] = []
    for dirpath, _, filenames in os.walk(root):
        for fn in filenames:
            if not fn.endswith(".h"):
                continue
            # Skip hidden files
            if fn.startswith('.'):
                continue
            out.append(os.path.join(dirpath, fn))
    return out


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def find_pragma_once_line(lines: List[str]) -> Optional[int]:
    for i, ln in enumerate(lines):
        if RE_PRAGMA_ONCE.match(ln):
            return i
    return None


def find_uwvm_module_guard(lines: List[str], start_idx: int) -> Optional[int]:
    for i in range(start_idx + 1, len(lines)):
        if RE_UWVM_MODULE_GUARD.match(lines[i]):
            return i
    return None


def find_include_before_guard(lines: List[str], start_idx: int, guard_idx: int) -> Optional[int]:
    """Return the first active-looking include before the UWVM_MODULE guard.

    Blank text and comments are ignored. Non-include directives are allowed so
    headers may diagnose unsupported language modes before selecting their
    textual-header or named-module dependency surface.
    """
    in_block_comment = False
    i = start_idx + 1
    while i < guard_idx:
        s = lines[i]
        p = 0
        while True:
            if in_block_comment:
                end = s.find('*/', p)
                if end == -1:
                    # Entire line is still in a block comment
                    i += 1
                    break
                in_block_comment = False
                p = end + 2
                # Continue scanning remainder of the same line
                continue

            # Skip leading whitespace
            while p < len(s) and s[p].isspace():
                p += 1

            # Empty after trimming -> move to next line
            if p >= len(s):
                i += 1
                break

            # Line comment
            if s.startswith('//', p):
                i += 1
                break

            # Block comment start
            if s.startswith('/*', p):
                in_block_comment = True
                p += 2
                continue

            # First non-comment token
            if s[p] == '#':
                rest = s[p+1:].lstrip()
                m = re.match(r"(\w+)", rest)
                if m and m.group(1).lower() == "include":
                    return i
                i += 1
                break

            # Non-preprocessor content before any directive: treat as benign; continue to next line
            i += 1
            break

    return None


def check_header(path: str) -> Optional[str]:
    text = read_text(path)
    lines = text.splitlines()

    idx = find_pragma_once_line(lines)
    if idx is None:
        return None  # No pragma once; out of scope

    guard_idx = find_uwvm_module_guard(lines, idx)
    if guard_idx is None:
        return None  # Plain textual helper, not a dual header/module surface.

    include_idx = find_include_before_guard(lines, idx, guard_idx)
    if include_idx is None:
        return None
    return f"{path}:{include_idx+1}: `#include` escapes the `#ifndef UWVM_MODULE` dependency guard"


def main(argv: List[str]) -> int:
    root = SRC_ROOT
    if len(argv) > 1:
        root = os.path.abspath(argv[1])

    print(f"Scanning headers under: {root}")
    headers = list_header_files(root)

    problems: List[str] = []
    for h in headers:
        msg = check_header(h)
        if msg:
            problems.append(msg)

    if problems:
        print("Found violations:")
        for p in problems:
            print(p)
        return 1
    print("No violations found.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))


