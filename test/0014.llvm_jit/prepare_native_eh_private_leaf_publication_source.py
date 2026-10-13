#!/usr/bin/env python3
"""Capture source bytes/text receipts only; no compiler, guest, SSH or profiler."""
import argparse
import hashlib
import importlib.util
import json
import pathlib

BASE = "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/"
LIB = "src/uwvm2/runtime/lib/"
OLD = "build/wasm3-evidence/source-only-native-eh-private-leaf-20261002-v1"
BEFORE = "build/wasm3-evidence/source-only-native-eh-private-leaf-publication-before-20261002"
ROOT_BEFORE = "build/wasm3-evidence/source-only-debug-source-private-leaf-management-20261002-r1"
ADDITIONAL = [
    BASE + "section_memory_manager.h", BASE + "section_memory_manager.cppm",
    BASE + "native_eh_private_leaf_cfi_observer.h",
    LIB + "uwvm_runtime_pending_code_ranges.h", LIB + "uwvm_runtime_loaded_function_ranges.h",
    LIB + "native_eh_private_leaf_publication_decl.h", LIB + "native_eh_private_leaf_publication_impl.h",
    LIB + "uwvm_runtime_debug_source_api.h", LIB + "uwvm_runtime_debug_source_binding.h",
    LIB + "uwvm_runtime_native_exception_host.h", LIB + "uwvm_runtime_native_function_address.h",
    LIB + "uwvm_runtime_generation.h", LIB + "uwvm_runtime_execution_entry.h",
    LIB + "uwvm_runtime_native_unwind.h", LIB + "uwvm_runtime_native_unwind_execution_gate.h",
    "src/uwvm2/uwvm/run/owned_source.h",
    "test/0017.runtime/llvm_native_eh_private_leaf_publication_runtime.cc",
    "test/0014.llvm_jit/fixtures/native_eh_private_leaf_publication_valid.wat",
    "test/0017.runtime/llvm_native_eh_private_leaf_publication_20261002.md",
    "test/0014.llvm_jit/prepare_native_eh_private_leaf_publication_source.py",
]


