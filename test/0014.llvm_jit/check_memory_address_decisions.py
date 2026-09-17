#!/usr/bin/env python3
"""Build and run native address-decision oracles before/after LLVM O3.

This links an abort-only test sink, never uwvm_runtime. It executes emitted
ISA32/ISA64 integer decisions on the native host, not actual cross-ISA linear
memory accesses or production trap reporting. Bound CPU/memory externally;
all commands, hashes and temporary negative controls go to a new --out folder.
Use --compile-flag=--gcc-install-dir=... to select older headers independently
of the libstdc++ runtime required by the installed LLVM shared library.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import signal
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--fixture", type=Path, default=Path(__file__).with_name("memory_direct_lowering.cc"))
    parser.add_argument("--trap-stub", type=Path, default=Path(__file__).parent / "fixtures/memory_address_trap_stub.cpp")
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--llvm-config", default="llvm-config")
    parser.add_argument("--compile-flag", action="append", default=[])
    parser.add_argument("--link-flag", action="append", default=[])
    parser.add_argument("--negative-sentinel-control", action="store_true")
    parser.add_argument("--timeout", type=int, default=300, help="seconds per complete process group")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("--timeout must be positive")
    root, fixture, stub, out = (x.resolve() for x in
        (args.source_root, args.fixture, args.trap_stub, args.out))
    inputs = [fixture, stub,
              root / "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/memory_emit.h"]
    if not all(x.is_file() for x in inputs):
        parser.error("missing fixture, trap stub or production memory emitter")
    out.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    hashes = {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}
    commands, results = [], []

    def save():
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")

    def run(command, label, expected=0):
        command = [str(x) for x in command]
        record = dict(label=label, command=command, expected=expected)
        commands.append(record)
        save()
        with (out / (label + ".log")).open("w") as log:
            child = subprocess.Popen(command, cwd=root, stdout=log,
                stderr=subprocess.STDOUT, start_new_session=True)
            try:
                status = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
                status = "timeout"
        record["returncode"] = status
        save()
        if status != expected:
            raise RuntimeError(f"{label}: got {status}, expected {expected}; evidence: {out}")
        return (out / (label + ".log")).read_text()

    version = run([args.llvm_config, "--version"], "llvm-version").strip()
    include = run([args.llvm_config, "--includedir"], "llvm-includedir").strip()
    links = shlex.split(run([args.llvm_config, "--link-shared", "--ldflags", "--libs",
        "core", "support", "executionengine", "mcjit", "native", "passes", "--system-libs"], "llvm-links"))
    flags = ["-std=c++26", "-O3", "-fno-rtti", "-ffp-model=precise",
        "-Wno-deprecated-declarations", "-Wno-undefined-inline", "-DUWVM=2", "-DUWVM_TEST=2",
        "-DUWVM_USE_UWVM_INT", "-DUWVM_USE_LLVM_JIT", "-DUWVM_USE_THREAD_LOCAL",
        "-include", "uwvm2/uwvm/io/impl.h", "-Isrc", "-Ithird-parties/fast_io/include",
        "-Ithird-parties/bizwen/include", "-Ithird-parties/boost_unordered/include",
        "-I" + include, *args.compile_flag]
    stub_object = out / "trap-stub.o"
    run([args.cxx, "-std=c++26", *args.compile_flag, "-c", stub, "-o", stub_object], "stub-compile")

    def build(source, profile):
        obj, binary = out / (profile + ".o"), out / profile
        run([args.cxx, *flags, "-c", source, "-o", obj], profile + "-compile")
        # Compilation's GCC-header selection must not pin an older shared runtime:
        # libLLVM dependencies may require newer CXXABI symbols.
        run([args.cxx, obj, stub_object, *links, *args.link_flag, "-o", binary], profile + "-link")
        return binary

    def check(binary, profile, expected=0):
        for mode, options in (("baseline", []), ("O3", ["--optimize-ir"])):
            output = run([binary, *options], profile + "-" + mode, expected)
            if expected:
                if ("address=ffffffffffffffff length=ffffffffffffffff" not in output or
                    "actual=ffffffffffffffff/0 expected=ffffffffffffffff/1" not in output):
                    raise RuntimeError("negative control failed for an unexpected reason")
            elif "210 configurations, 454650 boundary/random cases" not in output or "IR=" + mode not in output:
                raise RuntimeError("incomplete address-decision matrix")
            results.append(dict(profile=profile, mode=mode, expected_returncode=expected,
                                passed=True, output=output))
            print(profile + "/" + mode + ": " + output, end="", flush=True)

    binary = build(fixture, "original")
    check(binary, "original")
    if args.negative_sentinel_control:
        source = fixture.read_text()
        anchor = "                b.SetInsertPoint(&block); b.CreateStore(b.getInt8(1), fn->getArg(2)); b.CreateRet(b.getInt64(~0ull));"
        mutant = """                b.SetInsertPoint(&block);
                llvm::Value* observed_trap{b.getInt8(1)};
                if(address_bits == 64u && offset == 0u && width == 1u && mode == protection::software)
                {
                    auto corner{b.CreateAnd(b.CreateICmpEQ(fn->getArg(0), b.getInt64(~0ull)),
                                           b.CreateICmpEQ(fn->getArg(1), b.getInt64(~0ull)))};
                    observed_trap = b.CreateSelect(corner, b.getInt8(0), b.getInt8(1));
                }
                b.CreateStore(observed_trap, fn->getArg(2)); b.CreateRet(b.getInt64(~0ull));"""
        if source.count(anchor) != 1:
            raise RuntimeError("trap observation anchor changed; review the negative control")
        source = source.replace(anchor, mutant)
        for profile in ("independent-trap", "sentinel-only"):
            text = source
            if profile == "sentinel-only":
                clause = " || trapped != static_cast<std::uint8_t>(expected.trapped)"
                if text.count(clause) != 1:
                    raise RuntimeError("comparison anchor changed; review the negative control")
                text = text.replace(clause, "")
            temporary = out / (profile + ".cc")
            temporary.write_text(text)
            check(build(temporary, profile), profile, 6 if profile == "independent-trap" else 0)
    if hashes != {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs}:
        raise RuntimeError("test inputs changed during verification")
    summary = dict(source_root=str(root), llvm_version=version, sha256=hashes,
        binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), results=results,
        original_configurations=210, original_cases_per_mode=454650,
        negative_sentinel_control=args.negative_sentinel_control,
        production_emitter_modified=False, real_linear_memory_access=False)
    (out / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS: " + str(out), flush=True)


if __name__ == "__main__":
    main()
