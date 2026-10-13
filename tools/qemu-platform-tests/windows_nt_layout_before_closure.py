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


SOURCE_SHA = "c058c6d133b3cbeb72a7af7ecc3360775cab8667384c59aca0ee91e56408672d"
TOOLS = {
    "clang++": "0e800e4decf0de00b5ebeefed814b2cf6551d17291a220ee107b5198cd7ca23b",
    "ld.lld": "2c67e1532947128d789d0c4696f75ff26d30400cd658361604b46806fd61b105",
    "llvm-readobj": "cc4a1825e38b09d9522508b50c7b5bafe99af3451ad23b7bc40da4a6d9c9a7b7",
    "llvm-objdump": "0925143d4d994e24f2875b9dd6a7778a880fd8372a60d7053092b59dda4fb7cd",
}


STATIC_RUNTIME = {
    "/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk/x86_64-w64-mingw32/lib/libc++.a":
        "aa0f98c5deb041ee1894938473c514de4c1b21384b3087831b1b6236f36c6b96",
    "/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk/x86_64-w64-mingw32/lib/libc++abi.a":
        "0741954703fa94b04af33a4e861df97423841c5db91e630c17432126e2eb257b",
    "/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk/x86_64-w64-mingw32/lib/libunwind.a":
        "6c26b3a7446360327d1c71197b134c2c536afbdf8c21e73f3c53ff3b9f450544",
    "/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/clang/23/lib/x86_64-w64-windows-gnu/libclang_rt.builtins.a":
        "17c4db794b21e489a1356d35666a647ae0203faff9c0f08b5144798940c6435f",
}
RUNTIME_RECIPE = ["--rtlib=compiler-rt", "--unwindlib=libunwind", "-static-libgcc", "-static-libstdc++"]


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


