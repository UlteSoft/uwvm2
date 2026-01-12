#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

# fix https://github.com/llvm/llvm-project/issues/170970

SUFFIX = Path("include/c++/v1/__functional/hash.h")
ALT_SUFFIX = Path("usr/include/c++/v1/__functional/hash.h")
UNDEF_LINE = "#undef _LIBCPP_AVAILABILITY_HAS_HASH_MEMORY"


def find_hash_h(llvm_prefix: Path) -> Path:
    direct = llvm_prefix / SUFFIX
    if direct.is_file():
        return direct

    candidates: list[Path] = []
    for path in llvm_prefix.rglob("hash.h"):
        if path.as_posix().endswith(ALT_SUFFIX.as_posix()):
            candidates.append(path)

    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise FileNotFoundError(
            f"Could not find libc++ header under {llvm_prefix} (tried {direct} and **/{ALT_SUFFIX})."
        )
    candidates_str = "\n".join(str(p) for p in sorted(candidates))
    raise RuntimeError(
        "Found multiple candidate libc++ headers; pass a more specific LLVM prefix.\n" + candidates_str
    )


def patch_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    if UNDEF_LINE in original:
        return False

    lines = original.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if "_LIBCPP_BEGIN_NAMESPACE_STD" in line:
            insertion = UNDEF_LINE + "\n"
            lines.insert(i + 1, insertion)
            path.write_text("".join(lines), encoding="utf-8")
            return True

    raise RuntimeError(f"Did not find _LIBCPP_BEGIN_NAMESPACE_STD in {path}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Work around LLVM libc++ issue by undefining _LIBCPP_AVAILABILITY_HAS_HASH_MEMORY "
            "in __functional/hash.h after _LIBCPP_BEGIN_NAMESPACE_STD."
        )
    )
    parser.add_argument("--llvm-prefix", required=True, help="LLVM install prefix (e.g. $(brew --prefix llvm))")
    parser.add_argument("--check", action="store_true", help="Exit 0 if already patched; exit 1 if needs patching")
    args = parser.parse_args()

    llvm_prefix = Path(args.llvm_prefix).expanduser().resolve()
    header = find_hash_h(llvm_prefix)

    if args.check:
        return 0 if UNDEF_LINE in header.read_text(encoding="utf-8") else 1

    changed = patch_file(header)
    print(f"{'patched' if changed else 'already patched'}: {header}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

