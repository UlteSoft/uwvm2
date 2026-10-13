#!/usr/bin/env python3
"""Pin a blocked current-source measurement plan. No build, guest, SSH or profiler."""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SIX = ("COMPACT_NUMERIC", "MANAGED_NUMERIC_PAGE", "SEALED_COMPACT_CURSOR",
       "SEALED_LOCAL_TABLE", "PENDING_NUMERIC_FUSED_CATCH", "PACKED_NUMERIC_ARRAYS")
EXTRA = ("PRECISE_GC_TRACE_METADATA", "NUMERIC_STRUCT_SET32",
         "NATIVE_EH_LEAF_OBSERVER", "NATIVE_EH_PRIVATE_LEAF")
FIXED_RUNNERS = {
    "run_current_host_pcore_hw_counting.py": "673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5",
    "run_current_pcore_hw_counting.py": "266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8",
}
PERF_SOURCE = (
    "benchmark/0004.wasm3-core/generate_general_gc.py",
    "benchmark/0004.wasm3-core/check_general_gc_generator.py",
    "benchmark/0004.wasm3-core/prepare_general_gc_hot_path.py",
    "benchmark/0004.wasm3-core/generate_native_eh_leaf_benchmark.py",
    "benchmark/0004.wasm3-core/prepare_current_vm_perf_source.py",
    "benchmark/0004.wasm3-core/CURRENT_VM_THREE_FAMILY_PLAN_20261002.md",
    "test/0017.runtime/llvm_native_eh_private_leaf_benchmark.cc",
    "src/uwvm2/uwvm/runtime/storage/gc_object.h",
    "src/uwvm2/uwvm/runtime/storage/compact_numeric/descriptor.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_struct_set32.h",
)


