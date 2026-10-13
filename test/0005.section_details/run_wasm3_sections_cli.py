#!/usr/bin/env python3
"""Fresh-product Core 3 diagnostic and debug-shortcut checks, Linux keeper only.

The source ID labels the keeper's separately qualified build. It is not a
substitute for that build's compiler/object/dependency provenance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import struct
import subprocess


FEATURES = ["-WFE-gc", "-WFE-function-references", "-WFE-exceptions",
            "-WFE-memory64", "-WFE-table64", "-WFE-multi-memory"]
ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def leb(value):
    result = bytearray()
    while True:
        part = value & 127
        value >>= 7
        result.append(part | (128 if value else 0))
        if not value:
            return bytes(result)


def custom(name, payload):
    name = name.encode("utf-8")
    contents = leb(len(name)) + name + payload
    return b"\x00" + leb(len(contents)) + contents


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "uwvm", "wasm-tools", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", args.source_id):
        parser.error("a separately qualified source identity is required")
    root = args.source_root.resolve(strict=True)
    binary = args.uwvm.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 << 20, 8 << 20))
    args.out.mkdir(parents=True, exist_ok=False)
    fixture = root / "test/0005.section_details/wasm3_sections.wat"
    immutable = [Path(__file__).resolve(), fixture, binary, wasm_tools]
    before = {str(path): digest(path) for path in immutable}
    rows = []

    def check(name, argv, required=(), forbidden=(), success=True, stdin=None):
        argv = list(map(str, argv))
        stdout = args.out / (name + ".stdout.log")
        stderr = args.out / (name + ".stderr.log")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            try:
                run = subprocess.run(argv, input=stdin, stdout=out, stderr=err, timeout=45)
                code = run.returncode
            except subprocess.TimeoutExpired:
                code = None
        text = ANSI.sub(b"", stdout.read_bytes() + stderr.read_bytes())
        passed = (code == 0 if success else code is not None and code > 0)
        passed = passed and all(part in text for part in required) and not any(part in text for part in forbidden)
        rows.append({"name": name, "argv": argv, "stdin": None if stdin is None else stdin.decode("ascii"),
                     "exit": code, "passed": passed, "expected_success": success,
                     "required": [part.decode() for part in required], "forbidden": [part.decode() for part in forbidden],
                     "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr)})
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(json.dumps({"name": name, "passed": passed}), flush=True)
        return passed

    wasm = args.out / "core3-sections.wasm"
    if not check("parse-core3", [wasm_tools, "parse", fixture, "-o", wasm]):
        raise RuntimeError("official WAT assembler rejected the Core 3 fixture")
    if not check("validate-core3", [wasm_tools, "validate", "--features", "all", wasm]):
        raise RuntimeError("official validator rejected the Core 3 fixture")
    image = wasm.read_bytes()
    image += custom(".debug_info", bytes([9,0,0,0,5,0,1,4,0,0,0,0,0]))
    image += custom(".debug_line", b"\x01\x02\x03")
    image += custom(".debug_abbrev", b"\x00")
    image += custom("unsafe\n)\\\0name", b"\x07")
    wasm.write_bytes(image)
    check("validate-opaque-custom", [wasm_tools, "validate", "--features", "all", wasm])
    check("section-details", [binary, "-m", "section-details", *FEATURES, "--run", wasm], required=(
        b"(struct (field (mut i8)) (field (ref null 0)))", b"(array (mut i16))",
        b"(func (param (ref null 0) v128) (result (ref 1)))", b"type: (ref null 2)",
        b"4294967296", b"4294967297", b"address64: 1", b"Tag[2]",
        b"Import[5]", b"table[0]: {type: (ref null 2)", b"memory[0]: {",
        b"global[0]: {type: (ref null 0), mutable: 1}", b"global[1]: {type: (ref 0), mutable: 0}",
        b"localtag[0] -> tag[1]: {type: 3}", b"localtag[1] -> tag[2]: {type: 3}",
        b"type: (ref null 0)", b"custom (.debug_info): size = 13, content-size = 25",
        b"unit-headers = complete, dwarf32-units = 1, dwarf64-units = 0, versions = 5",
        b"unit-headers = truncated", b"kind = DWARF auxiliary", b"unsafe\\x0a\\x29\\x5c\\x00name"),
        forbidden=(b"(array (field", b"(uwvm-debug)"))

    alias_wat = args.out / "debug-shortcut-modern-eh.wat"
    alias_wat.write_text('''(module (tag $e (param i32))
  (func (export "_start") (block $caught (result i32)
    (try_table (catch $e $caught) i32.const 7 throw $e) unreachable) drop))\n''')
    alias_wasm = args.out / "debug-shortcut-modern-eh.wasm"
    if not check("parse-shortcut-eh", [wasm_tools, "parse", alias_wat, "-o", alias_wasm]):
        raise RuntimeError("official assembler rejected the modern EH debug fixture")
    if not check("validate-shortcut-eh", [wasm_tools, "validate", "--features", "all", alias_wasm]):
        raise RuntimeError("official validator rejected the modern EH debug fixture")
    for name, mode in (("short", ["-Rdbg"]), ("long", ["--runtime-debug"]), ("mode", ["-m", "debug-jit"])):
        check("debug-" + name, [binary, *mode, "-Raot", "-WFE-exceptions", "-Rllvm-cache-path", "disable", "--run", alias_wasm],
              required=(b"(uwvm-debug) ",), stdin=b"quit\n")
    check("debug-int-fatal", [binary, "-Rdbg", "-Rint", "--run", alias_wasm], success=False,
          required=(b"[fatal]", b"debug-jit is unsupported in the current mode:"), forbidden=(b"(uwvm-debug)",))
    duplicates = (("repeat", ["-Rdbg", "--runtime-debug"]),
                  ("mode-first", ["-m", "run", "-Rdbg"]),
                  ("mode-last", ["-Rdbg", "-m", "run"]))
    for name, options in duplicates:
        check("duplicate-" + name, [binary, *options, "--run", alias_wasm], success=False, forbidden=(b"(uwvm-debug)",))
    if not args.ros:
        for compiler, compilation in (("jit", "lazy"), ("tiered", "lazy")):
            check("debug-" + compiler + "-lazy-fatal", [binary, "--runtime-debug", "-Rcc", compiler, "-Rcm", compilation,
                  "--run", alias_wasm], success=False, required=(b"[fatal]", b"debug-jit is unsupported in the current mode:"),
                  forbidden=(b"(uwvm-debug)",))
    # Parsing witnesses only: valid numbers reach the startup mode guard; these
    # are not qualified endpoint capabilities and are never passed to guests.
    word_max = (1 << (8 * struct.calcsize("P"))) - 1
    numeric = (
        ("fd", "--debug-jit-control-fd", ("3", "0003", "2147483647"),
         ("", "+3", "-3", " 3", "3 ", "3x", "2", "2147483648", "9999999999999999999999999999999999")),
        ("handle", "--debug-jit-control-handle", ("4", "0004", str(word_max-1)),
         ("", "+4", "-4", " 4", "4 ", "4x", "3", str(word_max), str(word_max+1))),
    )
    for kind, option, valid, invalid in numeric:
        for index, token in enumerate(valid):
            noun = b"FD" if kind == "fd" else b"HANDLE"
            check(kind + "-decimal-valid-" + str(index), [binary, "-m", "section-details", option, token,
                  "--run", alias_wasm], success=False, required=(b"[fatal]", b"debug-jit control " + noun +
                  b" is unsupported in the current mode."), forbidden=(b"Usage:",))
        for index, token in enumerate(invalid):
            check(kind + "-decimal-invalid-" + str(index), [binary, "-m", "section-details", option, token,
                  "--run", alias_wasm], success=False, required=(b"Usage:",), forbidden=(b"(uwvm-debug)",))
    after = {str(path): digest(path) for path in immutable}
    result = {"source_id": args.source_id, "scope": "fresh-product Core 3 diagnostic text and equivalent debug shortcuts",
              "build_provenance": "qualified separately by the native keeper", "performance_qualified": False,
              "immutable_before": before, "immutable_after": after, "fixture_wasm_sha256": digest(wasm),
              "alias_wasm_sha256": digest(alias_wasm), "passed": before == after and all(row["passed"] for row in rows), "runs": rows}
    (args.out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