def pin(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def save(path, value):
    raw = (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()
    path.write_bytes(raw)
    return pin(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    helper = repo / "test/0014.llvm_jit/prepare_native_eh_private_leaf_source.py"
    if pin(helper.read_bytes())["sha256"] != "49f2c74da7742d3666c168cb3dc17caf88a78fd618b04d037b2a7fa1d3e8e1d3":
        raise ValueError("immutable v1 text projection helper changed")
    spec = importlib.util.spec_from_file_location("immutable_private_projection", helper)
    project = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(project)
    old_raw = (repo / OLD / "snapshot-manifest.json").read_bytes()
    old_manifest = json.loads(old_raw)
    pub_before_raw = (repo / BEFORE / "manifest.json").read_bytes()
    pub_before = json.loads(pub_before_raw)
    selected = sorted(set(old_manifest) | set(ADDITIONAL))
    captured = {name: (repo / name).read_bytes() for name in selected}
    projections = {}
    for name, expected in pub_before["files"].items():
        original = (repo / BEFORE / name).read_bytes()
        if pin(original) != expected:
            raise ValueError("actual publication before-image changed: " + name)
        # Staged IR is a new independent candidate fragment, not normal code.
        if name.endswith("single_func_native_eh_private_leaf_stage.h"):
            continue
        actual_off, blocks = project.project_off(captured[name])
        original_off, _ = project.project_off(original)
        if actual_off != original_off or blocks == 0:
            raise ValueError("ordinary off text changed: " + name)
        projections[name] = dict(before=expected, off=pin(actual_off), blocks=blocks,
                                 actual_before_off=pin(original_off), byte_equal=True, text_only=True)
    for name in [LIB + "uwvm_runtime_debug_activation_api.h", LIB + "uwvm_runtime_debug_source_api.h"]:
        original = (repo / ROOT_BEFORE / "before" / name).read_bytes()
        actual_off, blocks = project.project_off(captured[name])
        original_off, _ = project.project_off(original)
        if actual_off != original_off or blocks == 0:
            raise ValueError("root management off text changed: " + name)
        projections[name] = dict(before=pin(original), off=pin(actual_off), blocks=blocks,
                                 actual_before_off=pin(original_off), byte_equal=True, text_only=True)
    for name, raw in captured.items():
        if (repo / name).read_bytes() != raw:
            raise ValueError("concurrent selected source mutation: " + name)
    args.out.mkdir(parents=True, exist_ok=False)
    snapshot = args.out / "snapshot"
    for name, raw in captured.items():
        path = snapshot / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    files = {name: pin(raw) for name, raw in captured.items()}
    manifest = save(args.out / "snapshot-manifest.json", files)
    receipt = save(args.out / "source-receipt.json", dict(
        schema="uwvm-native-eh-private-leaf-publication-source-v2", files=files,
        previous_staging_snapshot_manifest=pin(old_raw), actual_publication_before_manifest=pin(pub_before_raw),
        default_off_text=projections, native_compile=False, native_execution=False, performance_result=False,
        actual_source_id=None, complete_source_fingerprint_required_at_keeper=True,
        implementation="actual publisher/engine/explicit-ELF-st_size/FDE/ABI/rollback/drain/management source only",
        gate="UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF", exact_gate_value=1, request_default=False,
        observer_gate="UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER", observer_exact_gate_value=1,
        scope="Privately retained provenance is not loaded permission; native binding requires all actual final proofs."))
    plan = save(args.out / "cold-plan.json", dict(
        schema="uwvm-native-eh-private-leaf-publication-cold-plan-v2", source_only=True,
        snapshot_manifest=manifest, source_receipt=receipt, keeper="linux_fused_resume",
        actual_source_id=None, actual_compiler_argv=None, actual_runtime_argv=None, actual_executable_sha256=None,
        profiles=["production gates undefined/0/2 compile/import smoke",
            "PRIVATE_LEAF=1 observer undefined/0 import decline",
            "observer=1 private=1; runtime/fixture failure seam aligned; GC six experimental=0; source retention=0",
            "same aligned cold component under ASan/UBSan; LLVM/JIT instrumentation limits recorded"],
        fixture="test/0017.runtime/llvm_native_eh_private_leaf_publication_runtime.cc",
        wat="test/0014.llvm_jit/fixtures/native_eh_private_leaf_publication_valid.wat",
        commands=["<actual-fixture> <same-official-valid.wasm> " + mode + " " + workers
                  for workers in ["serial", "parallel"]
                  for mode in ["public", "private", "rollback", "debug-decline", "instruction-decline"]],
        separate_expected_fatal="<actual-fixture> <same-official-valid.wasm> uncaught serial",
        fatal_requirements=["nonzero original runtime fatal", "Uncaught WebAssembly exception", "payload[0] i32 bits=0x; independently decode raw bits 0x31 and signed=49",
            "Wasm call stack captured at throw", "original public leaf func_idx=0", "no fixture PASS"],
        mandatory=["same bytes official encoder/validator and Wasmtime; exact tag payloads 49/82/107",
            "actual positive private loaded binding and one clone; real selected CallBase type/CC/args/identity",
            "explicit ELF st_size exact relocated text extent and exact-start complete registered FDE",
            "forced failure only after real FDE proof; engine/listener retire then original public IR retry",
            "public/ref-retaining callers stay ordinary; no nounwind; native CFI/cleanup intact",
            "prepare/configure/observer/native metadata/source metadata/replacement reject active private publication",
            "stop/drain clears owners/validation; new genuine source request defaults false",
            "strict actual late invalid fused function check from existing approved Stage2/staging fixtures",
            "zero placeholders before execution; actual complete source/SDK/config_site/rsp/ELF/DSO before-after closure"],
        resources=dict(memory_max_bytes=64 * 1024**3, memory_swap_max_bytes=0, compile_cpus="16-31",
            guest_cpu=0, serialized=True, fresh_actual_scope_identity=True, temperature="observation-only"),
        outputs=["actual-source-before.json", "actual-source-after.json", "compiler.argv", "runtime.argv",
            "compiler.log", "sdk-closure.json", "official-encoder.log", "official-validator.log", "wasmtime-oracle.log",
            "public/private/rollback/decline-serial-parallel logs", "uncaught.log", "sanitizers.log", "closure.json"],
        no_performance_claim=True, all_native_qualification_pending=True))
    print(json.dumps(dict(snapshot_manifest=manifest, source_receipt=receipt, cold_plan=plan,
                          selected_files=len(selected), native_execution=False)))


if __name__ == "__main__":
    main()
