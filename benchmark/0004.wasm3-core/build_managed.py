#!/usr/bin/env python3
"""Build the managed GC-ring analogues inside the shared Linux cgroup.

JDK 27 may compile the Java source, but --release 21 keeps the class file
compatible with both JDK 27 and the measured GraalVM 25 distribution.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from run import cgroup_preflight, sha256


def invoke(command, log, env):
    with log.open("wb") as output:
        completed = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT,
                                   env=env, timeout=300)
    if completed.returncode:
        raise RuntimeError(f"build exited {completed.returncode}: {command}; see {log}")
    return {"command": command, "log": str(log), "exit": completed.returncode}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--javac", required=True, type=Path)
    parser.add_argument("--dotnet", required=True, type=Path)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists; build evidence must be immutable")
    before = cgroup_preflight(0)
    args.out.mkdir(parents=True)
    base = Path(__file__).parent
    sources = {"java": base / "managed/java/ManagedRing.java",
               "dotnet": base / "managed/dotnet/Program.cs",
               "dotnet-project": base / "managed/dotnet/ManagedRing.csproj",
               "node": base / "managed/node/managed_ring.mjs"}
    snapshots = args.out / "sources"
    snapshots.mkdir()
    for name, source in sources.items():
        shutil.copyfile(source, snapshots / source.name)
    shutil.copyfile(__file__, args.out / "build_managed.py")
    java_out = args.out / "java"
    dotnet_out = args.out / "dotnet"
    java_out.mkdir()
    dotnet_out.mkdir()
    env = os.environ.copy()
    env.update(DOTNET_CLI_HOME=str(args.out / "dotnet-home"),
               DOTNET_ROOT=str(args.dotnet.resolve(strict=True).parent),
               DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1",
               DOTNET_CLI_TELEMETRY_OPTOUT="1",
               DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1",
               NUGET_PACKAGES=str(args.out / "nuget-packages"))
    versions = {}
    for name, binary, option in (("javac", args.javac, "-version"),
                                 ("dotnet", args.dotnet, "--version")):
        probe = subprocess.run(["taskset", "-c", "16", str(binary), option],
                               capture_output=True, timeout=30, env=env)
        if probe.returncode:
            raise RuntimeError(f"{name} version command exited {probe.returncode}")
        versions[name] = (probe.stdout + probe.stderr).decode(errors="replace").strip()
    commands = [invoke(["taskset", "-c", "16", str(args.javac), "--release", "21",
                        "-d", str(java_out), str(sources["java"])],
                       args.out / "javac.log", env)]
    obj_dir = args.out / "dotnet-obj"
    obj_dir.mkdir()
    commands.append(invoke(["taskset", "-c", "16", str(args.dotnet), "build",
                            str(sources["dotnet-project"]), "-c", "Release",
                            "-o", str(dotnet_out), "-maxcpucount:1",
                            "-p:UseSharedCompilation=false",
                            f"-p:BaseIntermediateOutputPath={obj_dir}/",
                            f"-p:MSBuildProjectExtensionsPath={obj_dir}/"],
                           args.out / "dotnet-build.log", env))
    outputs = {"java_class": java_out / "ManagedRing.class",
               "dotnet_dll": dotnet_out / "ManagedRing.dll"}
    if not all(path.is_file() for path in outputs.values()):
        raise RuntimeError("managed build output missing")
    after = cgroup_preflight(0)
    before_events = dict(line.split() for line in before["memory.events"].splitlines())
    after_events = dict(line.split() for line in after["memory.events"].splitlines())
    if any(before_events[key] != after_events[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM occurred during managed build")
    metadata = {"source_sha256": {name: sha256(source) for name, source in sources.items()},
                "tool_sha256": {"javac": sha256(args.javac), "dotnet": sha256(args.dotnet)},
                "tool_versions": versions, "java_release": 21,
                "outputs": {name: {"path": str(path), "sha256": sha256(path)}
                            for name, path in outputs.items()},
                "artifact_sha256": {kind: {
                    str(path.relative_to(folder)): sha256(path)
                    for path in sorted(folder.rglob("*")) if path.is_file()}
                    for kind, folder in (("java", java_out), ("dotnet", dotnet_out))},
                "commands": commands, "cgroup_before": before, "cgroup_after": after,
                "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    (args.out / "build.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(args.out / "build.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
