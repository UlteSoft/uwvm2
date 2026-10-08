#!/usr/bin/env python3
"""Data checks around the fixed checkpoint s390x cold command plan.

The existing outer PIDFD supervisor performs admission and owns execution.
This helper never runs a compiler, emulator, shell, or target program.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


SOURCE_MANIFEST_SHA256 = "c42b492193ca87246d1f27c5591160ec7da185e020f06a1f97a06e0b08d97d02"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def file_record(path):
    path = Path(path)
    before = path.stat()
    result = {"path": str(path), "resolved": str(path.resolve(strict=True)),
              "bytes": before.st_size, "device": before.st_dev, "inode": before.st_ino,
              "sha256": sha(path)}
    after = path.stat()
    if (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) != (
            before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns):
        raise RuntimeError(f"input changed during hashing: {path}")
    return result


def write_new(path, value):
    data = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()
    with path.open("xb") as stream:
        stream.write(data)
    return hashlib.sha256(data).hexdigest()


def pinned_record(path, pinned):
    actual = file_record(Path(path))
    # Clang can spell its GCC runtime search directory with '..' components;
    # require the actual same frozen file identity instead of rejecting a
    # harmless alias or admitting an unrecorded host header/provider.
    candidates = ([pinned[path]] if path in pinned else
                  [row for row in pinned.values() if row["resolved"] == actual["resolved"]])
    identity = {name: value for name, value in actual.items() if name != "path"}
    if not any({name: value for name, value in row.items() if name != "path"} == identity
               for row in candidates):
        raise RuntimeError(f"actual unpinned or changed input: {path}")
    return actual


def elf_header(path, executable=False):
    with path.open("rb") as stream:
        header = stream.read(64)
    if len(header) != 64 or header[:4] != b"\x7fELF" or header[4:6] != b"\x02\x02":
        raise RuntimeError(f"not a real ELF64 big-endian target object: {path}")
    kind, machine = struct.unpack(">HH", header[16:20])
    if machine != 22 or (kind not in (2, 3) if executable else kind != 1):
        raise RuntimeError(f"wrong s390x target object kind/machine: {path}: {kind}/{machine}")
    return {"elf_class": 2, "elf_data": 2, "elf_machine": machine, "elf_type": kind}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("phase", choices=("before", "object", "binary", "output", "after"))
    parser.add_argument("--label")
    args = parser.parse_args()
    plan_path = args.plan.resolve(strict=True)
    plan = json.loads(plan_path.read_text())
    if (plan["schema"] != "uwvm-checkpoint-s390x-cold-v1"
            or plan["checkpoint_format_version"] != 3
            or plan["target"] != "s390x-linux-gnu" or plan["paired_runtime_or_product_qualification"] is not False):
        raise RuntimeError("unexpected cold plan")
    out, source, deps, toolchain = (Path(plan[name]) for name in ("output", "source", "deps", "toolchain"))
    if sha(out / "commands.json") != plan["commands_sha256"]:
        raise RuntimeError("fixed command manifest changed")
    source_manifest = source.parent / "manifest.json"
    if sha(source_manifest) != SOURCE_MANIFEST_SHA256:
        raise RuntimeError("frozen dual-product source manifest changed")
    source_files = json.loads(source_manifest.read_text())["files"]
    if args.phase == "before":
        paths = {plan_path, out / "commands.json", Path(__file__).resolve(), source_manifest}
        for row in source_files:
            path = source / row["path"]
            if path.stat().st_size != row["bytes"] or sha(path) != row["sha256"]:
                raise RuntimeError(f"frozen source changed: {row['path']}")
            paths.add(path)
        for root in (deps / "usr/s390x-linux-gnu/include", deps / "usr/s390x-linux-gnu/lib",
                     deps / "usr/lib/gcc-cross/s390x-linux-gnu/15"):
            paths.update(path for path in root.rglob("*") if path.is_file())
        for root in (toolchain / "lib/clang").glob("*/include"):
            paths.update(path for path in root.rglob("*") if path.is_file())
        paths.update(toolchain / "bin" / name for name in ("clang++", "ld.lld"))
        paths.add(deps / "usr/bin/qemu-s390x")
        for variant in plan["variants"]:
            for name in ("object", "binary", "dump", "depfile", "link_map"):
                if Path(variant[name]).exists():
                    raise RuntimeError(f"cold output already exists: {variant[name]}")
        record = {"phase": "before", "plan_sha256": sha(plan_path),
                  "files": [file_record(path) for path in sorted(paths)],
                  "target_cxx_provider": plan["target_cxx_provider"],
                  "full_vm_continuation_restore_accepted": False,
                  "paired_runtime_or_product_qualification": False}
        print(write_new(out / "input-before.json", record))
        return
    before = json.loads((out / "input-before.json").read_text())
    if before["plan_sha256"] != sha(plan_path):
        raise RuntimeError("plan changed after admission")
    pinned = {row["path"]: row for row in before["files"]}
    if args.phase == "after":
        changed = [name for name, row in pinned.items() if file_record(Path(name)) != row]
        if changed:
            raise RuntimeError(f"input closure changed: {changed[:10]}")
        outputs = []
        for variant in plan["variants"]:
            outputs.append(json.loads((out / (variant["label"] + "-output.json")).read_text()))
        print(write_new(out / "input-after.json", {"phase": "after", "input_files": len(pinned),
              "input_before_sha256": sha(out / "input-before.json"), "outputs": outputs,
              "native_byte_order_witness": "requires original QEMU command exit zero with exact --require-big; outer raw receipt is authoritative",
              "target_execution_accepted": False,
              "full_vm_continuation_restore_accepted": False,
              "paired_runtime_or_product_qualification": False}))
        return
    matches = [variant for variant in plan["variants"] if variant["label"] == args.label]
    if len(matches) != 1:
        raise RuntimeError("unknown exact cold variant")
    variant = matches[0]
    label = variant["label"]
    if args.phase == "object":
        dependency_text = Path(variant["depfile"]).read_text().replace("\\\n", " ")
        # Frozen paths have no whitespace; reject a make escaping convention
        # that this narrow witness cannot parse rather than omit dependencies.
        if "\\" in dependency_text or ":" not in dependency_text:
            raise RuntimeError("unsupported dependency-file encoding")
        headers = dependency_text.split(":", 1)[1].split()
        if not headers:
            raise RuntimeError("compiler emitted no actual dependency closure")
        for name in headers:
            pinned_record(name, pinned)
        path = Path(variant["object"])
        record = {"object": file_record(path), **elf_header(path), "actual_dependencies": headers,
                  "depfile": file_record(Path(variant["depfile"]))}
        print(write_new(out / (label + "-object.json"), record))
    elif args.phase == "binary":
        object_receipt = json.loads((out / (label + "-object.json")).read_text())
        if file_record(Path(variant["object"])) != object_receipt["object"]:
            raise RuntimeError("bound input object changed before link")
        map_path = Path(variant["link_map"])
        text = map_path.read_text()
        # Record every input archive/shared object/startup object present in
        # LLD's map. Names from symbols and output sections are not providers.
        providers = sorted(set(re.findall(r"(/[^\s():]+\.(?:a|o|so(?:\.[0-9]+)*))(?=\(|:|\s|$)", text)))
        if not providers:
            raise RuntimeError("actual LLD map contains no provider inputs")
        bound_providers = []
        for name in providers:
            if name == variant["object"]:
                continue
            bound_providers.append(pinned_record(name, pinned))
        binary = Path(variant["binary"])
        record = {"binary": file_record(binary), **elf_header(binary, executable=True),
                  "link_map": file_record(map_path), "providers": bound_providers,
                  "paired_runtime_or_product_qualification": False}
        print(write_new(out / (label + "-binary.json"), record))
    else:
        binary_receipt = json.loads((out / (label + "-binary.json")).read_text())
        if file_record(Path(variant["binary"])) != binary_receipt["binary"]:
            raise RuntimeError("target executable changed during QEMU test")
        dump = file_record(Path(variant["dump"]))
        if dump["bytes"] != plan["canonical_model_bytes"] or dump["sha256"] != plan["canonical_model_sha256"]:
            raise RuntimeError("actual big-endian target output does not match the independent canonical model")
        print(write_new(out / (label + "-output.json"), {"label": label, "dump": dump,
              "binary_receipt_sha256": sha(out / (label + "-binary.json")),
              "golden_model_sha256": plan["canonical_model_sha256"],
              "full_vm_continuation_restore_accepted": False,
              "paired_runtime_or_product_qualification": False}))


if __name__ == "__main__":
    main()
