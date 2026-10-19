#!/usr/bin/env python3
"""Install the pinned official toolchains into a task-owned Linux directory."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile
import urllib.request

SDK_VERSION = "8.0.408"
SDK_SHA512 = "a9a1e54d10a37f91e1bd9b2e9e8ce6ed31917559898e4d6d36296bd5324f67cc7a13a9106703003cbebc5a7ee50188747ba816f5d828c0cb3a4a9f9920ebac4a"
WASI_SHA256 = "7030139d495a19fbeccb9449150c2b1531e15d8fb74419872a719a7580aad0f9"


def download(request, destination, algorithm, expected):
    digest = hashlib.new(algorithm)
    try:
        with urllib.request.urlopen(request, timeout=60) as response, destination.open("wb") as output:
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                output.write(block)
                digest.update(block)
        if digest.hexdigest() != expected:
            raise RuntimeError("Checksum mismatch: " + str(destination))
    except BaseException:
        destination.unlink(missing_ok=True)
        raise


def main():
    if sys.platform != "linux" or len(sys.argv) != 2:
        raise SystemExit("Run inside the existing SSH Linux cgroup: bootstrap.py /absolute/task-directory")
    task_dir = Path(sys.argv[1])
    if not task_dir.is_absolute():
        raise SystemExit("Task directory must be absolute")
    task_dir.mkdir(parents=True, exist_ok=True)
    dotnet_dir = task_dir / "dotnet"
    wasi_dir = task_dir / "wasi-sdk-20.0"
    if not (dotnet_dir / "dotnet").exists():
        archive = task_dir / "dotnet-sdk.tar.gz"
        download("https://builds.dotnet.microsoft.com/dotnet/Sdk/8.0.408/dotnet-sdk-8.0.408-linux-x64.tar.gz",
                 archive, "sha512", SDK_SHA512)
        dotnet_dir.mkdir(exist_ok=True)
        with tarfile.open(archive) as source:
            source.extractall(dotnet_dir)
        archive.unlink()
    if not (wasi_dir / "bin/clang").exists():
        with urllib.request.urlopen("https://api.github.com/repos/WebAssembly/wasi-sdk/releases/tags/wasi-sdk-20", timeout=60) as response:
            release = json.load(response)
        asset = next(asset for asset in release["assets"] if asset["name"] == "wasi-sdk-20.0-linux.tar.gz")
        request = urllib.request.Request(asset["url"] + "?download=1", headers={"Accept": "application/octet-stream"})
        archive = task_dir / "wasi-sdk.tar.gz"
        download(request, archive, "sha256", WASI_SHA256)
        with tarfile.open(archive) as source:
            source.extractall(task_dir)
        archive.unlink()
    environment = {
        "DOTNET_ROOT": str(dotnet_dir),
        "DOTNET_CLI_HOME": str(task_dir / "dotnet-home"),
        "NUGET_PACKAGES": str(task_dir / "nuget"),
        "TMPDIR": str(task_dir),
        "WASI_SDK_PATH": str(wasi_dir),
        "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT": "1",
        "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
        "DOTNET_SKIP_FIRST_TIME_EXPERIENCE": "1",
    }
    process_env = dict(os.environ, **environment)
    process_env["PATH"] = str(dotnet_dir) + os.pathsep + os.environ["PATH"]
    version = subprocess.check_output([str(dotnet_dir / "dotnet"), "--version"], env=process_env, cwd=task_dir, text=True).strip()
    if version != SDK_VERSION:
        raise RuntimeError("Wrong SDK: " + version)
    subprocess.run([str(dotnet_dir / "dotnet"), "workload", "install", "wasi-experimental", "--skip-manifest-update", "--disable-parallel"],
                   env=process_env, cwd=task_dir, check=True)
    (task_dir / "env.sh").write_text("".join("export " + key + "=" + shlex.quote(value) + "\n" for key, value in environment.items()))
    print("Source " + str(task_dir / "env.sh") + ", then run build.sh /absolute/output-directory")


if __name__ == "__main__":
    main()
