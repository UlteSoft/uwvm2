#!/usr/bin/env python3
"""Verify actual GC aggregate LLVM helper calls using a source-matched product SDK.

Only the selected repository's actual emitter is compiled. A one-header -I
overlay cannot override its relative include, so this runner deliberately has no
overlay option. Both the source ID and the emitter SHA are mandatory. The probe
uses the real wasm_module_storage_t, real store layouts and the LLVM libraries
from the matched product. It executes helper-generated functions through MCJIT,
but does not parse/execute a validated Wasm module or implement collection.
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


HEADER = Path("src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_emit.h")
FIXTURE = Path("test/0014.llvm_jit/llvm_jit_gc_aggregate_callsite_ir.cc")
OPT_FLAGS = {"-O0", "-O1", "-O2", "-O3", "-Og", "-Os", "-Oz", "-Ofast"}
SCOPE = ("actual product LLVM aggregate helper, verifier, same-SDK O3 pipeline and x86-64 ELF object; "
         "actual MCJIT calls compared with direct generic bridge on separate real objects; "
         "no binary parser/validator qualification, validated Wasm module execution, performance score or collector qualification")


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def replace_unique_suffix(command, suffix, replacement):
    indices = [index for index, argument in enumerate(command) if argument.endswith(suffix)]
    if len(indices) != 1:
        raise ValueError(f"expected exactly one {suffix} input, found {indices}")
    result = command.copy()
    result[indices[0]] = str(replacement)
    return result


def replace_output(command, output):
    if command.count("-o") != 1 or command.index("-o") + 1 == len(command):
        raise ValueError("expected exactly one explicit output")
    result = command.copy()
    result[result.index("-o") + 1] = str(output)
    return result


def commands_from_product(runtime, cli, fixture, out, legacy):
    # Compile just this fixture with the actual runtime TU flags. A separate
    # compile avoids a multiple-source -MF file being overwritten by host_api.
    for configured in (runtime, cli):
        selected = [flag for flag in configured if flag in OPT_FLAGS]
        if not selected or selected[-1] != "-O3" or "-DUWVM_USE_LLVM_JIT" not in configured:
            raise ValueError("requires explicit O3 and the real LLVM backend in both product commands")
    if runtime.count("-c") != 1 or "-c" in cli:
        raise ValueError("expected one runtime compile and one complete CLI link")
    compile_command = replace_unique_suffix(runtime, "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp", fixture)
    compile_command = replace_output(compile_command, out / "callsite.o")
    compile_command += ["-MD", "-MF", str(out / "callsite.d")]
    if legacy:
        compile_command += ["-DUWVM2TEST_GC_LEGACY_AGGREGATE_ABI=1"]
    link_command = replace_unique_suffix(cli, "src/uwvm2/uwvm/main.default.cpp", out / "callsite.o")
    link_command = replace_output(link_command, out / "probe")
    return compile_command, link_command


def self_test():
    common = ["clang++", "-O3", "-DUWVM_USE_LLVM_JIT", "-I", "src"]
    runtime = common + ["-c", "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp", "-o", "/build/runtime.o"]
    cli = common + ["src/uwvm2/uwvm/main.default.cpp", "src/uwvm2/uwvm/host_api.default.cpp",
                    "/build/runtime.o", "@/sdk/consumer-link.rsp", "-pthread", "-o", "/build/uwvm"]
    before = (runtime.copy(), cli.copy())
    compile_, link = commands_from_product(runtime, cli, Path("/test/fixture.cc"), Path("/out"), True)
    assert runtime == before[0] and cli == before[1]
    assert compile_.count("/test/fixture.cc") == 1 and "-c" in compile_
    assert "src/uwvm2/uwvm/host_api.default.cpp" in link and "/build/runtime.o" in link
    assert "@/sdk/consumer-link.rsp" in link and "/out/callsite.o" in link
    assert "-DUWVM2TEST_GC_LEGACY_AGGREGATE_ABI=1" in compile_
    assert "-MD" in compile_ and compile_[compile_.index("-MF") + 1] == "/out/callsite.d"
    failures = 0
    for bad_runtime, bad_cli in ((runtime + ["-O0"], cli),
                                 (runtime, cli + ["src/uwvm2/uwvm/main.default.cpp"]),
                                 (runtime, cli + ["-o", "/second"]),
                                 ([flag for flag in runtime if flag != "-DUWVM_USE_LLVM_JIT"], cli)):
        try:
            commands_from_product(bad_runtime, bad_cli, Path("/fixture.cc"), Path("/out"), False)
        except ValueError:
            failures += 1
    assert failures == 4
    print("PASS command adaptation only; no C++/LLVM fixture was compiled or executed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--cli-build", type=Path)
    parser.add_argument("--expected-source-id")
    parser.add_argument("--expected-header-sha256")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--llvm-tools", type=Path, default=Path("/toolchain/bin"),
                        help="object readers only; O3 and object emission use the actual linked SDK")
    parser.add_argument("--stop-memory-bytes", type=int, default=60_000_000_000)
    parser.add_argument("--build-timeout", type=int, default=1200)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    for name in ("source_root", "cli_build", "expected_source_id", "expected_header_sha256", "out"):
        if getattr(args, name) is None:
            parser.error("--" + name.replace("_", "-") + " is required")
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("this source-bound ELF/assembly runner requires remote Linux x86-64")
    if not 0 < args.stop_memory_bytes < 68_719_476_736:
        parser.error("memory watchdog must stop below the hard 64 GiB cgroup limit")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    own_root = Path(__file__).resolve().parents[2]
    root = args.source_root.resolve(strict=True)
    build = args.cli_build.resolve(strict=True)
    header = root / HEADER
    fixture = own_root / FIXTURE
    if sha(header) != args.expected_header_sha256:
        raise ValueError("actual product emitter differs from the explicitly reviewed SHA")
    header_text = header.read_text()
    legacy = "llvm_jit_gc_aggregate_fixed_bridge(" not in header_text
    if legacy and "template<::std::uint_least32_t FixedOpcode" in header_text:
        raise ValueError("ABI7-template experiment is not the legacy product or the fixed5 product")
    guard = ["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")]
    subprocess.run(guard, check=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    os.chdir(root)
    cg = Path("/sys/fs/cgroup")

    def resources():
        state = {}
        for name in ("memory.current", "memory.peak", "memory.max", "memory.swap.max",
                     "memory.events", "cpu.stat", "cpuset.cpus.effective"):
            state[name] = (cg / name).read_text().strip()
        if state["memory.max"] != "68719476736" or state["memory.swap.max"] != "0" or state["cpuset.cpus.effective"] != "0,2,4,6,16-31":
            raise RuntimeError("requires the verified exact 64 GiB/swap0/4P+16E cgroup")
        return state

    def fingerprint(label):
        destination = out / (label + ".json")
        command = [sys.executable, str(root / "tools/ci/wasm3_source_fingerprint.py"), str(root), str(destination)]
        (out / (label + ".command")).write_text(shlex.join(command) + "\n")
        with (out / (label + ".log")).open("wb") as log:
            subprocess.run(command, cwd=root, stdout=log, stderr=log, check=True)
        value = json.loads(destination.read_text())
        if value["source_id"] != args.expected_source_id:
            raise ValueError("actual product source ID differs from the selected immutable source")
        return value

    before = fingerprint("source-before")
    metadata = json.loads((build / "build.json").read_text())
    if metadata["source_id"] != args.expected_source_id or Path(metadata["source"]).resolve() != root:
        raise ValueError("the qualified CLI/runtime build does not belong to this exact source tree")
    runtime = shlex.split((build / "runtime.command").read_text())
    cli = shlex.split((build / "cli.command").read_text())
    source_flag = '-DUWVM2_BUILD_SOURCE_ID=u8"' + args.expected_source_id + '"'
    if runtime.count(source_flag) != 1 or cli.count(source_flag) != 1:
        raise ValueError("qualified commands do not embed the same exact source ID")
    runtime_hash = sha(build / "runtime.o")
    if runtime_hash != metadata["runtime_object_sha256"] or sha(build / "uwvm") != metadata["binary_sha256"]:
        raise ValueError("qualified runtime or product binary changed after build")
    compile_, link = commands_from_product(runtime, cli, fixture, out, legacy)
    inputs = {"scope": SCOPE, "source_id": args.expected_source_id,
              "header": {"path": str(header), "sha256": sha(header)},
              "fixture": {"path": str(fixture), "sha256": sha(fixture)},
              "runner_sha256": sha(Path(__file__)), "build_json_sha256": sha(build / "build.json"),
              "runtime_object_sha256": runtime_hash, "product_binary_sha256": sha(build / "uwvm"),
              "abi": "legacy7" if legacy else "fixed5-with-large-generic7",
              "commands": {"compile": compile_, "link": link},
              "response_files": {argument[1:]: sha(argument[1:]) for argument in cli if argument.startswith("@")},
              "memory_watchdog_bytes": args.stop_memory_bytes,
              "resources_before": resources(),
              "performance_measurement": False, "full_wasm_execution": False,
              "mcjit_execution": "actual helper-generated functions; real direct generic native bridge is the ABI oracle",
              "object_optimizer": "actual linked SDK PassBuilder O3", "object_codegen": "same SDK TargetMachine aggressive/large"}
    (out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    commands = []
    summary = {"passed": False, "scope": SCOPE, "source_id": args.expected_source_id,
               "assembly_review": "pending; actual object/relocations will be retained", "full_vm_qualified": False,
               "performance_qualified": False, "collector_qualified": False}

    def run(label, command, timeout=180):
        subprocess.run(guard, check=True)
        (out / (label + ".command")).write_text(shlex.join(command) + "\n")
        row = {"label": label, "command": command, "exit": None, "memory_guard": False,
               "timeout": False, "resources_before": resources()}
        if int(row["resources_before"]["memory.current"]) >= args.stop_memory_bytes:
            raise RuntimeError("insufficient cgroup headroom before " + label)
        started = time.monotonic()
        peak = int(row["resources_before"]["memory.current"])
        with (out / (label + ".log")).open("wb") as log:
            process = subprocess.Popen(command, cwd=root, stdout=log, stderr=log, start_new_session=True)
            while process.poll() is None:
                current = int((cg / "memory.current").read_text())
                peak = max(peak, current)
                row["memory_guard"] = current >= args.stop_memory_bytes
                row["timeout"] = time.monotonic() - started >= timeout
                if row["memory_guard"] or row["timeout"]:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                    break
                try:
                    process.wait(timeout=0.25)
                except subprocess.TimeoutExpired:
                    pass
            row["exit"] = process.returncode
        row["seconds"] = time.monotonic() - started
        row["cgroup_peak_during_command"] = peak
        row["resources_after"] = resources()
        row["log_sha256"] = sha(out / (label + ".log"))
        commands.append(row)
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if row["exit"] != 0 or row["timeout"] or row["memory_guard"]:
            raise RuntimeError(label + " failed; exact raw command/log preserved")

    try:
        run("compiler-version", [runtime[0], "--version"])
        run("compile", ["taskset", "-c", "16-31", *compile_], args.build_timeout)
        dependency = (out / "callsite.d").read_text().replace("\\\n", " ")
        if str(header) not in dependency and str(HEADER) not in dependency:
            raise RuntimeError("depfile did not prove inclusion of the selected actual emitter")
        run("link", ["taskset", "-c", "16-31", *link], args.build_timeout)
        run("probe", ["taskset", "-c", "0", str(out / "probe"), str(out)])
        log = (out / "probe.log").read_text()
        cases = [json.loads(line.removeprefix("GC_CALLSITE_JSON ")) for line in log.splitlines()
                 if line.startswith("GC_CALLSITE_JSON ")]
        executions = [json.loads(line.removeprefix("GC_CALLSITE_EXEC ")) for line in log.splitlines()
                      if line.startswith("GC_CALLSITE_EXEC ")]
        environments = [json.loads(line.removeprefix("GC_CALLSITE_ENV ")) for line in log.splitlines()
                        if line.startswith("GC_CALLSITE_ENV ")]
        if len(cases) != 49 or len(executions) != 49 or len(environments) != 1 or {row["opcode"] for row in cases} != set(range(20)):
            raise RuntimeError("missing complete expected helper case/environment output")
        if [row["function"] for row in cases] != ["gc_case_" + str(index) for index in range(49)]:
            raise RuntimeError("case IDs are missing, duplicated or out of their independent expected order")
        if {row["case"] for row in executions} != set(range(49)) or any(
            row["opcode"] != cases[row["case"]]["opcode"] or not row["actual_mcjit_called"] or
            not row["matches_direct_generic_bridge"] or not row["separate_mutation_targets"] for row in executions):
            raise RuntimeError("incomplete actual MCJIT ABI/mutation comparison")
        if (environments[0]["pointer_bits"] != 64 or not environments[0]["optimized_ir_verified"] or
            environments[0]["cases"] != 49 or environments[0]["actual_mcjit_cases"] != 49 or
            environments[0]["collections"] != 0 or environments[0]["executed_wasm"]):
            raise RuntimeError("unexpected helper target or scope")
        if legacy and any(row["argument_count"] != 7 for row in cases):
            raise RuntimeError("legacy baseline unexpectedly selected a fixed ABI")
        if not legacy and any(row["argument_count"] != (7 if row["input_count"] > 8 else 5) for row in cases):
            raise RuntimeError("candidate fixed/generic ABI selection differs from independently expected input count")
        (out / "cases.json").write_text(json.dumps(cases, indent=2) + "\n")
        (out / "executions.json").write_text(json.dumps(executions, indent=2) + "\n")
        (out / "llvm-environment.json").write_text(json.dumps(environments[0], indent=2) + "\n")
        run("object-reader-version", [str(args.llvm_tools / "llvm-objdump"), "--version"])
        run("object-relocations", [str(args.llvm_tools / "llvm-readobj"), "--file-header", "--relocations", "--symbols", str(out / "aggregate.o")])
        run("object-disassembly", [str(args.llvm_tools / "llvm-objdump"), "-dr", "--no-show-raw-insn", str(out / "aggregate.o")])
        optimized = (out / "aggregate.optimized.ll").read_text()
        relocation = (out / "object-relocations.log").read_text()
        assembly = (out / "object-disassembly.log").read_text()
        for row in cases:
            body = re.search(r"(?ms)^define\b[^\n]*@" + re.escape(row["function"]) + r"\(.*?^}", optimized)
            if body is None:
                raise RuntimeError("missing actual O3 case function " + row["function"])
            selected = re.findall(r"\bcall\b[^\n]*@" + re.escape(row["bridge"]) + r"\(", body.group())
            if len(selected) != 1 or row["bridge"] not in relocation or row["bridge"] not in assembly:
                raise RuntimeError("selected native bridge was lost or absent from actual object relocation")
        if sha(header) != args.expected_header_sha256 or sha(fixture) != inputs["fixture"]["sha256"] or sha(build / "runtime.o") != runtime_hash:
            raise RuntimeError("an actual compiler/runtime input changed during qualification")
        after = fingerprint("source-after")
        assert before["source_id"] == after["source_id"]
        final = resources()
        def events(value):
            return {key: int(number) for key, number in (line.split() for line in value.splitlines())}
        for key in ("oom", "oom_kill"):
            if events(final["memory.events"])[key] != events(inputs["resources_before"]["memory.events"])[key]:
                raise RuntimeError("cgroup OOM counter changed during qualification")
        summary.update(passed=True, cases=cases, executions=executions, mcjit_helper_qualified=True,
                       llvm_environment=environments[0], resources_after=final,
                       artifacts={name: sha(out / name) for name in ("probe", "callsite.o", "aggregate.ll", "aggregate.optimized.ll", "aggregate.o")})
    except Exception as error:
        summary["failure"] = str(error)
        raise
    finally:
        summary["commands"] = len(commands)
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS actual LLVM GC aggregate helper/ABI/MCJIT/O3 object only; full Wasm and paired performance remain required")


if __name__ == "__main__":
    main()