def pe_imports(path):
    # Read a bounded immutable PE generated in this cold cell. Every descriptor,
    # RVA and string is mapped to a real file range before decoding it.
    size = path.stat().st_size
    if not 64 <= size <= 32 << 20:
        raise RuntimeError("cold standalone PE exceeds the fixed parser byte budget")
    data = path.read_bytes()
    if len(data) != size or data[:2] != b"MZ":
        raise RuntimeError("truncated standalone PE import input")
    pe = struct.unpack_from("<I", data, 60)[0]
    if not 64 <= pe <= size - 24 or data[pe:pe + 4] != b"PE\0\0":
        raise RuntimeError("actual PE signature outside the file")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_bytes = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    section = optional + optional_bytes
    if (not 1 <= count <= 4096 or optional_bytes < 128 or section > size
            or count > (size - section) // 40
            or struct.unpack_from("<H", data, optional)[0] != 0x20B):
        raise RuntimeError("actual PE32+ section/optional header is out of bounds")
    header_bytes = struct.unpack_from("<I", data, optional + 60)[0]
    directory_count = struct.unpack_from("<I", data, optional + 108)[0]
    if directory_count < 2 or header_bytes > size:
        raise RuntimeError("actual PE has no bounded import directory")
    rva, directory_bytes = struct.unpack_from("<II", data, optional + 120)
    if not rva or not 20 <= directory_bytes <= 128 << 10:
        raise RuntimeError("actual PE import directory has invalid bounds")
    rows = []
    for index in range(count):
        offset = section + index * 40
        virtual_bytes, virtual, raw_bytes, raw = struct.unpack_from("<IIII", data, offset + 8)
        if raw > size or raw_bytes > size - raw:
            raise RuntimeError("actual PE raw section exceeds the file")
        rows.append((virtual, raw_bytes, raw))

    def file_offset(value, length):
        candidates = []
        if value < header_bytes and length <= header_bytes - value:
            candidates.append(value)
        for virtual, raw_bytes, raw in rows:
            if value >= virtual and value - virtual < raw_bytes and length <= raw_bytes - (value - virtual):
                candidates.append(raw + value - virtual)
        if len(candidates) != 1 or candidates[0] > size or length > size - candidates[0]:
            raise RuntimeError("actual import RVA does not map to one complete real file range")
        return candidates[0]

    imports = []
    terminated = False
    for index in range(directory_bytes // 20):
        offset = file_offset(rva + index * 20, 20)
        descriptor = struct.unpack_from("<IIIII", data, offset)
        if not any(descriptor):
            terminated = True
            break
        name_rva = descriptor[3]
        name = bytearray()
        for position in range(256):
            char = data[file_offset(name_rva + position, 1)]
            if char == 0:
                break
            name.append(char)
        else:
            raise RuntimeError("actual DLL import name is not bounded/NUL terminated")
        if not name or any(char < 0x21 or char > 0x7E for char in name):
            raise RuntimeError("actual DLL import name is not printable ASCII")
        imports.append(name.decode("ascii").casefold())
    if not terminated or len(imports) != len(set(imports)):
        raise RuntimeError("actual import descriptors are incomplete/duplicated")
    return sorted(imports)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("phase", choices=("before", "object", "binary", "after"))
    parser.add_argument("--label")
    args = parser.parse_args()
    pp = args.plan.resolve(strict=True)
    plan = json.loads(pp.read_text())
    if (plan["schema"] != "uwvm-windows-nt-layout-before-sdk-cold-build-r1-v1"
            or plan["target"] != "x86_64-w64-windows-gnu"
            or plan["guest_execution_accepted"] is not False
            or plan["product_three_tu_abi_qualified"] is not False):
        raise RuntimeError("unexpected standalone Windows component scope")
    source, sdk, toolchain, output = (Path(plan[name]) for name in ("source", "sdk", "toolchain", "output"))
    if (str(sdk) != "/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk"
            or str(toolchain) != "/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm"):
        raise RuntimeError("unexpected Windows component provider roots")
    if plan.get("runtime_recipe") != RUNTIME_RECIPE:
        raise RuntimeError("unexpected LLVM static runtime recipe")
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
        for name, expected in STATIC_RUNTIME.items():
            path = Path(name)
            if not path.is_file() or sha(path) != expected:
                raise RuntimeError("fixed standalone LLVM target archive changed: " + name)
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
              "SDK_layout_guest_counterexample_accepted": False}))
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
        # LLVM23.1.1 COFF MapFile deliberately prints archive/object basenames.
        # The MinGW --verbose driver forwards -verbose to the COFF driver,
        # whose Reading records include the original absolute archive paths.
        # Bind those real inputs instead of guessing paths from a map basename.
        link_log = Path(variant["link_log"])
        if (link_log.parent != Path(plan["evidence"]) or link_log.is_symlink()
                or not link_log.is_file() or link_log.stat().st_size > 32 << 20):
            raise RuntimeError("missing actual completed regular linker log")
        readings = re.findall(r"^.*?\bReading (.+)$", link_log.read_text(), re.MULTILINE)
        providers = sorted({value for value in readings
                            if re.fullmatch(r"/[^\s():]+\.(?:a|o|obj|lib)", value)})
        if variant["object"] not in providers or not any(value.endswith("/libc++.a") for value in providers):
            raise RuntimeError("actual linker log has no complete absolute object/libc++ inputs")
        archive_names = {}
        for value in providers:
            if value.endswith((".a", ".lib")):
                archive_names.setdefault(Path(value).name, []).append(value)
        member_readings = []
        for value in readings:
            if value in providers:
                continue
            member = re.fullmatch(r"([^/\s()]+\.(?:a|lib))\(([^\r\n()]+)\)", value)
            if member is None or len(archive_names.get(member[1], [])) != 1:
                raise RuntimeError("unknown or ambiguous actual COFF Reading input: " + value)
            member_readings.append({"archive": archive_names[member[1]][0], "member": member[2]})
        actual = [pinned_record(name, pinned) for name in providers if name != variant["object"]]
        actual_resolved = {row["resolved"] for row in actual}
        # Pin the available ABI companion without inventing a contributing archive.
        # This SDK libc++.a can carry ABI members; demand actual libc++/unwind/
        # builtins Reading input, and bind libc++abi.a if it really contributes.
        required = {str(Path(name).resolve(strict=True)) for name in STATIC_RUNTIME
                    if Path(name).name != "libc++abi.a"}
        if not required <= actual_resolved:
            raise RuntimeError("actual Reading inputs omit a fixed LLVM static runtime archive")
        if any(re.search(r"(?:^|/)(?:libgcc(?:_s|_eh)?|libstdc\+\+)(?:[.-]|$)", value)
               for value in providers):
            raise RuntimeError("actual GNU unwind/C++ archive remains in the LLVM static recipe")
        binary = Path(variant["binary"])
        imports = pe_imports(binary)
        if not {"kernel32.dll", "ntdll.dll"} <= set(imports):
            raise RuntimeError("actual standalone PE does not import its fixed Windows kernel/NT APIs")
        if any(name.startswith(("libgcc", "libstdc++", "libc++", "libunwind")) for name in imports):
            raise RuntimeError("actual PE still imports a dynamic GNU or LLVM C++/unwind runtime")
        print(write_new(output / (variant["label"] + "-binary.json"),
              {"binary": file_record(binary), **pe_header(binary), "providers": actual,
               "link_map": file_record(link_map), "link_log": file_record(link_log),
               "actual_reading_inputs": providers, "actual_archive_members": member_readings,
               "actual_DLL_import_names": imports, "static_runtime_recipe": RUNTIME_RECIPE,
               "GNU_unwind_runtime_rejected": True, "SDK_static_runtime_provider_bound": True,
               "guest_execution_accepted": False,
               "product_three_tu_abi_qualified": False, "DLL_runtime_closure_qualified": False}))


if __name__ == "__main__":
    main()
