#!/usr/bin/env python3
"""Qualify AArch64 callee-pop stack restoration with machine and execution checks.

@brief Detect a caller spill reload placed before reserved-SP restoration.
@details The checked MIR deliberately retains the valid post-register-allocation
ordering that exposed a false memory64 trap. The direct and indirect TailCC
calls must preserve their spill values, and an ordinary ABI call is an
independent control. QEMU results establish correctness, not performance.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time


def sha256(path):
    """@brief Hash exact input or executable bytes for the retained receipt."""
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def vector(path):
    """@brief Read an explicit argv vector without invoking a command shell."""
    if path is None:
        return []
    result = json.loads(path.read_text())
    if not isinstance(result, list) or any(not isinstance(item, str) for item in result):
        raise ValueError(f"Expected a JSON string array: {path}")
    return result


def main():
    """@brief Execute the old-provider control or qualify one rebuilt provider."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llc", type=Path, required=True)
    parser.add_argument("--filecheck", type=Path, required=True)
    parser.add_argument("--clang", type=Path, required=True)
    parser.add_argument("--tool-runner", type=Path,
                        help="JSON argv prefix used to execute llc and FileCheck")
    parser.add_argument("--target-runner", type=Path,
                        help="JSON argv prefix used to execute the AArch64 probe")
    parser.add_argument("--compiler-flags", type=Path,
                        help="JSON array of explicit target sysroot and linker flags")
    parser.add_argument("--pei-pass", choices=("prolog-epilog", "prologepilog"), default="prolog-epilog",
                        help="Exact PEI registry name of the selected LLVM provider")
    parser.add_argument("--expect-broken", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    # Functional qualification does not assert a particular physical CPU topology.
    # The parent owns the execution lease; the live memory/swap membership is
    # verified below, independently of any supplied environment variable.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).parent / "fixtures/llvm23-aarch64-callee-pop"
    mir = fixture / "callee-pop-reload.mir"
    tool_runner = vector(args.tool_runner)
    target_runner = vector(args.target_runner)
    flags = vector(args.compiler_flags)
    inputs = [mir, fixture / "probe.c", fixture / "callee.s", args.llc, args.filecheck, args.clang]
    report = {
        "qualified": False,
        "expected_broken": args.expect_broken,
        "started": time.time(),
        "inputs": [{"path": str(path), "sha256": sha256(path)} for path in inputs],
        "commands": [],
    }
    cgroup = Path("/sys/fs/cgroup")
    report["cgroup"] = {name: (cgroup / name).read_text().strip()
                        for name in ("memory.max", "memory.swap.max", "cgroup.procs")}
    if (report["cgroup"]["memory.max"] != "68719476736" or
            report["cgroup"]["memory.swap.max"] != "0" or
            str(os.getpid()) not in report["cgroup"]["cgroup.procs"].split()):
        raise RuntimeError("The actual test process must belong to the 64 GiB, zero-swap cgroup")

    def save():
        """@brief Preserve completed commands, including original failures."""
        (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")

    def run(name, command, expected=0, stdin=None):
        """@brief Execute explicit arguments and retain both unmodified streams."""
        row = {"name": name, "argv": list(map(str, command)), "started": time.time()}
        report["commands"].append(row)
        save()
        stdout = args.out / (name + ".stdout")
        stderr = args.out / (name + ".stderr")
        with stdout.open("wb") as out_stream, stderr.open("wb") as err_stream:
            child = subprocess.run(row["argv"], input=stdin, stdout=out_stream,
                                   stderr=err_stream, timeout=120)
        row.update(returncode=child.returncode, finished=time.time())
        save()
        if child.returncode != expected:
            raise RuntimeError(f"{name}: expected exit {expected}, observed {child.returncode}")
        return stdout.read_bytes()

    try:
        for triple in ("aarch64-linux-gnu", "aarch64_be-linux-gnu"):
            output = run(triple, [*tool_runner, args.llc, "-mtriple=" + triple,
                                 "-run-pass=" + args.pei_pass, "-verify-machineinstrs", mir, "-o", "-"])
            run("check-" + triple, [*tool_runner, args.filecheck, mir],
                expected=1 if args.expect_broken else 0, stdin=output)
        obj = args.out / "probe.o"
        executable = args.out / "probe"
        run("object", [*tool_runner, args.llc, "-mtriple=aarch64-linux-gnu",
                       "-start-before=" + args.pei_pass, "-verify-machineinstrs", "-filetype=obj",
                       mir, "-o", obj])
        run("link", [args.clang, "--target=aarch64-linux-gnu", "-fuse-ld=lld", *flags,
                     fixture / "probe.c", fixture / "callee.s", obj, "-o", executable])
        output = run("execute", [*target_runner, executable], expected=1 if args.expect_broken else 0)
        if args.expect_broken:
            if b"ordinary=44\n" not in output or output == b"direct=42 indirect=43 ordinary=44\n":
                raise RuntimeError("The negative control did not isolate the callee-pop defect")
            report["negative_control_reproduced"] = True
        else:
            if output != b"direct=42 indirect=43 ordinary=44\n":
                raise RuntimeError("The generated program returned incorrect spill values")
            report["qualified"] = True
        report["object_sha256"] = sha256(obj)
        report["probe_sha256"] = sha256(executable)
        for item in report["inputs"]:
            if sha256(Path(item["path"])) != item["sha256"]:
                raise RuntimeError("An input changed during the qualification")
    finally:
        report["finished"] = time.time()
        save()


if __name__ == "__main__":
    main()
