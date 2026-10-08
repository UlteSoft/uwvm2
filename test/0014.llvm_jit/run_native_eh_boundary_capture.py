#!/usr/bin/env python3
"""Source-bound isolated EH diagnostic-capture helper regression.

The full product is not compiled. Exact helper text is extracted from a locked
v2 candidate and rejected v1. Only the TLS members the helpers read are exposed,
using the real FastIO frame vector type and the product's borrowed-record RAII.
No SDK mock, fake CFA, generated JIT qualification or performance claim.
--prepare-only performs extraction/metadata only, with no compiler invocation.
Otherwise all compiles and runs require the shared remote 64-GiB Linux cgroup.
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
import sys


SCOPE = "isolated actual candidate diagnostic capture/merge and native C++ propagation; no JIT/guest/Win64 qualification"
PRODUCT_INPUTS = (
    "src/uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h",
    "src/uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h",
    "src/uwvm2/runtime/lib/uwvm_runtime_activation_cleanup.h",
    "src/uwvm2/runtime/lib/uwvm_runtime_execution_entry.h",
)
AUX_INPUTS = (
    "test/0014.llvm_jit/native_eh_boundary_capture.cc",
    "test/0014.llvm_jit/run_native_eh_boundary_capture.py",
    "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py",
    "tools/ci/require_wasm3_test_cgroup.sh",
    "tools/ci/wasm3_source_fingerprint.py",
)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def item_end(text, start):
    opening = text.index("{", start)
    # Ignore comments/strings when finding the actual C++ braces. No C++ parser
    # or compiler is run during extraction, and exact unmodified text is kept.
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for match in token.finditer(text, opening):
        if match[0] == "{":
            depth += 1
        elif match[0] == "}":
            depth -= 1
            if depth == 0:
                end = match.end()
                return end + (end < len(text) and text[end] == ";")
    raise ValueError("unterminated actual C++ helper")


def extract_helpers(candidate, v1, destination):
    text, old = candidate.read_text(), v1.read_text()
    records = []

    def block(source, start, end, name):
        result = source[start:end]
        records.append({"name": name, "start_line": source[:start].count("\n") + 1,
                        "end_line": source[:end].count("\n") + 1,
                        "exact_text_sha256": hashlib.sha256(result.encode()).hexdigest()})
        return result

    def structure(name):
        match = re.search(r"\bstruct\s+" + name + r"\s*\{", text)
        if not match:
            raise ValueError("missing actual structure " + name)
        return block(text, match.start(), item_end(text, match.start()), name)

    merge_start = text.index("struct llvm_jit_native_frame_merge_state")
    merge_last = text.index("llvm_jit_native_frame_merge_finished(", merge_start)
    merge = block(text, merge_start, item_end(text, merge_last), "actual merge state and three helpers")
    callback = text.index("capture_llvm_jit_unwind_backtrace_frame(_Unwind_Context*")
    callback_start = text.rfind("template<bool StopAtBoundary>", 0, callback)
    last = text.index("capture_llvm_jit_exception_unwind_backtrace(call_stack_tls_state const&", callback)
    capture = block(text, callback_start, item_end(text, last), "actual v2 POSIX callback/capture/selector")
    old_selector = old.index("llvm_jit_exception_outer_native_boundary_cfa(call_stack_tls_state const&")
    old_start = old.rfind("[[nodiscard]]", 0, old_selector)
    legacy = block(old, old_start, item_end(old, old_selector), "rejected actual v1 oldest-boundary selector")
    # The sole old-code change is a distinct test namespace-local symbol name;
    # preserve/record original text above to make the historical bug executable.
    legacy = legacy.replace("llvm_jit_exception_outer_native_boundary_cfa(", "legacy_llvm_jit_exception_outer_native_boundary_cfa(", 1)
    pieces = [structure("call_stack_frame"), structure("llvm_jit_native_call_boundary")]
    tls = '''
// This fixture omits unrelated import caches/FP/scheduler members, while keeping
// the exact frame element, actual FastIO TLS allocator/vector and atomic slot.
struct call_stack_tls_state
{
    ::fast_io::containers::vector<call_stack_frame, ::fast_io::native_thread_local_allocator> frames{};
    alignas(::std::atomic_ref<llvm_jit_native_call_boundary const*>::required_alignment)
        llvm_jit_native_call_boundary const* llvm_jit_native_boundary{};
    inline constexpr call_stack_tls_state() noexcept { frames.reserve(4096uz); }
};
'''
    rendered = """// Generated from locked candidate helper text; no SDK mocks.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <fast_io.h>
