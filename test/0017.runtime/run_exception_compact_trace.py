#!/usr/bin/env python3
"""Actual compact exception-header ownership/allocation proof on remote Linux.

No old runtime object is linked. An explicit private exception-folder overlay
also prevents quoted includes from accidentally selecting the old value.h.
This is a native component proof, not a whole-VM performance qualification.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import shlex
import shutil
import signal
import subprocess
import sys
import time


BOOTSTRAP = "import json,os,signal,sys; os.kill(os.getpid(),signal.SIGSTOP); a=json.loads(sys.argv[1]); os.execvpe(a[0],a,os.environ)"
SCOPE = "actual exception header; native ownership, allocation count, concurrent lazy names and C++ propagation only"


def sha(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def load_helper(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("required source-bound process control helper is absent")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def bounded_invoke(command, out, name, root, controls, cgroup_reader, env=None, *, memory_budget_bytes=2 * 1024**3):
    """Pin our stopped Popen root and descendants; never signal a numeric peer."""
    command = list(map(str, command))
    log = out / (name + ".log")
    (out / (name + ".command")).write_text(shlex.join(command) + "\n")
    before = cgroup_reader(root)
    if memory_budget_bytes not in (2 * 1024**3, 3 * 1024**3):
        raise RuntimeError("native command budget must be an explicit 2 or 3 GiB slot")
    limit = memory_budget_bytes
    if int(before["memory.current"]) + limit >= 63_000_000_000:
        raise RuntimeError("shared cgroup has no independently reserved native component window")
    start, peak, child, owned, failure, status, usage = time.monotonic(), 0, None, None, None, None, None
    bootstrap_argv = [sys.executable, "-c", BOOTSTRAP, json.dumps(command)]
    with log.open("wb") as stream:
        try:
            child = subprocess.Popen(bootstrap_argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                     env=env, start_new_session=True)
            descriptor = os.pidfd_open(child.pid)
            try:
                birth = controls.start_time(child.pid)
                while True:
                    if time.monotonic() - start > 5:
                        raise RuntimeError("owned bootstrap did not stop before execution admission")
                    current_birth, state = controls.identity(child.pid)
                    if current_birth != birth or state in ("Z", "X"):
                        raise RuntimeError("owned bootstrap identity retired before admission")
                    if state in ("T", "t"):
                        break
                    time.sleep(0.01)
                rows = controls.tree(child.pid)
                if len(rows) != 1 or rows[0]["pid"] != child.pid or rows[0]["start_time"] != birth:
                    raise RuntimeError("fixed stopped bootstrap is not a unique owned process")
                if rows[0]["uid"] != os.getuid() or rows[0]["parent_pid"] != os.getpid():
                    raise RuntimeError("stopped bootstrap UID or direct ancestry changed")
                actual_argv = [part.decode() for part in Path(f"/proc/{child.pid}/cmdline").read_bytes().split(b"\0") if part]
                if actual_argv != bootstrap_argv:
                    raise RuntimeError("stopped bootstrap argv differs from the exact launched command")
                controls.validate_membership(rows, Path("/sys/fs/cgroup"), [16])
                owned = controls.OwnedWorkerTree(rows[0], os.getuid(), Path("/sys/fs/cgroup"), descriptor)
                descriptor = None
            finally:
                if descriptor is not None:
                    signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                    os.close(descriptor)
            signal.pidfd_send_signal(owned.pidfds[child.pid], signal.SIGCONT)
            while True:
                waited, raw, measured = os.wait4(child.pid, os.WNOHANG)
                if waited:
                    status, usage = os.waitstatus_to_exitcode(raw), measured
                    child.returncode = status
                    remaining = owned.refresh()
                    if any(row["state"] not in ("Z", "X") for row in remaining):
                        raise RuntimeError("native command retired with a still-live owned descendant")
                    break
                rows = owned.refresh()
                if not rows:
                    raise RuntimeError("live unreaped command has no readable process-tree identity")
                resident = sum(row["rss_bytes"] for row in rows)
                peak = max(peak, resident)
                current = cgroup_reader(root)
                if any(current["events"].get(key, 0) != before["events"].get(key, 0) for key in ("oom", "oom_kill")):
                    raise RuntimeError("shared cgroup OOM events changed")
                if resident > limit or int(current["memory.current"]) >= 63_000_000_000:
                    raise RuntimeError("native or shared resident-memory guard stopped this command")
                if time.monotonic() - start > 180:
                    raise RuntimeError("native component command exceeded its bounded deadline")
                time.sleep(0.1)
        except BaseException as error:
            failure = str(error)
            if owned is not None:
                errors = owned.stop_and_kill()
                if errors:
                    failure += "; teardown: " + "; ".join(errors)
            elif child is not None and child.returncode is None:
                # This is our unreaped direct Popen child in its newly created
                # session. Its PID/PGID cannot be reused until our wait4; no
                # separately discovered numeric process is ever signalled.
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if child is not None and child.returncode is None:
                _, raw, usage = os.wait4(child.pid, 0)
                status = os.waitstatus_to_exitcode(raw)
                child.returncode = status
        finally:
            if owned is not None:
                owned.close()
    if usage is not None:
        peak = max(peak, usage.ru_maxrss * 1024)
        if peak > limit:
            failure = failure or "wait4 resident-memory evidence exceeded the explicit native budget"
    return {"name": name, "command": command, "exit": status, "failure": failure,
            "elapsed_seconds": time.monotonic() - start, "peak_process_tree_rss_bytes": peak,
            "process_tree_rss_budget_bytes": limit, "log": str(log), "log_sha256": sha(log)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--value-header", type=Path, required=True)
    parser.add_argument("--value-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--tsan", action="store_true")
    args = parser.parse_args()
    root, out = args.source_root.resolve(strict=True), args.out.resolve()
    own_root = Path(__file__).resolve().parents[2]
    candidate = args.value_header.resolve(strict=True)
    if sha(candidate) != args.value_sha256:
        raise RuntimeError("reviewed exception header SHA does not match")
    if any(out.is_relative_to(root / part) for part in ("src", "third-parties")):
        raise RuntimeError("native proof must not mutate frozen source inputs")
    controls_path = own_root / "tools/ci/run_core3_component_bounded_slot.py"
    reader_path = own_root / "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py"
    controls = load_helper(controls_path, "uwvm_exception_owned_process_tree")
    reader = load_helper(reader_path, "uwvm_exception_cgroup_reader")
    before = reader.cgroup_state(root)
    if any(before["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("preceding shared cgroup OOM invalidates the native component proof")
    if 16 not in os.sched_getaffinity(0):
        raise RuntimeError("the established E16 CPU is not available")
    os.sched_setaffinity(0, {16})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    compiler = Path(shutil.which(args.cxx) or args.cxx).absolute()
    compiler_bytes = compiler.resolve(strict=True)
    out.mkdir(parents=True, exist_ok=False)
    fixture = own_root / "test/0017.runtime/exception_compact_trace.cc"
    regression = own_root / "test/0017.runtime/exception_value.cc"
    cold_capture = own_root / "test/0013.uwvm_int/wasm3/exception_diagnostic_trace.cc"
    inputs = {str(path): sha(path) for path in (candidate, fixture, regression, cold_capture, Path(__file__).resolve(),
        controls_path, reader_path, compiler_bytes, root / "tools/ci/wasm3_source_fingerprint.py")}
    summary = {"passed": False, "scope": SCOPE, "whole_vm_qualified": False, "performance_qualified": False,
        "source_id": args.source_id, "cgroup_before": before, "inputs_before": inputs, "commands": [], "profiles": []}
    copied = {}

    def run(name, command, env=None):
        row = bounded_invoke(command, out, name, root, controls, reader.cgroup_state, env)
        summary["commands"].append(row)
        (out / "commands.json").write_text(json.dumps(summary["commands"], indent=2) + "\n")
        if row["exit"] != 0 or row["failure"] is not None:
            raise RuntimeError(name + " failed; actual raw command/log retained")

    def fingerprint(name):
        destination = out / (name + ".json")
        run(name, [sys.executable, root / "tools/ci/wasm3_source_fingerprint.py", root, destination])
        if json.loads(destination.read_text())["source_id"] != args.source_id:
            raise RuntimeError("frozen product source ID differs from its selected cohort")

    try:
        fingerprint("source-before")
        run("compiler-version", [compiler, "--version"])
        prefix = out / "include"
        destination = prefix / "uwvm2/runtime/exception"
        shutil.copytree(root / "src/uwvm2/runtime/exception", destination)
        (destination / "value.h").write_bytes(candidate.read_bytes())
        for path in destination.rglob("*"):
            if path.is_file():
                copied[str(path)] = sha(path)
        includes = (prefix, root / "src", root / "third-parties/fast_io/include",
                    root / "third-parties/bizwen/include", root / "third-parties/boost_unordered/include")
        flags = [compiler, "-std=c++26", "-stdlib=libc++", "-pthread", "-fno-rtti",
            "-fstack-clash-protection", "-mstack-probe-size=4096", "-DNDEBUG", "-DUWVM_MODE_RELEASE",
            "-DUWVM=2", "-DUWVM_TEST=2", "-DUWVM_USE_DEFAULT_INT", "-DUWVM_DISABLE_JIT",
            "-DUWVM_DISABLE_DEBUG_INT", "-DUWVM_USE_THREAD_LOCAL", "-DUWVM_VERSION_X=2",
            "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0",
            *[item for path in includes for item in ("-I", path)]]
        link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1", TSAN_OPTIONS="halt_on_error=1")
        run("compact-syntax", [*flags, "-fsyntax-only", fixture])
        profiles = [("o3", ["-O3"]), ("asan-ubsan-lsan", ["-O1", "-g0", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])]
        if args.tsan:
            profiles.append(("tsan", ["-O1", "-g0", "-fsanitize=thread"]))
        for name, options in profiles:
            for label, source, extra in (("compact", fixture, ["-Wl,--wrap=_Znwm"]),
                                        ("legacy-value", regression, []), ("cold-capture", cold_capture, [])):
                binary, depfile = out / (name + "-" + label), out / (name + "-" + label + ".d")
                run(name + "-" + label + "-build", [*flags, *options, "-MD", "-MF", depfile, source, "-o", binary, *extra, *link])
                dependencies = depfile.read_text().replace("\\\n", " ").split()
                if str(destination / "value.h") not in dependencies or str(root / "src/uwvm2/runtime/exception/value.h") in dependencies:
                    raise RuntimeError("the actual compilation selected a stale exception header")
                run(name + "-" + label + "-run", [binary], env)
                if "PASS" not in (out / (name + "-" + label + "-run.log")).read_text():
                    raise RuntimeError("native fixture did not emit its actual success witness")
                summary["profiles"].append({"profile": name, "fixture": label, "executable_sha256": sha(binary), "dependency_sha256": sha(depfile)})
        fingerprint("source-after")
        if any(sha(path) != digest for path, digest in inputs.items()) or any(sha(path) != digest for path, digest in copied.items()):
            raise RuntimeError("native qualification input or private header closure changed")
        after = reader.cgroup_state(root)
        summary["cgroup_after"] = after
        if any(after["events"].get(key, 0) != before["events"].get(key, 0) for key in ("oom", "oom_kill")):
            raise RuntimeError("shared cgroup OOM events changed")
        summary["passed"] = True
    except BaseException as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["copied_exception_folder"] = copied
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS compact exception native header ownership/allocation/concurrency; whole-VM performance remains unqualified")


if __name__ == "__main__":
    main()
