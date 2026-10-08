#!/usr/bin/env python3
"""Source/hash/text inspection only. Never launches compiler, VM or encoder."""
import argparse
import hashlib
import json
import pathlib
import re

GATE = "UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER"
BASE = "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/"
BEFORE = "build/wasm3-evidence/source-only-native-eh-observer-before-20261002"
ADDED = [BASE + "translate/single_func_native_eh_leaf_observer.h",
         "test/0014.llvm_jit/llvm_native_eh_leaf_observer_fused.cc",
         "test/0014.llvm_jit/fixtures/native_eh_leaf_observer_valid.wat",
         "test/0014.llvm_jit/fixtures/native_eh_leaf_observer_invalid.wat",
         "test/0014.llvm_jit/prepare_native_eh_leaf_observer_source.py",
         "test/0017.runtime/llvm_native_eh_leaf_observer_stage2_implementation_20261002.md"]


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def pin(path):
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": digest(raw)}


def project_off(raw):
    """Only remove exact reviewed gate blocks; keep their existing #else path.

    This is a text projection, not a C++ preprocessor or assembly check.
    All other directives and bytes stay intact, including ROS pruning.
    """
    out = []
    in_gate = False
    depth = 0
    keep = False
    count = 0
    gate = re.compile(rb"^\s*#\s*if\s+defined\(" + GATE.encode() +
                      rb"\)\s*&&\s*" + GATE.encode() + rb"\s*==\s*1\s*$")
    for line in raw.splitlines(keepends=True):
        if not in_gate:
            if gate.match(line.rstrip(b"\r\n")):
                in_gate = True
                depth = 0
                keep = False
                count += 1
            else:
                out.append(line)
            continue
        match = re.match(rb"^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b", line)
        word = match.group(1) if match else None
        if word in (b"if", b"ifdef", b"ifndef"):
            depth += 1
        elif word == b"endif":
            if depth == 0:
                in_gate = False
                continue
            depth -= 1
        elif word == b"else" and depth == 0:
            if keep:
                raise ValueError("duplicate gate else")
            keep = True
            continue
        elif word == b"elif" and depth == 0:
            raise ValueError("unreviewed gate elif")
        if keep:
            out.append(line)
    if in_gate:
        raise ValueError("unterminated observer gate")
    return b"".join(out), count


def write(path, value):
    raw = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()
    path.write_bytes(raw)
    return {"path": str(path), **pin(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()
    repo = args.repo.resolve(strict=True)
    original = repo / BEFORE
    before = json.loads((original / "manifest.json").read_text())
    files = {}
    for relative, expected in before["files"].items():
        current = (repo / relative).read_bytes()
        if expected.get("unchanged"):
            if digest(current) != expected["sha256"] or len(current) != expected["bytes"]:
                raise ValueError("frozen independent header changed: " + relative)
            files[relative] = {**pin(repo / relative), "independent_header_unchanged": True}
            continue
        image = (original / relative).read_bytes()
        if digest(image) != expected["sha256"] or len(image) != expected["bytes"]:
            raise ValueError("before-image changed: " + relative)
        projected, blocks = project_off(current)
        if projected != image or blocks == 0:
            raise ValueError("observer-off text differs from captured before-image: " + relative)
        files[relative] = {**pin(repo / relative), "before": expected,
                           "exact_gate_blocks": blocks, "off_projection_sha256": digest(projected),
                           "off_projection_byte_equal": True}
    for relative in ADDED:
        files[relative] = {**pin(repo / relative), "new_source_file": True}
    receipt = dict(schema="uwvm-native-eh-fused-observer-source-v1", gate=GATE,
                   exact_gate_value=1, request_default=False, files=files,
                   before_manifest=pin(original / "manifest.json"),
                   scope="source implementation plus hash/text projection only",
                   native_compile=False, wasm_encoder=False, oracle=False, guest_execution=False,
                   performance_result=False, executable_permission=False,
                   limitation="Byte projection is not preprocessing, assembly or native validation. Later shared-file debug changes require a distinct actual snapshot.")
    plan = dict(schema="uwvm-native-eh-fused-observer-cold-plan-v1", source_only=True,
                source_manifest_sha256=None, actual_source_id=None, compiler_argv=None,
                compiler_sha256=None, runtime_argv=None, runtime_sha256=None, executable_sha256=None,
                keeper="linux_fused_resume", resources=dict(memory_max_bytes=64 * 1024**3,
                    memory_swap_max_bytes=0, compile_cpus="16-31", guest_cpu=0,
                    serialized=True, no_local_compile_or_vm=True),
                fixture="test/0014.llvm_jit/llvm_native_eh_leaf_observer_fused.cc",
                independent_candidates=dict(r11_collector=False, precise_trace_metadata=False,
                    numeric_struct_set32=False, six_gc_experiments=False),
                configurations=["default undefined gate: compile/import original path",
                    "gate=0: compile/import original path", "gate=1: actual source fused fixture O3",
                    "gate=1: actual source fused fixture ASan/UBSan", "gate=1: named C++ module imports where current toolchain supports them"],
                mandatory_controls=["official encode valid.wat; official validate+Wasmtime same bytes",
                    "official encode invalid.wat with no validation; official validator and Wasmtime reject same bytes",
                    "valid actual parsed initialized owner; serial and 2-worker fused compile; IR on/off byte equality",
                    "five actual functions; two emitted ordinary call witnesses; one consumed effect edge",
                    "two escaping tags; local consuming catch; first catch_ref blocks consumption; unreachable call has no emitted witness",
                    "instruction quota zero and absent owner decline observations, preserve full original IR",
                    "invalid late function rejects compilation; no complete observation; attempt slot restored; validation epoch remains zero",
                    "source/compiler/runtime/ELF/dependency before-after hashes and exact resource ownership; no concurrent VM/build/profiler"],
                output_files=["actual-source-before.json", "actual-source-after.json", "compiler.argv",
                    "runtime.argv", "compiler.log", "oracle-valid.log", "oracle-invalid.log",
                    "serial.log", "parallel.log", "sanitizers.log", "closure.json"],
                no_stage3_claim="No clone, nounwind, trace omission, EH bridge change or executable admission.")
    args.out.mkdir(parents=True, exist_ok=False)
    saved = write(args.out / "source-receipt.json", receipt)
    plan["source_manifest_sha256"] = saved["sha256"]
    result = write(args.out / "cold-plan.json", plan)
    print(json.dumps({"source_receipt": saved, "cold_plan": result, "native_execution": False}))


if __name__ == "__main__":
    main()
