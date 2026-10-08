#!/usr/bin/env python3
"""Qualify collection-pause coordination and a real exported module consumer.

Linux runs require the 64 GiB, swap-free, 4 P + 16 E CPU cgroup. macOS is an
explicit, bounded portability check; its sanitizer profile does not claim LSan.
Neither profile establishes VM root enumeration or a working Wasm collector.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
import traceback


HEADER_SHA = "24d994ccf848429a1e9a0fa206ead3747fe4daaecceefbdb6582c5b02d8856cb"
FIXTURE_SHA = "7cec5d4a91b8fc9acf82bc6aa9d834d53b8a90a7d009cb96e16c605d1e7fd326"
CPUSET = "0,2,4,6,16-31"
HARD_MEMORY = 64 * 1024 ** 3
CGROUP = Path("/sys/fs/cgroup")
PASS_LINE = "collection pause PASS;"
PROTOCOLS = [
    "empty domain", "collector does not wait for itself", "8 readers x 128 epochs",
    "peer cannot impersonate the collector", "admission during pause",
    "blocking root publication and wakeup", "timeout forbids stopped callback",
    "concurrent collector rejection", "commit versus close", "ticket drain",
    "RAII and native exception unwinding",
]


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def process_rss(pid):
    """Sample only this command's process tree, including compiler descendants."""
    pending, seen, total, high_water = [pid], set(), 0, 0
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        try:
            fields = {}
            for line in Path(f"/proc/{current}/status").read_text().splitlines():
                key, _, value = line.partition(":")
                if key in ("VmRSS", "VmHWM"):
                    fields[key] = int(value.split()[0]) * 1024
            total += fields.get("VmRSS", 0)
            high_water = max(high_water, fields.get("VmHWM", 0))
            children = Path(f"/proc/{current}/task/{current}/children").read_text()
            pending.extend(map(int, children.split()))
        except (FileNotFoundError, ProcessLookupError):
            pass
    return total, high_water


def cgroup_snapshot():
    result = {}
    for name in ("memory.max", "memory.swap.max", "memory.current", "memory.peak",
                 "memory.events", "memory.stat", "cpuset.cpus.effective", "cpu.max", "cpu.stat"):
        path = CGROUP / name
        if path.exists():
            text = path.read_text().strip()
            if name in ("memory.events", "memory.stat", "cpu.stat"):
                result[name] = {key: int(value) for key, value in
                                (line.split() for line in text.splitlines())}
            else:
                result[name] = text
    result["self_cgroup"] = Path("/proc/self/cgroup").read_text().strip()
    return result


