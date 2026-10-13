#!/usr/bin/env python3
"""Qualify real Core 3 precise-frame lowering against a fresh, matched runtime.

The selected source must already have an actual O3 CLI/runtime build. Reusing
an r2 runtime object with a new collector class/root ABI is forbidden: it would
mix C++ definitions across TUs. No one-header include overlay is accepted.
This is a functional single-thread JIT collection probe, not a P-core benchmark
or automatic/multithreaded VM collector qualification.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import resource
import shlex
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path("test/0019.gc_statepoint/llvm_frame_roots.cc")
WAT = FIXTURE.with_suffix(".wat")
HEADERS = (
    "src/uwvm2/runtime/gc/frame_roots.h",
    "src/uwvm2/uwvm/runtime/storage/gc_object.h",
    "src/uwvm2/uwvm/runtime/storage/gc_static_roots.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_roots_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_exception_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h",
    "src/uwvm2/runtime/exception/value.h",
    "src/uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h",
    "src/uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h",
    "test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h",
    "test/0014.llvm_jit/run_gc_aggregate_callsite_ir.py",
    "tools/ci/wasm3_source_fingerprint.py",
    "tools/ci/require_wasm3_test_cgroup.sh",
)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def load_product_adapter(source):
    path = source / "test/0014.llvm_jit/run_gc_aggregate_callsite_ir.py"
    spec = importlib.util.spec_from_file_location("frame_roots_product_command_adapter", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def events(value):
    return {key: int(number) for key, number in (line.split() for line in value.splitlines())}


def tree_rss(pid):
    pending = [pid]
    seen = set()
    result = 0
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        base = Path("/proc") / str(current)
        try:
            for line in (base / "status").read_text().splitlines():
                if line.startswith("VmRSS:"):
                    result += int(line.split()[1]) * 1024
            for task in (base / "task").iterdir():
                pending.extend(int(value) for value in (task / "children").read_text().split())
        except FileNotFoundError:
            continue
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--cli-build", type=Path, required=True)
    parser.add_argument("--fixture-root", type=Path, default=ROOT,
                        help="private aux package; never copied into the immutable product source")
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--expected-inputs", type=Path, required=True,
                        help="pre-reviewed source:<path>/aux:<path> SHA map from this private package")
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--llvm-objdump", type=Path, default=Path("/toolchain/bin/llvm-objdump"))
    parser.add_argument("--llvm-readobj", type=Path, default=Path("/toolchain/bin/llvm-readobj"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--stop-memory-bytes", type=int, default=63_000_000_000)
    parser.add_argument("--tree-rss-bytes", type=int, default=2 * 1024**3)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("requires the qualified remote Linux x86-64 cgroup/SDK")
    if not 0 < args.stop_memory_bytes < 68719476736 or not 0 < args.tree_rss_bytes <= 3 * 1024**3:
        raise ValueError("watchdog budgets exceed the authorized small native slot")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    source = args.source_root.resolve(strict=True)
    build = args.cli_build.resolve(strict=True)
    fixture_root = args.fixture_root.resolve(strict=True)
    if fixture_root == source or fixture_root.is_relative_to(source):
        raise RuntimeError("private fixtures must remain outside the immutable product snapshot")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    selected = json.loads(args.expected_inputs.read_text())
    files = {"source:" + path: source / path for path in HEADERS}
    files.update({"aux:" + path.as_posix(): fixture_root / path for path in (FIXTURE, WAT)})
    files["aux:test/0019.gc_statepoint/run_llvm_frame_roots.py"] = Path(__file__)
    if set(selected) != set(files) or any(sha(path) != selected[key] for key, path in files.items()):
        raise RuntimeError("selected actual scalar32 emitter/root/collector or private fixture bytes differ from review")
    if "uwvm_gc_root_frame_enter_checked_abi" not in (source / HEADERS[3]).read_text():
        raise RuntimeError("requires the reviewed fail-fast actual frame lowering")
    metadata = json.loads((build / "build.json").read_text())
    if metadata["source_id"] != args.expected_source_id or Path(metadata["source"]).resolve() != source:
        raise RuntimeError("actual CLI/runtime build belongs to a different source snapshot")
    if sha(build / "runtime.o") != metadata["runtime_object_sha256"] or sha(build / "uwvm") != metadata["binary_sha256"]:
        raise RuntimeError("matched product runtime/binary no longer matches its qualification")
    runtime = shlex.split((build / "runtime.command").read_text())
    cli = shlex.split((build / "cli.command").read_text())
    source_flag = '-DUWVM2_BUILD_SOURCE_ID=u8"' + args.expected_source_id + '"'
    if runtime.count(source_flag) != 1 or cli.count(source_flag) != 1:
        raise RuntimeError("actual compile/link commands embed a different source fingerprint")
    adapter = load_product_adapter(source)
    compile_, link = adapter.commands_from_product(runtime, cli, fixture_root / FIXTURE, out, False)
    # Preserve the actual C++ driver invocation; hash its resolved bytes independently.
    compiler_invocation = Path(shutil.which(compile_[0]) or compile_[0]).absolute()
    if not compiler_invocation.is_file():
        raise RuntimeError("actual product compiler invocation no longer exists")
    compile_ += ["-I", str(source / "test/0013.uwvm_int/strict")]
    cg = Path("/sys/fs/cgroup")

    def resources():
        value = {name: (cg / name).read_text().strip() for name in
                 ("memory.current", "memory.max", "memory.swap.max", "memory.events", "cpuset.cpus.effective")}
        if (value["memory.max"], value["memory.swap.max"], value["cpuset.cpus.effective"]) != (
                "68719476736", "0", "0,2,4,6,16-31"):
            raise RuntimeError("exact hard cgroup policy changed")
        return value

    initial = resources()
    sdk_inputs = {}
    commands = []
    summary = dict(passed=False, source_id=args.expected_source_id,
                   actual_wasm_compiled_and_executed=False, automatic_vm_gc=False,
                   multithreaded_vm_gc=False, performance_qualified=False)
    inputs = dict(source_id=args.expected_source_id, headers_and_fixtures=selected,
                  build_json_sha256=sha(build / "build.json"),
                  runtime_object_sha256=sha(build / "runtime.o"), binary_sha256=sha(build / "uwvm"),
                  runner_sha256=sha(Path(__file__)), adapter_sha256=sha(Path(adapter.__file__)),
                  compiler_invocation=str(compiler_invocation), compiler_realpath=str(compiler_invocation.resolve(strict=True)),
                  compiler_bytes_sha256=sha(compiler_invocation),
                  wasm_tools_sha256=sha(args.wasm_tools), llvm_objdump_sha256=sha(args.llvm_objdump),
                  llvm_readobj_sha256=sha(args.llvm_readobj), resources_before=initial,
                  budgets=dict(cgroup_memory_stop=args.stop_memory_bytes, whole_process_tree_rss=args.tree_rss_bytes))
    (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")

    def run(label, argv, timeout=1200):
        (out / (label + ".command")).write_text(shlex.join(argv) + "\n")
        before = resources()
        if int(before["memory.current"]) + args.tree_rss_bytes >= args.stop_memory_bytes:
            raise RuntimeError("insufficient shared cgroup reserve before " + label)
        row = dict(label=label, argv=argv, resources_before=before, timeout=False, memory_guard=False,
                   tree_rss_peak=0, cgroup_current_peak=int(before["memory.current"]))
        started = time.monotonic()
        with (out / (label + ".log")).open("wb") as log:
            process = subprocess.Popen(argv, cwd=source, stdout=log, stderr=log, start_new_session=True)
            while process.poll() is None:
                rss = tree_rss(process.pid)
                current = int((cg / "memory.current").read_text())
                row["tree_rss_peak"] = max(row["tree_rss_peak"], rss)
                row["cgroup_current_peak"] = max(row["cgroup_current_peak"], current)
                row["memory_guard"] = rss >= args.tree_rss_bytes or current >= args.stop_memory_bytes
                row["timeout"] = time.monotonic() - started >= timeout
                if row["memory_guard"] or row["timeout"]:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                    break
                try:
                    process.wait(timeout=.05)
                except subprocess.TimeoutExpired:
                    pass
            row.update(exit=process.returncode, seconds=time.monotonic() - started,
                       resources_after=resources(), log_sha256=sha(out / (label + ".log")))
        commands.append(row)
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if row["exit"] != 0 or row["memory_guard"] or row["timeout"]:
            raise RuntimeError(label + " failed; raw failure retained")

    def fingerprint(label):
        destination = out / (label + ".json")
        run(label, [sys.executable, str(source / "tools/ci/wasm3_source_fingerprint.py"), str(source), str(destination)])
        if json.loads(destination.read_text())["source_id"] != args.expected_source_id:
            raise RuntimeError("actual selected source fingerprint changed")

    try:
        fingerprint("source-before")
        run("parse", ["taskset", "-c", "16", str(args.wasm_tools), "parse", str(fixture_root / WAT), "-o", str(out / "fixture.wasm")])
        run("validate", ["taskset", "-c", "16", str(args.wasm_tools), "validate", str(out / "fixture.wasm")])
        syntax = adapter.replace_output(compile_, out / "syntax-unused.o")
        syntax = [flag for flag in syntax if flag != "-c"] + ["-fsyntax-only"]
        run("syntax", ["taskset", "-c", "16", *syntax])
        run("compile", ["taskset", "-c", "16", *compile_])
        dependency = (out / "callsite.d").read_text().replace("\\\n", " ")
        for path in HEADERS:
            if not path.endswith(".h"):
                continue  # Script/shell auxiliaries are byte-locked, not C++ depfile inputs.
            if str(source / path) not in dependency and path not in dependency:
                raise RuntimeError("actual depfile does not prove selected complete source header: " + path)
        # These added graph oracles must use the actual selected LLVM SDK;
        # record their real depfile inputs, never a guessed vendor/header overlay.
        sdk_dependencies = shlex.split(dependency)
        for suffix in ("llvm/IR/Dominators.h", "llvm/Analysis/ValueTracking.h", "llvm/IR/Instructions.h"):
            matches = [Path(value) for value in sdk_dependencies if value.endswith("/" + suffix)]
            if len(matches) != 1:
                raise RuntimeError("actual SDK header dependency is missing/ambiguous: " + suffix)
            path = matches[0]
            if not path.is_absolute():
                path = source / path
            path = path.resolve(strict=True)
            sdk_inputs[suffix] = dict(path=str(path), sha256=sha(path))
        inputs["actual_llvm_sdk_graph_headers"] = sdk_inputs
        (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
        run("link", ["taskset", "-c", "16", *link])
        run("probe", ["taskset", "-c", "16", str(out / "probe"), str(out / "fixture.wasm"), str(out)])
        if "PASS actual Core 3 LLVM frames:" not in (out / "probe.log").read_text():
            raise RuntimeError("missing genuine Core 3 LLVM/collector/native execution witness")
        probe_log = (out / "probe.log").read_text()
        if probe_log.count("PASS updated scalar32 GC bridge identities enabled=") != 4 or "signed_not_exercised=" not in probe_log:
            raise RuntimeError("actual disabled/enabled plus unoptimized/O3 scalar32 bridge identity witnesses incomplete")
        for prefix in ("no-roots", "roots"):
            object_ = out / (prefix + ".native.o")
            if not object_.is_file() or not object_.stat().st_size:
                raise RuntimeError("missing actual newly compiled MCJIT native machine object")
            run(prefix + "-disassembly", ["taskset", "-c", "16", str(args.llvm_objdump),
                                         "--disassemble", "--reloc", str(object_)])
            run(prefix + "-unwind", ["taskset", "-c", "16", str(args.llvm_readobj),
                                    "--file-headers", "--sections", "--unwind", str(object_)])
        fingerprint("source-after")
        if any(sha(path) != selected[key] for key, path in files.items()):
            raise RuntimeError("reviewed emitter/fixture input changed during qualification")
        if any(sha(row["path"]) != row["sha256"] for row in sdk_inputs.values()):
            raise RuntimeError("actual LLVM SDK graph header changed during qualification")
        final = resources()
        if any(events(initial["memory.events"])[key] != events(final["memory.events"])[key] for key in ("oom", "oom_kill")):
            raise RuntimeError("cgroup OOM counter changed")
        summary.update(passed=True, actual_wasm_compiled_and_executed=True,
                       actual_single_thread_forced_collection=True, actual_sdk_o3_verified=True,
                       artifacts={path.name: sha(path) for path in out.iterdir() if path.is_file()}, resources_after=final)
    except Exception as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["commands"] = len(commands)
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS actual Core 3 precise JIT frame component; automatic VM GC and performance remain unqualified")


if __name__ == "__main__":
    main()
