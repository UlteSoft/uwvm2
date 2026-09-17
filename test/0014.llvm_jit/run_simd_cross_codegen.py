#!/usr/bin/env python3
"""Execute production SIMD emission through target FP lowering and legalization.

Build simd_direct_lowering.cc with UWVM2TEST_SIMD_CROSS_TARGETS as --emitter,
and fixtures/simd_cross_finalize.cpp as --finalizer, using the same source and
LLVM library. --llvm-tools contains matching opt/llc. The JSON config is a list
of objects with name, triple, cpu, features, fp_mode, cxx (argument array), run
(argument array), and optional flags, llc_flags, env. fp_mode is i386 (mandatory
private integer FP ABI), native, or native-nan (e.g. SPARC). Choose the actual
destination contract, not the compiler-host predefines. Current generators
should be built on a native IEEE x86_64/AArch64 host, without fast-math.

Profiles run serially. Apply an aggregate cgroup memory/CPU limit outside this
script: a per-process timeout is not an aggregate memory limit. Each target's
output directory must be new, so failed evidence is never silently replaced.
QEMU execution tests generated objects, not a complete cross-platform CLI/JIT.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--config", type=Path, required=True)
parser.add_argument("--build-dir", type=Path, required=True)
parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument("--emitter", type=Path, required=True)
parser.add_argument("--finalizer", type=Path, required=True)
parser.add_argument("--llvm-tools", type=Path, required=True)
parser.add_argument("--only", help="Comma-separated configured target names")
parser.add_argument("--timeout", type=int, default=600, help="seconds per complete process group")
args = parser.parse_args()
if args.timeout < 1:
    parser.error("--timeout must be positive")
repo, build = args.source_root.resolve(), args.build_dir.resolve()
emitter, finalizer, llvm = args.emitter.resolve(), args.finalizer.resolve(), args.llvm_tools.resolve()
profiles = json.loads(args.config.read_text())
names = [p["name"] for p in profiles]
if len(names) != len(set(names)) or any(not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", name) for name in names):
    parser.error("Target names must be unique, safe path components")
if args.only:
    selected = set(args.only.split(","))
    if not selected <= set(names):
        parser.error("Unknown --only target")
    profiles = [p for p in profiles if p["name"] in selected]
if not profiles:
    parser.error("No profiles selected")
fixtures = repo / "test/0014.llvm_jit/fixtures"
runner = fixtures / "simd_direct_cross_runner.cpp"
bridge = fixtures / "fp_legacy_nan_bridge.cpp"
guard = repo / "test/0014.llvm_jit/simd_guarded_store_fault.c"
for path in (emitter, finalizer, llvm / "opt", llvm / "llc", runner, bridge, guard):
    if not path.is_file():
        parser.error("Missing input: " + str(path))
for profile in profiles:
    if profile.get("fp_mode") not in ("i386", "x86-extended", "native", "native-nan"):
        parser.error("Select an explicit target fp_mode")
    for key in ("cxx", "run", "flags", "llc_flags"):
        value = profile.get(key, [])
        if not isinstance(value, list) or any(not isinstance(x, str) for x in value):
            parser.error(key + " must be an argument array")
    if not profile.get("cxx"):
        parser.error("Missing cross compiler")
    if (build / profile["name"]).exists():
        parser.error("Keep previous evidence; use a new build directory: " + profile["name"])
build.mkdir(parents=True, exist_ok=True)

def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()

provenance = {str(path): digest(path) for path in (emitter, finalizer, runner, bridge, guard)}
rows = []
for profile in profiles:
    directory = build / profile["name"]
    directory.mkdir()
    prefix = directory / "simd"
    path = lambda suffix: str(prefix) + suffix
    mode = profile["fp_mode"]
    compiler = [*profile["cxx"], *profile.get("flags", []), "-O2"]
    # Features already live on each emitted function, and production lowering
    # can refine them (notably -x87 for bit-preserving transport). llc APPENDS
    # -mattr after those attributes; repeating the initial +x87 would silently
    # undo the safety repair. The final IR is the codegen contract.
    codegen = [str(llvm / "llc"), "-O3", "-verify-machineinstrs", "-mtriple=" + profile["triple"], "-mcpu=" + profile["cpu"],
               "-relocation-model=pic", *profile.get("llc_flags", [])]
    # VE uses one TargetMachine-wide subtarget, ignoring function attributes.
    # Other non-x86 targets also need their module-level ISA/ELF attributes.
    # Only x86 refines the initial features in the FP lowering pass (-x87).
    if not re.match(r'^(?:x86_64|i[3-6]86)-', profile['triple']):
        codegen.append('-mattr=' + profile['features'])
    # The oracle has only integer byte-buffer arguments/results. Establish the
    # exact production FP environment in C++ before entering generated code.
    # In particular PPC VSCR.NJ must not inherit libc's non-Java default.
    link = [*compiler, "-std=c++23", "-I" + str(repo / "src"), str(runner)]
    if mode in ("i386", "x86-extended"):
        link.append(str(bridge))
    phases = [
        ("emit", [str(emitter), profile["triple"], profile["cpu"], profile["features"], str(prefix)]),
        ("fp-lower", [str(finalizer), path(".ll"), path(".fp.ll"), mode]),
        ("opt", [str(llvm / "opt"), "-passes=default<O3>", path(".fp.ll"), "-o", path(".bc")]),
        ("legalize", [str(finalizer), path(".bc"), path(".final.ll"), "legalize"]),
        ("object", [*codegen, "-filetype=obj", path(".final.ll"), "-o", path(".o")]),
        ("assembly", [*codegen, "-filetype=asm", path(".final.ll"), "-o", path(".s")]),
        ("oracle", [*compiler, "-x", "c", "-Dmain=simd_cross_main", "-c", path(".c"), "-o", path(".oracle.o")]),
        ("link", [*link, path(".oracle.o"), path(".o"), "-lm", "-latomic", "-o", path(".test")]),
        ("run", [*profile["run"], path(".test")]),
        ("guard-oracle", [*compiler, "-x", "c", "-Dmain=simd_cross_main", "-c", str(guard), "-o", path(".guard-oracle.o")]),
        ("guard-link", [*link, path(".guard-oracle.o"), path(".o"), "-lm", "-latomic", "-o", path(".guard")]),
        ("guard-run", [*profile["run"], path(".guard")]),
    ]
    row = {"profile": profile, "source_root": str(repo), "sha256": provenance}
    for phase, command in phases:
        row[phase + "_command"] = command
        with (directory / (phase + ".log")).open("wb") as log:
            try:
                child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                         env={**os.environ, **profile.get("env", {})}, start_new_session=os.name == "posix")
                try:
                    status = child.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    if os.name == "posix":
                        try:
                            os.killpg(child.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    else:
                        child.kill()
                    child.wait()
                    status = "timeout"
            except OSError as error:
                log.write(str(error).encode())
                status = "unavailable"
        row[phase] = status
        if status != 0:
            row["failed_phase"] = phase
            break
    rows.append(row)
    (directory / "result.json").write_text(json.dumps(row, indent=2) + "\n")
    (build / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
    print(profile["name"], "PASS" if row.get("guard-run") == 0 else "FAIL at " + phase, flush=True)
raise SystemExit(not all(row.get("guard-run") == 0 for row in rows))
