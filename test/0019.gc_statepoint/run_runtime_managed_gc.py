#!/usr/bin/env python3
"""Source-bound actual CLI automatic-GC gate, queued only; no native mock roots.

Use only under the parent-authorized remote Linux host controller after a fresh
whole-source runtime/CLI build. This runner performs no product/native compile.
The native admission fixture is a separate component test, never this gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import resource
import shlex
import signal
import subprocess
import sys
import time

FIXTURE = "test/0019.gc_statepoint/runtime_managed_gc_ring.wat"
HEADERS = (
    "src/uwvm2/runtime/gc/entry_admission.h",
    "src/uwvm2/runtime/gc/instance_phase.h",
    "src/uwvm2/runtime/gc/allocation_policy.h",
    "src/uwvm2/runtime/gc/collection_transaction.h",
    "src/uwvm2/runtime/gc/managed_collection.h",
    "src/uwvm2/runtime/gc/frame_roots.h",
    "src/uwvm2/runtime/gc/impl.h",
    "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp",
    "src/uwvm2/runtime/lib/uwvm_runtime.h",
    "src/uwvm2/runtime/lib/uwvm_runtime_state_signature.h",
    "src/uwvm2/uwvm/runtime/storage/gc_object.h",
    "src/uwvm2/uwvm/runtime/storage/gc_static_roots.h",
    "src/uwvm2/uwvm/runtime/storage/wasm_module.h",
    "src/uwvm2/uwvm/runtime/initializer/init.h",
    "src/uwvm2/uwvm/run/run.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_roots_emit.h",
)

def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def tree_rss(root):
    # Walk actual descendants; the outer HOST guard independently binds the
    # Docker worker to its PID birth and cgroup, including namespace crossings.
    pending, seen, total = [root], set(), 0
    while pending:
        pid = pending.pop()
        if pid in seen:
            continue
        seen.add(pid)
        base = Path("/proc") / str(pid)
        try:
            for line in (base / "status").read_text().splitlines():
                if line.startswith("VmRSS:"):
                    total += int(line.split()[1]) * 1024
            for task in (base / "task").iterdir():
                pending.extend(int(value) for value in (task / "children").read_text().split())
        except (FileNotFoundError, ProcessLookupError):
            pass
    return total

def resources():
    root = Path("/sys/fs/cgroup")
    fields = ("memory.max", "memory.swap.max", "memory.current", "cpuset.cpus.effective", "memory.events")
    result = {key: (root / key).read_text().strip() for key in fields}
    result["events"] = {key: int(value) for key, value in
                        (line.split() for line in result["memory.events"].splitlines())}
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-root", type=Path, required=True)
    p.add_argument("--cli-build", type=Path, required=True)
    p.add_argument("--expected-source-id", required=True)
    p.add_argument("--expected-inputs", type=Path, required=True)
    p.add_argument("--repository", choices=("uwvm2", "uwvm2-ros"), required=True)
    p.add_argument("--wasm-tools", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--stop-memory-bytes", type=int, default=63_000_000_000)
    p.add_argument("--tree-rss-bytes", type=int, default=2 * 1024**3)
    a = p.parse_args()
    require(platform.system() == "Linux" and platform.machine() == "x86_64",
            "this initial actual CLI gate only qualifies remote Linux x86-64")
    require(0 < a.stop_memory_bytes < 68719476736 and 0 < a.tree_rss_bytes <= 2 * 1024**3,
            "budgets exceed this bounded correctness slot")
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", a.expected_source_id), "invalid canonical source ID")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    source = a.source_root.resolve(strict=True)
    build = a.cli_build.resolve(strict=True)
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    before = resources()
    require(before["memory.max"] == "68719476736" and before["memory.swap.max"] == "0" and
            before["cpuset.cpus.effective"] == "0,2,4,6,16-31", "require exact 64GiB/swap0/4P+16E cgroup")
    selected = json.loads(a.expected_inputs.read_text())
    needed = set(HEADERS) | {FIXTURE}
    require(set(selected) == needed, "reviewed complete source input list differs")
    require(all(sha(source / path) == selected[path] for path in needed), "reviewed source input changed")
    metadata = json.loads((build / "build.json").read_text())
    require(metadata["passed"] is True and metadata["fresh_product_compilation"] is True and
            metadata["old_runtime_object_reused"] is False, "require fresh successful whole product build")
    require(metadata["source_id"] == a.expected_source_id and Path(metadata["source"]).resolve() == source,
            "product/runtime belong to a different source snapshot")
    require(sha(build / "runtime.o") == metadata["runtime_object_sha256"] and
            sha(build / "uwvm") == metadata["binary_sha256"], "actual product/object hash changed")
    commands, summary = [], {"passed": False, "scope": "actual whole CLI full-LLVM managed GC correctness",
        "repository": a.repository, "source_id": a.expected_source_id, "resources_before": before,
        "selected": selected, "build_manifest_sha256": sha(build / "build.json"),
        "binary_sha256": sha(build / "uwvm"), "runtime_object_sha256": sha(build / "runtime.o"),
        "wasm_tools_path": str(a.wasm_tools.absolute()), "wasm_tools_sha256": sha(a.wasm_tools),
        "commands": commands, "performance_qualification": False}

    def run(label, argv, timeout=180):
        initial = resources()
        require(int(initial["memory.current"]) + a.tree_rss_bytes < a.stop_memory_bytes,
                "fresh guard refuses insufficient shared startup headroom")
        (out / (label + ".command")).write_text(shlex.join(map(str, argv)) + "\n")
        row = {"label": label, "argv": list(map(str, argv)), "resources_before": initial,
               "peak_tree_rss": 0, "peak_cgroup_current": 0, "guard_stopped": False, "timeout": False}
        started = time.monotonic()
        with (out / (label + ".log")).open("wb") as stream:
            child = subprocess.Popen(argv, cwd=source, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
            while child.poll() is None:
                rss, current = tree_rss(child.pid), int(resources()["memory.current"])
                row["peak_tree_rss"] = max(row["peak_tree_rss"], rss)
                row["peak_cgroup_current"] = max(row["peak_cgroup_current"], current)
                row["guard_stopped"] = rss >= a.tree_rss_bytes or current >= a.stop_memory_bytes
                row["timeout"] = time.monotonic() - started >= timeout
                if row["guard_stopped"] or row["timeout"]:
                    try:
                        os.killpg(child.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        child.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        try:
                            os.killpg(child.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                        child.wait()
                    break
                try:
                    child.wait(timeout=.05)
                except subprocess.TimeoutExpired:
                    pass
        row.update(exit=child.returncode, seconds=time.monotonic() - started,
                   resources_after=resources(), log_sha256=sha(out / (label + ".log")))
        commands.append(row)
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        require(row["exit"] == 0 and not row["guard_stopped"] and not row["timeout"],
                label + " failed; original argv and failure log preserved")

    def fingerprint(label):
        file = out / (label + ".json")
        run(label, [sys.executable, str(source / "tools/ci/wasm3_source_fingerprint.py"), str(source), str(file)])
        require(json.loads(file.read_text())["source_id"] == a.expected_source_id,
                "whole product source fingerprint changed")

    try:
        fingerprint("source-before")
        run("parse-ring", ["taskset", "-c", "16", str(a.wasm_tools), "parse", str(source / FIXTURE), "-o", str(out / "ring.wasm")])
        run("validate-ring", ["taskset", "-c", "16", str(a.wasm_tools), "validate", str(out / "ring.wasm")])
        # Unused real tag keeps the same ring semantics but rejects the first
        # automatic-GC slice because C++ exception owners are not enrolled yet.
        negative = out / "tag-ineligible.wat"
        text = (source / FIXTURE).read_text()
        require(text.startswith("(module\n"), "unexpected generator fixture")
        negative.write_text(text.replace("(module\n", "(module\n (tag $unused (param i32))\n", 1))
        run("parse-tag", ["taskset", "-c", "16", str(a.wasm_tools), "parse", str(negative), "-o", str(out / "tag.wasm")])
        run("validate-tag", ["taskset", "-c", "16", str(a.wasm_tools), "validate", str(out / "tag.wasm")])
        options = ["-Raot"] if a.repository == "uwvm2-ros" else ["-Rcc", "jit", "-Rcm", "full"]
        base = ["taskset", "-c", "16", str(build / "uwvm"), *options,
                "-Rllvm-cache-path", "disable", "-Rclog", "err", "-WFE-gc"]
        metrics = {}
        for mode in ("instruction", "unwind"):
            for kind in ("ring", "tag"):
                label = kind + "-" + mode
                path = out / ("ring.wasm" if kind == "ring" else "tag.wasm")
                argv = [*base, "-Rllvm-call-stack", mode, *(["-WFE-exceptions"] if kind == "tag" else []), "--run", str(path)]
                run(label, argv)
                found = re.findall(r"\[gc-managed\] ([^\n]+)", (out / (label + ".log")).read_text(errors="replace"))
                require(len(found) == 1, "missing exact native managed-GC report for " + label)
                values = dict(item.split("=", 1) for item in found[0].split())
                row = {key: int(value) for key, value in values.items()}
                require(row["roots_requested"] == 1, "current-source precise IR was not requested")
                if kind == "ring":
                    require(row["disabled"] == 0 and row["allocations"] == 65536 and
                            row["attempts"] == 16 and row["eligible"] > 0 and row["collections"] > 0 and
                            row["reclaimed"] > 0 and row["pauses"] >= row["collections"],
                            "no actual bounded automatic collection/reclamation witness")
                    require(row["peer_skips"] == 0 and row["heap_rejections"] == 0 and row["population_rejections"] == 0,
                            "single native CLI population failed to close")
                else:
                    require(row["disabled"] == 1 and row["collections"] == 0 and row["reclaimed"] == 0 and row["reason"] == 9,
                            "unadmitted exception population must execute normally without collecting")
                metrics[label] = row
        summary["metrics"] = metrics
        summary["guest_input_sha256"] = {name: sha(out / name) for name in ("ring.wasm", "tag.wasm", "tag-ineligible.wat")}
        fingerprint("source-after")
        require(all(sha(source / path) == selected[path] for path in needed), "reviewed input changed during execution")
        after = resources()
        require(after["events"]["oom"] == before["events"]["oom"] and
                after["events"]["oom_kill"] == before["events"]["oom_kill"], "OOM event invalidates qualification")
        summary.update(passed=True, resources_after=after)
    finally:
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS source-bound actual CLI managed GC ring and tag rejection; not a performance gate")

if __name__ == "__main__":
    main()
