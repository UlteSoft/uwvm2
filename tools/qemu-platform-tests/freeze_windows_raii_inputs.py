#!/usr/bin/env python3
"""Freeze the exact Windows RAII component and current fast_io headers.

This is source preparation only. It never invokes a compiler, guest, QEMU,
shell, or native product, and does not qualify the product's three-TU ABI.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile


OWNER_ARCHIVE_SHA = "dcd5ed14e1ad2b217634c3aa26cede22588c239fe2879755bcc9d900418d55b9"
OWNER_MANIFEST_SHA = "6e9ba180e3ddd34983203f90942a2cb7dd81685bd7723dfdf0fbd156b2ec93e6"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--owner-source", type=Path, required=True)
    parser.add_argument("--owner-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    roots = {"uwvm2": args.ordinary.resolve(strict=True),
             "uwvm2-ros": args.ros.resolve(strict=True)}
    archive = args.owner_source.read_bytes()
    owner_manifest = args.owner_manifest.read_bytes()
    if sha(archive) != OWNER_ARCHIVE_SHA or sha(owner_manifest) != OWNER_MANIFEST_SHA:
        raise RuntimeError("immutable Windows RAII owner source changed")
    original = json.loads(owner_manifest)
    wanted = {row["repo"] + "/" + row["path"]: row for row in original["files"]}
    if len(wanted) != 14:
        raise RuntimeError("unexpected Windows component inventory")
    records = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as stream:
        for member in stream:
            canonical = member.name.removeprefix("uwvm-native-windows-raii-source-frozen-20261003-r1/")
            if canonical == "manifest.json":
                if not member.isfile() or stream.extractfile(member).read() != owner_manifest:
                    raise RuntimeError("embedded Windows owner manifest differs")
                continue
            if canonical not in wanted:
                continue
            name = PurePosixPath(canonical)
            if (not member.isfile() or name.is_absolute() or ".." in name.parts
                    or canonical in records):
                raise RuntimeError("invalid Windows owner source member")
            data = stream.extractfile(member).read()
            row = wanted[canonical]
            if len(data) != row["bytes"] or sha(data) != row["sha256"]:
                raise RuntimeError("Windows owner source leaf differs from its manifest")
            records[canonical] = data
    if records.keys() != wanted.keys():
        raise RuntimeError("Windows owner source packet is incomplete")
    for repo, root in roots.items():
        for path in sorted((root / "third-parties/fast_io/include").rglob("*")):
            if path.is_symlink():
                raise RuntimeError("unexpected fast_io source symlink")
            if path.is_file():
                records[repo + "/" + path.relative_to(root).as_posix()] = path.read_bytes()
    for row in original["fast_io_required_dependency_hashes"]:
        if sha(records[row["repo"] + "/" + row["path"]]) != row["sha256"]:
            raise RuntimeError("Windows owner-required fast_io dependency changed")
    for repo, root in roots.items():
        observed = {repo + "/" + path.relative_to(root).as_posix()
                    for path in (root / "third-parties/fast_io/include").rglob("*") if path.is_file()}
        expected = {name for name in records if name.startswith(repo + "/third-parties/fast_io/include/")}
        if observed != expected:
            raise RuntimeError("fast_io header inventory changed while freezing")
        for name in expected:
            if (root / PurePosixPath(name).relative_to(repo)).read_bytes() != records[name]:
                raise RuntimeError("fast_io header changed while freezing")
    if records["uwvm2/test/0017.runtime/native_step_windows_raii_x86_64.cc"] != records[
            "uwvm2-ros/test/0017.runtime/native_step_windows_raii_x86_64.cc"]:
        raise RuntimeError("paired Windows test fixtures differ")
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    manifest = {"schema": "uwvm-windows-raii-component-source-v1", "kind": "source-only",
                "native_tests_executed": False, "product_three_tu_abi_qualified": False,
                "named_module_qualified": False, "target": "x86_64-w64-windows-gnu",
                "owner_archive_sha256": OWNER_ARCHIVE_SHA, "owner_manifest_sha256": OWNER_MANIFEST_SHA,
                "files": [{"path": name, "bytes": len(data), "sha256": sha(data)}
                          for name, data in sorted(records.items())]}
    manifest_data = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    (output / "manifest.json").write_bytes(manifest_data)
    with tarfile.open(output / "source.tar.gz", "w:gz") as stream:
        for name, data in sorted(records.items()):
            member = tarfile.TarInfo(name)
            member.size, member.mode = len(data), 0o600
            stream.addfile(member, io.BytesIO(data))
    packet = (output / "source.tar.gz").read_bytes()
    print(json.dumps({"source_archive_bytes": len(packet), "source_archive_sha256": sha(packet),
                      "manifest_sha256": sha(manifest_data), "files": len(records)}))


if __name__ == "__main__":
    main()
