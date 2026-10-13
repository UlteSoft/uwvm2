#!/usr/bin/env python3
"""Build each ROS interpreter combine/delay profile and execute Core 3 guests.

Run inside the existing 64 GiB, swap-free test cgroup. Each profile is built,
tested, hashed, and then removed before the next one is compiled.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time


COMBINE = {
    "none": (),
    "soft": ("UWVM_ENABLE_UWVM_INT_COMBINE_OPS",),
    "heavy": ("UWVM_ENABLE_UWVM_INT_COMBINE_OPS", "UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS"),
    "extra": (
        "UWVM_ENABLE_UWVM_INT_COMBINE_OPS",
        "UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS",
        "UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS",
    ),
}
DELAY = {
    "none": (),
    "soft": ("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT",),
    "heavy": ("UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT", "UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY"),
}
TESTS = {
    "memory64-bulk": ("memory64", "multi-memory"),
    "memory64-atomic": ("memory64", "threads"),
    "memory-indexed-load": ("multi-memory",),
    "memory-indexed-copy": ("multi-memory",),
    "eh-numeric": ("exceptions",),
    "eh-exnref": ("exceptions",),
    "wait-indexed": ("threads", "multi-memory"),
    "gc-array-new-elem": ("gc",),
}
FEATURE_GATES = {
    "memory64": "memory64-bulk",
    "multi-memory": "memory-indexed-load",
    "threads": "wait-indexed",
    "exceptions": "eh-exnref",
    "gc": "gc-array-new-elem",
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command, *, cwd=None, output=None, timeout=900):
    start = time.monotonic()
    result = subprocess.run(command, cwd=cwd, capture_output=True, timeout=timeout)
    if output is not None:
        output.write_bytes(result.stdout + result.stderr)
    return result.returncode, time.monotonic() - start, (result.stdout + result.stderr).decode(errors="replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("--only", help="One profile, e.g. none-none")
    parser.add_argument("--bottom-wasm", type=Path,
                        help="Core 3 polymorphic-bottom conversion validator witness")
    parser.add_argument("--gc-gate-wasm", type=Path,
                        help="GC-only shorthand element type for precise feature-off diagnostics")
    parser.add_argument("--exceptions-gate-wasm", type=Path,
                        help="exceptions-only shorthand element type for precise feature-off diagnostics")
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    assert Path("/sys/fs/cgroup/memory.max").read_text().strip() == "68719476736"
    assert Path("/sys/fs/cgroup/memory.swap.max").read_text().strip() == "0"
    oracle = json.loads(args.oracle.read_text())
    assert {row["name"] for row in oracle} == set(TESTS)
    assert all(row["exit"] == 0 and sha256(Path(row["wasm"])) == row["sha256"] for row in oracle)
    tests = {row["name"]: row for row in oracle}
    clang = "/toolchain/bin/clang++"
    base = [
        "-std=c++26", "-stdlib=libc++", "-fno-rtti", "-fasynchronous-unwind-tables",
        "-O1", "-g0", "-Wno-undefined-inline", "-DUWVM=2", "-DUWVM_USE_UWVM_INT",
        "-DUWVM_DISABLE_JIT", "-I", "src", "-I", "third-parties/bizwen/include",
        "-I", "third-parties/fast_io/include", "-I", "third-parties/boost_unordered/include",
    ]
    link = [clang, "-stdlib=libc++", "-rtlib=compiler-rt", "-unwindlib=libunwind", "-fuse-ld=lld"]
    sources = {
        "main": "src/uwvm2/uwvm/main.default.cpp",
        "runtime": "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp",
        "host": "src/uwvm2/uwvm/host_api.default.cpp",
    }
    source_hashes = {name: sha256(source / path) for name, path in sources.items()}
    results_path = output / "results.json"
    results = json.loads(results_path.read_text()) if results_path.exists() else []
    completed = {row["profile"] for row in results if row.get("passed")}
    for combine, combine_defines in COMBINE.items():
        for delay, delay_defines in DELAY.items():
            profile = f"{combine}-{delay}"
            if (args.only and args.only != profile) or profile in completed:
                continue
            defines = [f"-D{value}" for value in (*combine_defines, *delay_defines)]
            artifacts = {name: output / f"{profile}-{name}.o" for name in sources}
            binary = output / f"{profile}-uwvm"
            row = {
                "profile": profile, "defines": defines, "source": str(source),
                "source_sha256": source_hashes, "oracle_sha256": sha256(args.oracle),
                "build": [], "runs": [], "passed": False,
            }
            print(f"BUILD {profile}", flush=True)
            for name, relative_path in sources.items():
                command = [clang, *base, *defines, "-c", relative_path, "-o", str(artifacts[name])]
                rc, seconds, log = run(command, cwd=source, output=output / f"{profile}-{name}.build.log")
                row["build"].append({"unit": name, "command": command, "exit": rc, "seconds": seconds})
                if rc:
                    results.append(row)
                    results_path.write_text(json.dumps(results, indent=2) + "\n")
                    raise RuntimeError(f"{profile} {name} build failed:\n{log[-8000:]}")
            command = [*link, *(str(artifacts[name]) for name in sources), "-o", str(binary), "-lm", "-ldl", "-pthread"]
            rc, seconds, log = run(command, cwd=source, output=output / f"{profile}-link.build.log")
            row["build"].append({"unit": "link", "command": command, "exit": rc, "seconds": seconds})
            if rc:
                results.append(row)
                results_path.write_text(json.dumps(results, indent=2) + "\n")
                raise RuntimeError(f"{profile} link failed:\n{log[-8000:]}")
            row["binary_sha256"] = sha256(binary)
            row["object_sha256"] = {name: sha256(path) for name, path in artifacts.items()}
            for name, features in TESTS.items():
                command = [str(binary), "-Rint", *(f"-WFE-{feature}" for feature in features)]
                if name == "eh-exnref":
                    command.extend(("-WFD-gc", "-WFD-function-references"))
                command.extend(("--run", tests[name]["wasm"]))
                rc, seconds, log = run(command, cwd=source, output=output / f"{profile}-{name}.run.log", timeout=30)
                entry = {"test": name, "command": command, "exit": rc, "seconds": seconds}
                row["runs"].append(entry)
                if rc:
                    results.append(row)
                    results_path.write_text(json.dumps(results, indent=2) + "\n")
                    raise RuntimeError(f"{profile} {name} failed:\n{log[-8000:]}")
            for feature, name in FEATURE_GATES.items():
                witness = tests[name]["wasm"]
                if feature == "gc" and args.gc_gate_wasm:
                    witness = str(args.gc_gate_wasm.resolve())
                elif feature == "exceptions" and args.exceptions_gate_wasm:
                    witness = str(args.exceptions_gate_wasm.resolve())
                command = [str(binary), "-m", "validation",
                           *(f"-WFE-{item}" for item in TESTS[name] if item != feature),
                           f"-WFD-{feature}", "--run", witness]
                rc, seconds, log = run(command, cwd=source,
                                       output=output / f"{profile}-{feature}-disabled.run.log", timeout=30)
                entry = {"test": f"{feature}-disabled", "command": command,
                         "exit": rc, "seconds": seconds, "diagnostic": log[-500:],
                         "wasm_sha256": sha256(Path(witness))}
                row["runs"].append(entry)
                if rc == 0 or f"--wasm-feature-enable-{feature}" not in log:
                    results.append(row)
                    results_path.write_text(json.dumps(results, indent=2) + "\n")
                    raise RuntimeError(f"{profile} {feature} gate failed:\n{log[-8000:]}")
            if args.bottom_wasm:
                bottom = args.bottom_wasm.resolve()
                command = [str(binary), "-m", "validation", "-WFE-reference-types",
                           "-WFE-function-references", "-WFE-gc", "-WFE-exceptions",
                           "-WFE-tail-call", "--run", str(bottom)]
                rc, seconds, log = run(command, cwd=source,
                                       output=output / f"{profile}-cast-convert-bottom.run.log", timeout=30)
                row["runs"].append({"test": "cast-convert-bottom-validation", "command": command,
                                    "exit": rc, "seconds": seconds, "wasm_sha256": sha256(bottom)})
                if rc:
                    results.append(row)
                    results_path.write_text(json.dumps(results, indent=2) + "\n")
                    raise RuntimeError(f"{profile} cast-convert-bottom failed:\n{log[-8000:]}")
            if profile in ("none-heavy", "extra-heavy"):
                checker = source / "test/0017.runtime/check_thread_wait_tailcalls.py"
                for kind, option in (("memory32", []), ("memory64", ["--memory64"])):
                    assembly = output / f"{profile}-{kind}-tailcalls.json"
                    command = ["python3", str(checker), str(artifacts["runtime"]), str(assembly), *option]
                    rc, seconds, log = run(command, cwd=source, output=output / f"{profile}-{kind}-tailcalls.log", timeout=90)
                    row.setdefault("assembly", []).append({"kind": kind, "command": command, "exit": rc, "seconds": seconds})
                    if rc:
                        results.append(row)
                        results_path.write_text(json.dumps(results, indent=2) + "\n")
                        raise RuntimeError(f"{profile} {kind} assembly check failed:\n{log[-8000:]}")
                memory_checker = source / "test/0017.runtime/check_memory64_integer_codegen.py"
                memory_assembly = output / f"{profile}-memory64-load-store-asm"
                command = ["python3", str(memory_checker), str(binary), str(memory_assembly),
                           "--llvm", "/toolchain/bin"]
                rc, seconds, log = run(command, cwd=source,
                                       output=output / f"{profile}-memory64-load-store-asm.log", timeout=90)
                row.setdefault("assembly", []).append({"kind": "memory64-load-store", "command": command,
                                                       "exit": rc, "seconds": seconds})
                if rc:
                    results.append(row)
                    results_path.write_text(json.dumps(results, indent=2) + "\n")
                    raise RuntimeError(f"{profile} memory64 assembly check failed:\n{log[-8000:]}")
            row["passed"] = True
            results.append(row)
            results_path.write_text(json.dumps(results, indent=2) + "\n")
            for path in (*artifacts.values(), binary):
                path.unlink()
            print(f"PASS {profile}: {len(row['runs'])} Core 3 checks", flush=True)
    assert len([row for row in results if row.get("passed")]) == 12 if not args.only else True
    print("PASS ROS build-time matrix", len([row for row in results if row.get("passed")]), flush=True)


if __name__ == "__main__":
    main()
