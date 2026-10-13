#!/usr/bin/env python3
"""Freeze actual source bytes and text projections only; never execute native tools."""
import argparse
import hashlib
import json
import pathlib
import re

GATE = "UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF"
BASE = "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/"
BEFORE = "build/wasm3-evidence/source-only-native-eh-private-leaf-before-20261002"
RUNTIME_FILES = ["src/uwvm2/runtime/lib/uwvm_runtime.default.cpp",
                 "src/uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h"]
NEW = [BASE + "translate/single_func_native_eh_private_leaf_metadata.h",
       BASE + "translate/single_func_native_eh_private_leaf_stage.h",
       "src/uwvm2/runtime/lib/native_eh_private_leaf_numeric_bridge.h",
       "test/0014.llvm_jit/llvm_native_eh_private_leaf_staged.cc",
       "test/0014.llvm_jit/llvm_native_eh_private_leaf_imports.cc",
       "test/0014.llvm_jit/prepare_native_eh_private_leaf_source.py",
       "test/0017.runtime/llvm_native_eh_private_leaf_stage3_implementation_20261002.md"]
DEPENDENCIES = [BASE + "translate/single_func_emit.h",
                BASE + "translate/single_func_exception_emit.h",
                BASE + "translate/single_func_debug_host_bridge.h",
                BASE + "translate/single_func_validation_dispatch.h",
                BASE + "translate/opcode/control_flow_cases.h",
                BASE + "translate/opcode/branch_cases.h",
                BASE + "translate/opcode/call_cases.h",
                "src/uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h",
                "src/uwvm2/runtime/lib/uwvm_runtime.h",
                "src/uwvm2/runtime/lib/uwvm_runtime.module.cpp",
                "src/uwvm2/runtime/lib/uwvm_runtime_debug_activation.h",
                "src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_capture.h",
                "src/uwvm2/runtime/lib/uwvm_runtime_debug_activation_api.h",
                "src/uwvm2/uwvm/runtime/storage/full.h",
                "test/0014.llvm_jit/fixtures/native_eh_leaf_observer_valid.wat",
                "test/0014.llvm_jit/fixtures/native_eh_leaf_observer_invalid.wat",
                "test/0017.runtime/llvm_native_eh_private_leaf_stage3_design_20261002.md"]


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def pin(raw):
    return {"bytes": len(raw), "sha256": digest(raw)}


def project_off(raw):
    """Exact-gate textual projection only, not C++ preprocessing/assembly."""
    out, active, depth, keep, blocks = [], False, 0, False, 0
    pattern = re.compile(rb"^\s*#\s*if\s+defined\(" + GATE.encode() +
                         rb"\)\s*&&\s*" + GATE.encode() + rb"\s*==\s*1\s*$")
    for line in raw.splitlines(keepends=True):
        if not active:
            if pattern.match(line.rstrip(b"\r\n")):
                active, depth, keep = True, 0, False
                blocks += 1
            else:
                out.append(line)
            continue
        match = re.match(rb"^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b", line)
        word = match.group(1) if match else None
        if word in (b"if", b"ifdef", b"ifndef"):
            depth += 1
        elif word == b"endif":
            if depth == 0:
                active = False
                continue
            depth -= 1
        elif word == b"else" and depth == 0:
            if keep:
                raise ValueError("duplicate exact-gate else")
            keep = True
            continue
        elif word == b"elif" and depth == 0:
            raise ValueError("unreviewed exact-gate elif")
        if keep:
            out.append(line)
    if active:
        raise ValueError("unterminated exact gate")
    return b"".join(out), blocks