#include <fast_io_dsal/vector.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h>
#include <uwvm2/runtime/lib/uwvm_runtime_activation_cleanup.h>
namespace uwvm_eh_candidate_helpers
{
""" + "\n\n".join((*pieces, tls, merge, structure("llvm_jit_unwind_backtrace_storage"), capture, legacy)) + "\n}\n"
    destination.write_text(rendered)
    return {"generated_header": str(destination), "generated_header_sha256": sha(destination),
            "actual_blocks": records, "adapter_scope": "TLS members actually read only, actual FastIO allocator/vector and aligned slot",
            "legacy_transform": "function symbol renamed once; original block SHA recorded"}


def audit_ir(text):
    callbacks = {}
    for variant in (0, 1):
        # Itanium GNU/Clang ABI: inspect the ACTUAL extracted template body,
        # not a wrapper whose own noexcept attribute could hide a bad call.
        pattern = r"^define [^\n]*@[^\n]*capture_llvm_jit_unwind_backtrace_frameILb" + str(variant) + r"EE[^\n]*\{\n(.*?)^\}"
        match = re.search(pattern, text, re.M | re.S)
        if not match:
            raise RuntimeError("missing actual callback template IR variant " + str(variant))
        body = match[1]
        sites = [line.strip() for line in body.splitlines()
                 if "getelementptr" in line and "llvm_jit_unwind_backtrace_storage" in line and
                 re.search(r"i(?:32|64) 0, i(?:32|64) [56](?:\s|,|$)", line)]
        callbacks[str(variant)] = {"definition": match[0].splitlines()[0], "stop_field_accesses": sites}
    if callbacks["0"]["stop_field_accesses"] or not callbacks["1"]["stop_field_accesses"]:
        raise RuntimeError("full/trap callback lost compile-time elimination of boundary-stop accesses")
    groups = dict(re.findall(r"^attributes #(\d+) = \{([^\n]*)\}", text, re.M))
    genuine = {}
    for target in ("__cxa_throw", "__cxa_rethrow"):
        rows = [line.strip() for line in text.splitlines() if "@" + target + "(" in line]
        if not any(re.search(r"\b(?:call|invoke)\b", line) for line in rows):
            raise RuntimeError("genuine native C++ propagation missing: " + target)
        for row in rows:
            expanded = row + " " + " ".join(groups[index] for index in re.findall(r"#(\d+)", row))
            if re.search(r"\bnounwind\b", expanded):
                raise RuntimeError("genuine propagation incorrectly marked nounwind: " + target)
        genuine[target] = rows
    return {"actual_callback_variants": callbacks, "genuine_propagation": genuine}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--candidate-file", type=Path, required=True)
    parser.add_argument("--candidate-sha256", required=True)
    parser.add_argument("--rejected-v1", type=Path, required=True)
    parser.add_argument("--v1-sha256", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--compiler-flag", action="append", default=[])
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--cpu", type=int, default=16, choices=range(16, 32))
    args = parser.parse_args()
    root, out = args.source_root.resolve(), args.out.resolve()
    if any(out.is_relative_to(root / directory) for directory in ("src", "third-parties")):
        raise RuntimeError("no evidence or generated inputs inside product fingerprint roots")
    candidate, old = args.candidate_file.resolve(strict=True), args.rejected_v1.resolve(strict=True)
    if sha(candidate) != args.candidate_sha256 or sha(old) != args.v1_sha256:
        raise RuntimeError("candidate/rejected-v1 bytes differ from locked expectations")
    inputs = {name: sha(root / name) for name in (*PRODUCT_INPUTS, *AUX_INPUTS)}
    out.mkdir(parents=True, exist_ok=False)
    report = {"scope": SCOPE, "passed": False, "status": "prepared_not_compiled",
              "expected_source_id": args.source_id, "candidate_sha256": sha(candidate),
              "rejected_v1_sha256": sha(old), "inputs_before": inputs,
              "extraction": extract_helpers(candidate, old, out / "native_eh_candidate_helpers.h"), "rows": []}
    (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.prepare_only:
        print(json.dumps({"status": report["status"], "compiled": False, "scope": SCOPE}))
        return
    common_path = root / "test/0014.llvm_jit/run_native_unwind_noexcept_abi.py"
    spec = importlib.util.spec_from_file_location("uwvm_native_eh_guarded_helpers", common_path)
    common = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(common)  # Import contains no compile/run main side effects.
    before = common.cgroup_state(root)
    if any(before["events"].get(key, 0) for key in ("oom", "oom_kill")):
        raise RuntimeError("cgroup has prior OOM events")
    if args.cpu not in os.sched_getaffinity(0):
        raise RuntimeError("requested E-core unavailable")
    os.sched_setaffinity(0, {args.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    compiler = Path(shutil.which(args.cxx) or args.cxx).absolute()
    compiler_realpath = compiler.resolve(strict=True)
    report.update(status="running", compiler=str(compiler), compiler_realpath=str(compiler_realpath),
                  compiler_sha256=sha(compiler_realpath), cgroup_before=before,
                  resident_memory_limit_bytes=2 * 1024**3, cpu=args.cpu)
    error = None

    def execute(command, name, *, env=None):
        row = common.invoke(command, out, name, root, env=env)
        report["rows"].append(row)
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
        if row["diagnostic"] or row["exit"] != 0:
            raise RuntimeError(name + " failed; inspect " + row["log"] + "; SKIP is never PASS")
        return row

    def fingerprint(name):
        target = out / (name + ".json")
        execute([sys.executable, str(root / "tools/ci/wasm3_source_fingerprint.py"), str(root), str(target)], name)
        return json.loads(target.read_text())["source_id"]

    try:
        report["actual_source_id"] = fingerprint("source-before")
        if report["actual_source_id"] != args.source_id:
            raise RuntimeError("frozen base source ID mismatch; no native compilation started")
        execute([str(compiler), "--version"], "compiler-version")
        if "clang" not in (out / "compiler-version.log").read_text().lower():
            raise RuntimeError("actual template IR audit requires Clang")
        flags = [str(compiler), *args.compiler_flag, "-std=c++23", "-fexceptions", "-pthread",
                 "-fasynchronous-unwind-tables", "-fno-optimize-sibling-calls", "-g",
                 "-I" + str(out), "-I" + str(root / "src"), "-I" + str(root / "third-parties/fast_io/include")]
        fixture = root / AUX_INPUTS[0]
        for mode, optimization in (("o0", "-O0"), ("o3", "-O3")):
            binary = out / ("native-eh-" + mode)
            execute([*flags, optimization, str(fixture), "-o", str(binary)], "build-" + mode)
            row = execute([str(binary)], "run-" + mode)
            if "PASS native EH boundary:" not in Path(row["log"]).read_text():
                raise RuntimeError("missing exact helper PASS marker")
            row["binary_sha256"] = sha(binary)
        ir = out / "native-eh-o0.ll"
        execute([*flags, "-O0", "-S", "-emit-llvm", str(fixture), "-o", str(ir)], "emit-ir")
        report["IR"] = audit_ir(ir.read_text())
        report["IR_sha256"] = sha(ir)
        if args.sanitizers:
            binary = out / "native-eh-asan-ubsan"
            execute([*flags, "-O1", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
                     str(fixture), "-o", str(binary)], "build-asan-ubsan")
            row = execute([str(binary)], "run-asan-ubsan",
                          env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1"))
            if "PASS native EH boundary:" not in Path(row["log"]).read_text():
                raise RuntimeError("missing sanitizer helper PASS marker")
            row["binary_sha256"] = sha(binary)
        report["status"] = "passed"
    except Exception as exception:
        error = exception
        report.update(status="failed", failure=str(exception))
    finally:
        try:
            report["inputs_after"] = {name: sha(root / name) for name in (*PRODUCT_INPUTS, *AUX_INPUTS)}
            report["cgroup_after"] = common.cgroup_state(root)
            report["source_id_after"] = fingerprint("source-after")
            if (report["inputs_after"] != inputs or sha(candidate) != args.candidate_sha256 or sha(old) != args.v1_sha256 or
                    report["source_id_after"] != report.get("actual_source_id")):
                raise RuntimeError("locked native helper inputs changed while probing")
            if any(report["cgroup_after"]["events"][key] != before["events"][key] for key in ("oom", "oom_kill")):
                raise RuntimeError("cgroup OOM counters changed")
        except Exception as final_error:
            report.update(status="failed", final_verification_failure=str(final_error))
            error = error or final_error
        report["passed"] = report["status"] == "passed" and error is None
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("passed", "status", "scope")}))
    if error:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
