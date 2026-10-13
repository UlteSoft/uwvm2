#!/usr/bin/env python3
"""Compile cross-language platform-thread analogues on E core 16 in the test cgroup."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from run import cgroup_preflight, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--javac", required=True, type=Path)
    parser.add_argument("--dotnet", required=True, type=Path)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    base = Path(__file__).parent / "managed_threads"
    sources = {
        "java": base / "java/ManagedThreads.java",
        "dotnet": base / "dotnet/Program.cs",
        "dotnet_project": base / "dotnet/ManagedThreads.csproj",
        "node": base / "node/managed_threads.mjs",
    }
    snapshots = args.out / "sources"
    snapshots.mkdir()
    for source in sources.values():
        shutil.copyfile(source, snapshots / source.name)
    shutil.copyfile(__file__, args.out / "build_managed_threads.py")
    java_dir = args.out / "java"
    dotnet_dir = args.out / "dotnet"
    java_dir.mkdir()
    dotnet_dir.mkdir()
    (args.out / "dotnet-obj").mkdir()
    env = os.environ.copy()
    env.update(DOTNET_CLI_HOME=str(args.out / "dotnet-home"),
               DOTNET_ROOT=str(args.dotnet.resolve(strict=True).parent),
               DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1",
               DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1",
               NUGET_PACKAGES=str(args.out / "nuget-packages"))
    commands = [
        ["taskset", "-c", "16", str(args.javac), "--release", "21",
         "-d", str(java_dir), str(sources["java"])],
        ["taskset", "-c", "16", str(args.dotnet), "build",
         str(sources["dotnet_project"]), "-c", "Release", "-o", str(dotnet_dir),
         "-maxcpucount:1", "-p:UseSharedCompilation=false",
         f"-p:BaseIntermediateOutputPath={args.out / 'dotnet-obj'}/",
         f"-p:MSBuildProjectExtensionsPath={args.out / 'dotnet-obj'}/"],
    ]
    for index, command in enumerate(commands):
        with (args.out / f"build-{index}.log").open("wb") as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=300)
        if result.returncode:
            raise RuntimeError(f"build-{index} exited {result.returncode}")
    outputs = {"java_class": java_dir / "ManagedThreads.class",
               "dotnet_dll": dotnet_dir / "ManagedThreads.dll"}
    if not all(path.is_file() for path in outputs.values()):
        raise RuntimeError("managed thread analogue output missing")
    after = cgroup_preflight(0)
    first = dict(line.split() for line in before["memory.events"].splitlines())
    last = dict(line.split() for line in after["memory.events"].splitlines())
    if any(first[name] != last[name] for name in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM during managed-thread build")
    manifest = {"source_sha256": {name: sha256(path) for name, path in sources.items()},
                "tool_sha256": {"javac": sha256(args.javac), "dotnet": sha256(args.dotnet)},
                "outputs": {name: {"path": str(path), "sha256": sha256(path)}
                            for name, path in outputs.items()},
                "artifact_sha256": {kind: {str(path.relative_to(directory)): sha256(path)
                                            for path in sorted(directory.rglob("*")) if path.is_file()}
                                    for kind, directory in (("java", java_dir), ("dotnet", dotnet_dir))},
                "commands": commands, "cgroup_before": before, "cgroup_after": after,
                "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(args.out / "build.json")


if __name__ == "__main__":
    main()
