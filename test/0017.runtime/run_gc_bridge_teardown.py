#!/usr/bin/env python3
"""Bounded native regression for the final foreign extern-bridge lease.

The receiver drops its independent module lease roots before dropping its GC
store, matching runtime member destruction order. The bridge then owns the
source's last shared reference. Reentrant store teardown must complete after
the bridge is detached; a timed-out spin is a correctness failure.

Linux runs require the established 64 GiB swap-free cgroup. macOS uses a
bounded resident-memory watchdog. This is never a performance benchmark.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import resource
import signal
import subprocess
import sys
import time


HEADER = Path("uwvm2/uwvm/runtime/storage/gc_object.h")


def sha(file):
    with Path(file).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def process_tree_rss(pid):
    records = subprocess.check_output(["ps", "-A", "-o", "pid=,ppid=,rss="], text=True)
    children, sizes = {}, {}
    for line in records.splitlines():
        fields = line.split()
        if len(fields) != 3:
            continue
        child, parent, kib = map(int, fields)
        children.setdefault(parent, []).append(child)
        sizes[child] = kib * 1024
    stack, members, total = [pid], [], 0
    while stack:
        child = stack.pop()
        members.append(child)
        total += sizes.get(child, 0)
        stack.extend(children.get(child, ()))
    return total, members


def invoke(command, log, env, *, timeout, rss_limit, cwd):
    peak, reason, status, usage = 0, None, None, None
    started = time.monotonic()
    with log.open("wb") as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                   env=env, cwd=cwd, start_new_session=True)
        while status is None:
            observed, members = process_tree_rss(process.pid)
            peak = max(peak, observed)
            if observed > rss_limit:
                reason = "memory_limit_exceeded"
            elif time.monotonic() - started > timeout:
                reason = "timeout"
            if reason:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except (ProcessLookupError, PermissionError):
                    pass
                # Compiler helpers may create another process group. Keep the
                # bounded regression from leaving a detached child running.
                for child in reversed(members):
                    try:
                        os.kill(child, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            waited, raw_status, measured_usage = os.wait4(process.pid, os.WNOHANG)
            if waited:
                status = os.waitstatus_to_exitcode(raw_status)
                usage = measured_usage
                break
            time.sleep(0.025)
    process.returncode = status
    maxrss = usage.ru_maxrss * (1 if platform.system() == "Darwin" else 1024)
    peak = max(peak, maxrss)
    if peak > rss_limit:
        reason = "memory_limit_exceeded"
    return {"command": command, "exit": status, "diagnostic": reason,
            "elapsed_seconds": time.monotonic() - started,
            "peak_process_tree_rss_bytes": peak, "log": str(log),
            "log_sha256": sha(log)}


def source_id(root, manifest):
    return subprocess.check_output([sys.executable,
        str(root / "tools/ci/wasm3_source_fingerprint.py"), str(root), str(manifest)],
        text=True).strip()


def cgroup_state(root):
    if platform.system() != "Linux":
        return None
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    state = {key: (Path("/sys/fs/cgroup") / key).read_text().strip()
             for key in ("memory.max", "memory.swap.max", "memory.current",
                         "memory.events", "cpuset.cpus.effective")}
    if (state["memory.max"] != "68719476736" or state["memory.swap.max"] != "0" or
            state["cpuset.cpus.effective"] != "0,2,4,6,16-31"):
        raise RuntimeError("expected the established 64 GiB, swap-free 4P+16E cgroup")
    return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--overlay-root", type=Path,
                        help="isolated single-header overlay; product source is never edited")
    parser.add_argument("--expected-overlay-sha256")
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--timeout-seconds", type=float, default=5)
    parser.add_argument("--rss-limit-bytes", type=int,
                        help="default 512 MiB on macOS and 2 GiB on Linux; maximum 4 GiB")
    args = parser.parse_args()
    system = platform.system()
    if system not in ("Darwin", "Linux"):
        parser.error("this native lifecycle qualification supports macOS and Linux")
    if args.out.exists() or not 0 < args.timeout_seconds <= 60:
        parser.error("fresh output and a positive timeout no longer than 60 seconds required")
    if (args.overlay_root is None) != (args.expected_overlay_sha256 is None):
        parser.error("overlay root and its exact SHA-256 must be supplied together")
    rss_limit = args.rss_limit_bytes or (536870912 if system == "Darwin" else 2147483648)
    if not 1048576 <= rss_limit <= 4294967296:
        parser.error("memory watchdog must be between 1 MiB and 4 GiB")
    root = args.source_root.resolve(strict=True)
    if Path(__file__).resolve().parents[2] != root:
        parser.error("runner and source root must be the same frozen checkout")
    before_cgroup = cgroup_state(root)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True)
    out = args.out.resolve()
    before_id = source_id(root, out / "source-before.json")
    if before_id != args.expected_source_id:
        raise RuntimeError("native regression uses the wrong intended product source")
    header = root / "src" / HEADER
    overlay = []
    if args.overlay_root is not None:
        overlay_root = args.overlay_root.resolve(strict=True)
        contents = sorted(file.relative_to(overlay_root)
                          for file in overlay_root.rglob("*") if file.is_file())
        if contents != [HEADER]:
            raise RuntimeError("overlay must contain exactly the one GC header")
        header = overlay_root / HEADER
        if sha(header) != args.expected_overlay_sha256:
            raise RuntimeError("isolated teardown header differs from expected SHA-256")
        overlay = ["-I" + str(overlay_root)]
    fixture = root / "test/0017.runtime/wasm3_gc_bridge_teardown.cc"
    (out / fixture.name).write_bytes(fixture.read_bytes())
    (out / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    compiler = args.compiler or (Path(subprocess.check_output(
        ["xcrun", "--find", "clang++"], text=True).strip()) if system == "Darwin" else
        Path("/toolchain/bin/clang++"))
    inputs = {str(file): sha(file) for file in
              (fixture, Path(__file__), header, compiler,
               root / "tools/ci/wasm3_source_fingerprint.py",
               root / "src/uwvm2/uwvm/runtime/storage/wasm_module.h")}
    include = ["-I" + str(root / directory) for directory in
               ("src", "third-parties/fast_io/include", "third-parties/bizwen/include",
                "third-parties/boost_unordered/include")]
    binary, depfile = out / "fixture", out / "fixture.d"
    command = [str(compiler), "-std=c++26", "-stdlib=libc++", "-O0", "-g1",
               "-fno-rtti", "-Werror", "-Wno-undefined-inline",
               "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
               "-fno-omit-frame-pointer", *overlay, *include, "-MD", "-MF", str(depfile),
               str(fixture), "-o", str(binary)]
    sdk = None
    if system == "Darwin":
        # The absolute compiler path is hashed, but unlike invoking xcrun it
        # does not select the Apple SDK implicitly. Preserve that selection
        # explicitly so libc++ headers come from the matching SDK.
        sdk = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"],
                                      text=True).strip()
        command[1:1] = ["-isysroot", sdk]
    env = dict(os.environ, ASAN_OPTIONS=("detect_leaks=0:halt_on_error=1" if
               system == "Darwin" else "detect_leaks=1:halt_on_error=1"),
               UBSAN_OPTIONS="halt_on_error=1")
    if system == "Linux":
        command = ["taskset", "-c", "16", *command, "-fuse-ld=lld",
                   "-rtlib=compiler-rt", "-unwindlib=libunwind", "-pthread",
                   "-L/work/deps/usr/lib/x86_64-linux-gnu"]
        loader = ("/toolchain/lib/x86_64-unknown-linux-gnu", "/toolchain/lib",
                  "/work/deps/usr/lib/x86_64-linux-gnu",
                  "/work/wasm3-resume-20260924/tools/host-libs")
        env["LD_LIBRARY_PATH"] = ":".join((*loader, env.get("LD_LIBRARY_PATH", "")))
    report = {"scope": "bounded ASan/UBSan native correctness; no performance or collector claim",
              "source_id_before": before_id, "expected_source_id": args.expected_source_id,
              "input_sha256": inputs, "header_sha256": sha(header),
              "overlay": args.overlay_root is not None, "rss_limit_bytes": rss_limit,
              "runtime_timeout_seconds": args.timeout_seconds,
              "asan_options": env["ASAN_OPTIONS"], "ubsan_options": env["UBSAN_OPTIONS"],
              "leak_sanitizer_enabled": system == "Linux", "apple_sdk": sdk,
              "cgroup_before": before_cgroup}
    report["build"] = invoke(command, out / "build.log", env, timeout=300,
                             rss_limit=rss_limit, cwd=root)
    passed = False
    if report["build"]["exit"] == 0 and report["build"]["diagnostic"] is None:
        if str(header) not in depfile.read_text():
            raise RuntimeError("the compiler did not select the intended GC header")
        run = [str(binary)] if system == "Darwin" else ["taskset", "-c", "16", str(binary)]
        report["binary_sha256"] = sha(binary)
        report["depfile_sha256"] = sha(depfile)
        report["run"] = invoke(run, out / "run.log", env, timeout=args.timeout_seconds,
                               rss_limit=rss_limit, cwd=root)
        raw = (out / "run.log").read_text(errors="replace")
        passed = (report["run"]["exit"] == 0 and report["run"]["diagnostic"] is None and
                  "PASS one-direction foreign bridge teardown" in raw and
                  "TEARDOWN_BEGIN last foreign inner_owner pin" in raw)
    report["source_id_after"] = source_id(root, out / "source-after.json")
    report["cgroup_after"] = cgroup_state(root)
    if report["source_id_after"] != before_id or inputs != {
            file: sha(file) for file in inputs}:
        raise RuntimeError("source/header/compiler changed during native regression")
    if before_cgroup is not None:
        first = dict(line.split() for line in before_cgroup["memory.events"].splitlines())
        last = dict(line.split() for line in report["cgroup_after"]["memory.events"].splitlines())
        if any(first[key] != last[key] for key in ("oom", "oom_kill")):
            passed = False
            report["cgroup_failure"] = "OOM counter changed"
    report["passed"] = passed
    report["end_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(("PASS" if passed else "FAIL") + " one-direction foreign bridge native teardown")
    return 0 if passed else 2


if __name__ == "__main__":
    sys.exit(main())
