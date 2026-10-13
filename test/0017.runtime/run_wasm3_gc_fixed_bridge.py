#!/usr/bin/env python3
"""Qualify the exact native fixed-count GC bridge in the remote Linux cgroup.

This isolates the native ABI body, not the LLVM emitter or the full VM. The
candidate stays outside the product until full-VM semantics and paired P-core
measurements also pass. No generated substitute for wasm_module_storage_t is
used: every test includes the selected repository's actual runtime module.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


START = "template<::std::uint_least32_t FixedOpcode = 0xffff'ffffu, ::std::size_t FixedInputs = SIZE_MAX>"
STOP = "[[nodiscard]] inline ::std::uintptr_t llvm_jit_gc_input_allocate_bridge"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract_native_body(path):
    source = path.read_text()
    if source.count(START) != 1 or source.count(STOP) != 1:
        raise ValueError("candidate must contain exactly one fixed bridge template and one following ABI marker")
    start = source.index(START)
    end = source.index(STOP)
    if start >= end:
        raise ValueError("candidate bridge markers are reversed")
    body = source[start:end]
    if "::llvm::" in body or body.count("llvm_jit_gc_aggregate_bridge(") != 1:
        raise ValueError("native extraction unexpectedly includes LLVM definitions or multiple bodies")
    return body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--candidate-header", type=Path, required=True)
    parser.add_argument("--expected-candidate-sha256", required=True)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--objdump", default="/toolchain/bin/llvm-objdump")
    args = parser.parse_args()
    own_root = Path(__file__).resolve().parents[2]
    root = (args.source_root or own_root).resolve()
    candidate = args.candidate_header.resolve()
    if sha(candidate) != args.expected_candidate_sha256:
        raise ValueError("candidate hash differs from the reviewed input")
    fingerprint_script = root / "tools/ci/wasm3_source_fingerprint.py"
    os.chdir(root)
    guard = ["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")]
    subprocess.run(guard, check=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    def fingerprint_tree(name):
        destination = out / (name + ".json")
        command = [sys.executable, str(fingerprint_script), str(root), str(destination)]
        (out / (name + ".command")).write_text(shlex.join(command) + "\n")
        with (out / (name + ".log")).open("w") as log:
            subprocess.run(command, stdout=log, stderr=log, check=True)
        return json.loads(destination.read_text())
    fingerprint = fingerprint_tree("source-fingerprint-before")
    if fingerprint["source_id"] != args.expected_source_id:
        raise ValueError("actual source tree differs from the specified source ID")
    generated = out / "native_gc_aggregate_bridge.h"
    generated.write_text(extract_native_body(candidate))
    five_argument_abi = "llvm_jit_gc_aggregate_fixed_bridge(" in generated.read_text()
    fixture = own_root / "test/0017.runtime/wasm3_gc_fixed_bridge.cc"
    module = root / "src/uwvm2/uwvm/runtime/storage/wasm_module.h"
    store = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
    inputs = {
        "scope": "native bridge extraction only; LLVM emitter and full VM qualification remain required",
        "source_id": args.expected_source_id,
        "fixed_argument_count": 5 if five_argument_abi else 7,
        "candidate_header": {"path": str(candidate), "sha256": sha(candidate)},
        "native_extraction": {"path": str(generated), "sha256": sha(generated)},
        "fixture": {"path": str(fixture), "sha256": sha(fixture)},
        "runner": {"path": str(Path(__file__).resolve()), "sha256": sha(Path(__file__).resolve())},
        "real_module": {"path": str(module), "sha256": sha(module)},
        "real_store": {"path": str(store), "sha256": sha(store)},
        "source_fingerprint_script_sha256": sha(fingerprint_script),
    }
    (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    (out / "source-fingerprint.json").write_text(json.dumps(fingerprint, indent=2) + "\n")
    # This is a standalone native ABI-body experiment. Use the same real
    # module header as the selected tree, with explicit native-only backend
    # macros rather than pretending that an LLVM product module was linked.
    # Version 2.0.4 matches the qualified product build's X/Y/Z/S definitions.
    native_defines = ["-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT",
        "-DUWVM_DISABLE_JIT", "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL",
        "-DUWVM_VERSION_X=2", "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4",
        "-DUWVM_VERSION_S=0", "-DNDEBUG", "-DUWVM_MODE_RELEASE"]
    includes = [out, root / "src", root / "third-parties/fast_io/include",
        root / "third-parties/bizwen/include", root / "third-parties/boost_unordered/include"]
    if not all(directory.is_dir() for directory in includes):
        raise ValueError("a required real module/container include directory is missing")
    flags = [args.cxx, "-std=c++26", "-stdlib=libc++", *native_defines,
        "-fno-rtti", "-pthread", "-fstack-clash-protection", "-mstack-probe-size=4096",
        *[part for directory in includes for part in ("-I", str(directory))]]
    link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
    if five_argument_abi:
        flags.append("-DUWVM_FIXED_GC_FIVE_ARGUMENT_ABI=1")
    inputs["native_defines"] = native_defines
    inputs["include_directories"] = [str(directory) for directory in includes]
    inputs["native_compile_flags"] = flags
    inputs["native_link_flags"] = link
    inputs["mode_scope"] = "standalone default-interpreter module layout; LLVM emitter/product ABI is not qualified"
    (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
        UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    rows = []
    summary = {"passed": False, "source_id": args.expected_source_id,
        "candidate_sha256": args.expected_candidate_sha256, "scope": inputs["scope"],
        "fixed_argument_count": inputs["fixed_argument_count"],
        "profiles": ["O3 native semantics", "ASan/UBSan/LSan native semantics", "O3 native assembly"]}

    def run(name, command, timeout=180):
        subprocess.run(guard, check=True)
        (out / (name + ".command")).write_text(shlex.join(command) + "\n")
        row = {"name": name, "command": command, "exit": None, "timeout": False}
        try:
            with (out / (name + ".log")).open("w") as log:
                result = subprocess.run(command, stdout=log, stderr=log, timeout=timeout, env=env)
            row["exit"] = result.returncode
        except subprocess.TimeoutExpired:
            row["timeout"] = True
        rows.append(row)
        (out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
        if row["timeout"] or row["exit"] != 0:
            raise RuntimeError(name + " failed; see " + str(out / (name + ".log")))

    try:
        run("compiler-version", [args.cxx, "--version"])
        run("syntax-preflight", flags + ["-fsyntax-only", str(fixture)])
        for name, optimization in [("o3", ["-O3"]), ("asan-ubsan", ["-O1", "-g1",
            "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])]:
            executable = out / name
            run(name + "-build", flags + optimization + [str(fixture), "-o", str(executable)] + link)
            run(name + "-run", [str(executable)])
            summary[name + "_sha256"] = sha(executable)
        assembly = out / "native-bridge-o3.s"
        run("o3-assembly-build", flags + ["-O3", "-S", str(fixture), "-o", str(assembly)])
        run("o3-native-disassembly", [args.objdump, "--demangle", "--disassemble", str(out / "o3")])
        summary["assembly_sha256"] = sha(assembly)
        # Preserve exact output. The human review must follow tail branches into
        # template bodies rather than comparing only the three wrapper stubs.
        summary["assembly_review"] = "pending; compare generic and fixed native bodies including tail-called template instances"
        if (sha(candidate) != args.expected_candidate_sha256 or sha(fixture) != inputs["fixture"]["sha256"] or
            fingerprint_tree("source-fingerprint-after")["source_id"] != args.expected_source_id):
            raise RuntimeError("an input changed during qualification")
        subprocess.run(guard, check=True)
        summary["passed"] = True
    except Exception as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["commands"] = len(rows)
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS native GC bridge only; full LLVM emitter, VM semantics and P-core A/B still required")


if __name__ == "__main__":
    main()
