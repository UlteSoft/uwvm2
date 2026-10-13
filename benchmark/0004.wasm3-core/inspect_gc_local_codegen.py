#!/usr/bin/env python3
"""Source-bound O3 assembly audit for GC local get/set before/after stripes.

The wrapper is compiled on an E core; no timings are inferred from the
instruction counts. Inspect each saved local fast-path block and its cold
foreign fallback before claiming there is no regression.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from run import cgroup_preflight, sha256, verify_product_build


def invoke(command, log, env, cwd=None):
    with log.open("wb") as output:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    if result.returncode:
        raise RuntimeError(f"{log}: command exited {result.returncode}")


def count_instructions(disassembly):
    functions = {}
    current = None
    for line in disassembly.splitlines():
        match = re.search(r"<((?:gc_local_get|gc_local_set)(?:\.cold)?)>:$", line)
        if match:
            current = match.group(1)
            functions[current] = []
            continue
        if re.match(r"^[0-9a-fA-F]+\s+<.*>:$", line):
            current = None
        if current:
            match = re.match(r"\s*[0-9a-fA-F]+:\s+([a-z][a-z0-9.]*)\b", line)
            if match:
                functions[current].append(match.group(1))
    return {name: {"instructions": len(opcodes),
                   "calls": sum(op.startswith("call") for op in opcodes),
                   "locks": sum(op.startswith(("lock", "xchg")) for op in opcodes),
                   "conditional_branches": sum(op.startswith("j") and
                                               op not in ("jmp", "jmpq")
                                               for op in opcodes),
                   "mnemonics": opcodes}
            for name, opcodes in functions.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ordinary-baseline", required=True, type=Path)
    parser.add_argument("--ordinary-candidate", required=True, type=Path)
    parser.add_argument("--ros-baseline", required=True, type=Path)
    parser.add_argument("--ros-candidate", required=True, type=Path)
    parser.add_argument("--clang", default=Path("/toolchain/bin/clang++"), type=Path)
    parser.add_argument("--objdump", default=Path("/toolchain/bin/llvm-objdump"), type=Path)
    parser.add_argument("--fixture", default=Path(__file__).with_name("gc_local_codegen.cc"), type=Path)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output directory exists; evidence must be immutable")
    before = cgroup_preflight(0)
    start_utc = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    args.out.mkdir(parents=True)
    for path in (Path(__file__), args.fixture):
        shutil.copyfile(path, args.out / path.name)
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join(("/toolchain/lib/x86_64-unknown-linux-gnu",
        "/toolchain/lib", "/work/deps/usr/lib/x86_64-linux-gnu", env.get("LD_LIBRARY_PATH", "")))
    binaries = {"ordinary-baseline": args.ordinary_baseline,
                "ordinary-candidate": args.ordinary_candidate,
                "ros-baseline": args.ros_baseline,
                "ros-candidate": args.ros_candidate}
    builds = {name: verify_product_build(name, binary, args.out, "before")
              for name, binary in binaries.items()}
    rows = {}
    for name, build in builds.items():
        source = Path(build["source"])
        obj = args.out / f"{name}.o"
        command = ["taskset", "-c", "16", str(args.clang), "-std=c++26",
                   "-stdlib=libc++", "-fno-rtti", "-O3", "-Werror",
                   "-Wno-undefined-inline", "-I" + str(source / "src"),
                   "-I" + str(source / "third-parties/fast_io/include"),
                   "-I" + str(source / "third-parties/bizwen/include"),
                   "-I" + str(source / "third-parties/boost_unordered/include"),
                   "-c", str(args.fixture), "-o", str(obj)]
        invoke(command, args.out / f"{name}-compile.log", env, source)
        disassembly = "\n".join(subprocess.check_output([str(args.objdump), "-dr",
            "--no-show-raw-insn", f"--disassemble-symbols={symbol}", str(obj)],
            env=env, text=True) for symbol in ("gc_local_get", "gc_local_set"))
        (args.out / f"{name}-assembly.txt").write_text(disassembly)
        rows[name] = {"source_id": build["source_id"], "object_sha256": sha256(obj),
                      "command": command, "counts": count_instructions(disassembly)}
        (args.out / "summary.json").write_text(json.dumps(rows, indent=2) + "\n")
    after = {name: verify_product_build(name, binary, args.out, "after")
             for name, binary in binaries.items()}
    if after != builds:
        raise RuntimeError("product source or binary changed during codegen audit")
    if not all("gc_local_get" in value["counts"] and "gc_local_set" in value["counts"]
               for value in rows.values()):
        raise RuntimeError("local wrapper symbols missing from assembly")
    end = cgroup_preflight(0)
    start_events = dict(line.split() for line in before["memory.events"].splitlines())
    end_events = dict(line.split() for line in end["memory.events"].splitlines())
    if any(start_events[key] != end_events[key] for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup OOM changed during codegen audit")
    (args.out / "metadata.json").write_text(json.dumps({
        "start_utc": start_utc,
        "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "source_builds": builds, "clang_sha256": sha256(args.clang),
        "objdump_sha256": sha256(args.objdump), "fixture_sha256": sha256(args.fixture),
        "cgroup_before": before, "cgroup_after": end,
        "note": "Counts include complete wrapper bodies and are not proof of fast-path equivalence; inspect actual disassembly."
    }, indent=2) + "\n")
    print("PASS source-bound GC local get/set O3 assembly captured")


if __name__ == "__main__":
    sys.exit(main())