def require_cgroup(root):
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    value = cgroup_snapshot()
    if (value["memory.max"] != str(HARD_MEMORY) or value["memory.swap.max"] != "0" or
            value["cpuset.cpus.effective"] != CPUSET):
        raise RuntimeError("require the verified 64 GiB, swap0, 4P+16E cgroup")
    if any(value["memory.events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("collection pause qualification requires OOM/OOM-kill counters of zero")
    return value


def stop_own_group(process):
    # The Mac RSS helper starts the actual compiler in its own session. Killing
    # only the helper's process group could orphan that compiler on timeout.
    # Enumerate this command's descendants and stop them before the helper.
    children = {}
    if sys.platform == "darwin":
        table = subprocess.run(["ps", "-A", "-o", "pid=,ppid="],
                               capture_output=True, text=True, check=True).stdout
        for line in table.splitlines():
            fields = line.split()
            if len(fields) == 2:
                pid, parent = map(int, fields)
                children.setdefault(parent, []).append(pid)
    else:
        pending = [process.pid]
        while pending:
            pid = pending.pop()
            try:
                descendants = list(map(int, Path(f"/proc/{pid}/task/{pid}/children").read_text().split()))
            except (FileNotFoundError, ProcessLookupError):
                descendants = []
            children[pid] = descendants
            pending.extend(descendants)
    pending, pids, seen = [process.pid], [], set()
    while pending:
        pid = pending.pop()
        if pid not in seen:
            seen.add(pid)
            pids.append(pid)
            pending.extend(children.get(pid, ()))
    denied = []
    for pid in reversed(pids):
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except PermissionError:
            denied.append(pid)
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    except PermissionError:
        denied.append(process.pid)
    return sorted(set(denied))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--platform", choices=("linux", "macos"), default="linux")
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--sdk", type=Path, help="required explicit macOS SDK")
    parser.add_argument("--expected-header-sha256", default=HEADER_SHA)
    parser.add_argument("--expected-fixture-sha256", default=FIXTURE_SHA)
    parser.add_argument("--max-command-rss-bytes", type=int)
    parser.add_argument("--start-memory-bytes", type=int, default=48_000_000_000)
    parser.add_argument("--stop-memory-bytes", type=int, default=56_000_000_000)
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--min-free-bytes", type=int, default=512 * 1024 ** 2)
    parser.add_argument("--stop-free-bytes", type=int, default=64 * 1024 ** 2)
    parser.add_argument("--include-tsan", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows, dependency_hashes, initial_inputs = [], {}, {}
    summary = {"passed": False, "source_root": str(root), "platform": args.platform,
               "scope": "generic stop coordination; no VM/GC integration qualification",
               "commands": rows, "protocols": PROTOCOLS,
               "lsan_qualified": False, "full_thread_module_graph_built": False}
    cgroup_before = None

    def run(name, command, *, negative_module=False, protocol=False):
        original = [str(item) for item in command]
        free_before = shutil.disk_usage(out).free
        if free_before < args.min_free_bytes:
            summary["disk_preflight_refusal"] = {"step": name, "free_disk_bytes": free_before,
                                                  "required_bytes": args.min_free_bytes}
            raise RuntimeError("free-disk preflight rejected " + name)
        effective = original
        if args.platform == "macos":
            effective = [sys.executable, str(root / "test/0017.runtime/macos_rss_limit.py"),
                         "--limit-bytes", str(rss_limit), "--", *original]
        else:
            current = require_cgroup(root)
            if int(current["memory.current"]) >= args.start_memory_bytes:
                raise RuntimeError("cgroup start-memory guard rejected " + name)
        command_path, log_path = out / (name + ".command"), out / (name + ".log")
        command_path.write_text(shlex.join(effective) + "\n")
        row = {"name": name, "command": original, "effective_command": effective,
               "command_file": str(command_path), "log": str(log_path), "passed": False,
               "free_disk_before_bytes": free_before}
        rows.append(row)
        write_json(out / "summary.json", summary)
        started = time.monotonic()
        peak_rss, peak_cgroup, guard_reason, status = 0, 0, None, None
        minimum_free, denied = free_before, []
        with log_path.open("w") as log:
            process = subprocess.Popen(effective, cwd=root, env=environment,
                                       stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            while status is None:
                if args.platform == "linux":
                    current_rss, high_water = process_rss(process.pid)
                    peak_rss = max(peak_rss, current_rss, high_water)
                    observed = cgroup_snapshot()
                    current_memory = int(observed["memory.current"])
                    peak_cgroup = max(peak_cgroup, current_memory)
                    if current_memory >= args.stop_memory_bytes:
                        guard_reason = "cgroup memory stop threshold"
                    if peak_rss > rss_limit:
                        guard_reason = "command RSS limit"
                    for key in ("oom", "oom_kill"):
                        if observed["memory.events"][key] != cgroup_before["memory.events"][key]:
                            guard_reason = "cgroup " + key + " changed"
                if time.monotonic() - started >= args.timeout:
                    guard_reason = "command timeout"
                minimum_free = min(minimum_free, shutil.disk_usage(out).free)
                if minimum_free < args.stop_free_bytes:
                    guard_reason = "free-disk stop threshold"
                if guard_reason:
                    denied.extend(stop_own_group(process))
                waited, raw_status, usage = os.wait4(process.pid, os.WNOHANG)
                if waited:
                    status = os.waitstatus_to_exitcode(raw_status)
                    process.returncode = status
                    scale = 1 if args.platform == "macos" else 1024
                    peak_rss = max(peak_rss, usage.ru_maxrss * scale)
                    break
                time.sleep(0.02)
        text = log_path.read_text(errors="replace")
        if args.platform == "macos":
            samples = re.findall(r"PEAK_PROCESS_TREE_RSS_BYTES=(\d+)", text)
            if not samples:
                guard_reason = "macOS command watchdog produced no RSS result"
            peak_rss = max([peak_rss, *(int(value) for value in samples)])
            if "RSS_LIMIT_EXCEEDED" in text or peak_rss > rss_limit:
                guard_reason = "macOS command RSS limit"
        native_status = status
        if args.platform == "macos":
            native_results = re.findall(r"^COMMAND_EXIT=(-?\d+)$", text, re.MULTILINE)
            if len(native_results) == 1:
                native_status = int(native_results[0])
        diagnostic = "module 'uwvm2.utils.thread' not found" in text
        # A Mac helper encodes a child's negative signal status in its shell
        # exit code. A compiler crash must not satisfy the missing-module test.
        passed = (native_status > 0 and diagnostic) if negative_module else status == 0
        if protocol:
            passed = passed and PASS_LINE in text
        row.update(exit=status, elapsed_seconds=time.monotonic() - started,
                   raw_command_exit=native_status,
                   peak_process_tree_rss_bytes=peak_rss, peak_cgroup_memory_bytes=peak_cgroup,
                   guard_failure=guard_reason, log_sha256=sha(log_path),
                   minimum_free_disk_bytes=minimum_free, kill_denied_pids=sorted(set(denied)),
                   command_sha256=sha(command_path), expected_missing_module=negative_module,
                   protocol_pass_line=(PASS_LINE in text), passed=bool(passed and not guard_reason))
        write_json(out / "summary.json", summary)
        if not row["passed"]:
            raise RuntimeError(f"{name} failed (exit {status}, guard {guard_reason}); see {log_path}")
        for item in original:
            if item.endswith(".d") and Path(item).is_file():
                make_text = Path(item).read_text().replace("\\\n", " ").split(":", 1)[1]
                for entry in shlex.split(make_text):
                    path = Path(entry)
                    path = path if path.is_absolute() else root / path
                    if path.is_file():
                        resolved = str(path.resolve())
                        digest = sha(path)
                        previous = dependency_hashes.setdefault(resolved, digest)
                        if previous != digest:
                            raise RuntimeError("observed compiler dependency changed: " + resolved)

    try:
        if args.platform == "macos":
            if sys.platform != "darwin" or args.sdk is None or not args.sdk.is_dir():
                raise RuntimeError("macOS requires an explicit SDK on a Darwin host")
            rss_limit = args.max_command_rss_bytes or 512 * 1024 ** 2
            if not 0 < rss_limit <= 512 * 1024 ** 2:
                raise RuntimeError("bounded Mac probe requires at most 512 MiB per command")
        else:
            if sys.platform != "linux":
                raise RuntimeError("Linux profile must run in the remote Linux cgroup")
            cgroup_before = require_cgroup(root)
            summary["cgroup_before"] = cgroup_before
            os.sched_setaffinity(0, {16})
            rss_limit = args.max_command_rss_bytes or 2 * 1024 ** 3
            if not (0 < rss_limit <= 4 * 1024 ** 3 and
                    0 < args.start_memory_bytes < args.stop_memory_bytes < HARD_MEMORY):
                raise RuntimeError("invalid RSS/cgroup memory guard limits")
        if args.timeout <= 0 or not 0 < args.stop_free_bytes < args.min_free_bytes:
            raise RuntimeError("timeout and disk guard thresholds must be positive and ordered")
        header = root / "src/uwvm2/utils/thread/collection_pause_domain.h"
        partition = header.with_suffix(".cppm")
        fixture = root / "test/0017.runtime/collection_pause_domain.cc"
        primary_source = root / "src/uwvm2/utils/thread/impl.cppm"
        inputs = [header, partition, fixture, primary_source, Path(__file__).resolve(),
                  root / "src/uwvm2/utils/macro/push_macros.h",
                  root / "test/0017.runtime/macos_rss_limit.py",
                  root / "tools/ci/require_wasm3_test_cgroup.sh"]
        initial_inputs = {str(path): sha(path) for path in inputs}
        if sha(header) != args.expected_header_sha256 or sha(fixture) != args.expected_fixture_sha256:
            raise RuntimeError("collection pause header/fixture differs from the requested qualification input")
        copies = {}
        for path in inputs:
            relative = path.relative_to(root)
            copy = out / "inputs" / relative
            copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, copy)
            if sha(copy) != initial_inputs[str(path)]:
                raise RuntimeError("direct input changed while snapshotting: " + str(path))
            copies[str(path)] = {"copy": str(copy), "sha256": sha(copy)}
        summary["input_snapshots"] = copies
        export_line = "export import :collection_pause_domain;"
        if primary_source.read_text(encoding="utf-8-sig").count(export_line) != 1:
            raise RuntimeError("production primary module does not export the collection pause partition exactly once")
        # Preserve clang++'s invocation name: resolving its clang symlink would
        # select the C driver and omit the C++ runtime at link time on Darwin.
        compiler = Path(os.path.abspath(args.cxx))
        if not compiler.is_file():
            raise RuntimeError("compiler executable is missing: " + str(compiler))
        initial_inputs[str(compiler)] = sha(compiler)
        summary.update(inputs_before=initial_inputs, compiler=str(compiler), compiler_sha256=sha(compiler),
                       compiler_real_path=str(compiler.resolve(strict=True)),
                       max_command_rss_bytes=rss_limit, start_memory_bytes=args.start_memory_bytes,
                       min_free_disk_bytes=args.min_free_bytes, stop_free_disk_bytes=args.stop_free_bytes,
                       stop_memory_bytes=args.stop_memory_bytes, timeout_seconds=args.timeout,
                       source_hash_scope="direct inputs plus observed compiler dependency files")
        environment = dict(os.environ)
        environment.update(ASAN_OPTIONS=("detect_leaks=1:abort_on_error=1" if args.platform == "linux"
                                        else "detect_leaks=0:abort_on_error=1"),
                           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1",
                           TSAN_OPTIONS="halt_on_error=1")
        flags = [str(compiler), "-std=c++26", "-stdlib=libc++", "-DUWVM=2", "-fno-rtti",
                 "-pthread", "-Wall", "-Wextra", "-Werror", "-I", str(root / "src"),
                 "-I", str(root / "third-parties/fast_io/include"),
                 "-I", str(root / "third-parties/bizwen/include")]
        link = []
        if args.platform == "macos":
            flags += ["-isysroot", str(args.sdk.resolve())]
            summary["sdk"] = str(args.sdk.resolve())
        else:
            flags += ["-fstack-clash-protection", "-mstack-probe-size=4096"]
            link = ["-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind"]
        run("compiler-version", [str(compiler), "--version"])
        run("compiler-target", [str(compiler), "-dumpmachine"])
        if args.platform == "macos":
            run("compiler-runtime-directory", [str(compiler), "--print-runtime-dir"])
            lines = (out / "compiler-runtime-directory.log").read_text().splitlines()
            runtime_directories = [Path(line) for line in lines
                                   if line.startswith("/") and Path(line).is_dir()]
            if len(runtime_directories) != 1:
                raise RuntimeError("compiler did not identify exactly one sanitizer runtime directory")
            if environment.get("DYLD_INSERT_LIBRARIES"):
                raise RuntimeError("generic pause probe rejects injected Mac dylibs")
            runtime_directory = runtime_directories[0].resolve(strict=True)
            inherited_library_path = environment.get("DYLD_LIBRARY_PATH", "")
            environment["DYLD_LIBRARY_PATH"] = str(runtime_directory) + (
                ":" + inherited_library_path if inherited_library_path else "")
            summary["macos_sanitizer_runtime"] = {
                "compiler_query_directory": str(runtime_directory),
                "inherited_dyld_library_path": inherited_library_path,
                "effective_dyld_library_path": environment["DYLD_LIBRARY_PATH"],
                "scope": "compiler runtime first; no persistent shell environment change",
            }
            names = ["libclang_rt.asan_osx_dynamic.dylib"]
            if args.include_tsan:
                names.append("libclang_rt.tsan_osx_dynamic.dylib")
            for name in names:
                library = runtime_directory / name
                if not library.is_file():
                    raise RuntimeError("compiler sanitizer runtime is missing: " + str(library))
                initial_inputs[str(library)] = sha(library)
        summary["test_environment"] = {key: environment.get(key) for key in
            ("ASAN_OPTIONS", "UBSAN_OPTIONS", "TSAN_OPTIONS", "DYLD_LIBRARY_PATH",
             "DYLD_FALLBACK_LIBRARY_PATH", "LD_LIBRARY_PATH", "UWVM_TEST_CPUSET")}
        profiles = [("o3", ["-O3"]),
                    ("asan-ubsan-lsan" if args.platform == "linux" else "asan-ubsan",
                     ["-O1", "-g1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"])]
        if args.include_tsan:
            profiles.append(("tsan", ["-O1", "-g1", "-fno-omit-frame-pointer", "-fsanitize=thread"]))
        for name, optimization in profiles:
            executable = out / name
            run(name + "-build", flags + optimization + ["-MD", "-MF", str(out / (name + ".d")),
                str(fixture), "-o", str(executable)] + link)
            run(name + "-run", [str(executable)], protocol=True)
        # This focused primary actually exports the production partition. It
        # does not pretend to compile unrelated optional thread partitions.
        primary = out / "thread.cppm"
        primary.write_text("export module uwvm2.utils.thread;\n" + export_line + "\n")
        consumer = out / "consumer.cc"
        fixture_text = fixture.read_text()
        include_line = "#include <uwvm2/utils/thread/collection_pause_domain.h>\n"
        if fixture_text.count(include_line) != 1 or fixture_text.count("#define CHECK") != 1:
            raise RuntimeError("fixture structure changed; cannot construct faithful importing consumer")
        consumer.write_text(fixture_text.replace(include_line, "", 1).replace(
            "#define CHECK", "import uwvm2.utils.thread;\n\n#define CHECK", 1))
        summary["module_inputs"] = {str(path): sha(path) for path in (primary, consumer)}
        pcm, primary_pcm = out / "collection.pcm", out / "thread.pcm"
        reference = "-fmodule-file=uwvm2.utils.thread:collection_pause_domain=" + str(pcm)
        primary_reference = "-fmodule-file=uwvm2.utils.thread=" + str(primary_pcm)
        module_flags = flags + ["-O1", "-fno-implicit-modules", "-fno-implicit-module-maps"]
        if args.platform == "macos":
            # AppleClang 21 leaves standard C++ module parsing disabled unless
            # this switch is explicit, even for a .cppm under -std=c++26.
            module_flags += ["-fcxx-modules"]
        run("partition-precompile", module_flags + ["--precompile", "-MD", "-MF",
            str(out / "partition.d"), str(partition), "-o", str(pcm)])
        run("primary-precompile", module_flags + ["--precompile", str(primary), reference,
            "-o", str(primary_pcm)])
        # The PCM already contains the header search/stdlib selection. Passing
        # source-only switches while compiling a PCM triggers Clang's unused-
        # argument diagnostic under -Werror; omit those switches explicitly.
        object_flags, index = [], 0
        while index < len(module_flags):
            option = module_flags[index]
            if option == "-I":
                index += 2
                continue
            if option.startswith("-stdlib="):
                index += 1
                continue
            object_flags.append(option)
            index += 1
        run("partition-object", object_flags + ["-c", str(pcm), "-o", str(out / "collection.o")])
        run("primary-object", object_flags + ["-c", str(primary_pcm), reference,
            "-o", str(out / "thread.o")])
        run("module-unbound-negative", module_flags + ["-fsyntax-only", str(consumer)],
            negative_module=True)
        run("module-consumer-build", module_flags + ["-MD", "-MF", str(out / "consumer.d"),
            str(consumer), reference, primary_reference, str(out / "collection.o"),
            str(out / "thread.o"), "-o", str(out / "consumer")] + link)
        run("module-consumer-run", [str(out / "consumer")], protocol=True)
        summary["lsan_qualified"] = args.platform == "linux"
        summary["module_consumer_protocols"] = PROTOCOLS
        summary["module_profile"] = "O1, production partition + focused primary + full importing protocol fixture"
        summary["profiles"] = [name for name, _ in profiles]
        summary["passed"] = True
    except Exception as error:
        summary["failure"] = str(error)
        (out / "failure.log").write_text(traceback.format_exc())
    finally:
        summary["inputs_after"] = {path: sha(path) for path in initial_inputs if Path(path).is_file()}
        if initial_inputs and summary["inputs_after"] != initial_inputs:
            summary["passed"] = False
            summary["input_drift"] = True
        dependency_after = {path: sha(path) for path in dependency_hashes if Path(path).is_file()}
        summary["compiler_dependencies"] = dependency_hashes
        if dependency_after != dependency_hashes:
            summary["passed"] = False
            summary["compiler_dependency_drift"] = True
        if cgroup_before is not None:
            try:
                summary["cgroup_after"] = require_cgroup(root)
                for key in ("oom", "oom_kill"):
                    if summary["cgroup_after"]["memory.events"][key] != cgroup_before["memory.events"][key]:
                        summary["passed"] = False
                        summary["cgroup_event_drift"] = key
            except Exception as error:
                summary["passed"] = False
                summary["cgroup_final_failure"] = str(error)
        summary["command_count"] = len(rows)
        summary["passed_command_count"] = sum(row["passed"] for row in rows)
        summary["peak_process_tree_rss_bytes"] = max(
            (row.get("peak_process_tree_rss_bytes", 0) for row in rows), default=0)
        summary["artifacts"] = {str(path): sha(path) for path in out.rglob("*")
                                if path.is_file() and path.name not in ("summary.json", "summary.json.tmp")}
        write_json(out / "summary.json", summary)
    print(("PASS" if summary["passed"] else "FAIL") + " collection pause: " + str(out / "summary.json"))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