def pin(raw):
    return dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def prepare(repo, frozen):
    raw_manifest = (frozen / "snapshot-manifest.json").read_bytes()
    manifest = json.loads(raw_manifest)
    if not isinstance(manifest, dict) or len(manifest) != 50:
        raise ValueError("expected reviewed publication V3's 50 actual source files")
    for name, expected in manifest.items():
        path = Path(name)
        if path.is_absolute() or ".." in path.parts or pin((frozen / "snapshot" / path).read_bytes()) != expected:
            raise ValueError("frozen actual source pin differs: " + name)
    captured = {name: (repo / name).read_bytes() for name in PERF_SOURCE}
    for name in FIXED_RUNNERS:
        raw = (HERE / name).read_bytes()
        if pin(raw)["sha256"] != FIXED_RUNNERS[name]:
            raise ValueError("immutable historical counting runner changed")
    for name, raw in captured.items():
        if name.endswith(".py"):
            ast.parse(raw.decode())
    spec = importlib.util.spec_from_file_location("native_eh_pure_scalar_plan", HERE / "generate_native_eh_leaf_benchmark.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    cases = []
    for n in (65536, 8000000):
        for bad in (False, True):
            wat, case = generator.build(n, bad)
            if case["expected_checksum_u32"] != generator.expected(n)[0]:
                raise AssertionError("scalar workload contract differs")
            cases.append(dict(name="native-eh-leaf-" + str(n) + ("-bad-checksum" if bad else ""),
                              actual_wat=pin(wat), **case))
    profiles = []
    for selected in (None, "PRECISE_GC_TRACE_METADATA", "NUMERIC_STRUCT_SET32", "experiment6"):
        macros = {"UWVM_EXPERIMENTAL_" + name: int(selected == name or selected == "experiment6" and name in SIX)
                  for name in SIX + EXTRA}
        profiles.append(dict(name=selected or "default", same_values_in_runtime_and_consumer=True,
                             declared_macros=macros, actual_compiler_argv=None,
                             reject_unlisted_active_experimental_macros=True,
                             actual_source_id=None, actual_runtime_sha256=None, actual_executable_sha256=None))
    # The EH pair uses one actual RT/harness binary, changing only its real
    # canonical-source request after initialization. This is not a six-macro A/B.
    eh_macros = {"UWVM_EXPERIMENTAL_" + name: int(name in EXTRA[-2:]) for name in SIX + EXTRA}
    for name, raw in captured.items():
        if (repo / name).read_bytes() != raw:
            raise ValueError("performance source changed during capture: " + name)
    return dict(schema="uwvm-current-vm-three-measurement-source-plan-v1", execute_ready=False,
                source_only=True, native_compile=False, native_execution=False, performance_qualified=False,
                source_pins={name: pin(raw) for name, raw in captured.items()},
                reviewed_publication_manifest=pin(raw_manifest), complete_actual_source_id=None,
                complete_compiler_sdk_config_site_rsp_dso_closure=None,
                immutable_historical_counting_runners=FIXED_RUNNERS,
                old_runners_accept_new_workloads=False, new_runner_review_required=True,
                gc_profiles=profiles, gc_families=["mutable-struct", "reference-cycle", "numeric-array", "reference-array"],
                gc_phases=["allocate", "mutate"], cold_general_iterations=65536,
                first_long_general_iterations=2000000,
                native_eh_cases=cases,
                native_eh_profile=dict(declared_macros=eh_macros, force_cfi_failure_seam=False,
                    reject_unlisted_active_experimental_macros=True,
                    actual_runtime_argv=None, actual_harness_argv=None, actual_binary_sha256=None,
                    actual_cold_receipt=None, public_source_request=False, private_source_request=True,
                    require_actual_private_binding=True, required_private_clone_count=1, guest_runs_per_process=1),
                first_sample_order=["ordinary/public", "ordinary/private", "ordinary/private", "ordinary/public",
                                    "ros/public", "ros/private", "ros/private", "ros/public"],
                measurement_families=[
                    dict(name="unprofiled", wall=True, wait4_user_system=True, rss=True,
                         internal_interval="actual CLI guest time or harness fused/JIT/entry total; distinct labels",
                         actual_results=None),
                    dict(name="pure-hardware", domain="wholeguest single TID including startup/JIT/one run/teardown",
                         events=["cpu_core/event=0x3c/", "cpu_core/event=0xc0/"], no_sampling=True,
                         no_software_events=True, no_inherit=True, exact_enabled_equals_running=True,
                         actual_event_attributes=None, actual_counts=None),
                    dict(name="vtune-hardware", cpu_mask="0", actual_installed_help_required=True,
                         actual_mux_and_stack_warnings_required=True, unknown_jit_remains_unknown=True,
                         profiler_time_is_engine_time=False, actual_collection=None)],
                resource_contract=dict(memory_max_bytes=64 << 30, swap_max_bytes=0, compile_cpus="16-31",
                    allowed_cpus="0,2,4,6,16-31", guest_cpu=0, temperature="observation-only",
                    pair_median_frequency_max_ratio=1.1, p0_smt_host_noise="actual observer; unknown stays unknown",
                    fresh_scope_admission=None, serialized_no_compiler_vm_or_profiler_overlap=True),
                industry=dict(first_same_binary_control="actual installed Wasmtime49 copying; fresh ELF/help/cold recheck",
                    other_wasm_controls=["WasmEdge", "Wasmer", "WAVM"], actual_tool_pins=None,
                    gc_and_try_table_capability_must_be_executed=True,
                    unsupported_mode_is_not_timing=True, drc_cyclic_reclamation_required=True,
                    language_algorithm_controls_separate=["Java/OpenJDK/GraalVM", "JavaScript/V8", "C#/CoreCLR"],
                    same_binary_comparison_results=None, language_comparison_results=None))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--frozen", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    result = prepare(args.repo.resolve(strict=True), args.frozen.resolve(strict=True))
    raw = (json.dumps(result, sort_keys=True, indent=2) + "\n").encode()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("xb") as stream:
        stream.write(raw)
    print(json.dumps(dict(path=str(args.out), **pin(raw), execute_ready=False)))


if __name__ == "__main__":
    main()
