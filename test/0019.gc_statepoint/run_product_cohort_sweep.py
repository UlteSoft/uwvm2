#!/usr/bin/env python3
"""Native product-membership collector checks in the required Linux cgroup.

Candidate and rejected headers are explicit source-bound overlays. No VM
collection is activated, and no activation/host/exception root discovery or
GC performance qualification is claimed by this runner.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import sys


def sha(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def teardown_overlay(text):
    declaration = "    class gc_object_store;\n"
    if text.count(declaration) != 1:
        raise RuntimeError("collector forward declaration is not unique")
    text = text.replace(declaration, declaration +
        "    inline void gc_cohort_test_after_unregistration(gc_object_store const*) noexcept;\n", 1)
    anchor = "        ~gc_object_store()\n        {\n            unregister_cohort_member();\n"
    if text.count(anchor) != 1:
        raise RuntimeError("actual after-unregister destructor hook site is not unique")
    return text.replace(anchor, anchor + "            gc_cohort_test_after_unregistration(this);\n", 1)


def assembly_body(text, symbol):
    match = re.search(r"^" + re.escape(symbol) + r":.*?^\s*\.size\s+" +
                      re.escape(symbol) + r",[^\n]*", text, re.M | re.S)
    if not match:
        raise RuntimeError("actual ELF assembly wrapper is absent: " + symbol)
    lines = []
    labels = {}

    def local_label(match):
        label = match[0]
        if label not in labels:
            labels[label] = ".Lnative" + str(len(labels))
        return labels[label]

    for line in match[0].splitlines():
        line = line.split("#", 1)[0].strip()
        if not line or line.startswith((".loc", ".file")):
            continue
        lines.append(re.sub(r"\.L[A-Za-z_][A-Za-z_0-9.$]*", local_label, line))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--candidate-header", type=Path, required=True)
    parser.add_argument("--candidate-sha256", required=True)
    parser.add_argument("--rejected-header", type=Path, required=True)
    parser.add_argument("--rejected-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--cpu", type=int, choices=range(16, 32), default=16)
    parser.add_argument("--rss-limit-gib", type=int, choices=(1, 2, 3), default=2)
    args = parser.parse_args()
    root, out = args.source_root.resolve(strict=True), args.out.resolve()
    own_root = Path(__file__).resolve().parents[2]
    candidate = args.candidate_header.resolve(strict=True)
    rejected = args.rejected_header.resolve(strict=True)
    if sha(candidate) != args.candidate_sha256 or sha(rejected) != args.rejected_sha256:
        raise RuntimeError("reviewed collector input SHA mismatch")
    helper_path = own_root / "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py"
    spec = importlib.util.spec_from_file_location("uwvm_gc_cohort_controls", helper_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("native resource controls unavailable")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    before_cgroup = helper.cgroup_state(root)
    if any(before_cgroup["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("preceding cgroup OOM invalidates this qualification")
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError("requested E-core is unavailable")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if any(out.is_relative_to(root / part) for part in ("src", "third-parties")):
        raise RuntimeError("evidence may not change frozen product source")
    out.mkdir(parents=True, exist_ok=False)
    invocation = shutil.which(args.cxx)
    if invocation is None:
        raise RuntimeError("actual C++ driver not found")
    compiler = Path(invocation).absolute()
    real_compiler = compiler.resolve(strict=True)
    fixture = own_root / "test/0019.gc_statepoint/product_cohort_sweep.cc"
    hot_fixture = own_root / "test/0019.gc_statepoint/gc_hot_field_probe.cc"
    original = root / "src/uwvm2/uwvm/runtime/storage/gc_object.h"
    fingerprinter = root / "tools/ci/wasm3_source_fingerprint.py"
    input_paths = (Path(__file__).resolve(), helper_path, fixture, hot_fixture,
                   candidate, rejected, original, real_compiler, fingerprinter)
    inputs = {str(path): sha(path) for path in input_paths}
    copied = {}
    summary = {"passed": False, "source_id": args.source_id,
        "scope": "native explicit aggregate collection and defensive teardown rejection only; no product GC activation",
        "inputs": inputs, "cgroup_before": before_cgroup,
        "cpu": args.cpu, "process_tree_rss_budget_bytes": args.rss_limit_gib * 1024**3,
        "commands": [], "profiles": [], "hot_assembly": []}
    (out / "inputs.json").write_text(json.dumps(summary, indent=2) + "\n")

    def run(name, command, env=None, expected_assertion=False):
        helper.cgroup_state(root)
        row = helper.invoke(list(map(str, command)), out, name, root, timeout=300,
            env=env, rss_limit=summary["process_tree_rss_budget_bytes"])
        summary["commands"].append(row)
        (out / "commands.json").write_text(json.dumps(summary["commands"], indent=2) + "\n")
        if row["diagnostic"] is not None:
            raise RuntimeError(name + " resource control failed; raw log retained")
        if expected_assertion:
            line = next(index for index, text in enumerate(fixture.read_text().splitlines(), 1)
                if "CHECK(status == gc::gc_object_status::invalid_reference && retired == 0uz)" in text)
            marker = "FAIL product cohort sweep line " + str(line)
            log = (out / (name + ".log")).read_text()
            if row["exit"] not in (-signal.SIGILL, -signal.SIGABRT) or log.strip() != marker:
                raise RuntimeError("rejected candidate did not fail the exact teardown assertion")
            row["expected_negative_control"] = marker
        elif row["exit"] != 0:
            raise RuntimeError(name + " failed; raw log retained")
        return row

    def fingerprint(name):
        destination = out / (name + ".json")
        run(name, [sys.executable, fingerprinter, root, destination])
        if json.loads(destination.read_text())["source_id"] != args.source_id:
            raise RuntimeError("actual frozen source ID mismatch")

    def overlay(name, source, hook=False):
        prefix = out / name / "include"
        header = prefix / "uwvm2/uwvm/runtime/storage/gc_object.h"
        header.parent.mkdir(parents=True)
        if hook:
            header.write_text(teardown_overlay(source.read_text()))
        else:
            header.write_bytes(source.read_bytes())
            if sha(header) != inputs[str(source)]:
                raise RuntimeError("native overlay copy is not byte exact")
        copied[str(header)] = sha(header)
        summary["profiles"].append({"name": name, "source_sha256": inputs[str(source)],
            "effective_header_sha256": sha(header), "private_destructor_hook": hook})
        return prefix

    try:
        fingerprint("source-before")
        run("compiler-version", [compiler, "--version"])
        includes = (root / "src", root / "third-parties/fast_io/include",
            root / "third-parties/bizwen/include", root / "third-parties/boost_unordered/include")
        if any(not path.is_dir() for path in includes):
            raise RuntimeError("actual container/source header closure is incomplete")
        flags = [compiler, "-std=c++26", "-stdlib=libc++", "-pthread", "-fno-rtti",
            "-fstack-clash-protection", "-mstack-probe-size=4096", "-DNDEBUG", "-DUWVM_MODE_RELEASE",
            "-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT", "-DUWVM_DISABLE_JIT",
            "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL", "-DUWVM_VERSION_X=2",
            "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0"]
        link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        for name, source, hook in (("collector", candidate, False),
            ("defensive-teardown", candidate, True), ("rejected-teardown", rejected, True)):
            prefix = overlay(name, source, hook)
            profile_flags = [*flags, "-DUWVM_GC_PRODUCT_COHORT_PROBE=1"]
            if hook:
                profile_flags.append("-DUWVM_GC_COHORT_DTOR_PROBE=1")
            profile_flags += ["-I", prefix, *[item for path in includes for item in ("-I", path)]]
            run(name + "-syntax", [*profile_flags, "-fsyntax-only", fixture])
            for variant, options in (("o3", ["-O3"]), ("asan-ubsan-lsan",
                ["-O1", "-g0", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])):
                if name == "rejected-teardown" and variant != "o3":
                    continue
                binary = out / name / variant
                run(name + "-" + variant + "-build", [*profile_flags, *options, fixture, "-o", binary, *link])
                negative = name == "rejected-teardown"
                run(name + "-" + variant + "-run", [binary], env=env, expected_assertion=negative)
                summary["profiles"][-1].setdefault("executables", {})[variant] = sha(binary)
                if not negative:
                    log = (out / (name + "-" + variant + "-run.log")).read_text()
                    counts = "8 actual collections; 7 reclaimed" if hook else "7 actual collections; 6 reclaimed"
                    if "PASS product cohort sweep:" not in log or counts not in log:
                        raise RuntimeError("actual collection/reclamation proof is absent")
                    if hook and "PASS defensive teardown rejection:" not in log:
                        raise RuntimeError("actual destructor gap proof is absent")
        asm = {}
        for name, header in (("hot-original", original), ("hot-collector", candidate)):
            prefix = overlay(name, header)
            path = out / (name + ".s")
            include_flags = ["-I", prefix, *[item for directory in includes for item in ("-I", directory)]]
            run(name + "-assembly", [*flags, *include_flags, "-O3", "-S", hot_fixture, "-o", path])
            asm[name] = path.read_text()
        for name in ("struct_get", "struct_set", "array_get", "array_set"):
            symbol = "uwvm_gc_probe_" + name
            old, new = (assembly_body(asm[profile], symbol) for profile in ("hot-original", "hot-collector"))
            row = {"symbol": symbol, "baseline_sha256": hashlib.sha256(old.encode()).hexdigest(),
                   "collector_sha256": hashlib.sha256(new.encode()).hexdigest(), "identical": old == new}
            summary["hot_assembly"].append(row)
            (out / (symbol + "-original.normalized.s")).write_text(old)
            (out / (symbol + "-collector.normalized.s")).write_text(new)
            if old != new:
                raise RuntimeError("actual field wrapper assembly changed; inspect both normalized bodies")
        fingerprint("source-after")
        if any(sha(path) != digest for path, digest in inputs.items()):
            raise RuntimeError("native qualification input changed")
        if any(sha(path) != digest for path, digest in copied.items()):
            raise RuntimeError("private copied/native hook header changed")
        summary["cgroup_after"] = helper.cgroup_state(root)
        if any(summary["cgroup_after"]["events"].get(key, 0) != before_cgroup["events"].get(key, 0)
               for key in ("oom", "oom_kill")):
            raise RuntimeError("cgroup OOM events changed")
        summary["passed"] = True
    except BaseException as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["copied_headers"] = copied
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS native collector and exact teardown negative control; unchanged field assembly; product GC activation absent")


if __name__ == "__main__":
    main()