def save_json(path, value):
    raw = (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()
    path.write_bytes(raw)
    return pin(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    before = repo / BEFORE
    original_raw = (before / "manifest.json").read_bytes()
    original = json.loads(original_raw)
    correction = before / "independent-debug-correction"
    correction_raw = (correction / "actual-debug-after-manifest.json").read_bytes()
    if digest(correction_raw) != "d1c4350b848c00998c03b46138d027f127cf9598f7ea1955eb1351e0cea07e49":
        raise ValueError("actual independent correction manifest changed")
    corrected = json.loads(correction_raw)
    captured, files = {}, {}
    for relative, expected in original["files"].items():
        old, raw = (before / relative).read_bytes(), (repo / relative).read_bytes()
        if pin(old) != expected:
            raise ValueError("actual original before-image changed: " + relative)
        projected, blocks = project_off(raw)
        if blocks == 0:
            raise ValueError("missing reviewed exact gate: " + relative)
        result = dict(pin(raw), original_before=expected, exact_gate_blocks=blocks,
                      off_projection=pin(projected), text_only=True)
        if relative in RUNTIME_FILES:
            item = corrected["files"][repo.name + ":" + relative]
            actual_debug_before = (correction / relative).read_bytes()
            if digest(actual_debug_before) != item["before_sha256"] or actual_debug_before != old:
                raise ValueError("independent correction before is not actual Stage3 before")
            if digest(raw) != item["sha256"]:
                raise ValueError("current combined after does not match captured debugger after")
            result.update(independent_debug_correction=True,
                          combined_after_matches_actual_debug_after=True,
                          debug_before_equals_original_before=True,
                          stage3_off_equals_actual_debug_after_off=True,
                          limitation="Full file differs from original before by the separately retained debugger correction.")
        elif projected != old:
            raise ValueError("default-off text differs from actual compiler before: " + relative)
        else:
            result["off_projection_byte_equal_to_original_before"] = True
        captured[relative], files[relative] = raw, result
    for relative in NEW + DEPENDENCIES:
        raw = (repo / relative).read_bytes()
        captured[relative] = raw
        files[relative] = dict(pin(raw), new_stage3_file=relative in NEW,
                               actual_dependency=relative in DEPENDENCIES)
    # No source file is rewritten. Refuse a mixed snapshot if any selected
    # actual byte changed while the short source-only capture was in progress.
    for relative, raw in captured.items():
        if (repo / relative).read_bytes() != raw:
            raise ValueError("concurrent source mutation: " + relative)
    args.out.mkdir(parents=True, exist_ok=False)
    snapshot = args.out / "snapshot"
    for relative, raw in captured.items():
        dest = snapshot / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(raw)
    receipt = dict(schema="uwvm-native-eh-private-leaf-staging-source-v1", gate=GATE,
        exact_gate_value=1, request_default=False, files=files,
        original_before_manifest=pin(original_raw), independent_correction_manifest=pin(correction_raw),
        actual_source_id=None, complete_source_fingerprint_required_at_keeper=True,
        native_compile=False, native_execution=False, cfi_registration=False,
        loaded_private_code=False, runtime_selected=False, performance_result=False,
        scope="Actual selected source snapshot, exact default-off text projection and independent debug-delta provenance only.")
    saved = save_json(args.out / "source-receipt.json", receipt)
    plan = dict(schema="uwvm-native-eh-private-leaf-staging-cold-plan-v1", source_only=True,
        source_manifest_sha256=saved["sha256"], actual_source_id=None, compiler_argv=None,
        runtime_argv=None, executable_sha256=None, keeper="linux_fused_resume",
        fixture="test/0014.llvm_jit/llvm_native_eh_private_leaf_staged.cc",
        resources=dict(memory_max_bytes=64 * 1024**3, memory_swap_max_bytes=0,
            compile_cpus="16-31", guest_cpu=0, serialized=True, fresh_actual_scope_identity=True),
        configurations=["compile-only PRIVATE_LEAF undefined/0/2 with observer undefined/0/1",
            "compile-only PRIVATE_LEAF=1 with observer undefined/0/1",
            "both exact1: aligned runtime plus actual fused fixture O3",
            "both exact1: aligned runtime plus actual fused fixture ASan/UBSan"],
        mandatory_controls=["official encoder/validator and Wasmtime use identical valid/late-invalid bytes",
            "serial/2-worker actual initialized full source; one selected edge/clone and two private raises",
            "original public leaf raises and original caller unchanged; separate context; personality/CFI IR retained",
            "RV64 safely declines initial unsupported actual bridge materialization",
            "retaining-first handler and unreachable call remain unselected",
            "wrong MD shape/version, duplicate attachment, quota-zero and missing source decline safely",
            "strict actual late function parse-domain invalid end_result_mismatch",
            "fresh actual source/compiler/runtime/deps/ELF before-after closure; no concurrent build/VM/profiler"],
        output_files=["actual-source-before.json", "actual-source-after.json", "compiler.argv",
            "runtime.argv", "compiler.log", "oracle-valid.log", "oracle-invalid.log",
            "serial.log", "parallel.log", "sanitizers.log", "closure.json"],
        no_execution_permission="Original publisher does not read staged field; no private engine, loaded CFI or runtime timing claim.")
    planned = save_json(args.out / "cold-plan.json", plan)
    manifest = {relative: pin(raw) for relative, raw in captured.items()}
    pinned = save_json(args.out / "snapshot-manifest.json", manifest)
    print(json.dumps(dict(source_receipt=saved, cold_plan=planned, snapshot_manifest=pinned,
                          selected_files=len(captured), native_execution=False)))


if __name__ == "__main__":
    main()
