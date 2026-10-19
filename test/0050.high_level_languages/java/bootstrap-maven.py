#!/usr/bin/env python3
"""Install the pinned Maven distribution into an explicitly chosen scratch path."""
import hashlib
import pathlib
import sys
import tarfile
import urllib.request

VERSION = "3.9.11"
SHA512 = "bcfe4fe305c962ace56ac7b5fc7a08b87d5abd8b7e89027ab251069faebee516b0ded8961445d6d91ec1985dfe30f8153268843c89aa392733d1a3ec956c9978"

if len(sys.argv) != 2:
    raise SystemExit("usage: bootstrap-maven.py SCRATCH_DIRECTORY (run on authorized Linux host)")
if not sys.platform.startswith("linux"):
    raise SystemExit("Build and download this fixture only on the authorized SSH Linux host")
root = pathlib.Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)
archive = root / ("apache-maven-" + VERSION + "-bin.tar.gz")
try:
    urllib.request.urlretrieve(
        "https://repo.maven.apache.org/maven2/org/apache/maven/apache-maven/"
        + VERSION + "/apache-maven-" + VERSION + "-bin.tar.gz", archive)
    if hashlib.sha512(archive.read_bytes()).hexdigest() != SHA512:
        raise SystemExit("Maven archive checksum mismatch")
    with tarfile.open(archive) as stream:
        # The archive is pinned by hash; reject links/paths outside its fixed root.
        prefix = "apache-maven-" + VERSION + "/"
        for entry in stream.getmembers():
            if (entry.name != prefix[:-1] and not entry.name.startswith(prefix)) or ".." in pathlib.PurePosixPath(entry.name).parts:
                raise SystemExit("Unexpected path in Maven archive")
            if entry.issym() or entry.islnk():
                raise SystemExit("Unexpected link in Maven archive")
        stream.extractall(root)
finally:
    archive.unlink(missing_ok=True)
print(root / ("apache-maven-" + VERSION) / "bin/mvn")
