#!/usr/bin/env python3
"""Build a source-bound Win64 production-manager component on remote Linux.

This opt-in auxiliary runner builds a separate PE. It neither starts a Windows
VM nor qualifies whole-VM Wasm EH. Run the immutable product baseline first.
Every compilation runs in the established 64 GiB/swap0 cgroup with one E-core,
a 2 GiB whole-tree RSS cap, the 63e9 shared soft guard and 16 GiB disk floor.
No source, LLVM archive, page permission or dynamic function table is patched.
Importing this module performs no build or remote action.
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

TARGET = "x86_64-w64-windows-gnu"
RSS_LIMIT = 2 << 30
GLOBAL_LIMIT = 63_000_000_000
DISK_FLOOR = 16 << 30
SCOPE = "actual production SectionMemoryManager/native_exception_symbols native GNU Win64 component; no whole-VM/guest EH qualification"
HEADERS = (
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h",
)


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def state(cg):
    names = ("memory.max", "memory.swap.max", "memory.current", "memory.events",
             "memory.stat", "cpuset.cpus.effective", "cgroup.procs")
    value = {name: (cg / name).read_text().strip() for name in names}
    value["events"] = dict((k, int(v)) for k, v in
                           (row.split() for row in value["memory.events"].splitlines()))
    return value


def process(pid):
    text = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    return {"pid": pid, "parent": int(text[1]), "group": int(text[2]),
            "birth": int(text[19]), "state": text[0],
            "rss": max(0, int(text[21])) * os.sysconf("SC_PAGESIZE")}


def tree(root):
    all_rows, children = {}, {}
    for path in Path("/proc").iterdir():
        if not path.name.isdecimal():
            continue
        try:
            row = process(int(path.name))
        except (FileNotFoundError, ProcessLookupError):
            continue
        all_rows[row["pid"]] = row
        children.setdefault(row["parent"], []).append(row["pid"])
    pending, rows = [root], []
    while pending:
        pid = pending.pop()
        if pid in all_rows:
            rows.append(all_rows[pid])
            pending.extend(children.get(pid, ()))
    return rows


def watch(command, cwd, out, name, cg, environment, timeout=180):
    """Track real host children; pidfds and birth/cgroup proofs prevent reuse kills."""
    log = out / (name + ".log")
    (out / (name + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    started, peak, observed, reason = time.monotonic(), 0, {}, None
    admission = state(cg)
    filesystem = os.statvfs(out)
    require(int(admission["memory.current"]) + RSS_LIMIT < GLOBAL_LIMIT and
            filesystem.f_bavail * filesystem.f_frsize >= DISK_FLOOR and
            not admission["events"].get("oom") and not admission["events"].get("oom_kill"),
            "fresh command admission rejected; no tool process started")
    with log.open("xb") as output:
        child = subprocess.Popen(command, cwd=cwd, env=environment, stdout=output,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        # Popen's own unreaped child cannot have been replaced by another PID.
        # Establish its pidfd before any /proc sampling exception can occur.
        root_descriptor = os.pidfd_open(child.pid)
        root_row = process(child.pid)
        observed[(child.pid, root_row["birth"])] = (root_descriptor, root_row)
        try:
            while child.poll() is None:
                rows = tree(child.pid)
                current = sum(row["rss"] for row in rows)
                peak = max(peak, current)
                for row in rows:
                    key = (row["pid"], row["birth"])
                    if key not in observed:
                        try:
                            descriptor = os.pidfd_open(row["pid"])
                            fresh = process(row["pid"])
                            group = Path(f"/proc/{row['pid']}/cgroup").read_text().strip()
                            if fresh["birth"] != row["birth"] or group != Path("/proc/self/cgroup").read_text().strip():
                                os.close(descriptor)
                                raise RuntimeError("own child birth/cgroup identity changed")
                        except (FileNotFoundError, ProcessLookupError):
                            continue
                        observed[key] = (descriptor, row)
                shared = int((cg / "memory.current").read_text())
                filesystem = os.statvfs(out)
                free = filesystem.f_bavail * filesystem.f_frsize
                events = state(cg)["events"]
                if current > RSS_LIMIT:
                    reason = "own_process_tree_RSS_exceeded_2_GiB"
                elif shared >= GLOBAL_LIMIT:
                    reason = "shared_memory_current_exceeded_63e9"
                elif free < DISK_FLOOR:
                    reason = "disk_free_below_16_GiB"
                elif events.get("oom", 0) or events.get("oom_kill", 0):
                    reason = "cgroup_OOM"
                elif time.monotonic() - started > timeout:
                    reason = "timeout"
                if reason:
                    break
                time.sleep(0.05)
        except BaseException:
            reason = reason or "watchdog_exception"
            raise
        finally:
            if reason:
                # Only descriptors opened for actual descendants with their
                # original birth and exact shared cgroup can receive a signal.
                for descriptor, row in reversed(list(observed.values())):
                    try:
                        signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                child.wait()
            else:
                child.wait()
                alive = []
                for descriptor, row in observed.values():
                    try:
                        current = process(row["pid"])
                        if current["birth"] == row["birth"] and current["state"] != "Z":
                            alive.append((descriptor, row))
                    except (FileNotFoundError, ProcessLookupError):
                        pass
                if alive:
                    reason = "own_descendant_survived_driver_exit"
                    for descriptor, row in alive:
                        try:
                            signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
            for descriptor, row in observed.values():
                os.close(descriptor)
    row = {"name": name, "argv": command, "exit": child.returncode,
           "diagnostic": reason, "elapsed_seconds": time.monotonic() - started,
           "peak_whole_process_tree_rss_bytes": peak, "rss_limit_bytes": RSS_LIMIT,
           "shared_guard_bytes": GLOBAL_LIMIT, "disk_free_floor_bytes": DISK_FLOOR,
           "observed_births": [row for descriptor, row in observed.values()],
           "log_sha256": digest(log), "log_size": log.stat().st_size}
    (out / (name + ".result.json")).write_text(json.dumps(row, indent=2) + "\n")
    return row


def fingerprint(root, output):
    command = [sys.executable, str(root / "tools/ci/wasm3_source_fingerprint.py"),
               str(root), str(output)]
    result = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE)
    return result.stdout.strip()


def inspect_pe(path):
    # No platform execution: the LLVM/objdump imports remain separate evidence.
    import struct
    with path.open("rb") as stream:
        head = stream.read(64)
        require(len(head) == 64 and head[:2] == b"MZ", "DOS header unavailable")
        offset = struct.unpack_from("<I", head, 60)[0]
        require(64 <= offset <= (1 << 20), "invalid PE offset")
        stream.seek(offset)
        coff = stream.read(24)
        require(len(coff) == 24 and coff[:4] == b"PE\0\0" and
                struct.unpack_from("<H", coff, 4)[0] == 0x8664,
                "expected a real AMD64 PE")
    return {"machine": "IMAGE_FILE_MACHINE_AMD64", "sha256": digest(path), "size": path.stat().st_size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "product-build-json", "llvm-root", "sdk", "cxx", "lld", "readobj", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--docker-scope", required=True)
    parser.add_argument("--cpu", type=int, choices=range(16, 32), default=17)
    args = parser.parse_args()
    require(platform.system() == "Linux", "all component compilation belongs on remote Linux")
    require(hasattr(os, "pidfd_open") and hasattr(signal, "pidfd_send_signal"), "pidfd controls unavailable")
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", args.source_id) is not None and
            re.fullmatch(r"docker-[0-9a-f]{64}\.scope", args.docker_scope) is not None,
            "invalid source ID/cgroup scope")
    cg = Path("/sys/fs/cgroup/system.slice") / args.docker_scope
    before = state(cg)
    require(Path("/proc/self/cgroup").read_text().strip() == "0::/system.slice/" + args.docker_scope,
            "control is not in the existing shared cgroup")
    require(before["memory.max"] == str(64 << 30) and before["memory.swap.max"] == "0" and
            before["cpuset.cpus.effective"] == "0,2,4,6,16-31" and
            not before["events"].get("oom") and not before["events"].get("oom_kill"),
            "require actual 64 GiB/swap0/20 CPUs and OOM0")
    require(int(before["memory.current"]) + RSS_LIMIT < GLOBAL_LIMIT,
            "shared budget lacks the full 2 GiB startup reserve")
    root, llvm, sdk = (value.resolve(strict=True) for value in (args.source_root, args.llvm_root, args.sdk))
    out = args.out.absolute()
    require(not any(out.is_relative_to(root / name) for name in ("src", "third-parties")), "evidence would mutate frozen inputs")
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    stats = os.statvfs(out)
    require(stats.f_bavail * stats.f_frsize >= DISK_FLOOR, "16 GiB disk floor unavailable")
    require(args.cpu in os.sched_getaffinity(0), "requested E-core unavailable")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    product_file = args.product_build_json.resolve(strict=True)
    product = json.loads(product_file.read_text())
    require(product["source_id"] == args.source_id and product["status"] == "cross-built-awaiting-real-windows-vm",
            "exact complete product build is required before diagnostic component")
    require(fingerprint(root, out / "source-before.json") == args.source_id, "frozen input fingerprint differs")
    certificate_file = llvm / "qualified-llvm.json"
    certificate = json.loads(certificate_file.read_text())
    require(digest(certificate_file) == product["llvm_certificate_sha256"] and
            certificate["host_target"] == TARGET and certificate["archive_count"] >= 40,
            "LLVM certificate differs from the actual r2 product closure")
    require(digest(llvm / "CMakeCache.txt") == certificate["cmake_cache_sha256"] and
            digest(llvm / "consumer-link.rsp") == certificate["consumer_link_sha256"], "qualified LLVM metadata changed")
    archive_paths = {(llvm / name).resolve(strict=True) for name in certificate["archives"]}
    linked_paths = {Path(name).resolve(strict=True) for name in shlex.split((llvm / "consumer-link.rsp").read_text()) if name.endswith(".a")}
    require(archive_paths == linked_paths, "qualified response/archives differ")
    archive_hashes = {}
    for name, expected in certificate["archives"].items():
        archive = (llvm / name).resolve(strict=True)
        require(archive.parent == (llvm / "lib").resolve(strict=True) and
                archive.stat().st_size == expected["size"] and digest(archive) == expected["sha256"], "LLVM archive bytes changed: " + name)
        archive_hashes[str(archive)] = expected["sha256"]
    builtins = product["windows_builtins"]
    require(builtins and digest(builtins["archive"]) == builtins["sha256"] and
            digest(builtins["certificate_path"]) == builtins["certificate_sha256"], "real qualified target builtins changed")
    cxx, lld, readobj = args.cxx.absolute(), args.lld.absolute(), args.readobj.absolute()
    # Preserve argv[0] driver names: resolving clang++ or ld.lld symlinks before
    # invocation changes language/linker selection. Hash the resolved bytes.
    require(cxx.name == "clang++" and lld.name == "ld.lld", "preserve actual C++ and COFF-GNU driver names")
    for name, tool in (("clangxx", cxx), ("lld", lld)):
        require(digest(tool.resolve(strict=True)) == product["build_tools"][name]["sha256"], "diagnostic tool bytes differ from actual product: " + name)
    fixture = Path(__file__).with_name("win64_production_unwind_observer.cc").resolve(strict=True)
    inputs = {str(fixture): digest(fixture), str(Path(__file__).resolve()): digest(Path(__file__)),
              str(product_file): digest(product_file), str(certificate_file): digest(certificate_file),
              str(readobj.resolve(strict=True)): digest(readobj.resolve(strict=True)),
              **{str(root / name): digest(root / name) for name in HEADERS}, **archive_hashes}
    report = {"schema": 1, "scope": SCOPE, "status": "running-component-cross-build", "qualified": False,
              "source_id": args.source_id, "product_build_sha256": digest(product_file),
              "auxiliary_inputs_sha256": inputs, "cgroup_before": before,
              "cpu": args.cpu, "rss_budget_bytes": RSS_LIMIT, "commands": [],
              "dynamic_scope_pending": "actual Windows component; whole-VM baseline is a separate mandatory gate"}
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join((str(cxx.parent), str(lld.parent), environment.get("PATH", "")))
    compiler_flags = [str(cxx), "--target=" + TARGET, "--sysroot=" + str(sdk), "-std=c++23", "-stdlib=libc++", "-nostdinc++",
                      "-isystem", str(sdk / "include/c++/v1"), "-isystem", str(sdk / "x86_64-w64-mingw32/include"),
                      "-I" + str(root / "src"), "-I" + str(root / "third-parties/fast_io/include"),
                      "-I" + str(llvm / "include"), "-I" + str(root / "third-parties/llvm/llvm/include"),
                      "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00", "-DUWVM_TEST_OBSERVER_SOURCE_ID=" + json.dumps(args.source_id),
                      "-fno-rtti", "-fexceptions", "-fasynchronous-unwind-tables", "-O1", "-g"]
    obj, pe = out / "observer.obj", out / "observer.exe"
    commands = [("compile", [*compiler_flags, "-c", str(fixture), "-o", str(obj)]),
                ("link", [str(cxx), "--target=" + TARGET, "--sysroot=" + str(sdk), "-stdlib=libc++", "--ld-path=" + str(lld),
                          str(obj), "@" + str(llvm / "consumer-link.rsp"), builtins["archive"], "-static-libgcc", "-lntdll",
                          "-L" + str(sdk / "x86_64-w64-mingw32/lib"), "-Wl,--stack,8388608", "-o", str(pe)]),
                ("static-headers-imports", [str(readobj), "--file-headers", "--sections", "--coff-imports", str(pe)])]
    try:
        for name, command in commands:
            row = watch(command, root, out, name, cg, environment)
            report["commands"].append(row)
            require(row["exit"] == 0 and not row["diagnostic"], "component command failed: " + name)
        report["pe"] = inspect_pe(pe)
        require(fingerprint(root, out / "source-after.json") == args.source_id and
                all(digest(path) == expected for path, expected in inputs.items()) and
                digest(builtins["archive"]) == builtins["sha256"] and
                digest(llvm / "consumer-link.rsp") == certificate["consumer_link_sha256"], "source/auxiliary/closure changed during component build")
        report["status"] = "component-pe-built-awaiting-real-win64-run"
    except BaseException as error:
        report["status"] = "component-cross-build-failed"
        report["error"] = repr(error)
        raise
    finally:
        report["cgroup_after"] = state(cg)
        (out / "summary.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
