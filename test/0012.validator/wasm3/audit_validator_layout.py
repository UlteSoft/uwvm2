#!/usr/bin/env python3
"""Check the Core 3 validator aggregate and compiler byte-cursor diagrams.

This is a structural audit, not a proof that a comment's bounds claim is true.
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "src/uwvm2/validation/standard/wasm3"
CURSOR_SOURCES = (SOURCE, ROOT / "src/uwvm2/runtime/compiler/uwvm_int",
                  ROOT / "src/uwvm2/runtime/compiler/llvm_jit")
MODULE_PREFIX = "uwvm2.validation.standard.wasm3"
CURSOR_CHANGES = {
    name: re.compile(r"\b(?:\+\+" + name + r"|" + name + r"\+\+|" + name + r"\s*(?:\+=|-=|=(?!=)))")
    for name in ("code_curr", "cursor", "scan", "ip", "sp")
}
SAFE_DIAGRAM = re.compile(r"//\s*\[[^\n]*safe")


def aggregate_errors() -> list[str]:
    errors: list[str] = []
    headers = {path.stem for path in SOURCE.glob("*.h")} - {"impl"}
    modules = {path.stem for path in SOURCE.glob("*.cppm")} - {"impl"}
    if headers != modules:
        errors.append(f"header/module pairs differ: headers-only={sorted(headers - modules)}, modules-only={sorted(modules - headers)}")

    header_text = (SOURCE / "impl.h").read_text()
    included = set(re.findall(r'^#\s*include\s+"([a-z0-9_]+)\.h"', header_text, re.MULTILINE))
    if included != headers:
        errors.append(f"impl.h aggregate differs: missing={sorted(headers - included)}, extra={sorted(included - headers)}")

    module_text = (SOURCE / "impl.cppm").read_text()
    exported = set(re.findall(r"^export import ([^;]+);", module_text, re.MULTILINE))
    expected = {f"{MODULE_PREFIX}.{name}" for name in modules - {"validator"}} | {":validator"}
    if exported != expected:
        errors.append(f"impl.cppm aggregate differs: missing={sorted(expected - exported)}, extra={sorted(exported - expected)}")
    for name in modules:
        text = (SOURCE / f"{name}.cppm").read_text()
        declaration = f"export module {MODULE_PREFIX}{':validator' if name == 'validator' else '.' + name};"
        if declaration not in text:
            errors.append(f"{name}.cppm does not declare {declaration}")
    return errors


def old_validator_errors() -> list[str]:
    old = ["src/uwvm2/validation/standard/wasm1p1", "src/uwvm2/validation/standard/wasm2"]
    result = subprocess.run(["git", "status", "--porcelain", "--", *old], cwd=ROOT,
                            capture_output=True, text=True, check=True)
    return [f"old validator directories changed: {result.stdout.strip()}"] if result.stdout.strip() else []


def cursor_diagram_errors() -> list[str]:
    errors: list[str] = []
    for path in sorted(path for source in CURSOR_SOURCES for path in source.rglob("*.h")):
        lines = path.read_text().splitlines()
        for index, line in enumerate(lines):
            if line.lstrip().startswith("//"):
                continue
            before = "\n".join(lines[max(0, index - 14):index])
            after = "\n".join(lines[index + 1:index + 9])
            for name, pattern in CURSOR_CHANGES.items():
                # ip/sp are runtime operands, not validator cursors. Audit the
                # new Core 3 GC handler where these byte pointers are moved.
                if name in ("ip", "sp") and path.name != "gc.h":
                    continue
                if not pattern.search(line):
                    continue
                if not (SAFE_DIAGRAM.search(before) and f"^^ {name}" in before):
                    errors.append(f"{path.relative_to(ROOT)}:{index + 1}: missing preceding {name} bounds diagram")
                if not (SAFE_DIAGRAM.search(after) and f"^^ {name}" in after):
                    errors.append(f"{path.relative_to(ROOT)}:{index + 1}: missing following {name} bounds diagram")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aggregate-only", action="store_true", help="audit imports/exports and old directories only")
    args = parser.parse_args()
    errors = aggregate_errors() + old_validator_errors()
    if not args.aggregate_only:
        errors += cursor_diagram_errors()
    for error in errors:
        print(f"FAIL: {error}")
    if errors:
        return 1
    print("PASS: Core 3 validator aggregate, old-validator isolation" +
          ("" if args.aggregate_only else ", and validator/interpreter/JIT byte-pointer bounds diagrams"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
