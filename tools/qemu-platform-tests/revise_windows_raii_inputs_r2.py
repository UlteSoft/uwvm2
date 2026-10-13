#!/usr/bin/env python3
"""Freeze the exact two-leaf Windows fixture repair over the failed R1 input.

The complete immutable R1 fast_io/backend closure is retained. Other members
of the owner's range repair packet are outside this standalone component.
No native tool, test, module importer, product, or VM is run here.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


BASE_ARCHIVE = "180dacb4528cedee57cefcecf3266d7c20a5ac1ffc2fa6974432d88d2b8d624a"
BASE_MANIFEST = "2893d8263494c435c7ad6d4e4eac31b3722fea05447f8b3a3c9e621379f55be9"
DELTA_ARCHIVE = "7fa9b9ebeca1cdfa23a3d1a73c8f58919068f10d74dd9305e6877decab50753a"
DELTA_MANIFEST = "fa75ab316d86db4b674ee04c2f18afe9f9988c1f313a7a027acc16789c4f46ea"
FIXTURE = "test/0017.runtime/native_step_windows_raii_x86_64.cc"
BEFORE = "22c58bb778758a0254af3bf669f9d248cfcb4ded245ad9741399edc26117139f"
AFTER = "cd94145402a763b71e32fac462dfd39af9dcde5ce37234117f112acfd0551165"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--delta", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    archive = (args.base / "source.tar.gz").read_bytes()
    manifest = (args.base / "manifest.json").read_bytes()
    if sha(archive) != BASE_ARCHIVE or sha(manifest) != BASE_MANIFEST:
        raise RuntimeError("the preserved failed Windows R1 input changed")
    records = {}
    original = json.loads(manifest)
    wanted = {row["path"]: row for row in original["files"]}
    if len(wanted) != 1514:
        raise RuntimeError("unexpected original Windows source closure")
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as stream:
        for member in stream:
            if not member.isfile() or member.name not in wanted or member.name in records:
                raise RuntimeError("invalid original immutable source member")
            data = stream.extractfile(member).read()
            row = wanted[member.name]
            if len(data) != row["bytes"] or sha(data) != row["sha256"]:
                raise RuntimeError("original immutable source leaf changed")
            records[member.name] = data
    if records.keys() != wanted.keys():
        raise RuntimeError("incomplete original source closure")
    delta = args.delta.read_bytes()
    if sha(delta) != DELTA_ARCHIVE:
        raise RuntimeError("the owner's immutable range repair packet changed")
    prefix = "uwvm-native-fastio-range-source-frozen-20261003-r2/"
    with tarfile.open(fileobj=io.BytesIO(delta), mode="r:gz") as stream:
        owner_manifest = stream.extractfile(prefix + "manifest.json").read()
        if sha(owner_manifest) != DELTA_MANIFEST:
            raise RuntimeError("the owner's range repair manifest changed")
        owner_rows = json.loads(owner_manifest)["files"]
        for repo in ("uwvm2", "uwvm2-ros"):
            name = repo + "/" + FIXTURE
            selected = [row for row in owner_rows if row["repo"] == repo and row["path"] == FIXTURE]
            if (len(selected) != 1 or selected[0]["before_sha256"] != BEFORE or
                    selected[0]["after_sha256"] != AFTER or sha(records[name]) != BEFORE):
                raise RuntimeError("unexpected exact fixture repair provenance")
            member = stream.getmember(prefix + "source/" + name)
            if not member.isfile():
                raise RuntimeError("fixture repair is not a regular source leaf")
            repaired = stream.extractfile(member).read()
            if len(repaired) != selected[0]["after_bytes"] or sha(repaired) != AFTER:
                raise RuntimeError("fixture repair bytes changed")
            records[name] = repaired
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    updated = {**original, "source_revision": "r2-exact-fixture-range-repair",
               "base_manifest_sha256": BASE_MANIFEST, "base_archive_sha256": BASE_ARCHIVE,
               "fixture_delta_archive_sha256": DELTA_ARCHIVE,
               "fixture_delta_manifest_sha256": DELTA_MANIFEST,
               "new_readonly_sync_provider_qualified": False,
               "files": [{"path": name, "bytes": len(data), "sha256": sha(data)}
                         for name, data in sorted(records.items())]}
    data = (json.dumps(updated, indent=2, sort_keys=True) + "\n").encode()
    (output / "manifest.json").write_bytes(data)
    with tarfile.open(output / "source.tar.gz", "w:gz") as stream:
        for name, content in sorted(records.items()):
            member = tarfile.TarInfo(name)
            member.size, member.mode = len(content), 0o600
            stream.addfile(member, io.BytesIO(content))
    print(json.dumps({"files": len(records), "manifest_sha256": sha(data),
                      "source_archive_sha256": sha((output / "source.tar.gz").read_bytes()),
                      "source_archive_bytes": (output / "source.tar.gz").stat().st_size}))


if __name__ == "__main__":
    main()
