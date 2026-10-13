#!/usr/bin/env python3
"""Source-bound native frame-root ABI proof under the Linux cgroup.

Uses the actual frame-root header, native pause domain, C++ unwinding and an
explicitly overlaid reviewed product-storage collector. This does not qualify
VM compiler maps, native JIT cleanup, host handles or automatic collection.
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
    parser.add_argument("--frame-header", type=Path, required=True)
    parser.add_argument("--frame-sha256", required=True)
    parser.add_argument("--collector-header", type=Path, required=True)
    parser.add_argument("--collector-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--cpu", type=int, choices=range(16, 32), default=16)
    parser.add_argument("--rss-limit-gib", type=int, choices=(1, 2, 3), default=2)
    args = parser.parse_args()
    root, out = args.source_root.resolve(strict=True), args.out.resolve()
    own_root = Path(__file__).resolve().parents[2]
    frame, collector = (path.resolve(strict=True) for path in (args.frame_header, args.collector_header))
    if sha(frame) != args.frame_sha256 or sha(collector) != args.collector_sha256:
        raise RuntimeError("reviewed native frame/collector header SHA mismatch")
    helper_file = own_root / "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py"
    spec = importlib.util.spec_from_file_location("uwvm_frame_roots_native_controls", helper_file)
    if spec is None or spec.loader is None:
        raise RuntimeError("native process/cgroup control helper unavailable")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    before = helper.cgroup_state(root)
    if any(before["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("preceding cgroup OOM invalidates this qualification")
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError("requested E-core is outside the required cgroup")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if any(out.is_relative_to(root / part) for part in ("src", "third-parties")):
        raise RuntimeError("test evidence cannot mutate the frozen source")
    out.mkdir(parents=True, exist_ok=False)
    compiler_name = shutil.which(args.cxx)
    if compiler_name is None:
        raise RuntimeError("actual C++ compiler is absent")
    # argv[0] selects Clang's C++ driver; hash its real executable separately.
    compiler = Path(compiler_name).absolute()
    real_compiler = compiler.resolve(strict=True)
    fixture = own_root / "test/0019.gc_statepoint/frame_roots_runtime.cc"
    pause = root / "src/uwvm2/utils/thread/collection_pause_domain.h"
    original = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
    fingerprinter = root / "tools/ci/wasm3_source_fingerprint.py"
    paths = (Path(__file__).resolve(), helper_file, fixture, frame, collector,
             pause, original, real_compiler, fingerprinter)
    inputs = {str(path): sha(path) for path in paths}
    copied = {}
    summary = {"passed": False, "source_id": args.source_id, "inputs": inputs,
        "scope": "native frame-root ABI, genuine C++ cleanup and paused participants with explicit product-storage collection only",
        "vm_compiler_maps_qualified": False, "automatic_vm_collection_qualified": False,
        "cgroup_before": before, "cpu": args.cpu,
        "process_tree_rss_budget_bytes": args.rss_limit_gib * 1024**3,
        "commands": [], "profiles": []}
    (out / "inputs.json").write_text(json.dumps(summary, indent=2) + "\n")

    def run(name, command, env=None):
        helper.cgroup_state(root)
        row = helper.invoke(list(map(str, command)), out, name, root, timeout=300,
            env=env, rss_limit=summary["process_tree_rss_budget_bytes"])
        summary["commands"].append(row)
        (out / "commands.json").write_text(json.dumps(summary["commands"], indent=2) + "\n")
        if row["exit"] != 0 or row["diagnostic"] is not None:
            raise RuntimeError(name + " failed; actual raw log retained")

    def fingerprint(name):
        destination = out / (name + ".json")
        run(name, [sys.executable, fingerprinter, root, destination])
        if json.loads(destination.read_text())["source_id"] != args.source_id:
            raise RuntimeError("actual frozen source ID mismatch")

    try:
        fingerprint("source-before")
        run("compiler-version", [compiler, "--version"])
        prefix = out / "include"
        for relative, source in (("uwvm2/runtime/gc/frame_roots.h", frame),
                                 ("uwvm2/uwvm/runtime/storage/gc_object.h", collector)):
            destination = prefix / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(source.read_bytes())
            if sha(destination) != inputs[str(source)]:
                raise RuntimeError("explicit reviewed overlay copy changed")
            copied[str(destination)] = sha(destination)
        includes = (prefix, root / "src", root / "third-parties/fast_io/include",
                    root / "third-parties/bizwen/include", root / "third-parties/boost_unordered/include")
        if any(not path.is_dir() for path in includes):
            raise RuntimeError("actual source/header closure is incomplete")
        flags = [compiler, "-std=c++26", "-stdlib=libc++", "-pthread", "-fno-rtti",
            "-fstack-clash-protection", "-mstack-probe-size=4096", "-DNDEBUG", "-DUWVM_MODE_RELEASE",
            "-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT", "-DUWVM_DISABLE_JIT",
            "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL", "-DUWVM_VERSION_X=2",
            "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0",
            *[item for path in includes for item in ("-I", path)]]
        link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
        sanitizer_env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
            UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        run("native-frame-syntax", [*flags, "-fsyntax-only", fixture])
        for name, options in (("o3", ["-O3"]), ("asan-ubsan-lsan",
            ["-O1", "-g0", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])):
            binary = out / name
            run(name + "-build", [*flags, *options, fixture, "-o", binary, *link])
            run(name + "-run", [binary], env=sanitizer_env)
            log = (out / (name + "-run.log")).read_text()
            if "PASS native frame roots:" not in log or "6 actual collections; 15 reclaimed;" not in log:
                raise RuntimeError("actual root/pause/cleanup collection evidence is absent")
            summary["profiles"].append({"name": name, "executable_sha256": sha(binary)})
        fingerprint("source-after")
        if any(sha(path) != digest for path, digest in inputs.items()):
            raise RuntimeError("source-bound native qualification input changed")
        if any(sha(path) != digest for path, digest in copied.items()):
            raise RuntimeError("reviewed private header overlay changed")
        summary["cgroup_after"] = helper.cgroup_state(root)
        if any(summary["cgroup_after"]["events"].get(key, 0) != before["events"].get(key, 0)
               for key in ("oom", "oom_kill")):
            raise RuntimeError("cgroup OOM events changed")
        summary["passed"] = True
    except BaseException as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["copied_headers"] = copied
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS native root ABI/pause/C++ cleanup; compiler-root and automatic VM GC qualification remain pending")


if __name__ == "__main__":
    main()
