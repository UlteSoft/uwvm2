#!/usr/bin/env python3
"""Source-bound native exception-payload root experiment in the Linux cgroup.

The reviewed root scanner and optional sweep prototype are explicit overlays.
This does not activate product GC, qualify activation roots or claim a VM mode
passes the bounded-RSS release gate. Uses real immutable exception values and native C++ propagation.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shutil
import sys


def sha(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--candidate-header", type=Path, required=True)
    parser.add_argument("--candidate-sha256", required=True)
    parser.add_argument("--sweep-header", type=Path, required=True)
    parser.add_argument("--sweep-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--cpu", type=int, choices=range(16, 32), default=16)
    args = parser.parse_args()
    own_root = Path(__file__).resolve().parents[2]
    root, out = args.source_root.resolve(strict=True), args.out.resolve()
    candidate, sweep = args.candidate_header.resolve(strict=True), args.sweep_header.resolve(strict=True)
    if sha(candidate) != args.candidate_sha256 or sha(sweep) != args.sweep_sha256:
        raise RuntimeError("reviewed overlay hash mismatch")
    helper_file = own_root / "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py"
    spec = importlib.util.spec_from_file_location("uwvm_payload_roots_native_controls", helper_file)
    if spec is None or spec.loader is None:
        raise RuntimeError("native process/cgroup watchdog module is unavailable")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    before_cgroup = helper.cgroup_state(root)
    if any(before_cgroup["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup has a preceding OOM; preserve and use a clean qualification")
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError("requested E-core is not in the required cgroup")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for directory in ("src", "third-parties"):
        if out.is_relative_to(root / directory):
            raise RuntimeError("evidence cannot modify frozen product inputs")
    out.mkdir(parents=True, exist_ok=False)
    compiler_name = shutil.which(args.cxx)
    if compiler_name is None:
        raise RuntimeError("C++ compiler not found")
    # Clang dispatches its language driver using argv[0]. Keep clang++ when it
    # is a symlink to clang; hash the resolved executable separately.
    compiler = Path(compiler_name).absolute()
    compiler_real = compiler.resolve(strict=True)
    fixture = own_root / "test/0019.gc_statepoint/exception_payload_roots.cc"
    value_header = root / "src/uwvm2/runtime/exception/value.h"
    product_store = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
    fingerprinter = root / "tools/ci/wasm3_source_fingerprint.py"
    input_paths = [Path(__file__).resolve(), helper_file, candidate, sweep, fixture,
                   value_header, product_store, fingerprinter, compiler_real]
    inputs = {str(path): sha(path) for path in input_paths}
    copied_inputs = {}
    summary = {"passed": False, "source_id": args.source_id,
        "scope": "native immutable payload visitor and explicit aggregate sweep/C++ propagation only; no product collector or guest activation discovery",
        "compiler_invocation": str(compiler), "compiler_realpath": str(compiler_real),
        "inputs": inputs, "cgroup_before": before_cgroup, "commands": []}
    (out / "inputs.json").write_text(json.dumps(summary, indent=2) + "\n")

    def run(name, command, env=None):
        helper.cgroup_state(root)
        row = helper.invoke(list(map(str, command)), out, name, root, timeout=300, env=env)
        summary["commands"].append(row)
        (out / "commands.json").write_text(json.dumps(summary["commands"], indent=2) + "\n")
        if row["exit"] != 0 or row["diagnostic"] is not None:
            raise RuntimeError(name + " failed; raw log retained")
        return row

    def fingerprint(name):
        destination = out / (name + ".json")
        run(name, [sys.executable, fingerprinter, root, destination])
        observed = json.loads(destination.read_text())["source_id"]
        if observed != args.source_id:
            raise RuntimeError("actual source ID differs: " + observed)

    try:
        fingerprint("source-before")
        run("compiler-version", [compiler, "--version"])
        defines = ["-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT",
            "-DUWVM_DISABLE_JIT", "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL",
            "-DUWVM_VERSION_X=2", "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0",
            "-DNDEBUG", "-DUWVM_MODE_RELEASE"]
        includes = [root / "src", root / "third-parties/fast_io/include",
            root / "third-parties/bizwen/include", root / "third-parties/boost_unordered/include"]
        if any(not directory.is_dir() for directory in includes):
            raise RuntimeError("real source/container include closure is incomplete")
        common = [compiler, "-std=c++26", "-stdlib=libc++", *defines, "-fno-rtti", "-pthread",
            "-fstack-clash-protection", "-mstack-probe-size=4096"]
        link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
        sanitizer_env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
            UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        summary["profiles"] = []
        for name, store_header, extra in (("product-store", product_store, []),
            ("exclusive-sweep", sweep, ["-DUWVM_EXCEPTION_ROOT_SWEEP_PROBE=1"])):
            prefix = out / name / "include"
            destination = prefix / "uwvm2/uwvm/runtime/storage"
            destination.mkdir(parents=True)
            exception_destination = prefix / "uwvm2/runtime/exception"
            exception_destination.mkdir(parents=True)
            # The new visitor includes value.h using a quote include. Preserve
            # the ACTUAL frozen immutable-value bytes alongside this explicit
            # visitor overlay. The actual store is never an SDK/module mock.
            for source, target in ((value_header, exception_destination / "value.h"),
                (store_header, destination / "gc_object.h"), (candidate, exception_destination / "roots.h")):
                target.write_bytes(source.read_bytes())
                if sha(target) != inputs[str(source)]:
                    raise RuntimeError("overlay copy is not byte exact")
                copied_inputs[str(target)] = inputs[str(source)]
            profile = {"name": name, "prefix": str(prefix), "store_sha256": sha(store_header),
                "value_sha256": sha(exception_destination / "value.h"), "executables": {}}
            summary["profiles"].append(profile)
            flags = [*common, *extra, "-I", prefix,
                *[item for directory in includes for item in ("-I", directory)]]
            run(name + "-syntax", [*flags, "-fsyntax-only", fixture])
            for variant, optimization in (("o3", ["-O3"]),
                ("asan-ubsan-lsan", ["-O1", "-g1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])):
                executable = out / name / variant
                run(name + "-" + variant + "-build", [*flags, *optimization, fixture, "-o", executable, *link])
                run(name + "-" + variant + "-run", [executable], sanitizer_env)
                log = (out / (name + "-" + variant + "-run.log")).read_text()
                marker = "actual_collections=6 reclaimed=7 unwind_collections=3" if name == "exclusive-sweep" else "collector_activation=absent"
                if "exception payload roots PASS checks=" not in log or marker not in log:
                    raise RuntimeError("required actual collection/scope marker is absent")
                profile["executables"][variant] = {"sha256": sha(executable), "marker": marker}
        fingerprint("source-after")
        if any(sha(path) != digest for path, digest in inputs.items()):
            raise RuntimeError("qualification inputs changed")
        if any(sha(path) != digest for path, digest in copied_inputs.items()):
            raise RuntimeError("private overlay copies changed")
        summary["cgroup_after"] = helper.cgroup_state(root)
        if any(summary["cgroup_after"]["events"].get(key, 0) != before_cgroup["events"].get(key, 0)
               for key in ("oom", "oom_kill")):
            raise RuntimeError("cgroup OOM events changed during the qualification")
        summary["passed"] = True
    except BaseException as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["copied_inputs"] = copied_inputs
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS native exception payload roots and six exclusive prototype collections; product GC remains unqualified")


if __name__ == "__main__":
    main()
