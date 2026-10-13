#!/usr/bin/env python3
"""Bounded source-bound native unwind ABI regression, inside the Linux cgroup.

Checks SDK types, real non-tail native callers, typed C++ catch/rethrow, Clang
IR nounwind call attributes, and the global-module-fragment include boundary.
This is not a generated JIT, guest EH, Win64, or dynamic FDE qualification.
Compilation/running is deliberately opt-in; importing this file does nothing.
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
import shutil
import signal
import subprocess
import sys
import time


SCOPE = "native SDK/asm-link ABI and global module fragment; no JIT or guest EH qualification"
FILES = (
    "src/uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/dwarf_eh_frame_registration.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.cppm",
    "src/uwvm2/runtime/lib/uwvm_runtime_native_unwind.h",
    "src/uwvm2/runtime/lib/uwvm_runtime.module.cpp",
    "test/0014.llvm_jit/native_unwind_noexcept_abi.cc",
    "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py",
    "tools/ci/require_wasm3_test_cgroup.sh",
    "tools/ci/wasm3_source_fingerprint.py",
)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def cgroup_state(root):
    if platform.system() != "Linux":
        raise RuntimeError("this runner only runs inside the established remote Linux cgroup")
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    names = ("memory.max", "memory.swap.max", "memory.current", "memory.events", "cpuset.cpus.effective")
    result = {name: (Path("/sys/fs/cgroup") / name).read_text().strip() for name in names}
    if (result["memory.max"] != "68719476736" or result["memory.swap.max"] != "0" or
            result["cpuset.cpus.effective"] != "0,2,4,6,16-31"):
        raise RuntimeError("require the verified 64 GiB, swap-free 4P+16E cgroup")
    result["events"] = {key: int(value) for key, value in
                        (line.split() for line in result["memory.events"].splitlines())}
    return result


def process_tree_rss(pid):
    # /proc avoids a new ps subprocess on every watchdog sample.
    children, sizes = {}, {}
    for path in Path("/proc").iterdir():
        if not path.name.isdecimal():
            continue
        try:
            fields = path.joinpath("stat").read_text().rsplit(")", 1)[1].split()
            parent = int(fields[1])
            rss = max(0, int(fields[21])) * os.sysconf("SC_PAGESIZE")
        except (FileNotFoundError, ProcessLookupError, PermissionError, IndexError, ValueError):
            continue
        child = int(path.name)
        children.setdefault(parent, []).append(child)
        sizes[child] = rss
    pending, members, total = [pid], [], 0
    while pending:
        current = pending.pop()
        members.append(current)
        total += sizes.get(current, 0)
        pending.extend(children.get(current, ()))
    return total, members


def invoke(command, out, name, root, *, timeout=180, env=None, rss_limit=2 * 1024**3):
    if type(rss_limit) is not int or not (0 < rss_limit <= 3 * 1024**3):
        raise RuntimeError("native auxiliary process-tree RSS budget must be positive and at most 3 GiB")
    log = out / (name + ".log")
    (out / (name + ".command")).write_text(shlex.join(map(str, command)) + "\n")
    start = time.monotonic()
    peak, status, diagnostic, usage = 0, None, None, None
    limit = rss_limit
    with log.open("wb") as stream:
        child = subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                 env=env, start_new_session=True)
        while status is None:
            observed, members = process_tree_rss(child.pid)
            peak = max(peak, observed)
            if observed > limit:
                diagnostic = "process_tree_RSS_exceeded_budget"
            elif time.monotonic() - start > timeout:
                diagnostic = "timeout"
            if diagnostic:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                for member in reversed(members):
                    try:
                        os.kill(member, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            waited, raw, measured = os.wait4(child.pid, os.WNOHANG)
            if waited:
                status, usage = os.waitstatus_to_exitcode(raw), measured
                break
            time.sleep(0.05)
    child.returncode = status
    peak = max(peak, usage.ru_maxrss * 1024)
    if peak > limit:
        diagnostic = "process_tree_RSS_exceeded_budget"
    return {"name": name, "command": list(map(str, command)), "exit": status,
            "diagnostic": diagnostic, "elapsed_seconds": time.monotonic() - start,
            "process_tree_rss_budget_bytes": limit,
            "peak_process_tree_rss_bytes": peak, "log": str(log), "log_sha256": sha(log)}


def canonical_symbol(spelling):
    return spelling.strip('"').removeprefix("\\01")


def audit_ir(text):
    groups = dict(re.findall(r"^attributes #(\d+) = \{([^\n]*)\}", text, re.M))
    symbol_pattern = r'@("[^"\n]+"|[-A-Za-z0-9_.$]+)'
    declarations = {}
    for line in text.splitlines():
        if line.startswith("declare "):
            match = re.search(symbol_pattern, line)
            if match:
                declarations[canonical_symbol(match[1])] = line

    def attributes(line):
        values = line + " " + " ".join(groups[index] for index in re.findall(r"#(\d+)", line))
        return set(re.findall(r"\b[-A-Za-z0-9_]+\b", values))

    def body(name):
        pattern = r"^define [^\n]*@" + re.escape(name) + r"\([^\n]*\{\n(.*?)^\}"
        match = re.search(pattern, text, re.M | re.S)
        if not match:
            raise RuntimeError("missing IR definition: " + name)
        return match[0].splitlines()[0], match[1]

    rows = []
    for wrapper, target in (
        ("uwvm_native_abi_ip", "_Unwind_GetIPInfo"),
        ("uwvm_native_abi_cfa", "_Unwind_GetCFA"),
        ("uwvm_native_abi_region", "_Unwind_GetRegionStart"),
        ("uwvm_native_abi_backtrace", "_Unwind_Backtrace"),
    ):
        definition, code = body(wrapper)
        if "nounwind" not in attributes(definition):
            raise RuntimeError("wrapper is not nounwind: " + wrapper)
        found = []
        for line in code.splitlines():
            if not re.search(r"\b(?:call|invoke)\b", line):
                continue
            match = re.search(symbol_pattern, line)
            if not match or canonical_symbol(match[1]) != target:
                continue
            declaration = declarations.get(target, "")
            call_nounwind = "nounwind" in attributes(line)
            declaration_nounwind = "nounwind" in attributes(declaration)
            if "invoke " in line or not (call_nounwind or declaration_nounwind):
                raise RuntimeError("SDK call can unwind in IR: " + line.strip())
            found.append({"call": line.strip(), "declaration": declaration,
                          "call_nounwind": call_nounwind, "declaration_nounwind": declaration_nounwind})
        if len(found) != 1:
            raise RuntimeError("expected one exact SDK call in " + wrapper)
        rows.append({"wrapper": wrapper, "symbol": target, "sites": found})

    for throwing in ("uwvm_native_abi_throw", "uwvm_native_abi_rethrow"):
        definition, _ = body(throwing)
        if "nounwind" in attributes(definition):
            raise RuntimeError("genuine propagation incorrectly nounwind: " + throwing)
    for target in ("__cxa_throw", "__cxa_rethrow"):
        sites = []
        for line in text.splitlines():
            if not re.search(r"\b(?:call|invoke)\b", line):
                continue
            match = re.search(symbol_pattern, line)
            if match and canonical_symbol(match[1]) == target:
                if "nounwind" in attributes(line) or "nounwind" in attributes(declarations.get(target, "")):
                    raise RuntimeError("throw/rethrow import incorrectly nounwind: " + line.strip())
                sites.append(line.strip())
        if not sites:
            raise RuntimeError("no genuine propagation call emitted: " + target)
        rows.append({"symbol": target, "nounwind": False, "sites": sites})
    _, rethrow = body("uwvm_native_abi_rethrow")
    if not re.search(r"invoke [^\n]*@uwvm_native_abi_throw\(", rethrow) or "landingpad" not in rethrow:
        raise RuntimeError("typed C++ handler has lost invoke/landingpad propagation")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--source-id", required=True, help="exact previously frozen src+third-parties fingerprint")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--compiler-flag", action="append", default=[], help="repeat for toolchain flags, e.g. --compiler-flag=-stdlib=libc++")
    parser.add_argument("--sanitizers", action="store_true", help="also run ASan+UBSan under the same resident-memory watchdog")
    parser.add_argument("--cpu", type=int, default=16, choices=range(16, 32))
    args = parser.parse_args()
    root, out = args.source_root.resolve(), args.out.resolve()
    if any(out.is_relative_to(root / directory) for directory in ("src", "third-parties")):
        raise RuntimeError("evidence must not be written inside fingerprinted product inputs")
    before_cgroup = cgroup_state(root)
    if any(before_cgroup["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("requires a cgroup with OOM/OOM_kill=0")
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError("requested E-core is not available in this cgroup")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out.mkdir(parents=True, exist_ok=False)
    # clang++ is often a symlink to clang. Preserve its invocation name so the
    # link driver selects C++ runtime libraries; hash the resolved bytes below.
    compiler = Path(shutil.which(args.cxx) or args.cxx).absolute()
    compiler_realpath = compiler.resolve(strict=True)
    input_before = {name: sha(root / name) for name in FILES}
    report = {"passed": False, "status": "running", "scope": SCOPE, "expected_source_id": args.source_id,
              "compiler": str(compiler), "compiler_realpath": str(compiler_realpath),
              "compiler_sha256": sha(compiler_realpath), "inputs_before": input_before,
              "cpu": args.cpu, "resident_memory_limit_bytes": 2 * 1024**3,
              "cgroup_before": before_cgroup, "rows": []}
    error = None

    def execute(command, name, *, acceptable=(0,), env=None):
        row = invoke(command, out, name, root, env=env)
        report["rows"].append(row)
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
        if row["diagnostic"] or row["exit"] not in acceptable:
            raise RuntimeError(name + " failed; inspect " + row["log"])
        return row

    def fingerprint(name):
        execute([sys.executable, str(root / "tools/ci/wasm3_source_fingerprint.py"), str(root),
                 str(out / (name + ".json"))], name)
        return json.loads((out / (name + ".json")).read_text())["source_id"]

    try:
        actual = fingerprint("source-before")
        report["actual_source_id"] = actual
        if actual != args.source_id:
            raise RuntimeError("frozen source ID mismatch; no native compilation was started")
        execute([str(compiler), "--version"], "compiler-version")
        if "clang" not in (out / "compiler-version.log").read_text().lower():
            raise RuntimeError("LLVM IR audit requires Clang; this runner does not silently skip it")
        flags = [str(compiler), *args.compiler_flag, "-std=c++23", "-fexceptions",
                 "-fno-optimize-sibling-calls", "-fasynchronous-unwind-tables", "-g",
                 "-I" + str(root / "src"), "-I" + str(root / "third-parties/fast_io/include")]
        fixture = root / "test/0014.llvm_jit/native_unwind_noexcept_abi.cc"
        for mode, extra in (("o0", ["-O0"]), ("o3", ["-O3"])):
            binary = out / ("native-" + mode)
            execute([*flags, *extra, str(fixture), "-o", str(binary)], "build-" + mode)
            row = execute([str(binary)], "run-" + mode, acceptable=(0, 77))
            if row["exit"] == 77:
                report["status"] = "skipped_ineligible_native_unwind_ABI"
                raise RuntimeError("native ABI fixture returned SKIP; this is not a qualification PASS")
            if "PASS native unwind ABI:" not in (out / ("run-" + mode + ".log")).read_text():
                raise RuntimeError("missing native regression PASS marker")
            row["binary_sha256"] = sha(binary)

        ir_path = out / "native-o0.ll"
        execute([*flags, "-O0", "-S", "-emit-llvm", str(fixture), "-o", str(ir_path)], "emit-ir")
        report["IR"] = audit_ir(ir_path.read_text())
        report["IR_sha256"] = sha(ir_path)

        # This intentionally small consumer reproduces the GMF declaration
        # attachment used by runtime.module.cpp and section_memory_manager.cppm.
        # It does not compile those heavyweight product modules or claim they
        # have been execution-qualified.
        module = out / "native-unwind-module.cppm"
        module.write_text('module;\n#include <type_traits>\n'
            '#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h>\n'
            'export module uwvm2.test.native_unwind_abi;\n'
            'static_assert(noexcept(uwvm2::runtime::compiler::llvm_jit::native_unwind_abi::backtrace_noexcept(nullptr, nullptr)));\n'
            'export bool native_unwind_module_types_ok() noexcept { return true; }\n')
        pcm, module_object = out / "native-unwind.pcm", out / "native-unwind-module.o"
        execute([*flags, "-O0", "--precompile", str(module), "-o", str(pcm)], "module-precompile")
        execute([*flags, "-O0", "-c", str(pcm), "-o", str(module_object)], "module-object")
        consumer = out / "native-unwind-import.cc"
        consumer.write_text('import uwvm2.test.native_unwind_abi;\n'
                            'int main() { return native_unwind_module_types_ok() ? 0 : 1; }\n')
        imported = out / "native-unwind-import"
        execute([*flags, "-O0", "-fmodule-file=uwvm2.test.native_unwind_abi=" + str(pcm),
                 str(consumer), str(module_object), "-o", str(imported)], "module-import-link")
        execute([str(imported)], "module-import-run")
        report["global_module_fragment_probe"] = True

        if args.sanitizers:
            sanitized = out / "native-asan-ubsan"
            execute([*flags, "-O1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
                     str(fixture), "-o", str(sanitized)], "build-asan-ubsan")
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1")
            row = execute([str(sanitized)], "run-asan-ubsan", env=env)
            if "PASS native unwind ABI:" not in (out / "run-asan-ubsan.log").read_text():
                raise RuntimeError("missing sanitized PASS marker")
            row["binary_sha256"] = sha(sanitized)
        report["status"] = "passed"
    except Exception as exception:
        error = exception
        if report["status"] == "running":
            report["status"] = "failed"
        report["failure"] = str(exception)
    finally:
        try:
            report["inputs_after"] = {name: sha(root / name) for name in FILES}
            report["cgroup_after"] = cgroup_state(root)
            report["source_id_after"] = fingerprint("source-after")
            if (report["inputs_after"] != input_before or
                    report["source_id_after"] != report.get("actual_source_id")):
                raise RuntimeError("source bytes changed while probing")
            for key in ("oom", "oom_kill"):
                if report["cgroup_after"]["events"][key] != before_cgroup["events"][key]:
                    raise RuntimeError("cgroup OOM events changed")
        except Exception as final_error:
            report["final_verification_failure"] = str(final_error)
            if error is None:
                error = final_error
                report["failure"] = str(final_error)
            report["status"] = "failed"
        report["passed"] = report["status"] == "passed" and error is None
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("passed", "status", "scope")}), flush=True)
    if error:
        raise SystemExit(77 if report["status"] == "skipped_ineligible_native_unwind_ABI" else 1)


if __name__ == "__main__":
    main()
