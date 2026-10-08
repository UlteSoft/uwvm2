#!/usr/bin/env python3
"""Bind source/SDK/COFF/PE data for the fixed Windows component build.

Execution and process ownership stay in the original PIDFD guardian. This
helper does not execute native tools, the PE, a shell, or a guest.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


SOURCE_SHA = "2893d8263494c435c7ad6d4e4eac31b3722fea05447f8b3a3c9e621379f55be9"
TOOLS = {
    "clang++": "0e800e4decf0de00b5ebeefed814b2cf6551d17291a220ee107b5198cd7ca23b",
    "ld.lld": "2c67e1532947128d789d0c4696f75ff26d30400cd658361604b46806fd61b105",
    "llvm-readobj": "cc4a1825e38b09d9522508b50c7b5bafe99af3451ad23b7bc40da4a6d9c9a7b7",
    "llvm-objdump": "0925143d4d994e24f2875b9dd6a7778a880fd8372a60d7053092b59dda4fb7cd",
}


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def file_record(path):
    before = path.stat()
    row = {"path": str(path), "resolved": str(path.resolve(strict=True)),
           "bytes": before.st_size, "device": before.st_dev, "inode": before.st_ino,
           "sha256": sha(path)}
    after = path.stat()
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
        raise RuntimeError("source/provider changed while hashing")
    return row


def write_new(path, row):
    data = (json.dumps(row, indent=2, sort_keys=True) + "\n").encode()
    with path.open("xb") as stream:
        stream.write(data)
    return hashlib.sha256(data).hexdigest()


def pinned_record(path, pinned):
    actual = file_record(Path(path))
    candidates = ([pinned[path]] if path in pinned else
                  [row for row in pinned.values() if row["resolved"] == actual["resolved"]])
    identity = {name: value for name, value in actual.items() if name != "path"}
    if not any({name: value for name, value in row.items() if name != "path"} == identity for row in candidates):
        raise RuntimeError("actual unpinned or changed compiler/linker input: " + path)
    return actual


def coff_header(path):
    with path.open("rb") as stream:
        head = stream.read(20)
    if len(head) != 20:
        raise RuntimeError("truncated actual COFF object")
    machine, count, timestamp, symbols, symbol_count, optional_size, flags = struct.unpack("<HHIIIHH", head)
    if machine != 0x8664 or optional_size != 0 or not 0 < count < 32768:
        raise RuntimeError("not the expected actual Windows x64 COFF object")
    return {"coff_machine": machine, "coff_sections": count, "coff_symbol_count": symbol_count}


def pe_header(path):
    with path.open("rb") as stream:
        dos = stream.read(64)
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise RuntimeError("not an actual DOS/PE image")
        offset = struct.unpack_from("<I", dos, 60)[0]
        if not 64 <= offset <= path.stat().st_size - 26:
            raise RuntimeError("PE header offset outside its real file")
        stream.seek(offset)
        head = stream.read(26)
    if head[:4] != b"PE\0\0" or struct.unpack_from("<H", head, 4)[0] != 0x8664 or struct.unpack_from("<H", head, 24)[0] != 0x20B:
        raise RuntimeError("wrong actual Windows x64 PE32+ target")
    return {"pe_machine": 0x8664, "pe_optional_magic": 0x20B}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("phase", choices=("before", "object", "binary", "after"))
    parser.add_argument("--label")
    args = parser.parse_args()
    pp = args.plan.resolve(strict=True)
    plan = json.loads(pp.read_text())
    if (plan["schema"] != "uwvm-windows-raii-cold-build-v1"
            or plan["target"] != "x86_64-w64-windows-gnu"
            or plan["guest_execution_accepted"] is not False
            or plan["product_three_tu_abi_qualified"] is not False):
        raise RuntimeError("unexpected standalone Windows component scope")
    source, sdk, toolchain, output = (Path(plan[name]) for name in ("source", "sdk", "toolchain", "output"))
    if (str(sdk) != "/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk"
            or str(toolchain) != "/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm"):
        raise RuntimeError("unexpected Windows component provider roots")
    if sha(output / "commands.json") != plan["commands_sha256"]:
        raise RuntimeError("fixed Windows command manifest changed")
    sm = source.parent / "manifest.json"
    if sha(sm) != SOURCE_SHA or plan["source_manifest_sha256"] != SOURCE_SHA:
        raise RuntimeError("immutable Windows source/header closure changed")
    if args.phase == "before":
        paths = {pp, output / "commands.json", Path(__file__).resolve(), sm}
        for row in json.loads(sm.read_text())["files"]:
            path = source / row["path"]
            if path.stat().st_size != row["bytes"] or sha(path) != row["sha256"]:
                raise RuntimeError("frozen Windows component source changed")
            paths.add(path)
        for root in (sdk / "include/c++/v1", sdk / "x86_64-w64-mingw32/include",
                     sdk / "x86_64-w64-mingw32/lib", toolchain / "lib/clang/23/include",
                     toolchain / "lib/clang/23/lib/x86_64-w64-windows-gnu"):
            paths.update(path for path in root.rglob("*") if path.is_file())
        # Native compiler/LLVM inspection tools dynamically use the paired
        # host libraries selected by the exact toolchain LD_LIBRARY_PATH.
        # Pin their complete shared-library set independently of target PE
        # archive/import providers; neither establishes guest DLL closure.
        paths.update(path for path in (toolchain / "lib").rglob("*.so*") if path.is_file())
        for name, expected in TOOLS.items():
            path = toolchain / "bin" / name
            if sha(path) != expected:
                raise RuntimeError("exact native Windows cross-tool changed")
            paths.add(path)
        config = (sdk / "x86_64-w64-mingw32/include/__config_site").read_text()
        if "#define _LIBCPP_ABI_VERSION 1" not in config or "#define _LIBCPP_ABI_NAMESPACE __1" not in config:
            raise RuntimeError("actual target libc++ ABI1 config required")
        for variant in plan["variants"]:
            for name in ("object", "binary", "depfile", "link_map"):
                if Path(variant[name]).exists():
                    raise RuntimeError("cold Windows component output already exists")
        print(write_new(output / "input-before.json", {"phase": "before", "plan_sha256": sha(pp),
              "files": [file_record(path) for path in sorted(paths)], "actual_target_libcpp_abi": "__1",
              "product_three_tu_abi_qualified": False, "guest_execution_accepted": False}))
        return
    before = json.loads((output / "input-before.json").read_text())
    if before["plan_sha256"] != sha(pp):
        raise RuntimeError("fixed Windows plan changed after admission")
    pinned = {row["path"]: row for row in before["files"]}
    if args.phase == "after":
        for name, row in pinned.items():
            if file_record(Path(name)) != row:
                raise RuntimeError("Windows source/tool/provider closure changed")
        for variant in plan["variants"]:
            saved = json.loads((output / (variant["label"] + "-binary.json")).read_text())
            if file_record(Path(variant["binary"])) != saved["binary"]:
                raise RuntimeError("bound fresh Windows component binary changed")
        print(write_new(output / "input-after.json", {"phase": "after", "input_files": len(pinned),
              "input_before_sha256": sha(output / "input-before.json"),
              "guest_execution_accepted": False, "product_three_tu_abi_qualified": False,
              "native_instruction_bridge_review_accepted": False}))
        return
    variants = [row for row in plan["variants"] if row["label"] == args.label]
    if len(variants) != 1:
        raise RuntimeError("unknown exact Windows component variant")
    variant = variants[0]
    if args.phase == "object":
        dep = Path(variant["depfile"])
        text = dep.read_text().replace("\\\n", " ")
        if "\\" in text or ":" not in text:
            raise RuntimeError("unsupported actual dependency-file encoding")
        dependencies = text.split(":", 1)[1].split()
        if not dependencies:
            raise RuntimeError("compiler emitted no actual dependency closure")
        for name in dependencies:
            pinned_record(name, pinned)
        obj = Path(variant["object"])
        print(write_new(output / (variant["label"] + "-object.json"),
              {"object": file_record(obj), **coff_header(obj), "depfile": file_record(dep),
               "actual_dependencies": dependencies}))
    else:
        bound = json.loads((output / (variant["label"] + "-object.json")).read_text())
        if file_record(Path(variant["object"])) != bound["object"]:
            raise RuntimeError("bound Windows object changed before linking")
        link_map = Path(variant["link_map"])
        providers = sorted(set(re.findall(r"(/[^\s():]+\.(?:a|o|obj|lib))(?=\(|:|\s|$)", link_map.read_text())))
        if not providers:
            raise RuntimeError("actual Windows link map has no provider inputs")
        actual = [pinned_record(name, pinned) for name in providers if name != variant["object"]]
        binary = Path(variant["binary"])
        print(write_new(output / (variant["label"] + "-binary.json"),
              {"binary": file_record(binary), **pe_header(binary), "providers": actual,
               "link_map": file_record(link_map), "guest_execution_accepted": False,
               "product_three_tu_abi_qualified": False, "DLL_runtime_closure_qualified": False}))


if __name__ == "__main__":
    main()
