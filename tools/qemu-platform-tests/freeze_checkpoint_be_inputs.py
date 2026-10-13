#!/usr/bin/env python3
"""Freeze an immutable checkpoint test and both fast_io header closures.

This prepares source data only. It does not build or execute target code.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile


R2_ARCHIVE_SHA256 = "3445c4cf84f2d2630aa8a52b3a884c501f1108d5e99d149cff16d2273a24ca9e"
R2_MANIFEST_SHA256 = "918fb12bab295054e7649d9979b2cbd95acc76109dd6a6db2bf8a69c6c5c5686"
REVISIONS = {
    "r2": (R2_ARCHIVE_SHA256, R2_MANIFEST_SHA256, 4299,
           "8fe6dc9e4ab4cd629946e57d430db50a8601961fc31949b0165b543dece557d1"),
    "r3": ("fdb7b514b272ab03bb6722d39484a1d8891c3cb368341fb79985cd23cba555f5",
           "b77db778fb5a4b642f620d3c0bc4b3e93ba9e5f502a755fca94e45d7729f7f60", 4347,
           "731ddd5ee0febc08055ac4f19faf34fd63af7f0ea9690b658622c5dabee9ff01"),
    "r4": ("faca23801eda3d71d3862feb6a870d81a833001dbe88880f392b95a7c498a2f5",
           "8bcffa4f73ba80165af0250d3edbd328ef1bdf85c4680717a079e39a58ff8d2f", 4347,
           "731ddd5ee0febc08055ac4f19faf34fd63af7f0ea9690b658622c5dabee9ff01"),
}
REPOSITORIES = ("uwvm2", "uwvm2-ros")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ordinary", type=Path, required=True)
    parser.add_argument("--ros", type=Path, required=True)
    parser.add_argument("--checkpoint-source", "--checkpoint-r2", dest="checkpoint_source", type=Path, required=True)
    parser.add_argument("--revision", choices=REVISIONS, default="r2")
    parser.add_argument("--fastio-snapshot", type=Path,
                        help="Previously immutable dual-product header snapshot; avoids newer live APIs outside the checkpoint dependency pins")
    parser.add_argument("--fastio-scalar-patch", type=Path,
                        help="Immutable owner-approved scalar SHA self-containment source patch")
    parser.add_argument("--fastio-scalar-patch-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    roots = dict(zip(REPOSITORIES, (args.ordinary.resolve(strict=True), args.ros.resolve(strict=True))))
    r2 = args.checkpoint_source.resolve(strict=True)
    archive_sha256, manifest_sha256, model_bytes, model_sha256 = REVISIONS[args.revision]
    archive = (r2 / "source-candidate.tar.gz").read_bytes()
    raw_manifest = (r2 / "manifest.json").read_bytes()
    if sha(archive) != archive_sha256 or sha(raw_manifest) != manifest_sha256:
        raise RuntimeError("checkpoint R2 immutable input changed")
    original = json.loads(raw_manifest)
    records = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as stream:
        for member in stream:
            if member.isdir():
                continue
            if member.name == "manifest.json":
                if not member.isfile() or stream.extractfile(member).read() != raw_manifest:
                    raise RuntimeError("R2 embedded manifest changed")
                continue
            path = PurePosixPath(member.name)
            if (not member.isfile() or path.is_absolute() or ".." in path.parts
                    or path.parts[0] not in REPOSITORIES or member.name in records):
                raise RuntimeError(f"unexpected R2 archive member: {member.name}")
            records[member.name] = stream.extractfile(member).read()
    for row in original["source_files"]:
        name = row["repo"] + "/" + row["path"]
        data = records[name]
        if len(data) != row["bytes"] or sha(data) != row["sha256"]:
            raise RuntimeError(f"R2 member does not match its source manifest: {name}")
    # Module import files are retained as dependency data. This planned cold
    # target test uses headers, and cannot claim a named-module importer PASS.
    frozen_headers = {}
    if args.fastio_snapshot is not None:
        header_archive = args.fastio_snapshot.read_bytes()
        if sha(header_archive) != "976ece54210df2358b780515c7bfa55b9f00ebcdb1dad0c139e0a142f6034760":
            raise RuntimeError("previous immutable fast_io snapshot changed")
        with tarfile.open(fileobj=io.BytesIO(header_archive), mode="r:gz") as stream:
            for member in stream:
                if member.isfile() and "/third-parties/fast_io/" in member.name:
                    path = PurePosixPath(member.name)
                    if path.is_absolute() or ".." in path.parts or path.parts[0] not in REPOSITORIES:
                        raise RuntimeError("invalid previous fast_io snapshot path")
                    frozen_headers[member.name] = stream.extractfile(member).read()
        if len(frozen_headers) != 1502:
            raise RuntimeError("previous complete fast_io header snapshot inventory changed")
        records.update(frozen_headers)
    patch_records = []
    if args.fastio_scalar_patch is not None:
        if not frozen_headers:
            raise RuntimeError("scalar patch requires its complete previous immutable header snapshot")
        patch_bytes = args.fastio_scalar_patch.read_bytes()
        if (args.fastio_scalar_patch_sha256 is None
                or sha(patch_bytes) != args.fastio_scalar_patch_sha256):
            raise RuntimeError("immutable scalar SHA source patch changed")
        allowed = {repo + "/third-parties/fast_io/include/fast_io_crypto/hash/" + name
                   for repo in REPOSITORIES for name in ("sha256_scalar.h", "sha512_scalar.h")}
        preimages = {name: records[name] for name in allowed}
        with tarfile.open(fileobj=io.BytesIO(patch_bytes), mode="r:gz") as stream:
            for member in stream:
                if not member.isfile():
                    continue
                name = member.name.removeprefix("after/")
                if member.name.startswith("before/") and member.name[7:] in allowed:
                    if stream.extractfile(member).read() != preimages[member.name[7:]]:
                        raise RuntimeError("scalar patch preimage differs from the complete frozen header snapshot")
                    continue
                if name not in allowed:
                    continue
                data = stream.extractfile(member).read()
                if name in {row["path"] for row in patch_records}:
                    raise RuntimeError("duplicate scalar patch member")
                patch_records.append({"path": name, "before_sha256": sha(preimages[name]),
                                      "sha256": sha(data), "bytes": len(data)})
                records[name] = data
        if {row["path"] for row in patch_records} != allowed:
            raise RuntimeError("scalar patch did not contain both products' exact SHA headers")
    for repo, root in roots.items():
        header_root = root / "third-parties/fast_io/include"
        paths = [] if frozen_headers else sorted(header_root.rglob("*"))
        for path in paths:
            if path.is_symlink():
                raise RuntimeError(f"unexpected header symlink: {path}")
            if not path.is_file():
                continue
            records[repo + "/" + path.relative_to(root).as_posix()] = path.read_bytes()
        for row in original["dependency_pins"]:
            if row["repo"] != repo:
                continue
            path = root / row["path"]
            data = (frozen_headers[repo + "/" + row["path"]] if frozen_headers else path.read_bytes())
            if len(data) != row["bytes"] or sha(data) != row["sha256"]:
                raise RuntimeError(f"R2 fast_io dependency changed: {repo}/{row['path']}")
            records[repo + "/" + row["path"]] = data
    # A shared workspace may change while this reads it. The frozen header set
    # must still match the same source owners after every read completed.
    for repo, root in ([] if frozen_headers else roots.items()):
        actual_names = {repo + "/" + path.relative_to(root).as_posix()
                        for path in (root / "third-parties/fast_io/include").rglob("*") if path.is_file()}
        frozen_names = {name for name in records if name.startswith(repo + "/third-parties/fast_io/include/")}
        if actual_names != frozen_names:
            raise RuntimeError(f"fast_io header inventory changed while freezing: {repo}")
        for name in frozen_names:
            if (root / PurePosixPath(name).relative_to(repo)).read_bytes() != records[name]:
                raise RuntimeError(f"fast_io header changed while freezing: {name}")
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    manifest = {
        "schema": "uwvm-checkpoint-qemu-be-source-v1", "kind": "source-only",
        "native_tests_executed": False, "full_vm_continuation_restore_accepted": False,
        "checkpoint_source_revision": args.revision,
        "checkpoint_format_version": original["binary_format_version"],
        "checkpoint_source_archive_sha256": archive_sha256,
        "checkpoint_source_manifest_sha256": manifest_sha256,
        "fastio_headers_from_prior_immutable_snapshot": bool(frozen_headers),
        "fastio_scalar_patch_sha256": args.fastio_scalar_patch_sha256,
        "fastio_scalar_patch_records": patch_records,
        "first_planned_target": "s390x-linux-gnu", "required_native_byte_order": "big",
        "target_cxx_provider": "existing GCC15 target libstdc++ (standalone codec only)",
        "paired_runtime_or_product_qualification": False,
        "canonical_model_bytes": model_bytes,
        "canonical_model_sha256": model_sha256,
        "files": [{"path": name, "bytes": len(data), "sha256": sha(data)}
                  for name, data in sorted(records.items())],
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    (output / "manifest.json").write_bytes(manifest_bytes)
    with tarfile.open(output / "source.tar.gz", "w:gz") as stream:
        for name, data in sorted(records.items()):
            member = tarfile.TarInfo(name)
            member.size, member.mode = len(data), 0o600
            stream.addfile(member, io.BytesIO(data))
    packet = (output / "source.tar.gz").read_bytes()
    print(json.dumps({"source_archive_bytes": len(packet), "source_archive_sha256": sha(packet),
                      "manifest_sha256": sha(manifest_bytes), "files": len(records)}, sort_keys=True))


if __name__ == "__main__":
    main()
